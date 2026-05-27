// Copyright TechnoMancy. All rights reserved.

#include "RuneLibraryRegistry.h"

FRuneLibraryRegistry* FRuneLibraryRegistry::Instance = nullptr;

FRuneLibraryRegistry& FRuneLibraryRegistry::Get()
{
	if (!Instance) Instance = new FRuneLibraryRegistry();
	return *Instance;
}

void FRuneLibraryRegistry::Shutdown()
{
	if (Instance) { delete Instance; Instance = nullptr; }
}

void FRuneLibraryRegistry::Register(FName Namespace, FName FullPath, FName Method,
									bool bRequiresImport, FRuneLibFunction Fn)
{
	FRuneLibraryNamespace& NS = Namespaces.FindOrAdd(Namespace);
	NS.ShortName = Namespace;
	NS.FullPath = FullPath;
	NS.bRequiresImport = bRequiresImport;
	NS.Methods.Add(Method, MoveTemp(Fn));
}

void FRuneLibraryRegistry::RegisterSimple(FName Namespace, FName FullPath, FName Method,
										  bool bRequiresImport, float ManaCost,
										  const FString& Description, const FString& ParamDocs,
										  FRuneLibCallback Cb)
{
	FRuneLibFunction Fn;
	Fn.ManaCost = ManaCost;
	Fn.Description = Description;
	Fn.ParamDocs = ParamDocs;
	Fn.Callback = MoveTemp(Cb);
	Register(Namespace, FullPath, Method, bRequiresImport, MoveTemp(Fn));
}

bool FRuneLibraryRegistry::HasNamespace(FName Namespace) const
{
	return Namespaces.Contains(Namespace);
}

bool FRuneLibraryRegistry::RequiresImport(FName Namespace) const
{
	const FRuneLibraryNamespace* NS = Namespaces.Find(Namespace);
	return NS && NS->bRequiresImport;
}

FName FRuneLibraryRegistry::FullPathFor(FName Namespace) const
{
	const FRuneLibraryNamespace* NS = Namespaces.Find(Namespace);
	return NS ? NS->FullPath : Namespace;
}

const FRuneLibFunction* FRuneLibraryRegistry::Find(FName Namespace, FName Method) const
{
	const FRuneLibraryNamespace* NS = Namespaces.Find(Namespace);
	return NS ? NS->Methods.Find(Method) : nullptr;
}

namespace
{
	int32 LevenshteinDistance(const FString& A, const FString& B)
	{
		const int32 M = A.Len();
		const int32 N = B.Len();
		if (M == 0) return N;
		if (N == 0) return M;

		TArray<int32> Prev, Cur;
		Prev.SetNumUninitialized(N + 1);
		Cur.SetNumUninitialized(N + 1);
		for (int32 j = 0; j <= N; ++j) Prev[j] = j;

		for (int32 i = 1; i <= M; ++i)
		{
			Cur[0] = i;
			for (int32 j = 1; j <= N; ++j)
			{
				const int32 Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
				Cur[j] = FMath::Min3(Cur[j - 1] + 1, Prev[j] + 1, Prev[j - 1] + Cost);
			}
			Swap(Prev, Cur);
		}
		return Prev[N];
	}
}

FName FRuneLibraryRegistry::SuggestMethod(FName Namespace, FName Method) const
{
	const FRuneLibraryNamespace* NS = Namespaces.Find(Namespace);
	if (!NS) return NAME_None;

	const FString Want = Method.ToString();
	int32 BestDist = 3; // strictly less than 3 == ≤ 2
	FName Best = NAME_None;
	for (const TPair<FName, FRuneLibFunction>& Pair : NS->Methods)
	{
		const int32 D = LevenshteinDistance(Want, Pair.Key.ToString());
		if (D < BestDist)
		{
			BestDist = D;
			Best = Pair.Key;
		}
	}
	return Best;
}

TArray<FRuneLibraryEntry> FRuneLibraryRegistry::GetAllEntries() const
{
	TArray<FRuneLibraryEntry> Out;
	for (const TPair<FName, FRuneLibraryNamespace>& NSPair : Namespaces)
	{
		for (const TPair<FName, FRuneLibFunction>& MPair : NSPair.Value.Methods)
		{
			FRuneLibraryEntry E;
			E.Namespace = NSPair.Key.ToString();
			E.Method = MPair.Key.ToString();
			E.ManaCost = MPair.Value.ManaCost;
			E.Description = MPair.Value.Description;
			E.ParamDocs = MPair.Value.ParamDocs;
			Out.Add(E);
		}
	}
	return Out;
}
