// Copyright TechnoMancy. All rights reserved.
//
// Content Browser factory that creates URuneSpellAsset instances. Registers
// "RuneSpellAsset" in the Add New menu under Miscellaneous.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "RuneSpellAssetFactory.generated.h"

UCLASS()
class URuneSpellAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	URuneSpellAssetFactory();

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
									  EObjectFlags Flags, UObject* Context,
									  FFeedbackContext* Warn) override;

	virtual bool ShouldShowInNewMenu() const override { return true; }
};
