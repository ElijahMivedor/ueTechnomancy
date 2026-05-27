// Copyright TechnoMancy. All rights reserved.
//
// Blueprint façade for TechnoMancy. All static, all callable from BP graphs.
// Phase 3: validation, mana estimation, library introspection, transient asset
// creation. Phase 2-deferred items (RegisterBlueprintLibraryFunction) are not
// here yet.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RuneTypes.h"
#include "RuneScriptBPLib.generated.h"

class URuneSpellAsset;

UCLASS()
class TECHNOMANCY_API URuneScriptBPLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Lex + parse the source. Returns true if compilation would succeed; errors populated otherwise. */
	UFUNCTION(BlueprintCallable, Category = "TechnoMancy",
			  meta = (AutoCreateRefTerm = "OutErrors"))
	static bool ValidateSpellSource(const FString& Source, TArray<FString>& OutErrors);

	/** Snapshot of every registered library function (use for terminal autocomplete / docs). */
	UFUNCTION(BlueprintCallable, Category = "TechnoMancy")
	static TArray<FRuneLibraryEntry> GetLibraryFunctionList();

	/** Static mana cost estimate for an entry point. 0 if the source fails to compile or function is missing. */
	UFUNCTION(BlueprintCallable, Category = "TechnoMancy")
	static float GetManaEstimate(const FString& Source, FName EntryPoint);

	/**
	 * Create a transient URuneSpellAsset (in-memory only — not saved to disk).
	 * Useful for runtime-authored spells or unit tests. Pass any UObject (typically a
	 * GameInstance or PlayerController) as WorldContext.
	 */
	UFUNCTION(BlueprintCallable, Category = "TechnoMancy",
			  meta = (WorldContext = "WorldContextObject"))
	static URuneSpellAsset* CreateRuneSpellAsset(UObject* WorldContextObject,
												 const FString& Source,
												 int32 StorageCapacity = 128,
												 float MaxMana = 50.f);

	UFUNCTION(BlueprintPure, Category = "TechnoMancy")
	static int32 GetSpellCharacterCount(const FString& Source);

	UFUNCTION(BlueprintPure, Category = "TechnoMancy")
	static int32 GetSpellStorageRemaining(const FString& Source, int32 StorageCapacity);
};
