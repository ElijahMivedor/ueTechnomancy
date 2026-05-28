// Copyright TechnoMancy. All rights reserved.

#include "RuneSyntaxHighlightMarshaller.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/IRun.h"
#include "Styling/SlateTypes.h"

namespace
{
	enum class ECategory : uint8
	{
		Default,
		Keyword,
		Identifier,
		LibCall,
		Number,
		Bool,
		Operator,
		Comment,
		Punctuation,
	};

	struct FRange
	{
		int32 Start = 0;
		int32 Length = 0;
		ECategory Cat = ECategory::Default;
	};

	bool IsIdentStart(TCHAR C) { return FChar::IsAlpha(C) || C == TEXT('_'); }
	bool IsIdentChar(TCHAR C)  { return FChar::IsAlnum(C) || C == TEXT('_'); }
	bool IsDigitChar(TCHAR C)  { return FChar::IsDigit(C); }

	ECategory ClassifyKeyword(const FString& Word)
	{
		if (Word == TEXT("import") || Word == TEXT("Fn") || Word == TEXT("if") ||
			Word == TEXT("else") || Word == TEXT("while") || Word == TEXT("for") ||
			Word == TEXT("in") || Word == TEXT("return"))
		{
			return ECategory::Keyword;
		}
		if (Word == TEXT("true") || Word == TEXT("false"))
		{
			return ECategory::Bool;
		}
		return ECategory::Identifier;
	}

	/**
	 * Single-pass scanner that walks the line and emits one FRange per identifiable
	 * token. Whitespace and unrecognised characters are grouped into Default runs.
	 * Designed to be reasonably fast on every keystroke — no exceptions, no allocations
	 * beyond the output array.
	 */
	void ScanLine(const FString& Line, TArray<FRange>& OutRanges)
	{
		const int32 N = Line.Len();
		int32 i = 0;
		while (i < N)
		{
			const TCHAR C = Line[i];

			// Comment to EOL
			if (C == TEXT('/') && i + 1 < N && Line[i + 1] == TEXT('/'))
			{
				OutRanges.Add({ i, N - i, ECategory::Comment });
				return;
			}

			// Whitespace
			if (FChar::IsWhitespace(C))
			{
				int32 Start = i;
				while (i < N && FChar::IsWhitespace(Line[i])) ++i;
				OutRanges.Add({ Start, i - Start, ECategory::Default });
				continue;
			}

			// Number
			if (IsDigitChar(C))
			{
				int32 Start = i;
				while (i < N && IsDigitChar(Line[i])) ++i;
				if (i + 1 < N && Line[i] == TEXT('.') && Line[i + 1] != TEXT('.') && IsDigitChar(Line[i + 1]))
				{
					++i;
					while (i < N && IsDigitChar(Line[i])) ++i;
				}
				OutRanges.Add({ Start, i - Start, ECategory::Number });
				continue;
			}

			// Identifier / keyword / lib-call namespace
			if (IsIdentStart(C))
			{
				int32 Start = i;
				while (i < N && IsIdentChar(Line[i])) ++i;
				const FString Word = Line.Mid(Start, i - Start);
				const ECategory Cat = ClassifyKeyword(Word);

				// Look ahead for `.method` to mark this identifier as a library namespace.
				if (Cat == ECategory::Identifier && i < N && Line[i] == TEXT('.')
					&& i + 1 < N && IsIdentStart(Line[i + 1]))
				{
					OutRanges.Add({ Start, i - Start, ECategory::LibCall });
					OutRanges.Add({ i, 1, ECategory::Operator });
					++i;
					// the method name
					const int32 MethodStart = i;
					while (i < N && IsIdentChar(Line[i])) ++i;
					OutRanges.Add({ MethodStart, i - MethodStart, ECategory::LibCall });
					continue;
				}

				OutRanges.Add({ Start, i - Start, Cat });
				continue;
			}

			// Operators (multi-char first)
			auto AddOp = [&](int32 Len)
			{
				OutRanges.Add({ i, Len, ECategory::Operator });
				i += Len;
			};
			if (C == TEXT('=') && i + 1 < N && Line[i + 1] == TEXT('=')) { AddOp(2); continue; }
			if (C == TEXT('!') && i + 1 < N && Line[i + 1] == TEXT('=')) { AddOp(2); continue; }
			if (C == TEXT('<') && i + 1 < N && Line[i + 1] == TEXT('=')) { AddOp(2); continue; }
			if (C == TEXT('>') && i + 1 < N && Line[i + 1] == TEXT('=')) { AddOp(2); continue; }
			if (C == TEXT('&') && i + 1 < N && Line[i + 1] == TEXT('&')) { AddOp(2); continue; }
			if (C == TEXT('|') && i + 1 < N && Line[i + 1] == TEXT('|')) { AddOp(2); continue; }
			if (C == TEXT('.') && i + 1 < N && Line[i + 1] == TEXT('.')) { AddOp(2); continue; }

			if (C == TEXT('+') || C == TEXT('-') || C == TEXT('*') || C == TEXT('/') ||
				C == TEXT('%') || C == TEXT('=') || C == TEXT('<') || C == TEXT('>') ||
				C == TEXT('!') || C == TEXT('.'))
			{
				AddOp(1);
				continue;
			}

			if (C == TEXT('(') || C == TEXT(')') || C == TEXT('{') || C == TEXT('}') ||
				C == TEXT(',') || C == TEXT(':'))
			{
				OutRanges.Add({ i, 1, ECategory::Punctuation });
				++i;
				continue;
			}

			// Unrecognised — passthrough as default
			OutRanges.Add({ i, 1, ECategory::Default });
			++i;
		}
	}

