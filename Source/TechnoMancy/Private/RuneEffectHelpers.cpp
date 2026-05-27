// Copyright TechnoMancy. All rights reserved.

#include "RuneEffectHelpers.h"
#include "RuneProjectile.h"
#include "TechnoMancyLog.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	UWorld* WorldOf(const FRuneWeaponContext& Ctx)
	{
		return Ctx.WeaponOwner.IsValid() ? Ctx.WeaponOwner->GetWorld() : nullptr;
	}

	FVector SpawnLocationFor(const FRuneWeaponContext& Ctx, float ForwardOffset = 100.f)
	{
		if (!Ctx.WeaponOwner.IsValid()) return FVector::ZeroVector;
		const FVector Origin = Ctx.WeaponOwner->GetActorLocation();
		const FVector Forward = Ctx.AimDirection.IsNearlyZero()
			? Ctx.WeaponOwner->GetActorForwardVector()
			: Ctx.AimDirection.GetSafeNormal();
		return Origin + Forward * ForwardOffset;
	}
}

namespace TechnoMancy::Effects
{

AActor* SpawnProjectile(FRuneWeaponContext& Ctx,
						FName Element,
						float Speed,
						float Damage,
						int32 PierceCount,
						float SizeScale)
{
	UWorld* World = WorldOf(Ctx);
	if (!World) return nullptr;

	UClass* Class = Ctx.ProjectileClass ? Ctx.ProjectileClass : ARuneProjectile::StaticClass();
	if (!Class) return nullptr;

	const FVector Loc = SpawnLocationFor(Ctx);
	const FVector Forward = Ctx.AimDirection.IsNearlyZero()
		? (Ctx.WeaponOwner.IsValid() ? Ctx.WeaponOwner->GetActorForwardVector() : FVector::ForwardVector)
		: Ctx.AimDirection.GetSafeNormal();
	const FRotator Rot = Forward.Rotation();

	FActorSpawnParameters Params;
	Params.Owner = Ctx.WeaponOwner.Get();
	Params.Instigator = Cast<APawn>(Ctx.WeaponOwner.Get());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(Class, Loc, Rot, Params);
	if (!Spawned) return nullptr;

	if (ARuneProjectile* Proj = Cast<ARuneProjectile>(Spawned))
	{
		Proj->Element = Element;
		Proj->Damage = Damage;
		Proj->PierceCount = PierceCount;
		if (Proj->ProjectileMovement)
		{
			const float SpeedCms = Speed * UnitScale;
			Proj->ProjectileMovement->InitialSpeed = SpeedCms;
			Proj->ProjectileMovement->MaxSpeed = SpeedCms;
			Proj->ProjectileMovement->Velocity = Forward * SpeedCms;
		}
	}

	if (!FMath::IsNearlyEqual(SizeScale, 1.f))
	{
		Spawned->SetActorScale3D(FVector(SizeScale));
	}

	UE_LOG(LogTechnoMancy, Log, TEXT("[Spell] spawned projectile %s element=%s dmg=%.1f spd=%.1f pierce=%d"),
		*Class->GetName(), *Element.ToString(), Damage, Speed, PierceCount);
	return Spawned;
}

int32 DamageSphere(FRuneWeaponContext& Ctx, FVector WorldPos, float RadiusUnits, float Damage)
{
	UWorld* World = WorldOf(Ctx);
	if (!World) return 0;

	const float RadiusCm = RadiusUnits * UnitScale;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RuneDamageSphere), false, Ctx.WeaponOwner.Get());

	World->OverlapMultiByObjectType(
		Overlaps,
		WorldPos,
		FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeSphere(RadiusCm),
		Query);

	int32 Hits = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == Ctx.WeaponOwner.Get()) continue;
		UGameplayStatics::ApplyDamage(Target, Damage, nullptr, Ctx.WeaponOwner.Get(), nullptr);
		++Hits;
	}

	UE_LOG(LogTechnoMancy, Log, TEXT("[Spell] sphere damage @ %s r=%.1f dmg=%.1f hits=%d"),
		*WorldPos.ToCompactString(), RadiusUnits, Damage, Hits);
	return Hits;
}

void DamageLastHit(FRuneWeaponContext& Ctx, float Damage)
{
	if (!Ctx.LastHitTarget.IsValid()) return;
	UGameplayStatics::ApplyDamage(Ctx.LastHitTarget.Get(), Damage, nullptr, Ctx.WeaponOwner.Get(), nullptr);
}

void ApplyKnockback(FRuneWeaponContext& Ctx, FVector WorldPos, float RadiusUnits, float Force)
{
	UWorld* World = WorldOf(Ctx);
	if (!World) return;

	const float RadiusCm = RadiusUnits * UnitScale;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RuneKnockback), false, Ctx.WeaponOwner.Get());

	World->OverlapMultiByObjectType(
		Overlaps,
		WorldPos,
		FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeSphere(RadiusCm),
		Query);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Target == Ctx.WeaponOwner.Get()) continue;

		const FVector Dir = (Target->GetActorLocation() - WorldPos).GetSafeNormal2D();
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Target->GetRootComponent()))
		{
			if (Prim->IsSimulatingPhysics())
			{
				Prim->AddImpulse(Dir * Force, NAME_None, true);
				continue;
			}
		}
		// Fallback: launch character if applicable
		if (ACharacter* Char = Cast<ACharacter>(Target))
		{
			Char->LaunchCharacter(Dir * Force, true, true);
		}
	}
}

AActor* LineDamage(FRuneWeaponContext& Ctx, float RangeUnits, float Damage)
{
	UWorld* World = WorldOf(Ctx);
	if (!World || !Ctx.WeaponOwner.IsValid()) return nullptr;

	const FVector Start = Ctx.WeaponOwner->GetActorLocation();
	const FVector Forward = Ctx.AimDirection.IsNearlyZero()
		? Ctx.WeaponOwner->GetActorForwardVector()
		: Ctx.AimDirection.GetSafeNormal();
	const FVector End = Start + Forward * (RangeUnits * UnitScale);

	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RuneLineDamage), false, Ctx.WeaponOwner.Get());
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Query)) return nullptr;

	AActor* HitActor = Hit.GetActor();
	if (HitActor && HitActor != Ctx.WeaponOwner.Get())
	{
		UGameplayStatics::ApplyDamage(HitActor, Damage, nullptr, Ctx.WeaponOwner.Get(), nullptr);
		Ctx.LastHitTarget = HitActor;
		Ctx.LastHitPos = Hit.ImpactPoint;
	}
	return HitActor;
}

int32 CountActorsInSphere(FRuneWeaponContext& Ctx, FVector WorldPos, float RadiusUnits)
{
	UWorld* World = WorldOf(Ctx);
	if (!World) return 0;

	const float RadiusCm = RadiusUnits * UnitScale;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RuneCountActors), false, Ctx.WeaponOwner.Get());
	World->OverlapMultiByObjectType(
		Overlaps, WorldPos, FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects,
		FCollisionShape::MakeSphere(RadiusCm),
		Query);

	int32 Count = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* A = Overlap.GetActor();
		if (A && A != Ctx.WeaponOwner.Get()) ++Count;
	}
	return Count;
}

} // namespace TechnoMancy::Effects
