// Copyright TechnoMancy. All rights reserved.
//
// Recursive-descent parser for RuneScript.
//
//   program     = (import | fnDecl)*
//   import      = 'import' qualifiedName
//   fnDecl      = 'Fn' IDENT '(' paramList? ')' block
//   block       = '{' statement* '}'
//   statement   = assign | exprStmt | if | while | for | return
//   if          = 'if' expression block ('else' (if | block))?
//   while       = 'while' expression block
//   for         = 'for' IDENT 'in' expression '..' expression block
//
//   expression  = logicalOr
//   logicalOr   = logicalAnd ('||' logicalAnd)*
//   logicalAnd  = equality   ('&&' equality)*
//   equality    = comparison (('==' | '!=') comparison)*
//   comparison  = term       (('<' | '>' | '<=' | '>=') term)*
//   term        = factor     (('+' | '-') factor)*
//   factor      = unary      (('*' | '/' | '%') unary)*
//   unary       = ('-' | '!') unary | primary
//   primary     = NUMBER | TRUE | FALSE | '(' expression ')'
//               | IDENT                                  // variable
//               | IDENT '(' args ')'                     // user-fn call
//               | IDENT '.' IDENT '(' args ')'           // lib call
//   args        = (arg (',' arg)*)?
//   arg         = (IDENT ':')? expression                // named or positional

#include "RuneParser.h"

namespace
{
	struct FParseState
	{
		const TArray<FRuneToken>& Tokens;
		int32 Pos = 0;

		explicit FParseState(const TArray<FRuneToken>& InTokens) : Tokens(InTokens) {}

		const FRuneToken& Peek(int32 Offset = 0) const
		{
			const int32 Idx = FMath::Min(Pos + Offset, Tokens.Num() - 1);
			return Tokens[Idx];
		}

		const FRuneToken& Advance()
		{
			const FRuneToken& T = Tokens[Pos];
			if (Pos < Tokens.Num() - 1) ++Pos;
			return T;
		}

		bool Check(ERuneTokenType T) const { return Peek().Type == T; }

		bool Match(ERuneTokenType T)
		{
			if (Check(T)) { Advance(); return true; }
			return false;
		}

		const FRuneToken& Expect(ERuneTokenType T, const TCHAR* What)
		{
			if (!Check(T))
			{
				throw FRuneSyntaxError(Peek().Line,
					FString::Printf(TEXT("Expected %s, got '%s'"), What, *Peek().Lexeme));
			}
			return Advance();
		}
	};

	FRuneAstNodePtr ParseExpression(FParseState& S);
	FRuneAstNodePtr ParseBlock     (FParseState& S);
	FRuneAstNodePtr ParseStatement (FParseState& S);

	TArray<FRuneCallArg> ParseArgList(FParseState& S)
	{
		TArray<FRuneCallArg> Args;
		S.Expect(ERuneTokenType::LParen, TEXT("'('"));
		if (S.Match(ERuneTokenType::RParen)) return Args;

		while (true)
		{
			FRuneCallArg A;
			// Named arg form: IDENT ':' expr
			if (S.Peek().Type == ERuneTokenType::Identifier
				&& S.Peek(1).Type == ERuneTokenType::Colon)
			{
				A.Name = FName(*S.Advance().Lexeme);
				S.Advance(); // ':'
			}
			A.Value = ParseExpression(S);
			Args.Add(MoveTemp(A));
			if (!S.Match(ERuneTokenType::Comma)) break;
		}
		S.Expect(ERuneTokenType::RParen, TEXT("')'"));
		return Args;
	}

