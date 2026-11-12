// SunsetCoveMainMenuWidget.cpp
// SunsetCove - UI System / Menus. See Public/UISystem/Menus/SunsetCoveMainMenuWidget.h.

#include "UISystem/Menus/SunsetCoveMainMenuWidget.h"

#include "CommonTextBlock.h"
#include "Core/GameFlow/SunsetCoveGameFlowSubsystem.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UISystem/Foundation/BaseButton.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

#define LOCTEXT_NAMESPACE "SunsetCoveMainMenu"

USunsetCoveMainMenuWidget::USunsetCoveMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Texts of the GDD; editable per project in the Class Defaults of WBP_MainMenu.
	NewGameConfirmRequest.Title = LOCTEXT("NewGameTitle", "Start a new world?");
	NewGameConfirmRequest.Message = LOCTEXT("NewGameMessage", "Are you sure you want to delete your current world and start a new one?");
	NewGameConfirmRequest.ConfirmLabel = LOCTEXT("NewGameConfirm", "Confirm");
	NewGameConfirmRequest.CancelLabel = LOCTEXT("NewGameCancel", "Cancel");
	NewGameConfirmRequest.bFocusConfirmButton = false;

	// Back from Settings / the popup: the button that opened it gets the focus again.
	bAutoRestoreFocus = true;
}

UWidget* USunsetCoveMainMenuWidget::NativeGetDesiredFocusTarget() const
{
	return ResolveDefaultFocusWidget();
}

USunsetCoveGameFlowSubsystem* USunsetCoveMainMenuWidget::GetGameFlow() const
{
	return USunsetCoveGameFlowSubsystem::Get(this);
}

// ----------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------

void USunsetCoveMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// BindWidget: the Blueprint does not compile without these four, so they are never null here.
	ContinueButton->OnClicked().AddUObject(this, &USunsetCoveMainMenuWidget::HandleContinueClicked);
	NewGameButton->OnClicked().AddUObject(this, &USunsetCoveMainMenuWidget::HandleNewGameClicked);
	OptionsButton->OnClicked().AddUObject(this, &USunsetCoveMainMenuWidget::HandleOptionsClicked);
	QuitButton->OnClicked().AddUObject(this, &USunsetCoveMainMenuWidget::HandleQuitClicked);

	SetStatus(FText::GetEmpty());
}

void USunsetCoveMainMenuWidget::NativeOnActivated()
{
	if (USunsetCoveGameFlowSubsystem* GameFlow = GetGameFlow())
	{
		GameFlow->OnWorldSaveStateChanged.AddUniqueDynamic(this, &USunsetCoveMainMenuWidget::HandleWorldSaveStateChanged);
		GameFlow->OnBusyChanged.AddUniqueDynamic(this, &USunsetCoveMainMenuWidget::HandleFlowBusyChanged);
		GameFlow->OnFlowFailed.AddUniqueDynamic(this, &USunsetCoveMainMenuWidget::HandleFlowFailed);
		BoundGameFlow = GameFlow;
	}

	// Buttons first (Continue hidden until the save state is known), then Super schedules the default focus.
	RefreshButtons(false);
	Super::NativeOnActivated();

	// The answer arrives in a few frames; Continue then appears and takes the focus.
	if (USunsetCoveGameFlowSubsystem* GameFlow = BoundGameFlow.Get())
	{
		GameFlow->RefreshWorldSaveState();
	}
}

void USunsetCoveMainMenuWidget::NativeOnDeactivated()
{
	if (USunsetCoveGameFlowSubsystem* GameFlow = BoundGameFlow.Get())
	{
		GameFlow->OnWorldSaveStateChanged.RemoveDynamic(this, &USunsetCoveMainMenuWidget::HandleWorldSaveStateChanged);
		GameFlow->OnBusyChanged.RemoveDynamic(this, &USunsetCoveMainMenuWidget::HandleFlowBusyChanged);
		GameFlow->OnFlowFailed.RemoveDynamic(this, &USunsetCoveMainMenuWidget::HandleFlowFailed);
	}
	BoundGameFlow.Reset();

	Super::NativeOnDeactivated();
}

