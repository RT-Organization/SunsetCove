// SunsetCovePauseMenuWidget.h
// SunsetCove - UI System / Menus. The Pause Menu (parent class of WBP_PauseMenu).
//
// HIERARCHY
//   UCommonActivatableWidget -> UBaseActivatableWidget -> UBaseMenuWidget (CoreFramework) -> USunsetCovePauseMenuWidget
//     -> WBP_PauseMenu (layout + the three button clicks: Resume / Settings / Quit to Menu)
//
// WHAT THE FRAMEWORK ALREADY DOES
//   Pauses the game while the screen is open (Pause Game While Active), UI Only input, Back (Escape / gamepad B) closes
//   it, the PausedBackground blur. ASunsetCovePlayerController opens it with IA_Pause (IMC_Player: Escape, Start, Insert).
//
// WHAT THIS CLASS ADDS
//   * The pause key closes the menu again. While the menu is open the game input is blocked (UI Only), so IA_Pause cannot
//     fire: Close Pause Action is a CommonUI action (DT_SunsetCove_UIActions > ClosePause: gamepad Start, keyboard Insert)
//     handled by the menu itself. Escape / B stay the Back action.
//   * Focus: CommonUI asks the screen for its focus target every time it becomes the top screen (opened, back from
//     Settings, input device switch). This class answers (the Resume button, or the button that had the focus before
//     Settings, through Auto Restore Focus), so the controller never ends up with nothing selected.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Input/UIActionBindingHandle.h"
#include "UISystem/Menu/BaseMenuWidget.h"
#include "SunsetCovePauseMenuWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCovePauseMenuWidget : public UBaseMenuWidget
{
	GENERATED_BODY()

public:
	USunsetCovePauseMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget / UCommonActivatableWidget
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UUserWidget / UCommonActivatableWidget

	/** UI action that closes the menu from inside it (the pause key again). Empty: only Back and Resume close it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pause Menu", meta = (RowType = "/Script/CommonUI.CommonInputActionDataBase"))
	FDataTableRowHandle ClosePauseAction;

private:
	void HandleClosePauseAction();

	FUIActionBindingHandle ClosePauseHandle;
};
