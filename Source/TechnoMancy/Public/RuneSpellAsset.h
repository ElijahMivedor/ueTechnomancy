// Copyright TechnoMancy. All rights reserved.
//
// Data-asset wrapper for a single RuneScript spell. Designers create these in
// the Content Browser (right-click → Miscellaneous → Data Asset → RuneSpellAsset)
// and assign them to URuneWeaponComponent::SpellAsset. The editor authoring
// utility (Phase 6) creates / edits these directly.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RuneSpellAsset.generated.h"

UCLASS(BlueprintType)
class TECHNOMANCY_API URuneSpellAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Raw RuneScript source. Multi-line by convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript", meta = (MultiLine = true))
	FString SourceCode;

	/** Maximum source length, in characters. Compile fails if source exceeds this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript", meta = (ClampMin = 16, ClampMax = 4096))
	int32 StorageCapacity = 128;

	/** Mana pool size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript", meta = (ClampMin = 0))
	float MaxMana = 50.f;

	/** Designer-facing name shown in the terminal header. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	FText DisplayName;

	/** Flavour text / designer notes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript", meta = (MultiLine = true))
	FText Description;
};
