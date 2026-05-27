// Copyright TechnoMancy. All rights reserved.
//
// Tree-walking interpreter for RuneScript. Server-authoritative; never run on a
// client. Each invocation receives a fresh scope; variables do not persist
// between invocations.

#pragma once

#include "CoreMinimal.h"
#include "RuneTypes.h"
#include "RuneAst.h"

class TECHNOMANCY_API FRuneInterpreter
{
public:
	/** Compile source into an internal AST. Returns false on syntax error; OutErrors populated. */
	bool Compile(const FString& Source, TArray<FString>& OutErrors);

	/** True if a top-level function with this name was compiled. */
	bool HasFunction(FName Name) const;

	/**
	 * Invoke a top-level function. Resets execution state per call (fresh scope).
	 * Returns true on clean completion. Errors are appended to OutErrors. The
	 * function's return value (if any) is written to OutResult when non-null.
	 *
	 * If the function does not exist this is a silent no-op (returns true) — e.g.
	 * a weapon's spell may legitimately omit OnHit.
	 */
	bool Invoke(FName FunctionName,
				FRuneWeaponContext& Context,
				TArray<FString>& OutErrors,
				FRuneValue* OutResult = nullptr);

	/** Access the parsed program for static analysis (mana estimate, imports, etc.). */
	TSharedPtr<FRuneAstProgram> GetProgram() const { return Program; }

	/** Approximate the static mana cost of a top-level function by summing lib-call costs. */
	float EstimateManaCost(FName FunctionName) const;

	/** Run a built-in self-test of the language core. Returns true on pass. */
	static bool SelfTest(FString& OutLog);

	static constexpr int32 MaxInstructions = 10000;
	static constexpr int32 MaxCallDepth    = 32;

private:
	TSharedPtr<FRuneAstProgram> Program;

	int32 InstructionCounter = 0;
	int32 CallDepth = 0;
	TArray<TMap<FName, FRuneValue>> Scopes;

	void       Tick();
	FRuneValue Eval(const FRuneAstNodePtr& Node, FRuneWeaponContext& Ctx);
	void       Execute(const FRuneAstNodePtr& Node, FRuneWeaponContext& Ctx);
	FRuneValue CallUserFunction(const TSharedPtr<FRuneAstFnDecl>& Fn,
								const TArray<FRuneValue>& Args,
								FRuneWeaponContext& Ctx);
	FRuneValue& EnsureVar(FName Name);
	FRuneValue  ReadVar(FName Name) const;
	float       EstimateNodeCost(const FRuneAstNodePtr& Node) const;
};