	FLinearColor ColorFor(const FRuneTerminalStyle& Style, ECategory Cat)
	{
		switch (Cat)
		{
			case ECategory::Keyword:     return Style.ColorKeyword;
			case ECategory::Identifier:  return Style.ColorIdentifier;
			case ECategory::LibCall:     return Style.ColorLibCall;
			case ECategory::Number:      return Style.ColorNumber;
			case ECategory::Bool:        return Style.ColorBool;
			case ECategory::Operator:    return Style.ColorOperator;
			case ECategory::Comment:     return Style.ColorComment;
			case ECategory::Punctuation: return Style.ColorIdentifier;
			default:                     return Style.ColorIdentifier;
		}
	}

	FTextBlockStyle MakeRunStyle(const FRuneTerminalStyle& Style, ECategory Cat)
	{
		FTextBlockStyle TS;
		TS.SetFont(Style.FontInfo);
		TS.SetColorAndOpacity(FSlateColor(ColorFor(Style, Cat)));
		return TS;
	}
}

TSharedRef<FRuneSyntaxHighlightMarshaller> FRuneSyntaxHighlightMarshaller::Create(const FRuneTerminalStyle& Style)
{
	return MakeShareable(new FRuneSyntaxHighlightMarshaller(Style));
}

FRuneSyntaxHighlightMarshaller::FRuneSyntaxHighlightMarshaller(const FRuneTerminalStyle& InStyle)
	: Style(InStyle)
{
}

void FRuneSyntaxHighlightMarshaller::SetStyle(const FRuneTerminalStyle& InStyle)
{
	Style = InStyle;
	MakeDirty();
}

void FRuneSyntaxHighlightMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	TArray<FString> Lines;
	SourceString.ParseIntoArrayLines(Lines, /*bCullEmpty=*/ false);

	for (const FString& LineText : Lines)
	{
		TSharedRef<FString> LineRef = MakeShared<FString>(LineText);
		TArray<TSharedRef<IRun>> Runs;

		if (LineText.IsEmpty())
		{
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineRef, MakeRunStyle(Style, ECategory::Default)));
		}
		else
		{
			TArray<FRange> Ranges;
			ScanLine(LineText, Ranges);
			for (const FRange& R : Ranges)
			{
				if (R.Length <= 0) continue;
				Runs.Add(FSlateTextRun::Create(FRunInfo(), LineRef,
					MakeRunStyle(Style, R.Cat),
					FTextRange(R.Start, R.Start + R.Length)));
			}
		}

		TargetTextLayout.AddLine(FTextLayout::FNewLineData(LineRef, MoveTemp(Runs)));
	}
}

void FRuneSyntaxHighlightMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	SourceTextLayout.GetAsText(TargetString);
}
