// Copyright TechnoMancy. All rights reserved.
//
// Drop-in component that gives any actor a programmable spell. Compiles
// RuneScript at runtime, invokes the script's entry points (`Primary`,
// `OnHit`, `OnKill`, `Tick`), and tracks per-cast mana.
//
// Phase 3 wires CompileSpell / Invoke* and the FRuneWeaponContext bridge.
// Phase 4 (multiplayer) adds Server_RequestCompile RPC, OnRep_SpellSource,
// CurrentMana / SpellSource replication, and HasAuthority guards.
// Phase 6 (designer presets) adds DefaultSpellAsset, bAllowPlayerEdits,
// ResetToDefault().

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RuneTypes.h"
#include "RuneWeaponComponent.generated.h"

class FRuneInterpreter;
class URuneSpellAsset;
class ARuneProjectile;

UCLASS(ClassGroup = (TechnoMancy), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class TECHNOMANCY_API URuneWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URuneWeaponComponent();

	// =========================================================================
	// Designer-facing properties
	// =========================================================================

	/** Optional spell preset. If set, BeginPlay pulls SourceCode / StorageCapacity / MaxMana from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	TObjectPtr<URuneSpellAsset> SpellAsset;

	/** Current spell source. Authored at runtime via CompileSpell. Replicated in Phase 4. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript", meta = (MultiLine = true))
	FString SpellSource;

	/** Maximum source length, in characters. Overrides SpellAsset if non-zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuneScript", meta = (ClampMin = 16, ClampMax = 4096))
	int32 StorageCapacity = 128;

	/** Mana pool size. Overrides SpellAsset if > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuneScript", meta = (ClampMin = 0))
	float MaxMana = 50.f;

	/** Whether the script's `Tick()` entry point runs every frame. Opt-in for performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	bool bAllowTickSpell = false;

	/** Class spawned by `*.shot` / `*.lance` library callbacks. Defaults to ARuneProjectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	TSubclassOf<ARuneProjectile> ProjectileClass;

	/** Current mana. Replenished by mana.steal / mana.* libraries; refilled via RefillMana. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript")
	float CurrentMana = 50.f;

	/** True once a valid spell has compiled. Invoke* short-circuits when false. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript")
	bool bReady = false;

	// =========================================================================
	// Blueprint API
	// =========================================================================

	/**
	 * Compile new source into the interpreter. On success, SpellSource is replaced
	 * and bReady is set true. Returns true on success; errors populated otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool CompileSpell(const FString& NewSource, TArray<FString>& OutErrors);

	/** Populate Context.LastHitTarget / LastHitPos prior to InvokeOnHit / InvokeOnKill. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void SetLastHit(AActor* HitTarget, FVector HitPos);

	/** Invoke `Primary` — typically the spell's main "fire" action. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokePrimary(TArray<FString>& OutErrors);

	/** Invoke `OnHit` — call after a projectile hit; set the hit info via SetLastHit first. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokeOnHit(TArray<FString>& OutErrors);

	/** Invoke `OnKill` — call after target death. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokeOnKill(TArray<FString>& OutErrors);

	/** Invoke `Tick(dt)` — called automatically every frame when bAllowTickSpell is true. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokeTick(float DeltaTime, TArray<FString>& OutErrors);

	/** Static mana cost estimate of the named entry point. 0 if no such function. */
	UFUNCTION(BlueprintPure, Category = "RuneScript")
	float GetManaEstimate(FName EntryPoint) const;

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	float GetCurrentMana() const { return CurrentMana; }

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	float GetMaxMana() const { return MaxMana; }

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	int32 GetStorageRemaining() const { return FMath::Max(0, StorageCapacity - SpellSource.Len()); }

	/** Refill mana to MaxMana. Utility for testing and ammo-style refills. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void RefillMana();

	/** Direct access to the underlying interpreter (read-only). */
	TSharedPtr<FRuneInterpreter> GetInterpreter() const { return Interpreter; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TSharedPtr<FRuneInterpreter> Interpreter;
	FRuneWeaponContext Context;

	/** Refresh transient context fields (owner, aim, projectile class). LastHit fields untouched. */
	void RefreshContext();

	/** Pull SourceCode / StorageCapacity / MaxMana from SpellAsset when overrides are unset. */
	void ApplySpellAssetDefaults();
};
