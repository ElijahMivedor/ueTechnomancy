// Copyright TechnoMancy. All rights reserved.

#include "RuneSpellAssetFactory.h"
#include "RuneSpellAsset.h"

URuneSpellAssetFactory::URuneSpellAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = URuneSpellAsset::StaticClass();
}

UObject* URuneSpellAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
												  EObjectFlags Flags, UObject* Context,
												  FFeedbackContext* Warn)
{
	URuneSpellAsset* NewAsset = NewObject<URuneSpellAsset>(InParent, Class, Name, Flags | RF_Transactional);
	// Seed with a minimal valid spell so the asset compiles immediately.
	NewAsset->SourceCode = TEXT(
		"import runes.fire\n"
		"Fn Primary() {\n"
		"  fire.shot(spd: 10, dmg: 8)\n"
		"}\n");
	return NewAsset;
}
