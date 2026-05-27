// Copyright TechnoMancy. All rights reserved.
//
// Optional helper actor. Built-in libraries are already registered at module
// startup; this actor exists as a Blueprint-friendly extension point for
// projects that want to register custom libraries on level load (e.g. to bind
// callbacks that need a world reference).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuneLibraryInitialiser.generated.h"

UCLASS(Blueprintable, BlueprintType)
class TECHNOMANCY_API ARuneLibraryInitialiser : public AActor
{
	GENERATED_BODY()

public:
	ARuneLibraryInitialiser();

	/**
	 * Override in Blueprint to register additional library functions with
	 * URuneScriptBPLib::RegisterLibraryFunction (Phase 3+).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TechnoMancy")
	void RegisterAdditionalLibraries();

protected:
	virtual void BeginPlay() override;
};