	FRuneAstNodePtr ParsePrimary(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		const FRuneToken& T = S.Peek();

		switch (T.Type)
		{
			case ERuneTokenType::Number:
				S.Advance();
				return MakeShared<FRuneAstNumber>(T.NumberValue, Line);

			case ERuneTokenType::True:
				S.Advance();
				return MakeShared<FRuneAstBool>(true, Line);

			case ERuneTokenType::False:
				S.Advance();
				return MakeShared<FRuneAstBool>(false, Line);

			case ERuneTokenType::LParen:
			{
				S.Advance();
				FRuneAstNodePtr Inner = ParseExpression(S);
				S.Expect(ERuneTokenType::RParen, TEXT("')'"));
				return Inner;
			}

			case ERuneTokenType::Identifier:
			{
				const FName Name(*S.Advance().Lexeme);

				// IDENT '.' IDENT '(' ... ')' — lib call
				if (S.Check(ERuneTokenType::Dot))
				{
					S.Advance();
					const FRuneToken& MethodTok = S.Expect(ERuneTokenType::Identifier,
						TEXT("identifier after '.'"));
					const FName Method(*MethodTok.Lexeme);
					TSharedPtr<FRuneAstLibCall> Call = MakeShared<FRuneAstLibCall>(Name, Method, Line);
					Call->Args = ParseArgList(S);
					return Call;
				}

				// IDENT '(' ... ')' — user fn call
				if (S.Check(ERuneTokenType::LParen))
				{
					TSharedPtr<FRuneAstCall> Call = MakeShared<FRuneAstCall>(Name, Line);
					Call->Args = ParseArgList(S);
					return Call;
				}

				// Plain identifier reference
				return MakeShared<FRuneAstIdentifier>(Name, Line);
			}

			default:
				throw FRuneSyntaxError(Line,
					FString::Printf(TEXT("Unexpected token '%s'"), *T.Lexeme));
		}
	}

	FRuneAstNodePtr ParseUnary(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		if (S.Match(ERuneTokenType::Minus))
		{
			return MakeShared<FRuneAstUnary>(ERuneUnaryOp::Neg, ParseUnary(S), Line);
		}
		if (S.Match(ERuneTokenType::Not))
		{
			return MakeShared<FRuneAstUnary>(ERuneUnaryOp::Not, ParseUnary(S), Line);
		}
		return ParsePrimary(S);
	}

	FRuneAstNodePtr ParseFactor(FParseState& S)
	{
		FRuneAstNodePtr Left = ParseUnary(S);
		while (true)
		{
			const int32 Line = S.Peek().Line;
			ERuneBinaryOp Op;
			if      (S.Match(ERuneTokenType::Star))    Op = ERuneBinaryOp::Mul;
			else if (S.Match(ERuneTokenType::Slash))   Op = ERuneBinaryOp::Div;
			else if (S.Match(ERuneTokenType::Percent)) Op = ERuneBinaryOp::Mod;
			else break;
			Left = MakeShared<FRuneAstBinary>(Op, Left, ParseUnary(S), Line);
		}
		return Left;
	}

	FRuneAstNodePtr ParseTerm(FParseState& S)
	{
		FRuneAstNodePtr Left = ParseFactor(S);
		while (true)
		{
			const int32 Line = S.Peek().Line;
			ERuneBinaryOp Op;
			if      (S.Match(ERuneTokenType::Plus))  Op = ERuneBinaryOp::Add;
			else if (S.Match(ERuneTokenType::Minus)) Op = ERuneBinaryOp::Sub;
			else break;
			Left = MakeShared<FRuneAstBinary>(Op, Left, ParseFactor(S), Line);
		}
		return Left;
	}

	FRuneAstNodePtr ParseComparison(FParseState& S)
	{
		FRuneAstNodePtr Left = ParseTerm(S);
		while (true)
		{
			const int32 Line = S.Peek().Line;
			ERuneBinaryOp Op;
			if      (S.Match(ERuneTokenType::Less))         Op = ERuneBinaryOp::Lt;
			else if (S.Match(ERuneTokenType::Greater))      Op = ERuneBinaryOp::Gt;
			else if (S.Match(ERuneTokenType::LessEqual))    Op = ERuneBinaryOp::Le;
			else if (S.Match(ERuneTokenType::GreaterEqual)) Op = ERuneBinaryOp::Ge;
			else break;
			Left = MakeShared<FRuneAstBinary>(Op, Left, ParseTerm(S), Line);
		}
		return Left;
	}

	FRuneAstNodePtr ParseEquality(FParseState& S)
	{
		FRuneAstNodePtr Left = ParseComparison(S);
		while (true)
		{
			const int32 Line = S.Peek().Line;
			ERuneBinaryOp Op;
			if      (S.Match(ERuneTokenType::EqualEqual)) Op = ERuneBinaryOp::Eq;
			else if (S.Match(ERuneTokenType::NotEqual))   Op = ERuneBinaryOp::Ne;
			else break;
			Left = MakeShared<FRuneAstBinary>(Op, Left, ParseComparison(S), Line);
		}
		return Left;
	}

