// Copyright TechnoMancy. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RuneTypes.h"
#include "RuneAst.h"

class TECHNOMANCY_API FRuneParser
{
public:
	/** Parse a token stream into a Program. Throws FRuneSyntaxError on parse failure. */
	static TSharedPtr<FRuneAstProgram> Parse(const TArray<FRuneToken>& Tokens);
};
