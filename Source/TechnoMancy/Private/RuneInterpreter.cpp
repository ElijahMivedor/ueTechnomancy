// Copyright TechnoMancy. All rights reserved.

#include "RuneInterpreter.h"
#include "RuneLexer.h"
#include "RuneParser.h"
#include "RuneLibraryRegistry.h"
#include "TechnoMancyLog.h"
#include "Misc/ScopeExit.h"

// =============================================================================
// Compile
// =============================================================================

bool FRuneInterpreter::Compile(const FString& Source, TArray<FString>& OutErrors)
{
	Program.Reset();
	try
	{
		TArray<FRuneToken> Tokens = FRuneLexer::Tokenize(Source);
		Program = FRuneParser::Parse(Tokens);
		return true;
	}
	catch (const FRuneSyntaxError& E)
	{
		OutErrors.Add(FString::Printf(TEXT("Line %d: %s"), E.Line, *E.Message));
		Program.Reset();
		return false;
	}
	catch (...)
	{
		OutErrors.Add(TEXT("Unknown compile error"));
		Program.Reset();
		return false;
	}
}

bool FRuneInterpreter::HasFunction(FName Name) const
{
	return Program.IsValid() && Program->Functions.Contains(Name);
}

// =============================================================================
// Scope helpers
// =============================================================================

void FRuneInterpreter::Tick()
{
	if (++InstructionCounter > MaxInstructions)
	{
		throw FRuneCrashError(FString::Printf(
			TEXT("CRASH: Execution limit reached (%d instructions) — check for infinite loops"),
			MaxInstructions));
	}
}

FRuneValue& FRuneInterpreter::EnsureVar(FName Name)
{
	for (int32 i = Scopes.Num() - 1; i >= 0; --i)
	{
		if (FRuneValue* V = Scopes[i].Find(Name)) return *V;
	}
	return Scopes.Top().Add(Name, FRuneValue::MakeNumber(0.0));
}

FRuneValue FRuneInterpreter::ReadVar(FName Name) const
{
	for (int32 i = Scopes.Num() - 1; i >= 0; --i)
	{
		if (const FRuneValue* V = Scopes[i].Find(Name)) return *V;
	}
	// GML-style: undefined variables read as 0.
	return FRuneValue::MakeNumber(0.0);
}

// =============================================================================
// Invoke
// =============================================================================

bool FRuneInterpreter::Invoke(FName FunctionName,
							  FRuneWeaponContext& Context,
							  TArray<FString>& OutErrors,
							  FRuneValue* OutResult)
{
	if (!Program.IsValid())
	{
		OutErrors.Add(TEXT("No compiled program"));
		return false;
	}

	const TSharedPtr<FRuneAstFnDecl>* FnPtr = Program->Functions.Find(FunctionName);
	if (!FnPtr)
	{
		// Spell legitimately omits this hook — silent no-op.
		if (OutResult) *OutResult = FRuneValue::MakeNil();
		return true;
	}

	InstructionCounter = 0;
	CallDepth = 0;
	Scopes.Reset();

	try
	{
		const FRuneValue R = CallUserFunction(*FnPtr, TArray<FRuneValue>(), Context);
		if (OutResult) *OutResult = R;
		return true;
	}
	catch (const FRuneCrashError& E)
	{
		OutErrors.Add(E.Message);
		return false;
	}
	catch (const FRuneManaError& E)
	{
		OutErrors.Add(E.Message);
		return false;
	}
	catch (const FRuneRuntimeError& E)
	{
		OutErrors.Add(FString::Printf(TEXT("Line %d: %s"), E.Line, *E.Message));
		return false;
	}
	catch (...)
	{
		OutErrors.Add(TEXT("Unknown runtime error"));
		return false;
	}
}

