// SunsetCoveMenuButton.cpp
// SunsetCove - UI System / Menus. See Public/UISystem/Menus/SunsetCoveMenuButton.h.

#include "UISystem/Menus/SunsetCoveMenuButton.h"

#include "GameFramework/PlayerController.h"

bool USunsetCoveMenuButton::IsHovered() const
{
	return !bHoverSuppressed && Super::IsHovered();
}

void USunsetCoveMenuButton::NativeOnHovered()
{
	// A new mouse gesture on this button: it counts again.
	bHoverSuppressed = false;
	Super::NativeOnHovered();

	TakeFocusFromMouse();
}

void USunsetCoveMenuButton::NativeOnUnhovered()
{
	// Cleared before Super: the visual refresh in UBaseButton must see the final state.
	bHoverSuppressed = false;
	Super::NativeOnUnhovered();
}

void USunsetCoveMenuButton::HandleFocusLost()
{
	// The focus left while the mouse still rests here: the keyboard / gamepad (or another button's hover) took it. The resting mouse
	// must not keep this button highlighted. Set before Super, whose refresh reads IsHovered(). If the mouse left first, nothing to do;
	// if it leaves later, NativeOnUnhovered clears the flag.
	if (Super::IsHovered())
	{
		bHoverSuppressed = true;
	}

	Super::HandleFocusLost();
}

FReply USunsetCoveMenuButton::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The mouse moves over a button whose hover was suppressed by the keyboard: the mouse takes over again.
	// Only a REAL movement counts: after every focus or layout change Slate sends a synthetic mouse move with the cursor where it
	// already was (zero delta), and that must not take the focus back from the keyboard.
	if (bHoverSuppressed && Super::IsHovered() && !InMouseEvent.GetCursorDelta().IsNearlyZero())
	{
		bHoverSuppressed = false;
		TakeFocusFromMouse();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USunsetCoveMenuButton::TakeFocusFromMouse()
{
	if (!bFocusOnHover || !IsInteractionEnabled())
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController && !HasUserFocus(PlayerController))
	{
		SetFocus();
	}
}
