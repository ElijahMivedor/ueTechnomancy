// Copyright TechnoMancy. All rights reserved.
//
// UMG-compatible multi-line code editor with RuneScript syntax highlighting.
// Wraps a Slate SMultiLineEditableTextBox configured with
// FRuneSyntaxHighlightMarshaller. Drop into any UMG layout.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RuneTerminalStyle.h"
#include "RuneCodeEditor.generated.h"

class FRuneSyntaxHighlightMarshaller;
class SMultiLineEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRuneCodeEditorTextChanged, const FText&, NewText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRuneCodeEditorTextCommitted, const FText&, NewText, ETextCommit::Type, CommitMethod);

UCLASS(meta = (DisplayName = "Rune Code Editor"))
class TECHNOMANCY_API URuneCodeEditor : public UWidget
{
	GENERATED_BODY()

public:
	URuneCodeEditor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript", meta = (MultiLine = true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	FRuneTerminalStyle Style;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	FText HintText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneScript")
	bool bIsReadOnly = false;

	UPROPERTY(BlueprintAssignable, Category = "RuneScript|Events")
	FOnRuneCodeEditorTextChanged OnTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "RuneScript|Events")
	FOnRuneCodeEditorTextCommitted OnTextCommitted;

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	FText GetText() const;

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void SetText(const FText& NewText);

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void SetIsReadOnly(bool bNewReadOnly);

	UFUNCTION(BlueprintCallable, Category = "RuneScript")
	void ApplyStyle(const FRuneTerminalStyle& NewStyle);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

private:
	TSharedPtr<SMultiLineEditableTextBox> MyEditableTextBox;
	TSharedPtr<FRuneSyntaxHighlightMarshaller> Marshaller;

	void HandleTextChanged(const FText& NewText);
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod);
};
