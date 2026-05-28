// Copyright TechnoMancy. All rights reserved.

#include "TechnoMancyEditor.h"
#include "RuneSpellAssetActions.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

#define LOCTEXT_NAMESPACE "TechnoMancyEditor"

void FTechnoMancyEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	SpellAssetActions = MakeShared<FRuneSpellAssetActions>();
	AssetTools.RegisterAssetTypeActions(SpellAssetActions.ToSharedRef());
}

void FTechnoMancyEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		if (SpellAssetActions.IsValid())
		{
			AssetTools.UnregisterAssetTypeActions(SpellAssetActions.ToSharedRef());
			SpellAssetActions.Reset();
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTechnoMancyEditorModule, TechnoMancyEditor)
