// SunsetCoveControllerIconsSubsystem.h
// SunsetCove - UI System / Input. Which gamepad icons (Xbox or PlayStation) the prompts show.
//
// WHY
//   The button icons come from the CommonUI controller data (Content/UI/Input/Data: one asset per gamepad family) and the
//   CoreFramework input system (UInputFrameSubsystem, UInputPromptWidget) shows the family CommonUI has selected.
//   On Windows the engine reads gamepads through XInput, which does not say whether the pad is an Xbox or a PlayStation
//   one (Steam Input and DS4 tools also present every pad as an Xbox pad). So:
//     Automatic   CommonUI decides: Xbox icons by default; the hardware mappings of the controller data switch to the
//                 PlayStation set when an input plugin reports a Sony pad (GameInput plugin with a device override).
//     Xbox / PlayStation   forced by the player in Settings > Controls > Controller Icons, kept even if a hardware
//                 detection tries to switch.
//
// WHERE THE CHOICE IS STORED
//   GameUserSettings.ini, [SunsetCove.Input] ControllerIcons=0/1/2. Applied when the local player starts.
//   The family names come from Project Settings > Game > Sunset Cove UI (Xbox / PlayStation Gamepad Name).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "SunsetCoveControllerIconsSubsystem.generated.h"

class UCommonInputSubsystem;

UENUM(BlueprintType)
enum class ESunsetCoveControllerIcons : uint8
{
	Automatic,
	Xbox,
	PlayStation
};

UCLASS()
class SUNSETCOVE_API USunsetCoveControllerIconsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static USunsetCoveControllerIconsSubsystem* Get(const UObject* ContextObject);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	UFUNCTION(BlueprintPure, Category = "SunsetCove|Input")
	ESunsetCoveControllerIcons GetControllerIcons() const { return Choice; }

	/** Applies the choice at once (prompts refresh through the input system's device event). bSave writes the ini. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Input")
	void SetControllerIcons(ESunsetCoveControllerIcons NewChoice, bool bSave);

private:
	UCommonInputSubsystem* GetCommonInput() const;
	void ApplyChoice() const;
	void HandleGamepadChangeDetected(FName DetectedGamepadName);

	ESunsetCoveControllerIcons Choice = ESunsetCoveControllerIcons::Automatic;
	FDelegateHandle GamepadChangeHandle;
};
