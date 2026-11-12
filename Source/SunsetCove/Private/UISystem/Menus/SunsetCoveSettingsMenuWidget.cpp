// SunsetCoveSettingsMenuWidget.cpp
// SunsetCove - UI System / Menus. See Public/UISystem/Menus/SunsetCoveSettingsMenuWidget.h.

#include "UISystem/Menus/SunsetCoveSettingsMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonAnimatedSwitcher.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "CommonTabListWidgetBase.h"
#include "CommonTextBlock.h"
#include "Components/ScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "UISystem/Foundation/BaseButton.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "UISystem/Settings/BaseSettingRowWidget.h"

USunsetCoveSettingsMenuWidget::USunsetCoveSettingsMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Escape / gamepad B close the screen (through NativeOnHandleBackAction, which applies first).
	bIsBackHandler = true;
	// Back from Input Mapping: the control that had the focus gets it again.
	bAutoRestoreFocus = true;
}

UWidget* USunsetCoveSettingsMenuWidget::NativeGetDesiredFocusTarget() const
{
	return ResolveDefaultFocusWidget();
}

void USunsetCoveSettingsMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &USunsetCoveSettingsMenuWidget::HandleBackClicked);
	}
}

void USunsetCoveSettingsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct(); // Binds the framework's own tab handler first, so the active page is set when ours runs.

	if (TabList)
	{
		TabList->OnTabSelected.AddUniqueDynamic(this, &USunsetCoveSettingsMenuWidget::HandleTabSelectedForFocus);
	}
}

void USunsetCoveSettingsMenuWidget::NativeDestruct()
{
	if (TabList)
	{
		TabList->OnTabSelected.RemoveDynamic(this, &USunsetCoveSettingsMenuWidget::HandleTabSelectedForFocus);
	}
	bPendingContentFocus = false;

	Super::NativeDestruct();
}

void USunsetCoveSettingsMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// The Blueprint keeps a sample sentence for the designer: start hidden until a row is highlighted.
	DescribedRow.Reset();
	LastFocusedRow.Reset();
	ShownDescription = FText::GetEmpty();
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::GetEmpty());
		DescriptionText->SetVisibility(ESlateVisibility::Hidden);
	}

	if (FSlateApplication::IsInitialized())
	{
		LastCursorPosition = FSlateApplication::Get().GetCursorPos();
	}
	StartHoverSync();
	RefreshDescription();
}

void USunsetCoveSettingsMenuWidget::StartHoverSync()
{
	// Rows receive their mouse-enter a frame or two after they appear: give them a short window.
	const UWorld* World = GetWorld();
	HoverSyncDeadline = World ? World->GetRealTimeSeconds() + 0.35 : 0.0;
}

bool USunsetCoveSettingsMenuWidget::IsUsingGamepad() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UCommonInputSubsystem* CommonInput = LocalPlayer ? LocalPlayer->GetSubsystem<UCommonInputSubsystem>() : nullptr;
	return CommonInput && CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad;
}

void USunsetCoveSettingsMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A handful of rows: checking their highlight every frame is cheaper than wiring an event into every row Blueprint.
	if (IsActivated())
	{
		UpdatePendingContentFocus();
		RefreshDescription();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Focus
// ---------------------------------------------------------------------------------------------------------------------

UWidget* USunsetCoveSettingsMenuWidget::FindFirstFocusTarget(const UBaseTabWidget* Page) const
{
	if (!Page || !Page->WidgetTree)
	{
		return nullptr;
	}

	// Tree order is the visual order of the page (rows in a vertical list). Rows are user widgets, so their own inner
	// buttons are not visited: a row is asked for its focus target, a link button placed in the page is taken as is.
	UWidget* Found = nullptr;
	Page->WidgetTree->ForEachWidget([&Found](UWidget* Widget)
	{
		if (Found || !Widget->IsVisible() || !Widget->GetIsEnabled())
		{
			return;
		}
		if (const UBaseSettingRowWidget* Row = Cast<UBaseSettingRowWidget>(Widget))
		{
			UWidget* Target = Row->GetFocusTarget(); // Header rows have none.
			if (Target && Target->IsVisible() && Target->GetIsEnabled())
			{
				Found = Target;
			}
		}
		else if (UBaseButton* Button = Cast<UBaseButton>(Widget))
		{
			if (Button->IsFocusable())
			{
				Found = Button;
			}
		}
	});
	return Found;
}

UWidget* USunsetCoveSettingsMenuWidget::ResolveDefaultFocusWidget() const
{
	// A DefaultFocusWidget placed in the Blueprint wins, then the first row of the page, then the framework's tab button.
	if (UWidget* Explicit = UBaseActivatableWidget::ResolveDefaultFocusWidget())
	{
		return Explicit;
	}
	if (UWidget* FirstRow = FindFirstFocusTarget(GetTabWidget(GetActiveTabId())))
	{
		return FirstRow;
	}
	return Super::ResolveDefaultFocusWidget();
}

void USunsetCoveSettingsMenuWidget::HandleTabSelectedForFocus(FName TabId)
{
	// The player is on the tab bar (clicked a tab, or navigated up to it): leave the focus there.
	if (TabList && TabList->HasFocusedDescendants())
	{
		return;
	}
	bPendingContentFocus = true;
}

void USunsetCoveSettingsMenuWidget::UpdatePendingContentFocus()
{
	if (!bPendingContentFocus)
	{
		return;
	}

	// The animated switcher swaps the visible page halfway through the fade: wait for the end.
	if (const UCommonAnimatedSwitcher* Animated = Cast<UCommonAnimatedSwitcher>(TabSwitcher))
	{
		if (Animated->IsTransitionPlaying())
		{
			return;
		}
	}

	bPendingContentFocus = false;
	if (UWidget* Target = FindFirstFocusTarget(GetTabWidget(GetActiveTabId())))
	{
		Target->SetFocus();
	}
	StartHoverSync(); // The new page may have appeared under the cursor: that row wins over the first one.
}

// ---------------------------------------------------------------------------------------------------------------------
// Description and mouse
// ---------------------------------------------------------------------------------------------------------------------

void USunsetCoveSettingsMenuWidget::RefreshDescription()
{
	UBaseSettingRowWidget* Focused = nullptr;
	UBaseSettingRowWidget* Hovered = nullptr;

	const UBaseTabWidget* Page = GetTabWidget(GetActiveTabId());
	if (Page && Page->WidgetTree)
	{
		Page->WidgetTree->ForEachWidget([&Focused, &Hovered](UWidget* Widget)
		{
			UBaseSettingRowWidget* Row = Cast<UBaseSettingRowWidget>(Widget);
			if (!Row || !Row->IsRowHighlighted())
			{
				return;
			}
			if (Row->HasFocusedDescendants())
			{
				Focused = Focused ? Focused : Row;
			}
			else
			{
				Hovered = Hovered ? Hovered : Row;
			}
		});
	}

	// Only a real mouse movement moves the focus: a list scrolling under a still cursor must not steal it from the keys.
	bool bMouseMoved = false;
	if (FSlateApplication::IsInitialized())
	{
		const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
		bMouseMoved = !CursorPosition.Equals(LastCursorPosition, 1.0);
		LastCursorPosition = CursorPosition;
	}
	// A page that just appeared under a still cursor counts as a mouse move.
	const UWorld* World = GetWorld();
	if (!bMouseMoved && World && World->GetRealTimeSeconds() < HoverSyncDeadline)
	{
		bMouseMoved = true;
	}

	// Never with a controller: CommonUI moves the hidden cursor onto each newly focused control, and the row it just left
	// still reports a hover for a frame; following it would pull the focus back up and make navigation feel stuck.
	bool bFocusFromMouse = false;
	if (bHoverMovesFocus && bMouseMoved && Hovered && !IsUsingGamepad())
	{
		UWidget* Target = Hovered->GetFocusTarget(); // Disabled rows (Resolution in Borderless) and headers keep it.
		if (Target && Target->GetIsEnabled())
		{
			Target->SetFocus();
			Focused = Hovered;
			bFocusFromMouse = true;
		}
	}

	// The focused row changed through the keys: bring it into view if it would be hidden. A row the mouse reached is
	// already on screen, and scrolling under the cursor would move another row under it.
	if (Focused != LastFocusedRow.Get())
	{
		if (Focused && !bFocusFromMouse)
		{
			ScrollRowIntoView(Focused);
		}
		LastFocusedRow = Focused;
	}

	UBaseSettingRowWidget* Row = Focused ? Focused : Hovered;
	DescribedRow = Row;
	SetDescription(Row ? Row->GetDescription() : FText::GetEmpty());
}

void USunsetCoveSettingsMenuWidget::ScrollRowIntoView(UBaseSettingRowWidget* Row) const
{
	for (UWidget* Parent = Row ? Row->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
	{
		if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
		{
			// IntoView with no padding: the least scrolling that shows the whole row, none when it is already visible.
			Scroll->ScrollWidgetIntoView(Row, /*AnimateScroll=*/true, EDescendantScrollDestination::IntoView, /*Padding=*/0.0f);
			return;
		}
	}
}

void USunsetCoveSettingsMenuWidget::SetDescription(const FText& Description)
{
	if (!DescriptionText || Description.IdenticalTo(ShownDescription))
	{
		return;
	}
	ShownDescription = Description;
	DescriptionText->SetText(Description);
	DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------------------------------------------------------------
// Back
// ---------------------------------------------------------------------------------------------------------------------

void USunsetCoveSettingsMenuWidget::CommitPages()
{
	if (bApplyChangesOnClose)
	{
		ApplyAllTabs();
	}
	else
	{
		RevertAllTabs();
	}
}

bool USunsetCoveSettingsMenuWidget::NativeOnHandleBackAction()
{
	CommitPages();
	return Super::NativeOnHandleBackAction(); // Deactivates the screen.
}

void USunsetCoveSettingsMenuWidget::CloseSettings()
{
	CommitPages();
	CloseScreen();
}

void USunsetCoveSettingsMenuWidget::HandleBackClicked()
{
	CloseSettings();
}
