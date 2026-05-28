// Copyright TechnoMancy. All rights reserved.

#include "RuneTerminalWidget.h"
#include "RuneCodeEditor.h"
#include "RuneInterpreter.h"
#include "RuneSpellAsset.h"
#include "RuneWeaponComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void URuneTerminalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CodeEditor)
	{
		CodeEditor->ApplyStyle(Style);
		CodeEditor->OnTextChanged.AddDynamic(this, &URuneTerminalWidget::HandleEditorTextChanged);
	}
	if (FlashButton)
	{
		FlashButton->OnClicked.AddDynamic(this, &URuneTerminalWidget::HandleFlashClicked);
	}
	if (SimulateButton)
	{
		SimulateButton->OnClicked.AddDynamic(this, &URuneTerminalWidget::HandleSimulateClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.AddDynamic(this, &URuneTerminalWidget::HandleResetClicked);
	}
}

void URuneTerminalWidget::NativeDestruct()
{
	if (WeaponComponent)
	{
		WeaponComponent->OnCompileResult.RemoveDynamic(this, &URuneTerminalWidget::HandleCompileResult);
		WeaponComponent->OnSpellSourceChanged.RemoveDynamic(this, &URuneTerminalWidget::HandleSpellSourceChanged);
	}
	if (CodeEditor)
	{
		CodeEditor->OnTextChanged.RemoveDynamic(this, &URuneTerminalWidget::HandleEditorTextChanged);
	}
	Super::NativeDestruct();
}

void URuneTerminalWidget::SetWeaponComponent(URuneWeaponComponent* InComponent)
{
	if (WeaponComponent == InComponent) return;

	if (WeaponComponent)
	{
		WeaponComponent->OnCompileResult.RemoveDynamic(this, &URuneTerminalWidget::HandleCompileResult);
		WeaponComponent->OnSpellSourceChanged.RemoveDynamic(this, &URuneTerminalWidget::HandleSpellSourceChanged);
	}

	WeaponComponent = InComponent;

	if (WeaponComponent)
	{
		WeaponComponent->OnCompileResult.AddDynamic(this, &URuneTerminalWidget::HandleCompileResult);
		WeaponComponent->OnSpellSourceChanged.AddDynamic(this, &URuneTerminalWidget::HandleSpellSourceChanged);

		// Seed editor with whatever the player has authored, or fall back to default preset.
		FString InitialSource = WeaponComponent->SpellSource;
		if (InitialSource.IsEmpty() && WeaponComponent->DefaultSpellAsset)
		{
			InitialSource = WeaponComponent->DefaultSpellAsset->SourceCode;
		}
		if (CodeEditor)
		{
			CodeEditor->SetText(FText::FromString(InitialSource));
			CodeEditor->SetIsReadOnly(!WeaponComponent->bAllowPlayerEdits);
		}
		if (FlashButton)
		{
			FlashButton->SetIsEnabled(WeaponComponent->bAllowPlayerEdits);
		}
		RefreshSpellName();
		RefreshCharCounter(InitialSource);
		RefreshManaEstimate(InitialSource);
		if (!WeaponComponent->bAllowPlayerEdits)
		{
			SetStatus(TEXT("This weapon's spell is locked by the designer"), /*bError=*/ true);
		}
	}
}

// =============================================================================
// Button handlers
// =============================================================================

void URuneTerminalWidget::HandleFlashClicked()
{
	FlashToWeapon();
}

void URuneTerminalWidget::HandleSimulateClicked()
{
	SimulateSpell();
}

void URuneTerminalWidget::HandleResetClicked()
{
	RestoreDefault();
}

void URuneTerminalWidget::FlashToWeapon()
{
	if (!WeaponComponent || !CodeEditor) return;
	const FString Source = CodeEditor->GetText().ToString();
	SetStatus(TEXT("Compiling..."), /*bError=*/ false);
	WeaponComponent->RequestCompile(Source);
}