FRuneValue FRuneInterpreter::CallUserFunction(const TSharedPtr<FRuneAstFnDecl>& Fn,
											  const TArray<FRuneValue>& Args,
											  FRuneWeaponContext& Ctx)
{
	++CallDepth;
	ON_SCOPE_EXIT { --CallDepth; };
	if (CallDepth > MaxCallDepth)
	{
		throw FRuneCrashError(FString::Printf(
			TEXT("CRASH: Call stack depth exceeded (max %d)"), MaxCallDepth));
	}

	Scopes.AddDefaulted();
	ON_SCOPE_EXIT { Scopes.Pop(); };

	TMap<FName, FRuneValue>& Local = Scopes.Top();
	for (int32 i = 0; i < Fn->Params.Num(); ++i)
	{
		Local.Add(Fn->Params[i], i < Args.Num() ? Args[i] : FRuneValue::MakeNumber(0.0));
	}

	FRuneValue Result = FRuneValue::MakeNil();
	try
	{
		Execute(Fn->Body, Ctx);
	}
	catch (FRuneReturnSignal& R)
	{
		Result = R.Value;
	}
	return Result;
}

// =============================================================================
// Statement execution
// =============================================================================

void FRuneInterpreter::Execute(const FRuneAstNodePtr& Node, FRuneWeaponContext& Ctx)
{
	if (!Node) return;
	Tick();

	switch (Node->Kind)
	{
		case ERuneAstKind::Block:
		{
			const FRuneAstBlock* B = static_cast<const FRuneAstBlock*>(Node.Get());
			for (const FRuneAstNodePtr& Stmt : B->Statements) Execute(Stmt, Ctx);
			return;
		}

		case ERuneAstKind::Assign:
		{
			const FRuneAstAssign* A = static_cast<const FRuneAstAssign*>(Node.Get());
			const FRuneValue V = Eval(A->Value, Ctx);
			EnsureVar(A->Target) = V;
			return;
		}

		case ERuneAstKind::ExprStmt:
		{
			const FRuneAstExprStmt* E = static_cast<const FRuneAstExprStmt*>(Node.Get());
			Eval(E->Expr, Ctx);
			return;
		}

		case ERuneAstKind::If:
		{
			const FRuneAstIf* I = static_cast<const FRuneAstIf*>(Node.Get());
			if (Eval(I->Condition, Ctx).AsBool())
			{
				Execute(I->ThenBlock, Ctx);
			}
			else if (I->ElseBlock)
			{
				Execute(I->ElseBlock, Ctx);
			}
			return;
		}

		case ERuneAstKind::While:
		{
			const FRuneAstWhile* W = static_cast<const FRuneAstWhile*>(Node.Get());
			while (Eval(W->Condition, Ctx).AsBool())
			{
				Execute(W->Body, Ctx);
			}
			return;
		}

		case ERuneAstKind::For:
		{
			const FRuneAstFor* F = static_cast<const FRuneAstFor*>(Node.Get());
			const int32 Start = FMath::TruncToInt32(Eval(F->Start, Ctx).AsNumber());
			const int32 End   = FMath::TruncToInt32(Eval(F->End,   Ctx).AsNumber());
			for (int32 i = Start; i < End; ++i)
			{
				EnsureVar(F->VarName) = FRuneValue::MakeNumber(i);
				Execute(F->Body, Ctx);
			}
			return;
		}

		case ERuneAstKind::Return:
		{
			const FRuneAstReturn* R = static_cast<const FRuneAstReturn*>(Node.Get());
			const FRuneValue V = R->Value ? Eval(R->Value, Ctx) : FRuneValue::MakeNil();
			throw FRuneReturnSignal(V);
		}

		default:
			throw FRuneRuntimeError(Node->Line, TEXT("Internal: unexpected statement node"));
	}
}

// =============================================================================
// Expression evaluation
// =============================================================================

