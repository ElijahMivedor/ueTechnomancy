// Copyright TechnoMancy. All rights reserved.

#include "RuneSpellAssetActions.h"
#include "RuneSpellAsset.h"

#define LOCTEXT_NAMESPACE "TechnoMancyEditor"

FText FRuneSpellAssetActions::GetName() const
{
	return LOCTEXT("RuneSpellAssetActions_Name", "Rune Spell");
}

FColor FRuneSpellAssetActions::GetTypeColor() const
{
	return FColor(0xC0, 0x84, 0xFC); // brand purple
}

UClass* FRuneSpellAssetActions::GetSupportedClass() const
{
	return URuneSpellAsset::StaticClass();
}

uint32 FRuneSpellAssetActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

#undef LOCTEXT_NAMESPACE
