// SunsetCovePlayerPreferences.cpp
// SunsetCove - Core / Settings. See Public/Core/Settings/SunsetCovePlayerPreferences.h.

#include "Core/Settings/SunsetCovePlayerPreferences.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* const GInputSection = TEXT("SunsetCove.Input");
	const TCHAR* const GSprintModeKey = TEXT("SprintMode");
	const TCHAR* const GHintsKey = TEXT("ShowHints");

	/** -1 = not read yet, 0 = off, 1 = on. The HUD asks every frame: no ini lookup each time. */
	int32 GHintsCache = -1;
}

bool USunsetCovePlayerPreferences::AreHintsEnabled()
{
	if (GHintsCache < 0)
	{
		bool bSaved = true; // Default: hints on.
		if (GConfig)
		{
			GConfig->GetBool(GInputSection, GHintsKey, bSaved, GGameUserSettingsIni);
		}
		GHintsCache = bSaved ? 1 : 0;
	}
	return GHintsCache == 1;
}

void USunsetCovePlayerPreferences::SetHintsEnabled(bool bEnabled, bool bSave)
{
	GHintsCache = bEnabled ? 1 : 0;
	if (!GConfig)
	{
		return;
	}
	GConfig->SetBool(GInputSection, GHintsKey, bEnabled, GGameUserSettingsIni);
	if (bSave)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

ESunsetCoveSprintMode USunsetCovePlayerPreferences::GetSprintMode()
{
	int32 Saved = static_cast<int32>(ESunsetCoveSprintMode::Hold); // Default: hold to sprint.
	if (GConfig)
	{
		GConfig->GetInt(GInputSection, GSprintModeKey, Saved, GGameUserSettingsIni);
	}
	return Saved == static_cast<int32>(ESunsetCoveSprintMode::Toggle) ? ESunsetCoveSprintMode::Toggle : ESunsetCoveSprintMode::Hold;
}

void USunsetCovePlayerPreferences::SetSprintMode(ESunsetCoveSprintMode NewMode, bool bSave)
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetInt(GInputSection, GSprintModeKey, static_cast<int32>(NewMode), GGameUserSettingsIni);
	if (bSave)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