FRuneValue FRuneInterpreter::Eval(const FRuneAstNodePtr& Node, FRuneWeaponContext& Ctx)
{
	if (!Node) return FRuneValue::MakeNil();
	Tick();

	switch (Node->Kind)
	{
		case ERuneAstKind::Number:
			return FRuneValue::MakeNumber(static_cast<const FRuneAstNumber*>(Node.Get())->Value);

		case ERuneAstKind::Bool:
			return FRuneValue::MakeBool(static_cast<const FRuneAstBool*>(Node.Get())->Value);

		case ERuneAstKind::Identifier:
			return ReadVar(static_cast<const FRuneAstIdentifier*>(Node.Get())->Name);

		case ERuneAstKind::Unary:
		{
			const FRuneAstUnary* U = static_cast<const FRuneAstUnary*>(Node.Get());
			const FRuneValue V = Eval(U->Operand, Ctx);
			switch (U->Op)
			{
				case ERuneUnaryOp::Neg: return FRuneValue::MakeNumber(-V.AsNumber());
				case ERuneUnaryOp::Not: return FRuneValue::MakeBool(!V.AsBool());
			}
			return FRuneValue::MakeNil();
		}

		case ERuneAstKind::Binary:
		{
			const FRuneAstBinary* B = static_cast<const FRuneAstBinary*>(Node.Get());
			// Short-circuit logical operators
			if (B->Op == ERuneBinaryOp::And)
			{
				const FRuneValue L = Eval(B->Left, Ctx);
				if (!L.AsBool()) return FRuneValue::MakeBool(false);
				return FRuneValue::MakeBool(Eval(B->Right, Ctx).AsBool());
			}
			if (B->Op == ERuneBinaryOp::Or)
			{
				const FRuneValue L = Eval(B->Left, Ctx);
				if (L.AsBool()) return FRuneValue::MakeBool(true);
				return FRuneValue::MakeBool(Eval(B->Right, Ctx).AsBool());
			}

			const FRuneValue L = Eval(B->Left,  Ctx);
			const FRuneValue R = Eval(B->Right, Ctx);
			const double a = L.AsNumber();
			const double b = R.AsNumber();
			switch (B->Op)
			{
				case ERuneBinaryOp::Add: return FRuneValue::MakeNumber(a + b);
				case ERuneBinaryOp::Sub: return FRuneValue::MakeNumber(a - b);
				case ERuneBinaryOp::Mul: return FRuneValue::MakeNumber(a * b);
				case ERuneBinaryOp::Div:
					if (b == 0.0) throw FRuneRuntimeError(Node->Line, TEXT("Division by zero"));
					return FRuneValue::MakeNumber(a / b);
				case ERuneBinaryOp::Mod:
					if (b == 0.0) throw FRuneRuntimeError(Node->Line, TEXT("Modulo by zero"));
					return FRuneValue::MakeNumber(FMath::Fmod(a, b));
				case ERuneBinaryOp::Eq: return FRuneValue::MakeBool(a == b);
				case ERuneBinaryOp::Ne: return FRuneValue::MakeBool(a != b);
				case ERuneBinaryOp::Lt: return FRuneValue::MakeBool(a <  b);
				case ERuneBinaryOp::Gt: return FRuneValue::MakeBool(a >  b);
				case ERuneBinaryOp::Le: return FRuneValue::MakeBool(a <= b);
				case ERuneBinaryOp::Ge: return FRuneValue::MakeBool(a >= b);
				default: break;
			}
			return FRuneValue::MakeNil();
		}

		case ERuneAstKind::Call:
		{
			const FRuneAstCall* C = static_cast<const FRuneAstCall*>(Node.Get());
			const TSharedPtr<FRuneAstFnDecl>* FnPtr = Program->Functions.Find(C->FunctionName);
			if (!FnPtr)
			{
				throw FRuneRuntimeError(Node->Line,
					FString::Printf(TEXT("Unknown function '%s'"), *C->FunctionName.ToString()));
			}
			TArray<FRuneValue> ArgVals;
			ArgVals.Reserve(C->Args.Num());
			for (const FRuneCallArg& A : C->Args) ArgVals.Add(Eval(A.Value, Ctx));
			return CallUserFunction(*FnPtr, ArgVals, Ctx);
		}

		case ERuneAstKind::LibCall:
		{
			const FRuneAstLibCall* L = static_cast<const FRuneAstLibCall*>(Node.Get());
			const FRuneLibraryRegistry& Reg = FRuneLibraryRegistry::Get();

			if (!Reg.HasNamespace(L->Namespace))
			{
				throw FRuneRuntimeError(Node->Line,
					FString::Printf(TEXT("Unknown library '%s'"), *L->Namespace.ToString()));
			}

			if (Reg.RequiresImport(L->Namespace))
			{
				const FName FullPath = Reg.FullPathFor(L->Namespace);
				if (!Program->Imports.Contains(FullPath))
				{
					throw FRuneRuntimeError(Node->Line,
						FString::Printf(TEXT("'%s' is not imported — add 'import %s' at the top"),
							*L->Namespace.ToString(), *FullPath.ToString()));
				}
			}

			const FRuneLibFunction* Fn = Reg.Find(L->Namespace, L->Method);
			if (!Fn)
			{
				const FName Suggestion = Reg.SuggestMethod(L->Namespace, L->Method);
				if (Suggestion != NAME_None)
				{
					throw FRuneRuntimeError(Node->Line,
						FString::Printf(TEXT("'%s.%s' is not a known function. Did you mean '%s.%s'?"),
							*L->Namespace.ToString(), *L->Method.ToString(),
							*L->Namespace.ToString(), *Suggestion.ToString()));
				}
				throw FRuneRuntimeError(Node->Line,
					FString::Printf(TEXT("'%s.%s' is not a known function."),
						*L->Namespace.ToString(), *L->Method.ToString()));
			}

			// Mana check + deduct
			if (Fn->ManaCost > 0.f && Ctx.Mana < Fn->ManaCost)
			{
				throw FRuneManaError(FString::Printf(
					TEXT("Insufficient mana: %.0f required, %.0f available"),
					Fn->ManaCost, Ctx.Mana));
			}
			Ctx.Mana = FMath::Max(0.f, Ctx.Mana - Fn->ManaCost);

			// Args — library calls require named args (PRD Feature 1)
			FRuneLibCallArgs Args;
			for (const FRuneCallArg& A : L->Args)
			{
				if (A.Name.IsNone())
				{
					throw FRuneRuntimeError(Node->Line,
						TEXT("Library calls require named arguments (e.g. spd: 9)"));
				}
				Args.Named.Add(A.Name, Eval(A.Value, Ctx));
			}

			return Fn->Callback(Args, Ctx);
		}

		default:
			throw FRuneRuntimeError(Node->Line, TEXT("Internal: unexpected expression node"));
	}
}

