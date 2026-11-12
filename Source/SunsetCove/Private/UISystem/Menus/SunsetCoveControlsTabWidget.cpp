// SunsetCoveControlsTabWidget.cpp
// SunsetCove - UI System / Menus. See Public/UISystem/Menus/SunsetCoveControlsTabWidget.h.

#include "UISystem/Menus/SunsetCoveControlsTabWidget.h"

#include "UISystem/Settings/OptionSettingRowWidget.h"

#define LOCTEXT_NAMESPACE "SunsetCoveControls"

USunsetCoveControlsTabWidget::USunsetCoveControlsTabWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ControllerIconsNames = {
		LOCTEXT("IconsAutomatic", "Automatic"),
		LOCTEXT("IconsXbox", "Xbox"),
		LOCTEXT("IconsPlayStation", "PlayStation") };

	SprintModeNames = {
		LOCTEXT("SprintHold", "Hold"),
		LOCTEXT("SprintToggle", "Toggle") };
}

void USunsetCoveControlsTabWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ControllerIconsRow)
	{
		ControllerIconsRow->OnSelectionChanged.AddUniqueDynamic(this, &USunsetCoveControlsTabWidget::HandleControllerIconsChanged);
	}
	if (SprintModeRow)
	{
		SprintModeRow->OnSelectionChanged.AddUniqueDynamic(this, &USunsetCoveControlsTabWidget::HandleSprintModeChanged);
	}
}

void USunsetCoveControlsTabWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (const USunsetCoveControllerIconsSubsystem* Icons = USunsetCoveControllerIconsSubsystem::Get(this))
	{
		IconsSnapshot = Icons->GetControllerIcons();
	}
	SprintSnapshot = USunsetCovePlayerPreferences::GetSprintMode();
	RefreshControllerIconsRow();
	RefreshSprintModeRow();
}

void USunsetCoveControlsTabWidget::NotifyTabSelected()
{
	Super::NotifyTabSelected();
	RefreshControllerIconsRow();
	RefreshSprintModeRow();
}

void USunsetCoveControlsTabWidget::RefreshControllerIconsRow()
{
	if (!ControllerIconsRow)
	{
		return;
	}
	const USunsetCoveControllerIconsSubsystem* Icons = USunsetCoveControllerIconsSubsystem::Get(this);
	const int32 Current = Icons ? static_cast<int32>(Icons->GetControllerIcons()) : 0;
	ControllerIconsRow->SetOptions(ControllerIconsNames, Current);
}

void USunsetCoveControlsTabWidget::RefreshSprintModeRow()
{
	if (SprintModeRow)
	{
		SprintModeRow->SetOptions(SprintModeNames, static_cast<int32>(USunsetCovePlayerPreferences::GetSprintMode()));
	}
}

void USunsetCoveControlsTabWidget::HandleControllerIconsChanged(UOptionSettingRowWidget* Row, int32 SelectedIndex)
{
	if (USunsetCoveControllerIconsSubsystem* Icons = USunsetCoveControllerIconsSubsystem::Get(this))
	{
		Icons->SetControllerIcons(static_cast<ESunsetCoveControllerIcons>(FMath::Clamp(SelectedIndex, 0, 2)), /*bSave=*/false);
	}
}

void USunsetCoveControlsTabWidget::HandleSprintModeChanged(UOptionSettingRowWidget* Row, int32 SelectedIndex)
{
	USunsetCovePlayerPreferences::SetSprintMode(SelectedIndex == 1 ? ESunsetCoveSprintMode::Toggle : ESunsetCoveSprintMode::Hold, /*bSave=*/false);
}

bool USunsetCoveControlsTabWidget::HasPendingChanges_Implementation() const
{
	const USunsetCoveControllerIconsSubsystem* Icons = USunsetCoveControllerIconsSubsystem::Get(this);
	const bool bIconsChanged = Icons && Icons->GetControllerIcons() != IconsSnapshot;
	const bool bSprintChanged = USunsetCovePlayerPreferences::GetSprintMode() != SprintSnapshot;
	return bIconsChanged || bSprintChanged || Super::HasPendingChanges_Implementation();
}

void USunsetCoveControlsTabWidget::ApplyChanges_Implementation()
{
	Super::ApplyChanges_Implementation();

	if (USunsetCoveControllerIconsSubsystem* Icons = USunsetCoveControllerIconsSubsystem::Get(this))
	{
		Icons->SetControllerIcons(Icons->GetControllerIcons(), /*bSave=*/true);
		IconsSnapshot = Icons->GetControllerIcons();
	}

	SprintSnapshot = USunsetCovePlayerPreferences::GetSprintMode();
	USunsetCovePlayerPreferences::SetSprintMode(SprintSnapshot, /*bSave=*/true);
}

void USunsetCoveControlsTabWidget::RevertChanges_Implementation()
{
	Super::RevertChanges_Implementation();

	if (USunsetCoveControllerIconsSubsystem* Icons = USunsetCoveControllerIconsSubsystem::Get(this))
	{
		Icons->SetControllerIcons(IconsSnapshot, /*bSave=*/false);
	}
	USunsetCovePlayerPreferences::SetSprintMode(SprintSnapshot, /*bSave=*/false);
	RefreshControllerIconsRow();
	RefreshSprintModeRow();
}

#undef LOCTEXT_NAMESPACE
