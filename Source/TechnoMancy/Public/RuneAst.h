// Copyright TechnoMancy. All rights reserved.
//
// Abstract Syntax Tree nodes and interpreter exception types.
// AST nodes are plain C++ (no UObject) and held by TSharedPtr.

#pragma once

#include "CoreMinimal.h"
#include "RuneTypes.h"

enum class ERuneAstKind : uint8
{
	// Expressions
	Number, Bool, Identifier,
	Binary, Unary,
	Call, LibCall,
	// Statements
	Assign, ExprStmt,
	If, While, For,
	Return, Block,
	// Top-level
	Import, FnDecl, Program,
};

enum class ERuneBinaryOp : uint8 { Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Gt, Le, Ge, And, Or };
enum class ERuneUnaryOp  : uint8 { Neg, Not };

// -----------------------------------------------------------------------------
// Base node
// -----------------------------------------------------------------------------

struct FRuneAstNode
{
	ERuneAstKind Kind;
	int32 Line = 0;

	FRuneAstNode(ERuneAstKind InKind, int32 InLine) : Kind(InKind), Line(InLine) {}
	virtual ~FRuneAstNode() = default;
};

using FRuneAstNodePtr = TSharedPtr<FRuneAstNode>;

// -----------------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------------

struct FRuneAstNumber : FRuneAstNode
{
	double Value = 0.0;
	FRuneAstNumber(double InValue, int32 InLine) : FRuneAstNode(ERuneAstKind::Number, InLine), Value(InValue) {}
};

struct FRuneAstBool : FRuneAstNode
{
	bool Value = false;
	FRuneAstBool(bool InValue, int32 InLine) : FRuneAstNode(ERuneAstKind::Bool, InLine), Value(InValue) {}
};

struct FRuneAstIdentifier : FRuneAstNode
{
	FName Name;
	FRuneAstIdentifier(FName InName, int32 InLine) : FRuneAstNode(ERuneAstKind::Identifier, InLine), Name(InName) {}
};

struct FRuneAstBinary : FRuneAstNode
{
	ERuneBinaryOp Op = ERuneBinaryOp::Add;
	FRuneAstNodePtr Left;
	FRuneAstNodePtr Right;
	FRuneAstBinary(ERuneBinaryOp InOp, FRuneAstNodePtr InLeft, FRuneAstNodePtr InRight, int32 InLine)
		: FRuneAstNode(ERuneAstKind::Binary, InLine), Op(InOp), Left(InLeft), Right(InRight) {}
};

struct FRuneAstUnary : FRuneAstNode
{
	ERuneUnaryOp Op = ERuneUnaryOp::Neg;
	FRuneAstNodePtr Operand;
	FRuneAstUnary(ERuneUnaryOp InOp, FRuneAstNodePtr InOperand, int32 InLine)
		: FRuneAstNode(ERuneAstKind::Unary, InLine), Op(InOp), Operand(InOperand) {}
};

struct FRuneCallArg
{
	FName Name = NAME_None;
	FRuneAstNodePtr Value;
};

struct FRuneAstCall : FRuneAstNode
{
	FName FunctionName;
	TArray<FRuneCallArg> Args;
	FRuneAstCall(FName InName, int32 InLine) : FRuneAstNode(ERuneAstKind::Call, InLine), FunctionName(InName) {}
};

struct FRuneAstLibCall : FRuneAstNode
{
	FName Namespace;
	FName Method;
	TArray<FRuneCallArg> Args;
	FRuneAstLibCall(FName InNamespace, FName InMethod, int32 InLine)
		: FRuneAstNode(ERuneAstKind::LibCall, InLine), Namespace(InNamespace), Method(InMethod) {}
};

// -----------------------------------------------------------------------------
// Statements
// -----------------------------------------------------------------------------

struct FRuneAstAssign : FRuneAstNode
{
	FName Target;
	FRuneAstNodePtr Value;
	FRuneAstAssign(FName InTarget, FRuneAstNodePtr InValue, int32 InLine)
		: FRuneAstNode(ERuneAstKind::Assign, InLine), Target(InTarget), Value(InValue) {}
};

struct FRuneAstExprStmt : FRuneAstNode
{
	FRuneAstNodePtr Expr;
	FRuneAstExprStmt(FRuneAstNodePtr InExpr, int32 InLine)
		: FRuneAstNode(ERuneAstKind::ExprStmt, InLine), Expr(InExpr) {}
};

struct FRuneAstBlock : FRuneAstNode
{
	TArray<FRuneAstNodePtr> Statements;
	explicit FRuneAstBlock(int32 InLine) : FRuneAstNode(ERuneAstKind::Block, InLine) {}
};

struct FRuneAstIf : FRuneAstNode
{
	FRuneAstNodePtr Condition;
	FRuneAstNodePtr ThenBlock;
	FRuneAstNodePtr ElseBlock; // nullable
	explicit FRuneAstIf(int32 InLine) : FRuneAstNode(ERuneAstKind::If, InLine) {}
};

struct FRuneAstWhile : FRuneAstNode
{
	FRuneAstNodePtr Condition;
	FRuneAstNodePtr Body;
	explicit FRuneAstWhile(int32 InLine) : FRuneAstNode(ERuneAstKind::While, InLine) {}
};

struct FRuneAstFor : FRuneAstNode
{
	FName VarName;
	FRuneAstNodePtr Start;
	FRuneAstNodePtr End;
	FRuneAstNodePtr Body;
	explicit FRuneAstFor(int32 InLine) : FRuneAstNode(ERuneAstKind::For, InLine) {}
};

struct FRuneAstReturn : FRuneAstNode
{
	FRuneAstNodePtr Value; // nullable
	explicit FRuneAstReturn(int32 InLine) : FRuneAstNode(ERuneAstKind::Return, InLine) {}
};

// -----------------------------------------------------------------------------
// Top-level declarations
// -----------------------------------------------------------------------------

struct FRuneAstFnDecl : FRuneAstNode
{
	FName Name;
	TArray<FName> Params;
	FRuneAstNodePtr Body; // FRuneAstBlock
	FRuneAstFnDecl(FName InName, int32 InLine) : FRuneAstNode(ERuneAstKind::FnDecl, InLine), Name(InName) {}
};

struct FRuneAstProgram : FRuneAstNode
{
	TArray<FName> Imports; // fully-qualified names, e.g. "runes.fire"
	TMap<FName, TSharedPtr<FRuneAstFnDecl>> Functions;
	FRuneAstProgram() : FRuneAstNode(ERuneAstKind::Program, 0) {}
};

// -----------------------------------------------------------------------------
// Interpreter signals (exception types — bEnableExceptions = true)
// -----------------------------------------------------------------------------

struct FRuneSyntaxError
{
	int32 Line = 0;
	FString Message;
	FRuneSyntaxError(int32 InLine, FString InMessage) : Line(InLine), Message(MoveTemp(InMessage)) {}
};

struct FRuneRuntimeError
{
	int32 Line = 0;
	FString Message;
	FRuneRuntimeError(int32 InLine, FString InMessage) : Line(InLine), Message(MoveTemp(InMessage)) {}
};

struct FRuneCrashError
{
	FString Message;
	explicit FRuneCrashError(FString InMessage) : Message(MoveTemp(InMessage)) {}
};

struct FRuneManaError
{
	FString Message;
	explicit FRuneManaError(FString InMessage) : Message(MoveTemp(InMessage)) {}
};

// Internal control-flow signal (not a user-visible error).
struct FRuneReturnSignal
{
	FRuneValue Value;
	explicit FRuneReturnSignal(FRuneValue InValue) : Value(InValue) {}
};