// =============================================================================
// Static mana estimate
// =============================================================================

float FRuneInterpreter::EstimateNodeCost(const FRuneAstNodePtr& Node) const
{
	if (!Node) return 0.f;

	switch (Node->Kind)
	{
		case ERuneAstKind::LibCall:
		{
			const FRuneAstLibCall* L = static_cast<const FRuneAstLibCall*>(Node.Get());
			float Cost = 0.f;
			if (const FRuneLibFunction* Fn = FRuneLibraryRegistry::Get().Find(L->Namespace, L->Method))
			{
				Cost = Fn->ManaCost;
			}
			for (const FRuneCallArg& A : L->Args) Cost += EstimateNodeCost(A.Value);
			return Cost;
		}
		case ERuneAstKind::Call:
		{
			const FRuneAstCall* C = static_cast<const FRuneAstCall*>(Node.Get());
			float Cost = 0.f;
			for (const FRuneCallArg& A : C->Args) Cost += EstimateNodeCost(A.Value);
			if (Program.IsValid())
			{
				if (const TSharedPtr<FRuneAstFnDecl>* FnPtr = Program->Functions.Find(C->FunctionName))
				{
					Cost += EstimateNodeCost((*FnPtr)->Body);
				}
			}
			return Cost;
		}
		case ERuneAstKind::Block:
		{
			const FRuneAstBlock* B = static_cast<const FRuneAstBlock*>(Node.Get());
			float Cost = 0.f;
			for (const FRuneAstNodePtr& Stmt : B->Statements) Cost += EstimateNodeCost(Stmt);
			return Cost;
		}
		case ERuneAstKind::Assign:
			return EstimateNodeCost(static_cast<const FRuneAstAssign*>(Node.Get())->Value);
		case ERuneAstKind::ExprStmt:
			return EstimateNodeCost(static_cast<const FRuneAstExprStmt*>(Node.Get())->Expr);
		case ERuneAstKind::If:
		{
			const FRuneAstIf* I = static_cast<const FRuneAstIf*>(Node.Get());
			return EstimateNodeCost(I->Condition) + EstimateNodeCost(I->ThenBlock) + EstimateNodeCost(I->ElseBlock);
		}
		case ERuneAstKind::While:
		{
			const FRuneAstWhile* W = static_cast<const FRuneAstWhile*>(Node.Get());
			return EstimateNodeCost(W->Condition) + EstimateNodeCost(W->Body);
		}
		case ERuneAstKind::For:
		{
			const FRuneAstFor* F = static_cast<const FRuneAstFor*>(Node.Get());
			return EstimateNodeCost(F->Start) + EstimateNodeCost(F->End) + EstimateNodeCost(F->Body);
		}
		case ERuneAstKind::Return:
			return EstimateNodeCost(static_cast<const FRuneAstReturn*>(Node.Get())->Value);
		case ERuneAstKind::Binary:
		{
			const FRuneAstBinary* B = static_cast<const FRuneAstBinary*>(Node.Get());
			return EstimateNodeCost(B->Left) + EstimateNodeCost(B->Right);
		}
		case ERuneAstKind::Unary:
			return EstimateNodeCost(static_cast<const FRuneAstUnary*>(Node.Get())->Operand);
		default:
			return 0.f;
	}
}