UWidget* USunsetCoveMainMenuWidget::ResolveDefaultFocusWidget() const
{
	if (ContinueButton && ContinueButton->IsVisible() && ContinueButton->GetIsEnabled())
	{
		return ContinueButton.Get();
	}
	return NewGameButton ? NewGameButton.Get() : Super::ResolveDefaultFocusWidget();
}

// ----------------------------------------------------------------------
// Buttons
// ----------------------------------------------------------------------

void USunsetCoveMainMenuWidget::HandleContinueClicked()
{
	SetStatus(FText::GetEmpty());
	if (USunsetCoveGameFlowSubsystem* GameFlow = GetGameFlow())
	{
		GameFlow->ContinueWorld();
	}
}

void USunsetCoveMainMenuWidget::HandleNewGameClicked()
{
	SetStatus(FText::GetEmpty());
	USunsetCoveGameFlowSubsystem* GameFlow = GetGameFlow();
	if (!GameFlow || GameFlow->IsBusy())
	{
		return;
	}

	// No world yet (first play on a fresh install): nothing to lose, start at once.
	if (!GameFlow->HasWorldSave())
	{
		GameFlow->StartNewWorld();
		return;
	}

	// A world exists: ask before deleting it.
	bWaitingForConfirmation = true;
	RefreshButtons(false);

	UBaseConfirmPopupWidget::OpenConfirmation(GetUIFrameSubsystem(), ConfirmPopupScreenTag, NewGameConfirmRequest,
		[WeakThis = TWeakObjectPtr<USunsetCoveMainMenuWidget>(this)](EUIConfirmResult Result)
	{
		USunsetCoveMainMenuWidget* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}

		Self->bWaitingForConfirmation = false;
		Self->RefreshButtons(false);

		if (Result == EUIConfirmResult::Confirmed)
		{
			if (USunsetCoveGameFlowSubsystem* Flow = Self->GetGameFlow())
			{
				Flow->StartNewWorld();
			}
		}
		else if (Self->IsActivated())
		{
			// Back where the player was.
			Self->NewGameButton->SetFocus();
		}
	});
}

void USunsetCoveMainMenuWidget::HandleOptionsClicked()
{
	if (!OptionsScreenTag.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Main menu: Options Screen Tag is not set (WBP_MainMenu > Class Defaults)."));
		return;
	}
	if (UUIFrameSubsystem* UIFrame = GetUIFrameSubsystem())
	{
		UIFrame->PushScreen(OptionsScreenTag);
	}
}

void USunsetCoveMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

// ----------------------------------------------------------------------
// Flow events
// ----------------------------------------------------------------------

void USunsetCoveMainMenuWidget::HandleWorldSaveStateChanged(bool /*bHasWorldSave*/)
{
	RefreshButtons(true);
}

void USunsetCoveMainMenuWidget::HandleFlowBusyChanged(bool /*bBusy*/)
{
	RefreshButtons(true);
}

void USunsetCoveMainMenuWidget::HandleFlowFailed(const FText& Reason)
{
	SetStatus(Reason);
	RefreshButtons(true);
}

// ----------------------------------------------------------------------
// State
// ----------------------------------------------------------------------

void USunsetCoveMainMenuWidget::RefreshButtons(bool bRefocus)
{
	const USunsetCoveGameFlowSubsystem* GameFlow = BoundGameFlow.Get();
	const bool bHasSave = GameFlow && GameFlow->IsWorldSaveStateKnown() && GameFlow->HasWorldSave();
	const bool bInteractive = !bWaitingForConfirmation && !(GameFlow && GameFlow->IsBusy());

	const bool bContinueWasVisible = ContinueButton->IsVisible();
	ContinueButton->SetVisibility((bHasSave || !bHideContinueWithoutSave) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ContinueButton->SetIsEnabled(bInteractive && bHasSave);
	NewGameButton->SetIsEnabled(bInteractive);
	OptionsButton->SetIsEnabled(bInteractive);
	QuitButton->SetIsEnabled(bInteractive);

	// Continue just appeared (the save answer arrived) or the focused button became unusable: move focus to the default.
	if (bRefocus && IsActivated() && bInteractive)
	{
		const bool bContinueAppeared = !bContinueWasVisible && ContinueButton->IsVisible();
		if (bContinueAppeared || !HasFocusedDescendants())
		{
			RequestDefaultFocus();
		}
	}
}

void USunsetCoveMainMenuWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

#undef LOCTEXT_NAMESPACE
