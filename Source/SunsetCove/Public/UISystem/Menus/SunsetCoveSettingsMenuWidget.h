// SunsetCoveSettingsMenuWidget.h
// SunsetCove - UI System / Menus. The Settings screen (parent class of WBP_Settings): tabs, description line, Back.
//
// HIERARCHY
//   UCommonActivatableWidget -> UBaseActivatableWidget -> UBaseMenuWidget (CoreFramework) -> USunsetCoveSettingsMenuWidget
//     -> WBP_Settings (layout only)
//
// WHAT THE FRAMEWORK ALREADY DOES (UBaseMenuWidget)
//   Builds the tabs from DA_Settings_Tabs (Graphics, Controls, Audio) into TabList / TabSwitcher; TabSwitcher is a Common
//   Animated Switcher set to Fade Only, so pages fade out / in. PausedBackground (a Background Blur) is shown only while
//   the game is paused: opened from the Pause Menu the screen keeps the blurred look, opened from the Main Menu the live
//   level camera stays visible.
//
// WHAT THIS CLASS ADDS
//   * Description line: DescriptionText shows the Description of the row the player is on (keyboard / gamepad focus first,
//     else the row under the mouse), like "Adjust the camera sensitivity when using the mouse". Empty -> hidden.
//   * Focus lives in the list: opening the screen and switching page (Q / E, LB / RB) put keyboard / gamepad focus on the
//     first usable row of the page, so Up / Down work at once. The rows' controls sit in the right column, so Slate's
//     directional search from the tab bar would find nothing below it. Clicking or navigating on the tab bar keeps the
//     focus there.
//   * One highlighted row: when the mouse moves onto a row, that row takes the focus (bHoverMovesFocus), so the mouse and
//     the keyboard never highlight two different lines and the description always matches the highlighted one. Off
//     while a controller is in use: CommonUI moves the hidden cursor onto every newly focused control, and the row it
//     just left still counts as hovered for a frame, which would pull the focus back up.
//   * Focus is handed to CommonUI too (NativeGetDesiredFocusTarget, Auto Restore Focus): back from Input Mapping the
//     Input Mapping line is selected again, never nothing.
//   * Scrolling: the pages' Scroll Boxes never scroll by themselves on a focus change (Scroll When Focus Changes = No
//     Scroll) and show a thin scroll bar when a page is longer than the box. Only keyboard / gamepad navigation scrolls,
//     and only as much as needed to bring a row that would be hidden into view; the mouse (hover, wheel, scroll bar) never
//     moves the list on its own.
//   * Back applies: there is no Apply button. Back (Escape / gamepad B / the BackButton) applies every page that has
//     pending changes (graphics, controls, volumes) and closes the screen. bApplyChangesOnClose = false gives the
//     classic behavior (changes not applied are reverted).
//
// THE WIDGET BLUEPRINT (WBP_Settings) may contain, by these exact names (all optional):
//   TabList, TabSwitcher, PausedBackground   (UBaseMenuWidget)
//   DescriptionText  (Common Text)   the description line
//   BackButton       (UBaseButton)   the Back prompt bottom right (its InputPrompt shows the gamepad glyph)

#pragma once

#include "CoreMinimal.h"
#include "UISystem/Menu/BaseMenuWidget.h"
#include "SunsetCoveSettingsMenuWidget.generated.h"

class UBaseButton;
class UBaseSettingRowWidget;
class UBaseTabWidget;
class UCommonTextBlock;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveSettingsMenuWidget : public UBaseMenuWidget
{
	GENERATED_BODY()

public:
	USunsetCoveSettingsMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** Applies (or reverts, per bApplyChangesOnClose) the pages and closes the screen. What Back does. */
	UFUNCTION(BlueprintCallable, Category = "Settings Menu")
	void CloseSettings();

	/** The row whose description is shown (null when none). */
	UFUNCTION(BlueprintPure, Category = "Settings Menu")
	UBaseSettingRowWidget* GetDescribedRow() const { return DescribedRow.Get(); }

	/** First control of the page that can take focus (an enabled row control or a link button), or null. */
	UFUNCTION(BlueprintPure, Category = "Settings Menu")
	UWidget* FindFirstFocusTarget(const UBaseTabWidget* Page) const;

protected:
	//~ Begin UUserWidget / UCommonActivatableWidget / UBaseActivatableWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnHandleBackAction() override;
	virtual UWidget* ResolveDefaultFocusWidget() const override;
	/** CommonUI's own focus request (activation, back from Input Mapping, device switch): the same first row. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UUserWidget / UCommonActivatableWidget / UBaseActivatableWidget

	/** Back applies the pending changes of every page. Off: Back reverts them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Menu")
	bool bApplyChangesOnClose = true;

	/** Moving the mouse onto a row gives that row the keyboard / gamepad focus (one highlighted row at a time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Menu")
	bool bHoverMovesFocus = true;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UBaseButton> BackButton;

private:
	/** Finds the focused (else hovered) row of the active page, follows the mouse, shows the row's description. */
	void RefreshDescription();
	void SetDescription(const FText& Description);

	/** Puts the focus into the new page once its fade has finished (the page is not focusable before). */
	void UpdatePendingContentFocus();

	/** Keyboard / gamepad moved the focus to Row: scroll the page just enough to show it (no-op when already visible). */
	void ScrollRowIntoView(UBaseSettingRowWidget* Row) const;

	UFUNCTION()
	void HandleTabSelectedForFocus(FName TabId);

	void CommitPages();
	void HandleBackClicked();

	TWeakObjectPtr<UBaseSettingRowWidget> DescribedRow;
	TWeakObjectPtr<UBaseSettingRowWidget> LastFocusedRow;
	FText ShownDescription;

	bool bPendingContentFocus = false;
	FVector2D LastCursorPosition = FVector2D::ZeroVector;

	/**
	 * Until this real time, a row that is hovered counts as "reached by the mouse" even with a still cursor. Set when the
	 * screen opens and when a page appears: the page shows up under a resting cursor (the click on "Settings" / a tab),
	 * and without this the hovered row and the focused row would both be highlighted.
	 */
	double HoverSyncDeadline = 0.0;

	void StartHoverSync();
	bool IsUsingGamepad() const;
};
