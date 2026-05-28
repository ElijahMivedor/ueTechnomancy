// Copyright TechnoMancy. All rights reserved.
//
// Slate text-layout marshaller that re-lexes RuneScript on every SetText call
// and produces coloured runs by token category. Plug into SMultiLineEditableTextBox
// to get syntax highlighting; reused by both the in-game terminal (URuneCodeEditor)
// and the editor-time authoring widget (Phase 6).

#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/BaseTextLayoutMarshaller.h"
#include "Framework/Text/TextLayout.h"
#include "RuneTerminalStyle.h"

class TECHNOMANCY_API FRuneSyntaxHighlightMarshaller : public FBaseTextLayoutMarshaller
{
public:
	static TSharedRef<FRuneSyntaxHighlightMarshaller> Create(const FRuneTerminalStyle& Style);

	virtual ~FRuneSyntaxHighlightMarshaller() override = default;

	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

	void SetStyle(const FRuneTerminalStyle& InStyle);

private:
	explicit FRuneSyntaxHighlightMarshaller(const FRuneTerminalStyle& InStyle);

	FRuneTerminalStyle Style;
};
