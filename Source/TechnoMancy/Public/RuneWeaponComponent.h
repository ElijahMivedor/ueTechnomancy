// Copyright TechnoMancy. All rights reserved.
//
// Drop-in component that gives any actor a programmable spell. Compiles
// RuneScript at runtime, invokes the script's entry points (Primary, OnHit,
// OnKill, Tick), and tracks per-cast mana.
//
// Server-authoritative: the interpreter only runs on the server. Clients
// observe CurrentMana / SpellSource (replicated) for HUD display; they call
// RequestCompile to push player-authored source up to the server.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RuneTypes.h"
#include "RuneWeaponComponent.generated.h"

class FRuneInterpreter;
class URuneSpellAsset;
class ARuneProjectile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRuneCompileResult, bool, bSuccess, const TArray<FString>&, Errors);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRuneSpellSourceChanged, const FString&, NewSource);

UCLASS(ClassGroup = (TechnoMancy), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class TECHNOMANCY_API URuneWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URuneWeaponComponent();

	// =========================================================================
	// Designer-facing properties
	// =========================================================================

	/**
	 * Designer-assigned preset spell. Loaded on BeginPlay when SpellSource is empty.
	 * Players cannot modify this asset; it is the immutable baseline.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript|Preset")
	TObjectPtr<URuneSpellAsset> DefaultSpellAsset;

	/** If false, RequestCompile is rejected — the spell is locked to the preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript|Preset")
	bool bAllowPlayerEdits = true;

	/**
	 * Optional player-facing spell asset. Authoritative source for SpellSource at BeginPlay
	 * if set and SpellSource is empty. Overrides DefaultSpellAsset when both are set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	TObjectPtr<URuneSpellAsset> SpellAsset;

	/** Replicated spell source. Authoritative on server; clients receive via OnRep_SpellSource. */
	UPROPERTY(ReplicatedUsing = OnRep_SpellSource, VisibleAnywhere, BlueprintReadOnly,
			  Category = "RuneScript", meta = (MultiLine = true))
	FString SpellSource;

	/** Maximum source length, in characters. Overrides asset when non-default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuneScript",
			  meta = (ClampMin = 16, ClampMax = 4096))
	int32 StorageCapacity = 128;

	/** Mana pool size. Overrides asset when > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuneScript", meta = (ClampMin = 0))
	float MaxMana = 50.f;

	/** Whether the script's `Tick()` entry point runs every frame. Opt-in for performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	bool bAllowTickSpell = false;

	/** Class spawned by `*.shot` / `*.lance` library callbacks. Defaults to ARuneProjectile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	TSubclassOf<ARuneProjectile> ProjectileClass;

	/** Replicated current mana. Server-authoritative; clients observe for HUD display. */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript")
	float CurrentMana = 50.f;

	/** True once a valid spell has compiled on the server. Invoke* short-circuits when false. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuneScript")
	bool bReady = false;

	// =========================================================================
	// Events
	// =========================================================================

	/** Fires on the requesting client after Server_RequestCompile completes. */
	UPROPERTY(BlueprintAssignable, Category = "RuneScript")
	FOnRuneCompileResult OnCompileResult;

	/** Fires on every client when SpellSource replicates. */
	UPROPERTY(BlueprintAssignable, Category = "RuneScript")
	FOnRuneSpellSourceChanged OnSpellSourceChanged;

	// =========================================================================
	// Compile / request API
	// =========================================================================

	/**
	 * Client / server entry point for compiling new source. Forwards to Server_RequestCompile
	 * when called on a client. Server callers may pass authority paths directly.
	 *
	 * The result arrives asynchronously on the calling client via OnCompileResult.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void RequestCompile(const FString& NewSource);

	/** Server-side direct compile. Server only; rejects on clients. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool CompileSpell(const FString& NewSource, TArray<FString>& OutErrors);

	/** Restore SpellSource from DefaultSpellAsset and recompile (server only, requires bAllowPlayerEdits). */
	UFUNCTION(BlueprintCallable, Category = "RuneScript|Preset")
	void ResetToDefault();

	UFUNCTION(BlueprintPure, Category = "RuneScript|Preset")
	bool HasCustomSpell() const;

	UFUNCTION(BlueprintPure, Category = "RuneScript|Preset")
	FText GetDefaultSpellName() const;

	// =========================================================================
	// Invocation
	// =========================================================================

	/** Populate Context.LastHitTarget / LastHitPos prior to InvokeOnHit / InvokeOnKill. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void SetLastHit(AActor* HitTarget, FVector HitPos);

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokePrimary(TArray<FString>& OutErrors);

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokeOnHit(TArray<FString>& OutErrors);

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokeOnKill(TArray<FString>& OutErrors);

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool InvokeTick(float DeltaTime, TArray<FString>& OutErrors);

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	float GetManaEstimate(FName EntryPoint) const;

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	float GetCurrentMana() const { return CurrentMana; }

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	float GetMaxMana() const { return MaxMana; }

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	int32 GetStorageRemaining() const { return FMath::Max(0, StorageCapacity - SpellSource.Len()); }

	/** Refill mana to MaxMana. Server only. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void RefillMana();

	/** Direct access to the underlying interpreter (read-only). Server-only; null on clients. */
	TSharedPtr<FRuneInterpreter> GetInterpreter() const { return Interpreter; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server RPC: client asks server to compile new source. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestCompile(const FString& NewSource);

	/** Client RPC: server reports compile result back to requesting client. */
	UFUNCTION(Client, Reliable)
	void Client_OnCompileResult(bool bSuccess, const TArray<FString>& Errors);

	UFUNCTION()
	void OnRep_SpellSource();

private:
	TSharedPtr<FRuneInterpreter> Interpreter; // server only
	FRuneWeaponContext Context;

	void RefreshContext();
	void ApplySpellAssetDefaults();

	/** Server-side helper invoked by both BeginPlay and Server_RequestCompile. */
	bool CompileSpellInternal(const FString& NewSource, TArray<FString>& OutErrors);
};
