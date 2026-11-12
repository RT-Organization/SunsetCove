// SunsetCoveVolumeRowWidget.h
// SunsetCove - Audio System / Widgets. One volume line of the settings menu: "Music   ----o----   70%".
//
// HIERARCHY
//   UBaseUserWidget -> UBaseSettingRowWidget -> USliderSettingRowWidget (CoreFramework) -> USunsetCoveVolumeRowWidget (this)
//     -> WBP_VolumeRow (layout only, same look as every other slider row of the settings menu)
//
// WHAT IT DOES
//   It shows and edits the volume of ONE audio category (CoreFramework.Audio.Category.Master / Music / SFX / Ambient / UI,
//   or any category the project adds). It holds no audio state of its own: every value is read from and written to the
//   CoreFramework audio subsystem (UAudioFrameSubsystem), which applies it to the category's Sound Class and stores it in
//   GameUserSettings.ini.
//
//     slider moved          -> SetCategoryVolume(Category, value)      heard at once (live preview)
//     slider released       -> PlayUISound(PreviewCue)                 optional sample at the new level
//     volume changed by anyone (another row, Revert, game code) -> OnCategoryVolumeChanged -> the row refreshes
//
//   Everything else (label, value text, Left / Right steps, focus highlight, description) is the framework slider row.
//
// USE IT
//   Place WBP_VolumeRow instances in WBP_Tab_Audio (parent USunsetCoveAudioTabWidget). Per instance, in the Details panel:
//   Volume Row > Category, Preview Cue; Setting Row > Label, Description.
//
// THE WIDGET BLUEPRINT (WBP_VolumeRow) must contain, by these exact names:
//   ValueSlider   (Slider, required)       LabelText, ValueText, FocusHighlight (optional) - see USliderSettingRowWidget.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UISystem/Settings/SliderSettingRowWidget.h"
#include "SunsetCoveVolumeRowWidget.generated.h"

class UAudioFrameSubsystem;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveVolumeRowWidget : public USliderSettingRowWidget
{
	GENERATED_BODY()

public:
	USunsetCoveVolumeRowWidget(const FObjectInitializer& ObjectInitializer);

	/** The audio category this row edits. */
	UFUNCTION(BlueprintPure, Category = "Volume Row")
	FGameplayTag GetCategory() const { return Category; }

	/** Current volume 0..1 of the category, as the audio subsystem knows it (1 without a game instance). */
	UFUNCTION(BlueprintPure, Category = "Volume Row")
	float GetVolume() const;

	/** Re-reads the volume from the audio subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Volume Row")
	void RefreshFromAudio();

protected:
	//~ Begin UUserWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget

	/** Category edited by this row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume Row", meta = (Categories = "CoreFramework.Audio.Category"))
	FGameplayTag Category;

	/**
	 * Optional cue played when the player releases the slider, so the new level can be heard (a click for UI, a short
	 * effect for SFX). It plays in the cue's own category. Leave empty for Music and Ambient (they are already playing).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Volume Row", meta = (Categories = "Audio.Cue"))
	FGameplayTag PreviewCue;

private:
	UFUNCTION()
	void HandleRowValueChanged(USliderSettingRowWidget* Row, float NewValue);

	UFUNCTION()
	void HandleSliderReleased();

	UFUNCTION()
	void HandleCategoryVolumeChanged(FGameplayTag ChangedCategory, float Volume);

	UAudioFrameSubsystem* GetAudio() const;

	TWeakObjectPtr<UAudioFrameSubsystem> BoundAudio;

	/** True while the row writes the volume itself, so the subsystem's echo does not write back. */
	bool bWritingVolume = false;
};
