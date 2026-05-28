// Copyright TechnoMancy. All rights reserved.
//
// Editor utility widget for authoring URuneSpellAsset content with full
// syntax highlighting and validation. Designers subclass this in the editor
// (EUW_RuneEditorTerminal.uasset) to compose the UMG layout — the C++ base
// supplies the New / Open / Save / Validate logic.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "RuneEditorTerminalWidget.generated.h"

class URuneSpellAsset;

UCLASS(BlueprintType, Blueprintable)
class URuneEditorTerminalWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	/** Currently open spell asset (null until NewSpell / OpenSpell). */
	UPROPERTY(BlueprintReadWrite, Category = "RuneScript")
	TObjectPtr<URuneSpellAsset> ActiveAsset;

	/** True if Source differs from ActiveAsset->SourceCode. */
	UPROPERTY(BlueprintReadWrite, Category = "RuneScript")
	bool bUnsavedChanges = false;

	/**
	 * Create a new URuneSpellAsset via IAssetTools and pop the Save dialog.
	 * Sets ActiveAsset to the created asset on success.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	URuneSpellAsset* NewSpell();

	/** Open the Content Browser asset picker filtered to URuneSpellAsset. Sets ActiveAsset. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	URuneSpellAsset* OpenSpell();

	/**
	 * Save the provided source / capacity / mana / name into ActiveAsset and mark its package dirty.
	 * Returns true on success; false if ActiveAsset is null.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool SaveSpell(const FString& Source, int32 StorageCapacity, float MaxMana, const FText& DisplayName);

	/** Reveal ActiveAsset in the Content Browser. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void RevealActiveAssetInContentBrowser();

	/**
	 * Lex + parse only (never executes). Returns true if the source would compile.
	 * Use to drive the editor's error display.
	 */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	bool ValidateSource(const FString& Source, TArray<FString>& OutErrors);
};
