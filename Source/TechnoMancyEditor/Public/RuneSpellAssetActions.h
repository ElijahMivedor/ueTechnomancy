// Copyright TechnoMancy. All rights reserved.
//
// AssetTypeActions for URuneSpellAsset — gives the asset a name, color, and
// category in the Content Browser. Default double-click behavior opens the
// asset in the standard property editor; designers use the Editor Utility
// Widget (Phase 6) for the syntax-highlighted authoring experience.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FRuneSpellAssetActions : public FAssetTypeActions_Base
{
public:
	virtual FText  GetName()           const override;
	virtual FColor GetTypeColor()      const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories()     override;
};
