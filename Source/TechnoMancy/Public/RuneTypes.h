// Copyright TechnoMancy. All rights reserved.
//
// Core value types, tokens, and runtime context for the RuneScript interpreter.
// Most types here are plain C++ (no UObject overhead). FRuneLibraryEntry is a
// USTRUCT only because it is exposed to Blueprint via URuneScriptBPLib in
// Phase 3.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "RuneTypes.generated.h"

class AActor;
class UClass;

// =============================================================================
// Tokens
// =============================================================================

enum class ERuneTokenType : uint8
{
	// Literals
	Number,
	Identifier,
	True,
	False,

	// Keywords
	Import,
	Fn,
	If,
	Else,
	While,
	For,
	In,
	Return,

	// Operators
	Plus, Minus, Star, Slash, Percent,
	EqualEqual, NotEqual, Less, Greater, LessEqual, GreaterEqual,
	And, Or, Not,
	Assign,

	// Punctuation
	LParen, RParen, LBrace, RBrace, Comma, Dot, Colon, DotDot,

	// Sentinels
	EndOfFile,
};

struct FRuneToken
{
	ERuneTokenType Type = ERuneTokenType::EndOfFile;
	FString Lexeme;
	double NumberValue = 0.0;
	int32 Line = 0;
	int32 Column = 0;
};

// =============================================================================
// Values
// =============================================================================

enum class ERuneValueType : uint8
{
	Nil,
	Number,
	Bool,
};

struct FRuneValue
{
	ERuneValueType Type = ERuneValueType::Nil;
	double Number = 0.0;
	bool Bool = false;

	static FRuneValue MakeNil()                 { return FRuneValue{}; }
	static FRuneValue MakeNumber(double InVal)  { FRuneValue V; V.Type = ERuneValueType::Number; V.Number = InVal;  return V; }
	static FRuneValue MakeBool(bool InVal)      { FRuneValue V; V.Type = ERuneValueType::Bool;   V.Bool   = InVal;  return V; }

	double AsNumber() const
	{
		switch (Type)
		{
			case ERuneValueType::Number: return Number;
			case ERuneValueType::Bool:   return Bool ? 1.0 : 0.0;
			default:                     return 0.0;
		}
	}

	bool AsBool() const
	{
		switch (Type)
		{
			case ERuneValueType::Bool:   return Bool;
			case ERuneValueType::Number: return Number != 0.0;
			default:                     return false;
		}
	}
};

// =============================================================================
// Library call arguments + runtime context
// =============================================================================

struct FRuneLibCallArgs
{
	TMap<FName, FRuneValue> Named;

	FRuneValue Get(FName Key, FRuneValue Default = FRuneValue::MakeNil()) const
	{
		if (const FRuneValue* V = Named.Find(Key)) return *V;
		return Default;
	}

	double GetFloat(FName Key, double Default) const
	{
		if (const FRuneValue* V = Named.Find(Key)) return V->AsNumber();
		return Default;
	}

	bool GetBool(FName Key, bool Default) const
	{
		if (const FRuneValue* V = Named.Find(Key)) return V->AsBool();
		return Default;
	}

	int32 GetInt(FName Key, int32 Default) const
	{
		if (const FRuneValue* V = Named.Find(Key)) return FMath::TruncToInt32(V->AsNumber());
		return Default;
	}
};

struct FRuneWeaponContext
{
	TWeakObjectPtr<AActor> WeaponOwner;
	TWeakObjectPtr<AActor> LastHitTarget;
	FVector LastHitPos = FVector::ZeroVector;

	/** Forward direction used by projectile / beam spawn helpers. Populated by URuneWeaponComponent. */
	FVector AimDirection = FVector::ForwardVector;

	float Mana = 0.f;
	float MaxMana = 0.f;
	float DeltaTime = 0.f;

	/**
	 * Class spawned by *.shot / *.lance / *.bolt / *.boulder library callbacks.
	 * Populated by URuneWeaponComponent; defaults to ARuneProjectile.
	 * Plain UClass* to avoid pulling in heavy headers — callers cast where needed.
	 */
	UClass* ProjectileClass = nullptr;
};

using FRuneLibCallback = TFunction<FRuneValue(const FRuneLibCallArgs&, FRuneWeaponContext&)>;

struct FRuneLibFunction
{
	float ManaCost = 0.f;
	FString Description;
	FString ParamDocs;
	FRuneLibCallback Callback;
};

// =============================================================================
// Blueprint-visible library metadata
// =============================================================================

USTRUCT(BlueprintType)
struct FRuneLibraryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TechnoMancy")
	FString Namespace;

	UPROPERTY(BlueprintReadOnly, Category = "TechnoMancy")
	FString Method;

	UPROPERTY(BlueprintReadOnly, Category = "TechnoMancy")
	float ManaCost = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "TechnoMancy")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category = "TechnoMancy")
	FString ParamDocs;
};
