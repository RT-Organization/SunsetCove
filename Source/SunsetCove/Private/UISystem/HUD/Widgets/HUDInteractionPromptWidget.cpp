// HUDInteractionPromptWidget.cpp
// SunsetCove - UI System / HUD. See Public/UISystem/HUD/Widgets/HUDInteractionPromptWidget.h.

#include "UISystem/HUD/Widgets/HUDInteractionPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Settings/SunsetCovePlayerPreferences.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InteractionSystem/InteractionComponent.h"
#include "UISystem/HUD/SunsetCoveHUDSubsystem.h"
#include "UISystem/Input/InputPromptWidget.h"

#define LOCTEXT_NAMESPACE "SunsetCoveInteractionPrompt"

UHUDInteractionPromptWidget::UHUDInteractionPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HoldLabelFormat = LOCTEXT("HoldLabelFormat", "{Label} (Hold)");
	ReleaseLabelFormat = LOCTEXT("ReleaseLabelFormat", "{Label} (Release)");
}

// ----------------------------------------------------------------------
// UUserWidget
// ----------------------------------------------------------------------

void UHUDInteractionPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!PromptList)
	{
		BuildDefaultLayout();
	}
}

void UHUDInteractionPromptWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		BuildDesignerPreview();
	}
}

void UHUDInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Nothing to show until the pawn's prompts arrive: no fade-in of an empty panel.
	VisibilityAlpha = 0.0f;
	SetRenderOpacity(0.0f);
}

void UHUDInteractionPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const float Target = (ShownPrompts.Num() > 0 && ShouldBeVisible()) ? 1.0f : 0.0f;
	VisibilityAlpha = (FadeSpeed <= 0.0f) ? Target : FMath::FInterpTo(VisibilityAlpha, Target, InDeltaTime, FadeSpeed);
	if (VisibilityAlpha < 0.01f && Target == 0.0f)
	{
		VisibilityAlpha = 0.0f;
	}
	SetRenderOpacity(VisibilityAlpha);

	// Fully faded out: collapse the list and drop the old rows, so nothing at all can stay on screen (some brushes, such as a
	// rounded-box outline, do not follow the inherited opacity).
	if (PromptList)
	{
		const bool bHidden = VisibilityAlpha <= 0.0f;
		const ESlateVisibility Wanted = bHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
		if (PromptList->GetVisibility() != Wanted)
		{
			PromptList->SetVisibility(Wanted);
		}
		if (bHidden && ShownPrompts.Num() == 0)
		{
			for (UInputPromptWidget* Row : Rows)
			{
				if (Row && Row->GetParent())
				{
					Row->RemoveFromParent();
				}
			}
		}
	}
}

// ----------------------------------------------------------------------
// Pawn binding
// ----------------------------------------------------------------------

void UHUDInteractionPromptWidget::HandlePawnBound(APawn* Pawn)
{
	HandlePawnUnbound(nullptr);

	UInteractionComponent* Component = Pawn ? Pawn->FindComponentByClass<UInteractionComponent>() : nullptr;
	Interaction = Component;
	if (Component)
	{
		Component->OnPromptsChanged.AddUniqueDynamic(this, &UHUDInteractionPromptWidget::HandlePromptsChanged);
		ApplyPrompts(Component->GetCurrentPrompts());
	}
}

void UHUDInteractionPromptWidget::HandlePawnUnbound(APawn* /*Pawn*/)
{
	if (UInteractionComponent* Component = Interaction.Get())
	{
		Component->OnPromptsChanged.RemoveDynamic(this, &UHUDInteractionPromptWidget::HandlePromptsChanged);
	}
	Interaction.Reset();
	ApplyPrompts(TArray<FInteractionPrompt>());
}

void UHUDInteractionPromptWidget::HandlePromptsChanged(const TArray<FInteractionPrompt>& Prompts)
{
	ApplyPrompts(Prompts);
}

// ----------------------------------------------------------------------
// Rows
// ----------------------------------------------------------------------

void UHUDInteractionPromptWidget::ApplyPrompts(const TArray<FInteractionPrompt>& Prompts)
{
	ShownPrompts = Prompts;

	// Nothing to do: the old rows stay as they are while the prompt fades out (no empty box flashing).
	if (Prompts.Num() > 0 && PromptList)
	{
		const int32 Count = FMath::Min(Prompts.Num(), MaxRows);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UInputPromptWidget* Row = GetOrCreateRow(Index);
			if (!Row)
			{
				break;
			}

			const FInteractionPrompt& Prompt = Prompts[Index];
			Row->SetCollapseWhenNoKey(bHideRowsWithoutKey);
			Row->SetLabel(FormatLabel(Prompt));
			Row->SetInputAction(Prompt.InputAction); // Looks the key up for the device in use; the row follows device changes by itself.
			Row->SetRenderOpacity(Prompt.bAvailable ? 1.0f : UnavailableOpacity);

			// Rows are always children 0..Count-1 in pool order: only the tail is added or removed.
			if (Row->GetParent() != PromptList)
			{
				PromptList->AddChild(Row);
				if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Row->Slot))
				{
					BoxSlot->SetHorizontalAlignment(HAlign_Center);
					BoxSlot->SetPadding(FMargin(0.0f, 2.0f));
				}
			}
		}

		for (int32 Index = Count; Index < Rows.Num(); ++Index)
		{
			if (Rows[Index] && Rows[Index]->GetParent())
			{
				Rows[Index]->RemoveFromParent();
			}
		}
	}

	BP_OnPromptsChanged(Prompts);
}

