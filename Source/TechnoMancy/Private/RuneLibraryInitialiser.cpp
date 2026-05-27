// Copyright TechnoMancy. All rights reserved.

#include "RuneLibraryInitialiser.h"
#include "TechnoMancyLog.h"

ARuneLibraryInitialiser::ARuneLibraryInitialiser()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ARuneLibraryInitialiser::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTechnoMancy, Display, TEXT("ARuneLibraryInitialiser: registering additional libraries"));
	RegisterAdditionalLibraries();
}
