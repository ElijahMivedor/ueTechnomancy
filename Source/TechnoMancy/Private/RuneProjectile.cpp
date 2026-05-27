// Copyright TechnoMancy. All rights reserved.

#include "RuneProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ARuneProjectile::ARuneProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(15.f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	RootComponent = CollisionSphere;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 1000.f;
	ProjectileMovement->MaxSpeed = 1000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ARuneProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (CollisionSphere)
	{
		CollisionSphere->OnComponentHit.AddDynamic(this, &ARuneProjectile::OnProjectileHit);
	}
	SetLifeSpan(Lifetime);
}

void ARuneProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
									  UPrimitiveComponent* OtherComp, FVector NormalImpulse,
									  const FHitResult& Hit)
{
	if (!HasAuthority()) return;
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	UGameplayStatics::ApplyDamage(OtherActor, Damage, nullptr, this, nullptr);

	if (PierceCount > 0) --PierceCount;
	else                 Destroy();
}
