// Copyright TechnoMancy. All rights reserved.
//
// Styling for the in-game terminal: font, foreground / background, syntax
// highlight colours. Designers tweak per-project in the URuneCodeEditor
// or URuneTerminalWidget details panel.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "RuneTerminalStyle.generated.h"

USTRUCT(BlueprintType)
struct TECHNOMANCY_API FRuneTerminalStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FSlateFontInfo FontInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorKeyword = FLinearColor::FromSRGBColor(FColor(0xC0, 0x84, 0xFC));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorIdentifier = FLinearColor::FromSRGBColor(FColor(0xE2, 0xE8, 0xF0));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorLibCall = FLinearColor::FromSRGBColor(FColor(0x38, 0xBD, 0xF8));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorNumber = FLinearColor::FromSRGBColor(FColor(0xF8, 0x71, 0x71));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorBool = FLinearColor::FromSRGBColor(FColor(0x6E, 0xE7, 0xB7));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorOperator = FLinearColor::FromSRGBColor(FColor(0xFB, 0x92, 0x3C));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorComment = FLinearColor::FromSRGBColor(FColor(0x4B, 0x55, 0x63));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Syntax")
	FLinearColor ColorError = FLinearColor::FromSRGBColor(FColor(0xEF, 0x44, 0x44));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BackgroundColor = FLinearColor::FromSRGBColor(FColor(0x0A, 0x0A, 0x0F));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor CursorColor = FLinearColor::FromSRGBColor(FColor(0xA7, 0x8B, 0xFA));
};