	FRuneAstNodePtr ParseLogicalAnd(FParseState& S)
	{
		FRuneAstNodePtr Left = ParseEquality(S);
		while (S.Check(ERuneTokenType::And))
		{
			const int32 Line = S.Peek().Line;
			S.Advance();
			Left = MakeShared<FRuneAstBinary>(ERuneBinaryOp::And, Left, ParseEquality(S), Line);
		}
		return Left;
	}

	FRuneAstNodePtr ParseLogicalOr(FParseState& S)
	{
		FRuneAstNodePtr Left = ParseLogicalAnd(S);
		while (S.Check(ERuneTokenType::Or))
		{
			const int32 Line = S.Peek().Line;
			S.Advance();
			Left = MakeShared<FRuneAstBinary>(ERuneBinaryOp::Or, Left, ParseLogicalAnd(S), Line);
		}
		return Left;
	}

	FRuneAstNodePtr ParseExpression(FParseState& S)
	{
		return ParseLogicalOr(S);
	}

	FRuneAstNodePtr ParseBlock(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		S.Expect(ERuneTokenType::LBrace, TEXT("'{'"));
		TSharedPtr<FRuneAstBlock> Block = MakeShared<FRuneAstBlock>(Line);
		while (!S.Check(ERuneTokenType::RBrace) && !S.Check(ERuneTokenType::EndOfFile))
		{
			Block->Statements.Add(ParseStatement(S));
		}
		S.Expect(ERuneTokenType::RBrace, TEXT("'}'"));
		return Block;
	}

	FRuneAstNodePtr ParseIf(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		S.Advance(); // 'if'
		FRuneAstNodePtr Cond = ParseExpression(S);
		if (!S.Check(ERuneTokenType::LBrace))
		{
			throw FRuneSyntaxError(S.Peek().Line,
				TEXT("Expected '{' after if condition — braces are required"));
		}
		FRuneAstNodePtr Then = ParseBlock(S);
		TSharedPtr<FRuneAstIf> Node = MakeShared<FRuneAstIf>(Line);
		Node->Condition = Cond;
		Node->ThenBlock = Then;
		if (S.Match(ERuneTokenType::Else))
		{
			if (S.Check(ERuneTokenType::If))
			{
				Node->ElseBlock = ParseIf(S);
			}
			else if (S.Check(ERuneTokenType::LBrace))
			{
				Node->ElseBlock = ParseBlock(S);
			}
			else
			{
				throw FRuneSyntaxError(S.Peek().Line,
					TEXT("Expected '{' or 'if' after 'else'"));
			}
		}
		return Node;
	}

	FRuneAstNodePtr ParseWhile(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		S.Advance(); // 'while'
		FRuneAstNodePtr Cond = ParseExpression(S);
		if (!S.Check(ERuneTokenType::LBrace))
		{
			throw FRuneSyntaxError(S.Peek().Line,
				TEXT("Expected '{' after while condition — braces are required"));
		}
		TSharedPtr<FRuneAstWhile> Node = MakeShared<FRuneAstWhile>(Line);
		Node->Condition = Cond;
		Node->Body = ParseBlock(S);
		return Node;
	}

	FRuneAstNodePtr ParseFor(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		S.Advance(); // 'for'
		const FRuneToken& VarTok = S.Expect(ERuneTokenType::Identifier, TEXT("loop variable"));
		const FName VarName(*VarTok.Lexeme);
		S.Expect(ERuneTokenType::In, TEXT("'in'"));
		FRuneAstNodePtr Start = ParseExpression(S);
		S.Expect(ERuneTokenType::DotDot, TEXT("'..'"));
		FRuneAstNodePtr End = ParseExpression(S);
		if (!S.Check(ERuneTokenType::LBrace))
		{
			throw FRuneSyntaxError(S.Peek().Line,
				TEXT("Expected '{' after for range — braces are required"));
		}
		TSharedPtr<FRuneAstFor> Node = MakeShared<FRuneAstFor>(Line);
		Node->VarName = VarName;
		Node->Start = Start;
		Node->End = End;
		Node->Body = ParseBlock(S);
		return Node;
	}

