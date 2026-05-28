// Copyright TechnoMancy. All rights reserved.
//
// In-game terminal. Compose your WBP_RuneTerminal Blueprint inheriting from
// this class and add UMG children whose names match the BindWidget properties:
//
//   CodeEditor     (URuneCodeEditor, required)
//   FlashButton    (UButton, required)
//   SimulateButton (UButton, optional)
//   ResetButton    (UButton, optional)
//   CharCounterText (UTextBlock, optional)
//   ManaEstimateText (UTextBlock, optional)
//   ErrorListText   (UTextBlock, optional)
//   StatusText      (UTextBlock, optional)
//   SpellNameText   (UTextBlock, optional)
//
// Then call SetWeaponComponent from the player's HUD code to bind the terminal
// to the weapon currently in hand.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuneTerminalStyle.h"
#include "RuneTerminalWidget.generated.h"

class URuneCodeEditor;
class URuneWeaponComponent;
class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class TECHNOMANCY_API URuneTerminalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	FRuneTerminalStyle Style;

	/** Bind the terminal to a weapon's RuneWeaponComponent. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void SetWeaponComponent(URuneWeaponComponent* InComponent);

	UFUNCTION(BlueprintPure, Category = "RuneScript")
	URuneWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** Submit the editor's current text to the weapon (compiles on the server). */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void FlashToWeapon();

	/** Validate + estimate mana without flashing. Updates status / error displays. */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void SimulateSpell();

	/** Restore the editor text from the weapon's DefaultSpellAsset (no flash). */
	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void RestoreDefault();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// =========================================================================
	// BindWidget targets (created by the WBP_RuneTerminal Blueprint subclass)
	// =========================================================================

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "RuneScript")
	TObjectPtr<URuneCodeEditor> CodeEditor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "RuneScript")
	TObjectPtr<UButton> FlashButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UButton> SimulateButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UButton> ResetButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UTextBlock> CharCounterText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UTextBlock> ManaEstimateText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UTextBlock> ErrorListText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "RuneScript")
	TObjectPtr<UTextBlock> SpellNameText;

private:
	UPROPERTY(BlueprintReadOnly, Category = "RuneScript", meta = (AllowPrivateAccess = true))
	TObjectPtr<URuneWeaponComponent> WeaponComponent;

	UFUNCTION()
	void HandleEditorTextChanged(const FText& NewText);

	UFUNCTION()
	void HandleFlashClicked();

	UFUNCTION()
	void HandleSimulateClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleCompileResult(bool bSuccess, const TArray<FString>& Errors);

	UFUNCTION()
	void HandleSpellSourceChanged(const FString& NewSource);

	void RefreshCharCounter(const FString& Source);
	void RefreshManaEstimate(const FString& Source);
	void RefreshSpellName();
	void SetStatus(const FString& Message, bool bError = false);
	void SetErrors(const TArray<FString>& Errors);
};