UInputPromptWidget* UHUDInteractionPromptWidget::GetOrCreateRow(int32 Index)
{
	if (Rows.IsValidIndex(Index) && Rows[Index])
	{
		return Rows[Index];
	}

	if (!EntryClass)
	{
		if (!bWarnedMissingEntryClass && !IsDesignTime())
		{
			bWarnedMissingEntryClass = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("Interaction prompt '%s' has no Entry Class (Class Defaults > Interaction Prompt). Set it to WBP_InputPrompt; no prompt is shown until then."), *GetClass()->GetName());
		}
		return nullptr;
	}

	UInputPromptWidget* Row = CreateWidget<UInputPromptWidget>(this, EntryClass);
	if (Row)
	{
		Rows.SetNum(FMath::Max(Rows.Num(), Index + 1));
		Rows[Index] = Row;
	}
	return Row;
}

FText UHUDInteractionPromptWidget::FormatLabel(const FInteractionPrompt& Prompt) const
{
	FText Label = Prompt.ActionText;

	if (Prompt.Trigger == EInteractionPromptTrigger::Hold && !HoldLabelFormat.IsEmpty())
	{
		Label = FText::FormatNamed(HoldLabelFormat, TEXT("Label"), Label);
	}
	else if (Prompt.Trigger == EInteractionPromptTrigger::Release && !ReleaseLabelFormat.IsEmpty())
	{
		Label = FText::FormatNamed(ReleaseLabelFormat, TEXT("Label"), Label);
	}
	return Label;
}

bool UHUDInteractionPromptWidget::ShouldBeVisible() const
{
	// Settings > Controls > Hints = Off hides the prompt (cached preference, cheap every frame).
	if (!USunsetCovePlayerPreferences::AreHintsEnabled())
	{
		return false;
	}

	if (const USunsetCoveHUDSubsystem* HUDSubsystem = GetHUDSubsystem())
	{
		if (HUDSubsystem->AreInteractionPromptsHiddenByRequest())
		{
			return false;
		}
	}

	if (bHideWhileCursorVisible)
	{
		const APlayerController* PlayerController = GetOwningPlayer();
		if (PlayerController && PlayerController->ShouldShowMouseCursor())
		{
			return false;
		}
	}

	return true;
}

// ----------------------------------------------------------------------
// Layout fallbacks
// ----------------------------------------------------------------------

void UHUDInteractionPromptWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		return;
	}

	UPanelWidget* Root = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (!Root)
	{
		if (WidgetTree->RootWidget)
		{
			return; // A non-panel root: nowhere to put the list.
		}
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PromptRoot"));
		WidgetTree->RootWidget = Root;
	}

	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PromptList"));
	Root->AddChild(List);
	if (UOverlaySlot* ListSlot = Cast<UOverlaySlot>(List->Slot))
	{
		// Bottom center, above the lower edge: clear of the crosshair and of the tool wheel in the middle of the screen.
		ListSlot->SetHorizontalAlignment(HAlign_Center);
		ListSlot->SetVerticalAlignment(VAlign_Bottom);
		ListSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 220.0f));
	}
	PromptList = List;
}

void UHUDInteractionPromptWidget::BuildDesignerPreview()
{
	if (!PromptList || !EntryClass)
	{
		return;
	}

	PromptList->ClearChildren();

	struct FPreviewRow { FText Label; FKey Keyboard; FKey Gamepad; float Opacity; };
	const FPreviewRow Preview[] =
	{
		{ LOCTEXT("PreviewPickUp", "Pick up"), EKeys::E, EKeys::Gamepad_FaceButton_Left, 1.0f },
		{ LOCTEXT("PreviewPlace", "Place (Hold)"), EKeys::LeftMouseButton, EKeys::Gamepad_RightTrigger, UnavailableOpacity },
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Preview); ++Index)
	{
		if (UInputPromptWidget* Row = GetOrCreateRow(Index))
		{
			Row->SetKeys(Preview[Index].Keyboard, Preview[Index].Gamepad);
			Row->SetLabel(Preview[Index].Label);
			Row->SetRenderOpacity(Preview[Index].Opacity);
			PromptList->AddChild(Row);
			if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Row->Slot))
			{
				BoxSlot->SetHorizontalAlignment(HAlign_Center);
				BoxSlot->SetPadding(FMargin(0.0f, 2.0f));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
