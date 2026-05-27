// Copyright TechnoMancy. All rights reserved.

#include "RuneLexer.h"
#include "RuneAst.h"

namespace
{
	struct FLexState
	{
		const TCHAR* Cur;
		const TCHAR* End;
		int32 Line = 1;
		int32 Col = 1;

		bool IsAtEnd() const { return Cur >= End; }

		TCHAR Peek(int32 Offset = 0) const
		{
			const TCHAR* P = Cur + Offset;
			return P < End ? *P : TEXT('\0');
		}

		TCHAR Advance()
		{
			const TCHAR C = *Cur++;
			if (C == TEXT('\n')) { ++Line; Col = 1; } else { ++Col; }
			return C;
		}
	};

	bool IsIdentStart(TCHAR C) { return FChar::IsAlpha(C) || C == TEXT('_'); }
	bool IsIdentChar(TCHAR C)  { return FChar::IsAlnum(C) || C == TEXT('_'); }
	bool IsDigitChar(TCHAR C)  { return FChar::IsDigit(C); }

	void SkipWhitespaceAndComments(FLexState& S)
	{
		while (!S.IsAtEnd())
		{
			const TCHAR C = S.Peek();
			if (C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\r') || C == TEXT('\n'))
			{
				S.Advance();
			}
			else if (C == TEXT('/') && S.Peek(1) == TEXT('/'))
			{
				while (!S.IsAtEnd() && S.Peek() != TEXT('\n')) S.Advance();
			}
			else
			{
				break;
			}
		}
	}

	FRuneToken MakeToken(ERuneTokenType InType, FString InLexeme, int32 InLine, int32 InCol)
	{
		FRuneToken Tk;
		Tk.Type = InType;
		Tk.Lexeme = MoveTemp(InLexeme);
		Tk.Line = InLine;
		Tk.Column = InCol;
		return Tk;
	}

	FRuneToken ReadNumber(FLexState& S)
	{
		const int32 StartLine = S.Line;
		const int32 StartCol  = S.Col;

		FString Lex;
		while (!S.IsAtEnd() && IsDigitChar(S.Peek())) Lex.AppendChar(S.Advance());

		// `5..2` is range syntax — do NOT consume the dot as part of the number.
		if (!S.IsAtEnd() && S.Peek() == TEXT('.') && S.Peek(1) != TEXT('.') && IsDigitChar(S.Peek(1)))
		{
			Lex.AppendChar(S.Advance()); // '.'
			while (!S.IsAtEnd() && IsDigitChar(S.Peek())) Lex.AppendChar(S.Advance());
		}

		FRuneToken Tk = MakeToken(ERuneTokenType::Number, Lex, StartLine, StartCol);
		Tk.NumberValue = FCString::Atod(*Lex);
		return Tk;
	}

	FRuneToken ReadIdentifierOrKeyword(FLexState& S)
	{
		const int32 StartLine = S.Line;
		const int32 StartCol  = S.Col;

		FString Lex;
		while (!S.IsAtEnd() && IsIdentChar(S.Peek())) Lex.AppendChar(S.Advance());

		if (Lex.Len() > 64)
		{
			throw FRuneSyntaxError(StartLine,
				FString::Printf(TEXT("Identifier exceeds 64-character limit")));
		}

		ERuneTokenType T = ERuneTokenType::Identifier;
		if      (Lex == TEXT("import")) T = ERuneTokenType::Import;
		else if (Lex == TEXT("Fn"))     T = ERuneTokenType::Fn;
		else if (Lex == TEXT("if"))     T = ERuneTokenType::If;
		else if (Lex == TEXT("else"))   T = ERuneTokenType::Else;
		else if (Lex == TEXT("while"))  T = ERuneTokenType::While;
		else if (Lex == TEXT("for"))    T = ERuneTokenType::For;
		else if (Lex == TEXT("in"))     T = ERuneTokenType::In;
		else if (Lex == TEXT("return")) T = ERuneTokenType::Return;
		else if (Lex == TEXT("true"))   T = ERuneTokenType::True;
		else if (Lex == TEXT("false"))  T = ERuneTokenType::False;

		return MakeToken(T, Lex, StartLine, StartCol);
	}
}

TArray<FRuneToken> FRuneLexer::Tokenize(const FString& Source)
{
	FLexState S{ *Source, *Source + Source.Len(), 1, 1 };
	TArray<FRuneToken> Out;

	while (true)
	{
		SkipWhitespaceAndComments(S);
		if (S.IsAtEnd()) break;

		const int32 Line = S.Line;
		const int32 Col  = S.Col;
		const TCHAR C    = S.Peek();

		if (IsDigitChar(C))
		{
			Out.Add(ReadNumber(S));
			continue;
		}
		if (IsIdentStart(C))
		{
			Out.Add(ReadIdentifierOrKeyword(S));
			continue;
		}

		S.Advance();
		const TCHAR N = S.Peek();
		switch (C)
		{
			case TEXT('+'): Out.Add(MakeToken(ERuneTokenType::Plus,    TEXT("+"),  Line, Col)); break;
			case TEXT('-'): Out.Add(MakeToken(ERuneTokenType::Minus,   TEXT("-"),  Line, Col)); break;
			case TEXT('*'): Out.Add(MakeToken(ERuneTokenType::Star,    TEXT("*"),  Line, Col)); break;
			case TEXT('/'): Out.Add(MakeToken(ERuneTokenType::Slash,   TEXT("/"),  Line, Col)); break;
			case TEXT('%'): Out.Add(MakeToken(ERuneTokenType::Percent, TEXT("%"),  Line, Col)); break;
			case TEXT('('): Out.Add(MakeToken(ERuneTokenType::LParen,  TEXT("("),  Line, Col)); break;
			case TEXT(')'): Out.Add(MakeToken(ERuneTokenType::RParen,  TEXT(")"),  Line, Col)); break;
			case TEXT('{'): Out.Add(MakeToken(ERuneTokenType::LBrace,  TEXT("{"),  Line, Col)); break;
			case TEXT('}'): Out.Add(MakeToken(ERuneTokenType::RBrace,  TEXT("}"),  Line, Col)); break;
			case TEXT(','): Out.Add(MakeToken(ERuneTokenType::Comma,   TEXT(","),  Line, Col)); break;
			case TEXT(':'): Out.Add(MakeToken(ERuneTokenType::Colon,   TEXT(":"),  Line, Col)); break;

			case TEXT('.'):
				if (N == TEXT('.')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::DotDot, TEXT(".."), Line, Col)); }
				else                {              Out.Add(MakeToken(ERuneTokenType::Dot,    TEXT("."),  Line, Col)); }
				break;

			case TEXT('='):
				if (N == TEXT('=')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::EqualEqual, TEXT("=="), Line, Col)); }
				else                {              Out.Add(MakeToken(ERuneTokenType::Assign,     TEXT("="),  Line, Col)); }
				break;

			case TEXT('!'):
				if (N == TEXT('=')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::NotEqual, TEXT("!="), Line, Col)); }
				else                {              Out.Add(MakeToken(ERuneTokenType::Not,      TEXT("!"),  Line, Col)); }
				break;

			case TEXT('<'):
				if (N == TEXT('=')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::LessEqual, TEXT("<="), Line, Col)); }
				else                {              Out.Add(MakeToken(ERuneTokenType::Less,      TEXT("<"),  Line, Col)); }
				break;

			case TEXT('>'):
				if (N == TEXT('=')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::GreaterEqual, TEXT(">="), Line, Col)); }
				else                {              Out.Add(MakeToken(ERuneTokenType::Greater,      TEXT(">"),  Line, Col)); }
				break;

			case TEXT('&'):
				if (N == TEXT('&')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::And, TEXT("&&"), Line, Col)); }
				else throw FRuneSyntaxError(Line, TEXT("Unexpected '&' — did you mean '&&'?"));
				break;

			case TEXT('|'):
				if (N == TEXT('|')) { S.Advance(); Out.Add(MakeToken(ERuneTokenType::Or, TEXT("||"), Line, Col)); }
				else throw FRuneSyntaxError(Line, TEXT("Unexpected '|' — did you mean '||'?"));
				break;

			default:
				throw FRuneSyntaxError(Line,
					FString::Printf(TEXT("Unexpected character '%c'"), C));
		}
	}

	Out.Add(MakeToken(ERuneTokenType::EndOfFile, FString(), S.Line, S.Col));
	return Out;
}