float FRuneInterpreter::EstimateManaCost(FName FunctionName) const
{
	if (!Program.IsValid()) return 0.f;
	const TSharedPtr<FRuneAstFnDecl>* FnPtr = Program->Functions.Find(FunctionName);
	if (!FnPtr) return 0.f;
	return EstimateNodeCost((*FnPtr)->Body);
}

// =============================================================================
// Self-test
// =============================================================================

bool FRuneInterpreter::SelfTest(FString& OutLog)
{
	const FString Source = TEXT(
		"import runes.fire\n"
		"Fn Primary() {\n"
		"  x = 3\n"
		"  if x > 2 {\n"
		"    x = x + 1\n"
		"  }\n"
		"  return x\n"
		"}\n");

	FRuneInterpreter Interp;
	TArray<FString> Errors;
	if (!Interp.Compile(Source, Errors))
	{
		OutLog += TEXT("Compile failed:\n");
		for (const FString& E : Errors) OutLog += FString::Printf(TEXT("  %s\n"), *E);
		return false;
	}

	FRuneWeaponContext Ctx;
	Ctx.Mana = 50.f;
	Ctx.MaxMana = 50.f;

	FRuneValue Result;
	const bool bOk = Interp.Invoke(FName(TEXT("Primary")), Ctx, Errors, &Result);
	if (!bOk)
	{
		OutLog += TEXT("Invoke failed:\n");
		for (const FString& E : Errors) OutLog += FString::Printf(TEXT("  %s\n"), *E);
		return false;
	}

	const double Got = Result.AsNumber();
	OutLog += FString::Printf(TEXT("Primary() returned %.2f (expected 4)\n"), Got);
	return FMath::IsNearlyEqual(Got, 4.0);
}
