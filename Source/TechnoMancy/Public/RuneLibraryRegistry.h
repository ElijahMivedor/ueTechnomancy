// Copyright TechnoMancy. All rights reserved.
//
// Singleton registry of RuneScript library functions. Game code registers
// callbacks (typically during module startup); the interpreter resolves
// fire.shot(...) style calls against this registry.
//
// THREAD SAFETY: registration is single-threaded. Register during module
// startup or before BeginPlay; the interpreter only reads after that.

#pragma once

#include "CoreMinimal.h"
#include "RuneTypes.h"

struct FRuneLibraryNamespace
{
	FName ShortName;                            // e.g. "fire"
	FName FullPath;                             // e.g. "runes.fire" (helpers store their short name)
	bool  bRequiresImport = false;              // true for elemental libs (runes.*)
	TMap<FName, FRuneLibFunction> Methods;
};

class TECHNOMANCY_API FRuneLibraryRegistry
{
public:
	static FRuneLibraryRegistry& Get();
	static void Shutdown();

	/** Register one function. Creates the namespace entry if it does not exist. */
	void Register(FName Namespace, FName FullPath, FName Method,
				  bool bRequiresImport, FRuneLibFunction Fn);

	/** Convenience overload that builds an FRuneLibFunction inline. */
	void RegisterSimple(FName Namespace, FName FullPath, FName Method,
						bool bRequiresImport, float ManaCost,
						const FString& Description, const FString& ParamDocs,
						FRuneLibCallback Cb);

	bool   HasNamespace(FName Namespace) const;
	bool   RequiresImport(FName Namespace) const;
	FName  FullPathFor(FName Namespace) const;

	const FRuneLibFunction* Find(FName Namespace, FName Method) const;

	/** Returns a method name in this namespace with Levenshtein distance ≤ 2, or NAME_None. */
	FName SuggestMethod(FName Namespace, FName Method) const;

	/** Returns a snapshot of all registered functions (used by the BP library / terminal autocomplete). */
	TArray<FRuneLibraryEntry> GetAllEntries() const;

	/** Returns true if RegisterBuiltInLibraries() has been invoked. */
	bool IsInitialised() const { return bInitialised; }
	void MarkInitialised()     { bInitialised = true; }

private:
	FRuneLibraryRegistry() = default;
	static FRuneLibraryRegistry* Instance;

	TMap<FName, FRuneLibraryNamespace> Namespaces;
	bool bInitialised = false;
};
