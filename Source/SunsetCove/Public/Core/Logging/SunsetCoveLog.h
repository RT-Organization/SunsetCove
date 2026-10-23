// SunsetCoveLog.h
// SunsetCove - Core area. The log category of the game module.
//
// Use it in every SunsetCove class:  UE_LOG(LogSunsetCove, Warning, TEXT("..."));
// In the editor: Window > Output Log, then type "LogSunsetCove" in the filter box.
// CoreFramework uses the same pattern for its UI (LogCoreUI).

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

SUNSETCOVE_API DECLARE_LOG_CATEGORY_EXTERN(LogSunsetCove, Log, All);
