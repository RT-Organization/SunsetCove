// SunsetCoveControlsTabWidget.h
// SunsetCove - UI System / Menus. The Controls page of the settings menu (parent class of WBP_Tab_Controls).
//
// HIERARCHY
//   UBaseTabWidget -> UControlsSettingsTabWidget (CoreFramework) -> USunsetCoveControlsTabWidget (this) -> WBP_Tab_Controls
//
// WHAT THE FRAMEWORK ALREADY DOES
//   Mouse / Controller Sensitivity, Invert X / Y rows bound to UInputFrameSubsystem (applied at once, saved by the menu's
//   Back), and the Input Mapping button (KeyBindingsButton) that pushes UI.Screen.KeyBindings. The framework also knows Aim
//   Assist and Vibration rows: SunsetCove has neither feature, so WBP_Tab_Controls does not place them (both are optional).
//
// WHAT THIS CLASS ADDS
//   ControllerIconsRow (option row): Automatic / Xbox / PlayStation, through USunsetCoveControllerIconsSubsystem.
//   SprintModeRow (option row): Hold / Toggle, through USunsetCovePlayerPreferences (default Hold).
//   Changes preview at once, are saved on Apply (Back) and restored on Revert like the rest.

#pragma once

#include "CoreMinimal.h"
#include "UISystem/Settings/ControlsSettingsTabWidget.h"
#include "UISystem/Input/SunsetCoveControllerIconsSubsystem.h"
#include "Core/Settings/SunsetCovePlayerPreferences.h"
#include "SunsetCoveControlsTabWidget.generated.h"

class UOptionSettingRowWidget;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveControlsTabWidget : public UControlsSettingsTabWidget
{
	GENERATED_BODY()

public:
	USunsetCoveControlsTabWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UBaseTabWidget
	virtual void NotifyTabSelected() override;
	virtual bool HasPendingChanges_Implementation() const override;
	virtual void ApplyChanges_Implementation() override;
	virtual void RevertChanges_Implementation() override;
	//~ End UBaseTabWidget

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	/** Names of Automatic / Xbox / PlayStation, in that order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls Settings")
	TArray<FText> ControllerIconsNames;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> ControllerIconsRow;

	/** Names of Hold / Toggle, in that order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls Settings")
	TArray<FText> SprintModeNames;

	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UOptionSettingRowWidget> SprintModeRow;

private:
	UFUNCTION()
	void HandleControllerIconsChanged(UOptionSettingRowWidget* Row, int32 SelectedIndex);

	UFUNCTION()
	void HandleSprintModeChanged(UOptionSettingRowWidget* Row, int32 SelectedIndex);

	void RefreshControllerIconsRow();
	void RefreshSprintModeRow();

	ESunsetCoveControllerIcons IconsSnapshot = ESunsetCoveControllerIcons::Automatic;
	ESunsetCoveSprintMode SprintSnapshot = ESunsetCoveSprintMode::Hold;
};
