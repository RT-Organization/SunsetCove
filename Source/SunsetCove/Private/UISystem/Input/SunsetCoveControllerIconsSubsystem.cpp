// SunsetCoveControllerIconsSubsystem.cpp
// SunsetCove - UI System / Input. See Public/UISystem/Input/SunsetCoveControllerIconsSubsystem.h.

#include "UISystem/Input/SunsetCoveControllerIconsSubsystem.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "InputSystem/Manager/InputFrameSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "UISystem/HUD/SunsetCoveUISettings.h"

namespace
{
	const TCHAR* const GIconsSection = TEXT("SunsetCove.Input");
	const TCHAR* const GIconsKey = TEXT("ControllerIcons");
}

USunsetCoveControllerIconsSubsystem* USunsetCoveControllerIconsSubsystem::Get(const UObject* ContextObject)
{
	// Same lookup rules as the framework input system (widget owner, controller, pawn, else first local player).
	const UInputFrameSubsystem* Input = UInputFrameSubsystem::Get(ContextObject);
	const ULocalPlayer* LocalPlayer = Input ? Input->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<USunsetCoveControllerIconsSubsystem>() : nullptr;
}

void USunsetCoveControllerIconsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UCommonInputSubsystem>();

	int32 Saved = 0;
	if (GConfig)
	{
		GConfig->GetInt(GIconsSection, GIconsKey, Saved, GGameUserSettingsIni);
	}
	Choice = static_cast<ESunsetCoveControllerIcons>(FMath::Clamp(Saved, 0, 2));

	if (UCommonInputSubsystem* CommonInput = GetCommonInput())
	{
		// A hardware detection may switch the family: a forced choice wins again right after.
		GamepadChangeHandle = CommonInput->GetOnGamepadChangeDetected().AddUObject(this, &USunsetCoveControllerIconsSubsystem::HandleGamepadChangeDetected);
	}
	ApplyChoice();
}

void USunsetCoveControllerIconsSubsystem::Deinitialize()
{
	// No explicit Remove: the event lives in the Common Input preprocessor, which Common Input may already have destroyed
	// at this point (local player subsystems are not deinitialized in dependency order, and calling
	// GetOnGamepadChangeDetected then asserts). The binding is AddUObject, so it is skipped once this object is gone.
	GamepadChangeHandle.Reset();

	Super::Deinitialize();
}

UCommonInputSubsystem* USunsetCoveControllerIconsSubsystem::GetCommonInput() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UCommonInputSubsystem>() : nullptr;
}

void USunsetCoveControllerIconsSubsystem::SetControllerIcons(ESunsetCoveControllerIcons NewChoice, bool bSave)
{
	Choice = NewChoice;
	if (GConfig)
	{
		GConfig->SetInt(GIconsSection, GIconsKey, static_cast<int32>(Choice), GGameUserSettingsIni);
		if (bSave)
		{
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
	ApplyChoice();
}

void USunsetCoveControllerIconsSubsystem::ApplyChoice() const
{
	UCommonInputSubsystem* CommonInput = GetCommonInput();
	const UCommonInputPlatformSettings* Platform = UCommonInputPlatformSettings::Get();
	if (!CommonInput || !Platform || !Platform->CanChangeGamepadType())
	{
		return;
	}

	const USunsetCoveUISettings* Settings = GetDefault<USunsetCoveUISettings>();
	FName Wanted;
	switch (Choice)
	{
	case ESunsetCoveControllerIcons::Xbox:        Wanted = Settings->XboxGamepadName; break;
	case ESunsetCoveControllerIcons::PlayStation: Wanted = Settings->PlayStationGamepadName; break;
	case ESunsetCoveControllerIcons::Automatic:
	default:
		// Leaving a forced family: back to the platform default; hardware detection may switch it later.
		if (CommonInput->GetCurrentGamepadName() == Settings->PlayStationGamepadName || CommonInput->GetCurrentGamepadName() == Settings->XboxGamepadName)
		{
			Wanted = Platform->GetDefaultGamepadName();
		}
		break;
	}

	if (!Wanted.IsNone() && CommonInput->GetCurrentGamepadName() != Wanted)
	{
		CommonInput->SetGamepadInputType(Wanted); // Broadcasts: every prompt refreshes.
	}
}

void USunsetCoveControllerIconsSubsystem::HandleGamepadChangeDetected(FName DetectedGamepadName)
{
	if (Choice != ESunsetCoveControllerIcons::Automatic)
	{
		ApplyChoice();
	}
}
