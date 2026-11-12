// SunsetCovePlayerPreferences.cpp
// SunsetCove - Core / Settings. See Public/Core/Settings/SunsetCovePlayerPreferences.h.

#include "Core/Settings/SunsetCovePlayerPreferences.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* const GInputSection = TEXT("SunsetCove.Input");
	const TCHAR* const GSprintModeKey = TEXT("SprintMode");
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
