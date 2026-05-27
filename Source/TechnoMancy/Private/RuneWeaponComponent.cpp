// Copyright TechnoMancy. All rights reserved.

#include "RuneWeaponComponent.h"
#include "RuneInterpreter.h"
#include "RuneSpellAsset.h"
#include "RuneProjectile.h"
#include "TechnoMancyLog.h"
#include "GameFramework/Actor.h"

URuneWeaponComponent::URuneWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true); // Phase 4 will wire actual replicated properties
}

void URuneWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplySpellAssetDefaults();
	CurrentMana = MaxMana;

	if (!SpellSource.IsEmpty())
	{
		TArray<FString> Errors;
		if (!CompileSpell(SpellSource, Errors))
		{
			for (const FString& E : Errors)
			{
				UE_LOG(LogTechnoMancy, Warning, TEXT("[RuneWeapon] compile failed: %s"), *E);
			}
		}
	}

	SetComponentTickEnabled(bAllowTickSpell);
}

void URuneWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bAllowTickSpell && bReady)
	{
		TArray<FString> Errors;
		InvokeTick(DeltaTime, Errors);
		// Tick errors are swallowed to avoid log spam — a broken Tick simply stops working.
	}
}

void URuneWeaponComponent::ApplySpellAssetDefaults()
{
	if (!SpellAsset) return;

	if (SpellSource.IsEmpty())
	{
		SpellSource = SpellAsset->SourceCode;
	}
	// Property overrides honored if explicitly set in the details panel.
	if (StorageCapacity == 128 && SpellAsset->StorageCapacity > 0)
	{
		StorageCapacity = SpellAsset->StorageCapacity;
	}
	if (FMath::IsNearlyEqual(MaxMana, 50.f) && SpellAsset->MaxMana > 0.f)
	{
		MaxMana = SpellAsset->MaxMana;
	}
}

bool URuneWeaponComponent::CompileSpell(const FString& NewSource, TArray<FString>& OutErrors)
{
	if (NewSource.Len() > StorageCapacity)
	{
		OutErrors.Add(FString::Printf(
			TEXT("Spell is %d characters — storage limit is %d"),
			NewSource.Len(), StorageCapacity));
		bReady = false;
		return false;
	}

	if (!Interpreter.IsValid())
	{
		Interpreter = MakeShared<FRuneInterpreter>();
	}

	bReady = Interpreter->Compile(NewSource, OutErrors);
	if (bReady)
	{
		SpellSource = NewSource;
		UE_LOG(LogTechnoMancy, Display, TEXT("[RuneWeapon] compiled %d chars"), NewSource.Len());
	}
	return bReady;
}

void URuneWeaponComponent::SetLastHit(AActor* HitTarget, FVector HitPos)
{
	Context.LastHitTarget = HitTarget;
	Context.LastHitPos = HitPos;
}

void URuneWeaponComponent::RefreshContext()
{
	Context.WeaponOwner = GetOwner();
	Context.MaxMana = MaxMana;
	Context.ProjectileClass = ProjectileClass.Get() ? ProjectileClass.Get() : ARuneProjectile::StaticClass();
	if (AActor* Owner = GetOwner())
	{
		Context.AimDirection = Owner->GetActorForwardVector();
	}
}

void URuneWeaponComponent::RefillMana()
{
	CurrentMana = MaxMana;
}

namespace
{
	bool InvokeEntry(URuneWeaponComponent& Self,
					 TSharedPtr<FRuneInterpreter> Interp,
					 FRuneWeaponContext& Ctx,
					 FName EntryPoint,
					 float& CurrentMana,
					 TArray<FString>& OutErrors)
	{
		if (!Interp.IsValid())
		{
			OutErrors.Add(TEXT("Weapon is not ready (no interpreter)"));
			return false;
		}
		Ctx.Mana = CurrentMana;
		const bool bOk = Interp->Invoke(EntryPoint, Ctx, OutErrors);
		CurrentMana = Ctx.Mana;
		return bOk;
	}
}

bool URuneWeaponComponent::InvokePrimary(TArray<FString>& OutErrors)
{
	if (!bReady) { OutErrors.Add(TEXT("Weapon is not ready")); return false; }
	RefreshContext();
	return InvokeEntry(*this, Interpreter, Context, FName(TEXT("Primary")), CurrentMana, OutErrors);
}

bool URuneWeaponComponent::InvokeOnHit(TArray<FString>& OutErrors)
{
	if (!bReady) { OutErrors.Add(TEXT("Weapon is not ready")); return false; }
	RefreshContext();
	return InvokeEntry(*this, Interpreter, Context, FName(TEXT("OnHit")), CurrentMana, OutErrors);
}

bool URuneWeaponComponent::InvokeOnKill(TArray<FString>& OutErrors)
{
	if (!bReady) { OutErrors.Add(TEXT("Weapon is not ready")); return false; }
	RefreshContext();
	return InvokeEntry(*this, Interpreter, Context, FName(TEXT("OnKill")), CurrentMana, OutErrors);
}

bool URuneWeaponComponent::InvokeTick(float DeltaTime, TArray<FString>& OutErrors)
{
	if (!bReady) return true; // Tick is silently no-op when not ready
	RefreshContext();
	Context.DeltaTime = DeltaTime;
	return InvokeEntry(*this, Interpreter, Context, FName(TEXT("Tick")), CurrentMana, OutErrors);
}

float URuneWeaponComponent::GetManaEstimate(FName EntryPoint) const
{
	return Interpreter.IsValid() ? Interpreter->EstimateManaCost(EntryPoint) : 0.f;
}
