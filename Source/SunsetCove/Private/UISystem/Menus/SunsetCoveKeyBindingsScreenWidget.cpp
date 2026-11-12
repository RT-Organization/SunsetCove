// SunsetCoveKeyBindingsScreenWidget.cpp
// SunsetCove - UI System / Menus. See Public/UISystem/Menus/SunsetCoveKeyBindingsScreenWidget.h.

#include "UISystem/Menus/SunsetCoveKeyBindingsScreenWidget.h"

#include "Kismet/GameplayStatics.h"
#include "UISystem/Foundation/BaseButton.h"

void USunsetCoveKeyBindingsScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (BackButton)
	{
		BackButton->OnClicked().AddUObject(this, &USunsetCoveKeyBindingsScreenWidget::HandleBackClicked);
	}
}

void USunsetCoveKeyBindingsScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (PausedBackground)
	{
		PausedBackground->SetVisibility(UGameplayStatics::IsGamePaused(this) ? PausedBackgroundVisibility : ESlateVisibility::Collapsed);
	}
}

UWidget* USunsetCoveKeyBindingsScreenWidget::NativeGetDesiredFocusTarget() const
{
	return ResolveDefaultFocusWidget();
}

void USunsetCoveKeyBindingsScreenWidget::HandleBackClicked()
{
	// While a key is being captured the capture owns every input (mouse buttons included), so this only runs when idle.
	CloseScreen();
}
