// SunsetCovePauseMenuWidget.cpp
// SunsetCove - UI System / Menus. See Public/UISystem/Menus/SunsetCovePauseMenuWidget.h.

#include "UISystem/Menus/SunsetCovePauseMenuWidget.h"

#include "Input/CommonUIInputTypes.h"

USunsetCovePauseMenuWidget::USunsetCovePauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Back from Settings: the Settings button gets the focus again, not nothing.
	bAutoRestoreFocus = true;
}

void USunsetCovePauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!ClosePauseAction.IsNull())
	{
		ClosePauseHandle = RegisterUIActionBinding(FBindUIActionArgs(ClosePauseAction, /*bShouldDisplayInActionBar=*/false,
			FSimpleDelegate::CreateUObject(this, &USunsetCovePauseMenuWidget::HandleClosePauseAction)));
	}
}

UWidget* USunsetCovePauseMenuWidget::NativeGetDesiredFocusTarget() const
{
	// CommonUI's own focus request (activation, return from a covering screen, device switch): the framework's default
	// focus widget (Resume). Without an answer CommonUI focuses the game viewport and the controller selects nothing.
	return ResolveDefaultFocusWidget();
}

void USunsetCovePauseMenuWidget::HandleClosePauseAction()
{
	if (IsActivated())
	{
		CloseScreen();
	}
}