void URuneTerminalWidget::SimulateSpell()
{
	if (!CodeEditor) return;

	const FString Source = CodeEditor->GetText().ToString();
	FRuneInterpreter Interp;
	TArray<FString> Errors;
	const bool bOk = Interp.Compile(Source, Errors);

	SetErrors(Errors);
	if (bOk)
	{
		const float Cost = Interp.EstimateManaCost(FName(TEXT("Primary")));
		SetStatus(FString::Printf(TEXT("OK — estimated %.1f mana / Primary"), Cost), /*bError=*/ false);
	}
	else
	{
		SetStatus(TEXT("Has errors"), /*bError=*/ true);
	}
}

void URuneTerminalWidget::RestoreDefault()
{
	if (!WeaponComponent || !WeaponComponent->DefaultSpellAsset || !CodeEditor) return;
	const FString DefaultSource = WeaponComponent->DefaultSpellAsset->SourceCode;
	CodeEditor->SetText(FText::FromString(DefaultSource));
	RefreshCharCounter(DefaultSource);
	RefreshManaEstimate(DefaultSource);
	SetStatus(TEXT("Restored default — Flash to apply"), /*bError=*/ false);
}

// =============================================================================
// Reactive updates
// =============================================================================

void URuneTerminalWidget::HandleEditorTextChanged(const FText& NewText)
{
	const FString Source = NewText.ToString();
	RefreshCharCounter(Source);
	RefreshManaEstimate(Source);
}

void URuneTerminalWidget::HandleCompileResult(bool bSuccess, const TArray<FString>& Errors)
{
	SetErrors(Errors);
	if (bSuccess)
	{
		SetStatus(TEXT("Flashed successfully"), /*bError=*/ false);
	}
	else
	{
		SetStatus(TEXT("Compile failed"), /*bError=*/ true);
	}
}

void URuneTerminalWidget::HandleSpellSourceChanged(const FString& NewSource)
{
	// Server replicated a new source — only update the editor if the player isn't actively editing.
	if (!CodeEditor) return;
	const FString CurrentEditor = CodeEditor->GetText().ToString();
	if (CurrentEditor == NewSource) return;
	// Heuristic: if editor is empty, accept the replicated source verbatim.
	if (CurrentEditor.IsEmpty())
	{
		CodeEditor->SetText(FText::FromString(NewSource));
		RefreshCharCounter(NewSource);
		RefreshManaEstimate(NewSource);
	}
}

// =============================================================================
// Display helpers
// =============================================================================

void URuneTerminalWidget::RefreshCharCounter(const FString& Source)
{
	if (!CharCounterText || !WeaponComponent) return;
	const int32 Used = Source.Len();
	const int32 Cap = WeaponComponent->StorageCapacity;
	CharCounterText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Used, Cap)));
	if (Used > Cap)
	{
		CharCounterText->SetColorAndOpacity(FSlateColor(Style.ColorError));
	}
	else
	{
		CharCounterText->SetColorAndOpacity(FSlateColor(Style.ColorIdentifier));
	}
}

void URuneTerminalWidget::RefreshManaEstimate(const FString& Source)
{
	if (!ManaEstimateText) return;
	FRuneInterpreter Interp;
	TArray<FString> Errors;
	float Estimate = 0.f;
	if (Interp.Compile(Source, Errors))
	{
		Estimate = Interp.EstimateManaCost(FName(TEXT("Primary")));
	}
	ManaEstimateText->SetText(FText::FromString(FString::Printf(TEXT("~%.1f mana"), Estimate)));
}

void URuneTerminalWidget::RefreshSpellName()
{
	if (!SpellNameText || !WeaponComponent) return;
	SpellNameText->SetText(WeaponComponent->GetDefaultSpellName());
}

void URuneTerminalWidget::SetStatus(const FString& Message, bool bError)
{
	if (!StatusText) return;
	StatusText->SetText(FText::FromString(Message));
	StatusText->SetColorAndOpacity(FSlateColor(bError ? Style.ColorError : Style.ColorBool));
}

void URuneTerminalWidget::SetErrors(const TArray<FString>& Errors)
{
	if (!ErrorListText) return;
	if (Errors.Num() == 0)
	{
		ErrorListText->SetText(FText::GetEmpty());
		return;
	}
	ErrorListText->SetText(FText::FromString(FString::Join(Errors, TEXT("\n"))));
	ErrorListText->SetColorAndOpacity(FSlateColor(Style.ColorError));
}
