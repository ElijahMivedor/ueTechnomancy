// Copyright TechnoMancy. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRuneSpellAssetActions;

class FTechnoMancyEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FRuneSpellAssetActions> SpellAssetActions;
};
