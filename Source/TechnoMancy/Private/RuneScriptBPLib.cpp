// Copyright TechnoMancy. All rights reserved.

#include "RuneScriptBPLib.h"
#include "RuneInterpreter.h"
#include "RuneLibraryRegistry.h"
#include "RuneSpellAsset.h"

bool URuneScriptBPLib::ValidateSpellSource(const FString& Source, TArray<FString>& OutErrors)
{
	FRuneInterpreter Interp;
	return Interp.Compile(Source, OutErrors);
}

TArray<FRuneLibraryEntry> URuneScriptBPLib::GetLibraryFunctionList()
{
	return FRuneLibraryRegistry::Get().GetAllEntries();
}

float URuneScriptBPLib::GetManaEstimate(const FString& Source, FName EntryPoint)
{
	FRuneInterpreter Interp;
	TArray<FString> Errors;
	if (!Interp.Compile(Source, Errors)) return 0.f;
	return Interp.EstimateManaCost(EntryPoint);
}

URuneSpellAsset* URuneScriptBPLib::CreateRuneSpellAsset(UObject* WorldContextObject,
														const FString& Source,
														int32 StorageCapacity,
														float MaxMana)
{
	UObject* Outer = WorldContextObject ? WorldContextObject : (UObject*)GetTransientPackage();
	URuneSpellAsset* Asset = NewObject<URuneSpellAsset>(Outer);
	if (!Asset) return nullptr;
	Asset->SourceCode = Source;
	Asset->StorageCapacity = StorageCapacity;
	Asset->MaxMana = MaxMana;
	return Asset;
}

int32 URuneScriptBPLib::GetSpellCharacterCount(const FString& Source)
{
	return Source.Len();
}

int32 URuneScriptBPLib::GetSpellStorageRemaining(const FString& Source, int32 StorageCapacity)
{
	return FMath::Max(0, StorageCapacity - Source.Len());
}
