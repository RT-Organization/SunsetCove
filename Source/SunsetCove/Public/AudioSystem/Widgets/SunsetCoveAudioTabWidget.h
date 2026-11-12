// SunsetCoveAudioTabWidget.h
// SunsetCove - Audio System / Widgets. The Audio tab of the settings menu (parent class of WBP_Tab_Audio).
//
// HIERARCHY
//   UBaseUserWidget -> UBaseTabWidget (CoreFramework) -> USunsetCoveAudioTabWidget (this) -> WBP_Tab_Audio (layout only)
//   Hosted by WBP_Settings (UBaseMenuWidget), registered in DA_Settings_Tabs.
//
// WHAT IT DOES
//   * It has NO named widgets. It manages every USunsetCoveVolumeRowWidget (WBP_VolumeRow) placed anywhere in its layout.
//     Adding a category to the menu = placing one more row and picking its Category tag.
//   * Volumes are live: a row writes to the audio subsystem while the slider moves, so the player hears the change.
//   * It implements the framework tab contract, so the Apply / Revert buttons of the settings menu work like on any tab:
//       HasPendingChanges   a row differs from the values the tab saw when it opened (or at the last Apply / Revert)
//       ApplyChanges        writes GameUserSettings.ini now (UAudioFrameSubsystem::SaveAudioSettings)
//       RevertChanges       puts the old values back through the audio subsystem; the rows refresh from its event
//   * Without an Apply button nothing is lost: the framework also saves the volumes when the game closes.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Menu/Tabs/BaseTabWidget.h"
#include "SunsetCoveAudioTabWidget.generated.h"

class USunsetCoveVolumeRowWidget;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveAudioTabWidget : public UBaseTabWidget
{
	GENERATED_BODY()

public:
	USunsetCoveAudioTabWidget(const FObjectInitializer& ObjectInitializer);

	//~ Begin UBaseTabWidget
	virtual void NotifyTabSelected() override;
	virtual bool HasPendingChanges_Implementation() const override;
	virtual void ApplyChanges_Implementation() override;
	virtual void RevertChanges_Implementation() override;
	//~ End UBaseTabWidget

	/** Every volume row in this tab's layout. */
	UFUNCTION(BlueprintPure, Category = "Audio Tab")
	TArray<USunsetCoveVolumeRowWidget*> GetVolumeRows() const;

protected:
	//~ Begin UUserWidget
	virtual void NativeConstruct() override;
	//~ End UUserWidget

private:
	struct FCategoryState
	{
		float Volume = 1.0f;
		bool bMuted = false;
	};

	/** Records the current value of every row's category: the reference for HasPendingChanges and RevertChanges. */
	void TakeSnapshot();

	TMap<FGameplayTag, FCategoryState> Snapshot;
};