	FRuneAstNodePtr ParseReturn(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		S.Advance(); // 'return'
		TSharedPtr<FRuneAstReturn> Node = MakeShared<FRuneAstReturn>(Line);

		// `return` followed by a value is optional — if the next token can start
		// an expression, parse it; otherwise treat as a bare return.
		const ERuneTokenType N = S.Peek().Type;
		const bool bCanStartExpr =
			N == ERuneTokenType::Number ||
			N == ERuneTokenType::Identifier ||
			N == ERuneTokenType::True ||
			N == ERuneTokenType::False ||
			N == ERuneTokenType::LParen ||
			N == ERuneTokenType::Minus ||
			N == ERuneTokenType::Not;
		if (bCanStartExpr)
		{
			Node->Value = ParseExpression(S);
		}
		return Node;
	}

	FRuneAstNodePtr ParseAssignOrExprStmt(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		// IDENT '=' expr  →  assignment
		if (S.Peek().Type == ERuneTokenType::Identifier
			&& S.Peek(1).Type == ERuneTokenType::Assign)
		{
			const FName Target(*S.Advance().Lexeme);
			S.Advance(); // '='
			FRuneAstNodePtr Value = ParseExpression(S);
			return MakeShared<FRuneAstAssign>(Target, Value, Line);
		}
		FRuneAstNodePtr Expr = ParseExpression(S);
		return MakeShared<FRuneAstExprStmt>(Expr, Line);
	}

	FRuneAstNodePtr ParseStatement(FParseState& S)
	{
		if (S.Check(ERuneTokenType::If))     return ParseIf    (S);
		if (S.Check(ERuneTokenType::While))  return ParseWhile (S);
		if (S.Check(ERuneTokenType::For))    return ParseFor   (S);
		if (S.Check(ERuneTokenType::Return)) return ParseReturn(S);
		return ParseAssignOrExprStmt(S);
	}

	FName ParseQualifiedName(FParseState& S)
	{
		FString Acc = S.Expect(ERuneTokenType::Identifier, TEXT("identifier")).Lexeme;
		while (S.Match(ERuneTokenType::Dot))
		{
			Acc += TEXT(".");
			Acc += S.Expect(ERuneTokenType::Identifier, TEXT("identifier after '.'")).Lexeme;
		}
		return FName(*Acc);
	}

	TSharedPtr<FRuneAstFnDecl> ParseFnDecl(FParseState& S)
	{
		const int32 Line = S.Peek().Line;
		S.Advance(); // 'Fn'
		const FRuneToken& NameTok = S.Expect(ERuneTokenType::Identifier, TEXT("function name"));
		const FName Name(*NameTok.Lexeme);
		TSharedPtr<FRuneAstFnDecl> Fn = MakeShared<FRuneAstFnDecl>(Name, Line);
		S.Expect(ERuneTokenType::LParen, TEXT("'('"));
		if (!S.Check(ERuneTokenType::RParen))
		{
			while (true)
			{
				const FRuneToken& P = S.Expect(ERuneTokenType::Identifier, TEXT("parameter name"));
				Fn->Params.Add(FName(*P.Lexeme));
				if (!S.Match(ERuneTokenType::Comma)) break;
			}
		}
		S.Expect(ERuneTokenType::RParen, TEXT("')'"));
		Fn->Body = ParseBlock(S);
		return Fn;
	}
}

TSharedPtr<FRuneAstProgram> FRuneParser::Parse(const TArray<FRuneToken>& Tokens)
{
	FParseState S(Tokens);
	TSharedPtr<FRuneAstProgram> Prog = MakeShared<FRuneAstProgram>();

	while (!S.Check(ERuneTokenType::EndOfFile))
	{
		if (S.Check(ERuneTokenType::Import))
		{
			S.Advance();
			const FName NS = ParseQualifiedName(S);
			Prog->Imports.AddUnique(NS);
		}
		else if (S.Check(ERuneTokenType::Fn))
		{
			TSharedPtr<FRuneAstFnDecl> Fn = ParseFnDecl(S);
			Prog->Functions.Add(Fn->Name, Fn);
		}
		else
		{
			throw FRuneSyntaxError(S.Peek().Line,
				FString::Printf(TEXT("Expected 'import' or 'Fn', got '%s'"), *S.Peek().Lexeme));
		}
	}
	return Prog;
}
