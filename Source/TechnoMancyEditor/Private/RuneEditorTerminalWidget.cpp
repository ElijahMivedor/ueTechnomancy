// Copyright TechnoMancy. All rights reserved.

#include "RuneEditorTerminalWidget.h"
#include "RuneInterpreter.h"
#include "RuneSpellAsset.h"
#include "RuneSpellAssetFactory.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"

URuneSpellAsset* URuneEditorTerminalWidget::NewSpell()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	URuneSpellAssetFactory* Factory = NewObject<URuneSpellAssetFactory>();
	UObject* Created = AssetTools.CreateAssetWithDialog(URuneSpellAsset::StaticClass(), Factory);

	if (URuneSpellAsset* Spell = Cast<URuneSpellAsset>(Created))
	{
		ActiveAsset = Spell;
		bUnsavedChanges = false;
		return Spell;
	}
	return nullptr;
}

URuneSpellAsset* URuneEditorTerminalWidget::OpenSpell()
{
	FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FOpenAssetDialogConfig OpenConfig;
	OpenConfig.DialogTitleOverride = FText::FromString(TEXT("Open Spell Asset"));
	OpenConfig.AssetClassNames.Add(URuneSpellAsset::StaticClass()->GetClassPathName());
	OpenConfig.bAllowMultipleSelection = false;

	const TArray<FAssetData> SelectedAssets = ContentBrowser.Get().CreateModalOpenAssetDialog(OpenConfig);
	if (SelectedAssets.Num() == 0) return nullptr;

	URuneSpellAsset* Spell = Cast<URuneSpellAsset>(SelectedAssets[0].GetAsset());
	if (Spell)
	{
		ActiveAsset = Spell;
		bUnsavedChanges = false;
	}
	return Spell;
}

bool URuneEditorTerminalWidget::SaveSpell(const FString& Source, int32 StorageCapacity,
										   float MaxMana, const FText& DisplayName)
{
	if (!ActiveAsset) return false;

	ActiveAsset->SourceCode = Source;
	ActiveAsset->StorageCapacity = FMath::Clamp(StorageCapacity, 16, 4096);
	ActiveAsset->MaxMana = FMath::Max(0.f, MaxMana);
	ActiveAsset->DisplayName = DisplayName;
	ActiveAsset->MarkPackageDirty();

	bUnsavedChanges = false;
	return true;
}

void URuneEditorTerminalWidget::RevealActiveAssetInContentBrowser()
{
	if (!ActiveAsset) return;
	FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> Assets;
	Assets.Add(FAssetData(ActiveAsset));
	ContentBrowser.Get().SyncBrowserToAssets(Assets);
}

bool URuneEditorTerminalWidget::ValidateSource(const FString& Source, TArray<FString>& OutErrors)
{
	FRuneInterpreter Interp;
	return Interp.Compile(Source, OutErrors);
}
