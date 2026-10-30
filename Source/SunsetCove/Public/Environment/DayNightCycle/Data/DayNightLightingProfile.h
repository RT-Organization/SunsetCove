// DayNightLightingProfile.h
// SunsetCove - Environment area (Day/Night Cycle). The "look" of the day, authored as curves in a data asset.
//
// WHAT THIS IS
//   A plain data asset. It holds no logic. ADayNightManager samples these curves with the time of day and pushes
//   the result into the lights, the sky and the post process volume.
//   Keeping the look in an asset (and not on the manager) lets art make presets (clear day, overcast, storm) and
//   swap them with ADayNightManager::SetLightingProfile without touching the level.
//
// HOW THE CURVES ARE READ
//   X axis = hour of the day, 0 to 24 (13.5 = 13:30). Every curve is optional: an EMPTY slot, or a curve asset with NO KEYS, means "this channel
//   is not driven", and the manager leaves that value exactly as it is set on the light or volume in the level.
//   (A curve asset with no keys reads 0 at every hour, so the manager ignores it on purpose: 0 saturation would turn
//   the world gray and a 0 sun intensity would switch the sun off. Add keys to make it count.)
//   Intensity curves are MULTIPLIERS (0 to 1): the absolute strength is the Max value of the same section, so
//   one number rescales the whole day.
//
// HIERARCHY
//   UDataAsset -> UDayNightLightingProfile (this class) -> DA_DayNightLightingProfile_* (data asset)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Environment/DayNightCycle/Data/DayNightTypes.h"
#include "DayNightLightingProfile.generated.h"

class UCurveFloat;
class UCurveLinearColor;

UCLASS(BlueprintType)
class SUNSETCOVE_API UDayNightLightingProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// Sun
	// ------------------------------------------------------------------

	/**
	 * Position of the sun on its arc, in degrees, for each hour.
	 * 0 = sun on the horizon at sunrise, 90 = highest point, 180 = on the horizon at sunset, 270 = lowest point.
	 * The value may go past 360: at hour 24 it should equal the value at hour 0 plus 360, so the day loops with
	 * no jump at midnight. Slow the curve down around sunset (more hours per degree) to make the golden hour last.
	 * Empty = the manager uses its built-in fallback arc (see FallbackSunriseHour / FallbackSunsetHour).
	 * This is the GDD "SunLightingCurve", split in one curve per channel because one float curve cannot carry
	 * rotation and intensity together.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sun")
	TObjectPtr<UCurveFloat> SunRotationCurve;

	/** Sun strength multiplier (0 to 1) over the day. Empty = the sun intensity stays as set on the light. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sun")
	TObjectPtr<UCurveFloat> SunIntensityCurve;

	/** Intensity of the sun at multiplier 1, in lux. Start from the value the sun has in the level today. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sun", meta = (ClampMin = "0.0"))
	float SunMaxIntensity = 10.f;

	/**
	 * Tint of the sun over the day. Optional: with the light marked as Atmosphere Sun Light the atmosphere already
	 * reddens the sun near the horizon, so use this only for an extra artistic push.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sun")
	TObjectPtr<UCurveLinearColor> SunColorCurve;

	// ------------------------------------------------------------------
	// Moon
	// ------------------------------------------------------------------

	/** Moon strength multiplier (0 to 1). The moon follows the sun on the opposite side of the sky. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Moon")
	TObjectPtr<UCurveFloat> MoonIntensityCurve;

	/**
	 * Intensity of the moon at multiplier 1, in lux. Keep it LOW: the moon is an atmosphere light, so the sky is lit by it
	 * the way it is lit by the sun. At a few percent of the sun the night sky already looks like a dim day.
	 * Start around 0.05 (with a sun of 10) and raise it only until the sand is readable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Moon", meta = (ClampMin = "0.0"))
	float MoonMaxIntensity = 0.05f;

	// ------------------------------------------------------------------
	// Sky light and atmosphere
	// ------------------------------------------------------------------

	/** Sky light strength multiplier (0 to 1): the ambient light, which should never fall to zero at night. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sky")
	TObjectPtr<UCurveFloat> SkyLightIntensityCurve;

	/** Intensity of the sky light at multiplier 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sky", meta = (ClampMin = "0.0"))
	float SkyLightMaxIntensity = 1.f;

	/**
	 * Mie Scattering Scale of the SkyAtmosphere over the day, as an absolute value. The atmosphere's own default is
	 * about 0.004: read the value on the SkyAtmosphere actor and build the curve around it. Raising it around sunset
	 * strengthens the glow around the sun. Empty = the atmosphere is not touched.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Sky")
	TObjectPtr<UCurveFloat> MieScatteringScaleCurve;

	// ------------------------------------------------------------------
	// Post process: exposure
	// ------------------------------------------------------------------

	/**
	 * Which auto exposure method the volume uses. LeaveAsIs = the volume keeps whatever it has.
	 * Auto exposure must be enabled in the project (Project Settings > Rendering > Default Settings > Auto Exposure)
	 * for the exposure range below to have any effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Exposure")
	EDayNightExposureMode ExposureMode = EDayNightExposureMode::LeaveAsIs;

	/**
	 * Calibration value, in EV100: the exposure at which a sunny noon looks right. The two range curves below are
	 * OFFSETS from this number, so recalibrating the whole day means changing this one value.
	 * Find it by pinning the exposure (see the Setup Guide). A first guess for a sun of 10 lux is about 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Exposure")
	float DayExposureEV100 = 1.f;

	/**
	 * Lowest EV100 the eye may adapt to, as an offset in stops from DayExposureEV100, over the day.
	 * A LOWER EV100 means a BRIGHTER image, so this curve limits how much a dark scene (night, cave) may brighten:
	 * the higher it is, the darker the night stays. Empty = the Min EV100 of the volume is not touched.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Exposure")
	TObjectPtr<UCurveFloat> ExposureMinOffsetCurve;

	/**
	 * Highest EV100 the eye may adapt to, as an offset in stops from DayExposureEV100, over the day.
	 * It limits how much a very bright view may darken. If it ever falls below the Min curve it is raised to match it
	 * (and eye adaptation is off at that hour, because Min = Max). Empty = the Max EV100 of the volume is not touched.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Exposure")
	TObjectPtr<UCurveFloat> ExposureMaxOffsetCurve;

	/**
	 * Exposure Compensation in EV, added on top of the exposure (0 = no change, +1 = one stop brighter).
	 * It REPLACES the compensation of the volume and of the project: if the picture gets darker than before this curve
	 * was assigned, the project's default compensation (r.DefaultFeature.AutoExposure.Bias in DefaultEngine.ini) is
	 * no longer applied, so add it to the curve. Empty = not driven.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Exposure")
	TObjectPtr<UCurveFloat> ExposureBiasCurve;

	// ------------------------------------------------------------------
	// Post process: color
	// ------------------------------------------------------------------

	/** Color saturation of the post process volume (1 = unchanged, 0 = black and white). Empty = not driven. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Color")
	TObjectPtr<UCurveFloat> SaturationCurve;

	/**
	 * White balance temperature of the post process volume, in Kelvin (6500 = neutral). The manager keeps the value
	 * between 1500 and 15000. In Unreal's white balance a higher value looks warmer. Empty = not driven.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|PostProcess|Color")
	TObjectPtr<UCurveFloat> WhiteTemperatureCurve;
};
