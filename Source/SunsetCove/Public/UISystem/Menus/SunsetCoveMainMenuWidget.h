// SunsetCoveMainMenuWidget.h
// SunsetCove - UI System / Menus. The main menu: Continue Game, New Game, Options, Quit.
//
// HIERARCHY
//   UCommonActivatableWidget -> UBaseActivatableWidget -> UBaseMenuWidget (CoreFramework) -> USunsetCoveMainMenuWidget (this)
//     -> WBP_MainMenu (layout only: positions, button labels, logo)
//
// WHAT IT DOES
//   * On every activation it asks USunsetCoveGameFlowSubsystem whether the world save exists (CoreFramework save system).
//     Continue Game is shown only when it does; until the answer arrives it is hidden and New Game has the focus.
//   * Continue Game -> GameFlow ContinueWorld (loads the save, opens its map).
//   * New Game      -> no save: GameFlow StartNewWorld at once.
//                      a save exists: the confirmation popup (UBaseConfirmPopupWidget, screen ConfirmPopupScreenTag) with
//                      NewGameConfirmRequest; Confirm -> StartNewWorld (deletes the old world first), Cancel -> nothing.
//   * Options       -> pushes OptionsScreenTag (the settings menu).
//   * Quit          -> quits the game.
//   * While the flow is busy (loading, deleting, saving) every button is disabled, so nothing runs twice.
//   * A failure (file error, save too new) is written in the optional StatusText.
//
// FOCUS (keyboard / gamepad)
//   Default focus = Continue Game when it is visible, otherwise New Game. The buttons sit in one vertical box, so Up / Down
//   navigation is automatic; Enter / gamepad A clicks. The buttons (UBaseButton) highlight on focus like on hover.
//
// THE WIDGET BLUEPRINT (WBP_MainMenu) must contain, by these exact names:
//   ContinueButton, NewGameButton, OptionsButton, QuitButton   (any child of UBaseButton, e.g. WBP_Button_Menu)
//   StatusText                                                 (optional Common Text)
//   Everything else (logo, layout, labels) is free design. Screen tags and popup texts are Class Defaults.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Menu/BaseMenuWidget.h"
#include "UISystem/Popups/BaseConfirmPopupWidget.h"
#include "SunsetCoveMainMenuWidget.generated.h"

class UBaseButton;
class UCommonTextBlock;
class USunsetCoveGameFlowSubsystem;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveMainMenuWidget : public UBaseMenuWidget
{
	GENERATED_BODY()

public:
	USunsetCoveMainMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UUserWidget / UCommonActivatableWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* ResolveDefaultFocusWidget() const override;
	//~ End UUserWidget / UCommonActivatableWidget

	// ------------------------------------------------------------------
	// Configuration (Class Defaults of WBP_MainMenu)
	// ------------------------------------------------------------------

	/** Screen pushed by Options (registered in DA_UIConfig > Screens). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	FGameplayTag OptionsScreenTag;

	/** Confirmation popup screen (a UBaseConfirmPopupWidget registered in DA_UIConfig > Screens on the Modal layer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	FGameplayTag ConfirmPopupScreenTag;

	/** What the popup asks before New Game deletes the existing world. Focus stays on Cancel (destructive action). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	FUIConfirmRequest NewGameConfirmRequest;

	/** True: without a save Continue Game is removed from the list. False: it stays visible but disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	bool bHideContinueWithoutSave = true;

	// ------------------------------------------------------------------
	// Widgets (WBP_MainMenu, exact names)
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBaseButton> ContinueButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBaseButton> NewGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBaseButton> OptionsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBaseButton> QuitButton;

	/** Optional line for flow errors ("the save was made by a newer version"...). Collapsed when empty. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> StatusText;

private:
	void HandleContinueClicked();
	void HandleNewGameClicked();
	void HandleOptionsClicked();
	void HandleQuitClicked();

	UFUNCTION()
	void HandleWorldSaveStateChanged(bool bHasWorldSave);

	UFUNCTION()
	void HandleFlowBusyChanged(bool bBusy);

	UFUNCTION()
	void HandleFlowFailed(const FText& Reason);

	/** Visibility and enabled state of the buttons from the flow state. bRefocus moves focus when the focused button vanished. */
	void RefreshButtons(bool bRefocus);
	void SetStatus(const FText& Message);

	USunsetCoveGameFlowSubsystem* GetGameFlow() const;

	TWeakObjectPtr<USunsetCoveGameFlowSubsystem> BoundGameFlow;

	/** A confirmation popup is open for New Game: buttons stay disabled until it answers. */
	bool bWaitingForConfirmation = false;
};
