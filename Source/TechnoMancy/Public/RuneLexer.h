// Copyright TechnoMancy. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RuneTypes.h"

class TECHNOMANCY_API FRuneLexer
{
public:
	/** Tokenize RuneScript source. Throws FRuneSyntaxError on lex failure. */
	static TArray<FRuneToken> Tokenize(const FString& Source);
};
