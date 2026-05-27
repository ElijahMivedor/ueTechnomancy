// Copyright TechnoMancy. All rights reserved.
//
// Bare projectile base class for library callbacks to spawn. Game projects
// subclass this in Blueprint or C++ to add meshes, VFX, and project-specific
// damage handling. Replicates by default so projectiles spawned on the server
// are visible to all clients.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuneProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UPrimitiveComponent;

UCLASS(BlueprintType, Blueprintable)
class TECHNOMANCY_API ARuneProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARuneProjectile();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Damage applied to actors hit by this projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	float Damage = 10.f;

	/** Element tag (e.g. "fire", "ice") for downstream damage handling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	FName Element = NAME_None;

	/** Additional targets this projectile can pierce before despawning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	int32 PierceCount = 0;

	/** Lifetime in seconds before auto-despawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	float Lifetime = 5.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
						 UPrimitiveComponent* OtherComp, FVector NormalImpulse,
						 const FHitResult& Hit);
};
