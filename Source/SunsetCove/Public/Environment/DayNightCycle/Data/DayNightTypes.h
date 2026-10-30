// DayNightTypes.h
// SunsetCove - Environment area (Day/Night Cycle). Plain types shared by the Day/Night Cycle classes.

#pragma once

#include "CoreMinimal.h"
#include "DayNightTypes.generated.h"

/**
 * Coarse phase of the day, derived from the clock only. It has no gameplay meaning on its own:
 * what the beach does in each phase is decided by other systems that listen to the manager.
 * The hour thresholds are properties of ADayNightManager (StartHour, SunsetStartHour, NightStartHour).
 */
UENUM(BlueprintType)
enum class EDayNightPhase : uint8
{
	/** From StartHour until SunsetStartHour. */
	Day,

	/** From SunsetStartHour until NightStartHour: the sun goes down, the golden hour of the game. */
	Sunset,

	/** From NightStartHour until StartHour of the next morning. */
	Night
};

/**
 * Which auto exposure method the manager sets on the post process volume.
 * The exposure range and compensation only act while auto exposure is enabled in the project.
 */
UENUM(BlueprintType)
enum class EDayNightExposureMode : uint8
{
	/** The manager does not touch the exposure method of the volume. */
	LeaveAsIs,

	/** Auto Exposure Histogram: Unreal's default metering mode. Finer control, a bit more cost. */
	Histogram,

	/** Auto Exposure Basic: simpler and cheaper metering. */
	Basic
};
