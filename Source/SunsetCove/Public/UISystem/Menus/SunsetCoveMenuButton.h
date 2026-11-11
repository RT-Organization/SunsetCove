// SunsetCoveMenuButton.h
// SunsetCove - UI System / Menus. The parent of the menu button preset (WBP_Button_Menu): one highlighted button at a time.
//
// THE PROBLEM IT SOLVES
//   UBaseButton (CoreFramework) highlights a button when it is hovered by the mouse OR focused by keyboard / gamepad. Those are two
//   independent states: a focused New Game stayed highlighted while the mouse hovered Options, so two buttons looked selected.
//
// THE RULE (the "last input wins" behavior of console and PC menus)
//   * Mouse enters a button   -> that button takes the focus (the previous one loses its highlight at once). One highlight.
//   * Keyboard / gamepad moves the focus to another button while the mouse still rests on a button -> the resting mouse no
//     longer counts as hover for that button (IsHovered returns false). It counts again when the mouse leaves and enters, or
//     really moves over the button (Slate's synthetic mouse moves, sent after every layout / focus change with the cursor
//     unmoved, are ignored: zero cursor delta), so the next mouse gesture takes over.
//   Result: the highlighted button is always the one the player used last, whatever the device.
//
// WHAT IT DOES NOT CHANGE
//   Everything else is UBaseButton: label, per-state text style, SelectionIndicator, sounds. A disabled button is never focused
//   by hover. Buttons that are not menu buttons (tabs) keep the plain UBaseButton behavior.
//
// USE IT
//   Parent class of WBP_Button_Menu. Class Defaults > Menu Button > Focus On Hover turns the hover focus off for a button.

#pragma once

#include "CoreMinimal.h"
#include "UISystem/Foundation/BaseButton.h"
#include "SunsetCoveMenuButton.generated.h"

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveMenuButton : public UBaseButton
{
	GENERATED_BODY()

public:
	//~ Begin UWidget Interface
	/** False while the hover is suppressed (the keyboard / gamepad moved the focus away while the mouse rested here). */
	virtual bool IsHovered() const override;
	//~ End UWidget Interface

protected:
	//~ Begin UCommonButtonBase / UBaseButton
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void HandleFocusLost() override;
	//~ End UCommonButtonBase / UBaseButton

	//~ Begin UUserWidget Interface
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End UUserWidget Interface

	/** True: the mouse entering the button moves the focus to it. Turn off for a button that must not take focus from the mouse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Menu Button")
	bool bFocusOnHover = true;

private:
	/** Focuses this button if it is enabled and not focused yet. */
	void TakeFocusFromMouse();

	/** The mouse rests on this button but the focus is elsewhere because of the keyboard / gamepad. */
	bool bHoverSuppressed = false;
};
