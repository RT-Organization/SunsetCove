// SunsetCoveKeyBindingsScreenWidget.h
// SunsetCove - UI System / Menus. The Input Mapping screen (parent class of WBP_KeyBindings), opened from Settings > Controls.
//
// HIERARCHY
//   UCommonActivatableWidget -> UBaseActivatableWidget -> UKeyBindingsScreenWidget (CoreFramework)
//     -> USunsetCoveKeyBindingsScreenWidget -> WBP_KeyBindings (layout only)
//
// WHAT THE FRAMEWORK ALREADY DOES (UKeyBindingsScreenWidget)
//   One row per player-mappable action of the Remappable Contexts (Project Settings > CoreFramework Input), grouped by
//   Display Category with a header row; clicking a binding listens for the next key ("Press a key"), conflicts are
//   swapped, Reset to Defaults, feedback in StatusText. Escape / gamepad B close the screen (it is a back handler);
//   while listening the key capture eats every input, so Escape / gamepad View cancel the capture instead.
//
// WHAT THIS CLASS ADDS
//   * The visible Back prompt (bottom right, same widget as the Settings screen): clicking it closes the screen.
//   * PausedBackground, same rule as the Settings screen (UBaseMenuWidget): shown only while the game is paused, so from
//     the Main Menu the live level camera stays visible and from the Pause Menu the game stays blurred.
//
// THE WIDGET BLUEPRINT (WBP_KeyBindings) may contain, by these exact names:
//   RowsContainer (required), ResetAllButton, StatusText   (UKeyBindingsScreenWidget)
//   BackButton       (UBaseButton, optional)                the Back prompt
//   PausedBackground (any widget, optional)                 Background Blur shown only while paused
//
// USAGE
//   DA_UIConfig > Screens: UI.Screen.KeyBindings -> WBP_KeyBindings, Menu layer. The Controls page opens it through its
//   Key Bindings Screen Tag.

#pragma once

#include "CoreMinimal.h"
#include "UISystem/Settings/KeyBindingsScreenWidget.h"
#include "SunsetCoveKeyBindingsScreenWidget.generated.h"

class UBaseButton;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveKeyBindingsScreenWidget : public UKeyBindingsScreenWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget / UCommonActivatableWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	/** CommonUI's own focus request: the first binding box (the framework only sets it a tick later, which CommonUI may undo). */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UUserWidget / UCommonActivatableWidget

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> BackButton;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> PausedBackground;

	/** Visibility of PausedBackground while the game is paused (collapsed otherwise). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Bindings")
	ESlateVisibility PausedBackgroundVisibility = ESlateVisibility::HitTestInvisible;

private:
	void HandleBackClicked();
};
