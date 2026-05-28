// Copyright TechnoMancy. All rights reserved.

#include "RuneCodeEditor.h"
#include "RuneSyntaxHighlightMarshaller.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

URuneCodeEditor::URuneCodeEditor()
{
	// Sensible default font so the editor isn't unreadable out of the box.
	Style.FontInfo.Size = 14;
}

FText URuneCodeEditor::GetText() const
{
	if (MyEditableTextBox.IsValid())
	{
		return MyEditableTextBox->GetText();
	}
	return Text;
}

void URuneCodeEditor::SetText(const FText& NewText)
{
	Text = NewText;
	if (MyEditableTextBox.IsValid())
	{
		MyEditableTextBox->SetText(NewText);
	}
}

void URuneCodeEditor::SetIsReadOnly(bool bNewReadOnly)
{
	bIsReadOnly = bNewReadOnly;
	if (MyEditableTextBox.IsValid())
	{
		MyEditableTextBox->SetIsReadOnly(bIsReadOnly);
	}
}

void URuneCodeEditor::ApplyStyle(const FRuneTerminalStyle& NewStyle)
{
	Style = NewStyle;
	if (Marshaller.IsValid())
	{
		Marshaller->SetStyle(Style);
	}
	if (MyEditableTextBox.IsValid())
	{
		// Re-trigger a layout refresh so the new style is applied to existing runs.
		MyEditableTextBox->Refresh();
	}
}

TSharedRef<SWidget> URuneCodeEditor::RebuildWidget()
{
	Marshaller = FRuneSyntaxHighlightMarshaller::Create(Style);

	MyEditableTextBox = SNew(SMultiLineEditableTextBox)
		.Marshaller(Marshaller)
		.Text(Text)
		.HintText(HintText)
		.IsReadOnly(bIsReadOnly)
		.AutoWrapText(true)
		.OnTextChanged(FOnTextChanged::CreateUObject(this, &URuneCodeEditor::HandleTextChanged))
		.OnTextCommitted(FOnTextCommitted::CreateUObject(this, &URuneCodeEditor::HandleTextCommitted));

	return MyEditableTextBox.ToSharedRef();
}

void URuneCodeEditor::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyEditableTextBox.Reset();
	Marshaller.Reset();
}

void URuneCodeEditor::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!MyEditableTextBox.IsValid()) return;

	if (Marshaller.IsValid())
	{
		Marshaller->SetStyle(Style);
	}
	MyEditableTextBox->SetText(Text);
	MyEditableTextBox->SetHintText(HintText);
	MyEditableTextBox->SetIsReadOnly(bIsReadOnly);
}

void URuneCodeEditor::HandleTextChanged(const FText& NewText)
{
	Text = NewText;
	OnTextChanged.Broadcast(NewText);
}

void URuneCodeEditor::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitMethod)
{
	Text = NewText;
	OnTextCommitted.Broadcast(NewText, CommitMethod);
}
