// DayNightManager.h
// SunsetCove - Environment area. The clock and the sky of the cove (Day/Night Cycle).
//
// WHAT THIS CLASS DOES (AND NOTHING ELSE)
//   1. Keeps the game clock.
//   2. Turns the clock into light: sun and moon rotation and intensity, sky light, sky atmosphere, post process.
//   3. Announces what happened to the clock through delegates.
//   It does NOT know about the barrier, visitors, shops, opening hours or the economy. Those belong to other
//   systems (for example a workday manager) that LISTEN to the delegates below and, when they need to, CALL the
//   control functions (StartTime, PauseTime, ResetDay, SetTimeOfDay). The dependency goes one way only:
//   gameplay -> DayNightManager, never the other way around.
//
// HOW THE DAY FLOWS (GDD, Feature Sheet - Day/Night Cycle)
//   Pre-day    The clock is frozen at StartHour (07:00) with bIsTimePaused = true. A gameplay system calls StartTime().
//   Running    The clock advances. RealWorldDayDurationMinutes real minutes = StartHour -> MainSpanEndHour (07:00 -> 19:00).
//   After 19   The clock does NOT stop. Sunset, evening and night follow on their own; if the player is late, the
//              cove simply gets dark.
//   Reset      ResetDay() puts the clock back to StartHour, pauses it and fires OnDayReset.
//   At 24:00 the clock wraps to 00:00 and keeps running (proposal, the GDD does not say). Only ResetDay() stops it.
//
// HIERARCHY
//   AActor -> ADayNightManager (this class) -> BP_DayNightManager (Blueprint, placed once per gameplay level)
//
// FINDING THE MANAGER FROM OTHER SYSTEMS
//   UDayNightCycleSubsystem::GetDayNightManager(this). The manager registers itself in PostInitializeComponents,
//   so the lookup already works inside any BeginPlay.
//
// EVENT ORDER INSIDE ONE TICK (lighting is always applied BEFORE the first event, so listeners see the final look)
//   OnTimeUpdated -> OnHourChanged (once per hour crossed) -> OnSunsetStarted / OnNightStarted -> OnTimeReached
//
// JUMPS (ResetDay, SetTimeOfDay, or the time written directly)
//   A jump fires OnTimeUpdated and, if the hour changed, OnHourChanged. It never fires OnTimeReached, OnSunsetStarted
//   or OnNightStarted: those mean "the clock ran through that hour", not "the clock was moved past it".
//
// NETWORK
//   Single player. Nothing here replicates.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Environment/DayNightCycle/Data/DayNightTypes.h"
#include "DayNightManager.generated.h"

class ADirectionalLight;
class APostProcessVolume;
class ASkyLight;
class UCurveFloat;
class UDayNightLightingProfile;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

/** The clock started running (it was paused). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayNightTimeStarted);

/** The clock was paused (it was running). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayNightTimePaused);

/** The clock moved. Fires every frame the clock runs, and once on every jump. CurrentTimeInHours: 13.5 = 13:30. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayNightTimeUpdated, float, CurrentTimeInHours);

/** A whole hour has been crossed (08:00, 09:00, ...). CurrentHour is a whole number, 0 to 23. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayNightHourChanged, float, CurrentHour);

/** The clock ran through an hour registered with RegisterTimeTarget (or listed in TimeReachedTargets). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayNightTimeReached, float, TargetHour);

/** The clock ran through SunsetStartHour. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayNightSunsetStarted);

/** The clock ran through NightStartHour. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayNightNightStarted);

/** ResetDay was called: the clock is back at StartHour and paused. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayNightDayReset);

UCLASS(Blueprintable)
class SUNSETCOVE_API ADayNightManager : public AActor
{
	GENERATED_BODY()

public:
	ADayNightManager();

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor Interface

	// ------------------------------------------------------------------
	// Clock control (called by gameplay systems)
	// ------------------------------------------------------------------

	/** Lets the clock run. Fires OnTimeStarted if it was paused. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Control")
	void StartTime();

	/** Freezes the clock. The light stays where it is. Fires OnTimePaused if it was running. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Control")
	void PauseTime();

	/** StartTime or PauseTime in one call. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Control")
	void SetTimePaused(bool bPause);

	/**
	 * Back to the morning: clock at StartHour, paused, lighting updated.
	 * Fires OnTimePaused (if it was running), OnTimeUpdated, OnHourChanged (if the hour changed) and OnDayReset.
	 */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Control")
	void ResetDay();

	/**
	 * Moves the clock to Hour (0 to 24, 24 = 00:00) without changing the paused state. This is a jump: see the
	 * note at the top of this file. Use it for events such as waking the player at 02:00.
	 */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Control")
	void SetTimeOfDay(float Hour);

	/** Registers an hour for OnTimeReached (for example 18.0 for a warning, 19.0 for closing). Duplicates are ignored. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Events")
	void RegisterTimeTarget(float Hour);

	/** Removes an hour registered with RegisterTimeTarget. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Events")
	void UnregisterTimeTarget(float Hour);

	/** Removes every registered hour, including the ones set in the Details panel. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Events")
	void ClearTimeTargets();

	/** Swaps the lighting profile (for example overcast for storm) and refreshes the lighting at once. */
	UFUNCTION(BlueprintCallable, Category = "DayNightCycle|Lighting")
	void SetLightingProfile(UDayNightLightingProfile* NewProfile);

	/**
	 * Reapplies the lighting for the current time. In the editor this is a button in the Details panel: press it after
	 * editing a curve asset (curve assets do not notify the manager). In code it is needed only after changing a curve
	 * or a reference by hand.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DayNightCycle|Lighting")
	void RefreshLighting();

	/**
	 * Editor button. Switches OFF every post process override this manager writes on the referenced volume (exposure
	 * method, exposure range, exposure compensation, saturation, white balance). Use it to clean a volume left in a bad
	 * state (for example gray), then press Refresh Lighting. The volume goes back to the project's own defaults.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DayNightCycle|Lighting")
	void ResetPostProcessOverrides();

	// ------------------------------------------------------------------
	// Queries (read only)
	// ------------------------------------------------------------------

	/** Whole hour of the clock, 0 to 23. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	int32 GetCurrentHour() const;

	/** Minute of the clock, 0 to 59. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	int32 GetCurrentMinute() const;

	/** The clock as "HH:MM", ready for an HUD. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	FText GetFormattedTime() const;

	/** Phase of the day at the current time. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	EDayNightPhase GetCurrentPhase() const;

	/** Phase of the day at any hour (0 to 24). */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	EDayNightPhase GetPhaseAtHour(float Hour) const;

	/**
	 * Unit vector from the world TOWARD the sun (the opposite of the direction the light shines).
	 * Meant for systems that need the sun position, for example shade under an umbrella.
	 */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	FVector GetSunDirection() const;

	/** Height of the sun above the horizon in degrees: 90 overhead, 0 on the horizon, negative at night. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle|Query")
	float GetSunElevationDegrees() const;

	// ------------------------------------------------------------------
	// Time settings (all editable in the Details panel; all can be changed at runtime)
	// ------------------------------------------------------------------

	/** Real minutes that StartHour -> MainSpanEndHour takes. GDD default 12 (test range 10 to 15). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "30.0"))
	float RealWorldDayDurationMinutes = 12.f;

	/** Hour the day starts at, and the hour ResetDay goes back to. GDD default 7 (07:00). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float StartHour = 7.f;

	/** End of the span that RealWorldDayDurationMinutes refers to. GDD: 19 (19:00). It sets the clock speed only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float MainSpanEndHour = 19.f;

	/** Extra speed factor on top of the duration above. 1 = normal. Handy to skim through the night while testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time", meta = (ClampMin = "0.0", UIMax = "50.0"))
	float TimeSpeedMultiplier = 1.f;

	/**
	 * Current time, 13.5 = 13:30. In the editor, drag it to preview the light in the viewport.
	 * At runtime prefer SetTimeOfDay: writing this directly also works (it is detected and treated as a jump).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float CurrentTimeInHours = 7.f;

	/** True = clock frozen. GDD default true: the day waits for a gameplay system to call StartTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time")
	bool bIsTimePaused = true;

	/**
	 * True = at BeginPlay the clock jumps to StartHour, so a time left over from editor previewing never leaks into
	 * the game. Turn it off to start a test session at the hour set in CurrentTimeInHours.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Time")
	bool bSnapToStartHourOnBeginPlay = true;

	// ------------------------------------------------------------------
	// Phases and time targets
	// ------------------------------------------------------------------

	/** Hour OnSunsetStarted fires at and the Sunset phase begins. GDD: 19 (19:00). Keep it in line with SunRotationCurve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Phases", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float SunsetStartHour = 19.f;

	/** Hour OnNightStarted fires at and the Night phase begins. Proposal: 21 (about when the sky is dark). Tune to the curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Phases", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float NightStartHour = 21.f;

	/**
	 * Hours that fire OnTimeReached. Empty by default on purpose: which hours matter (18:00 warning, 19:00 closing)
	 * is a gameplay decision, so gameplay systems add them with RegisterTimeTarget.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNightCycle|Events")
	TArray<float> TimeReachedTargets;

	// ------------------------------------------------------------------
	// Sun path (depends on how the level is oriented)
	// ------------------------------------------------------------------

	/**
	 * World yaw in degrees of the direction the sun RISES from (0 = +X axis). The sun sets on the opposite side.
	 * Rotate it until the sun sets over the water.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Sun Path", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float SunPathAzimuthDegrees = 0.f;

	/**
	 * Tilts the sun arc sideways, in degrees. 0 = the sun passes straight overhead. Higher = lower noon sun, the
	 * way it sits at higher latitudes (noon height = 90 minus this). Positive leans toward the +90 degree yaw side
	 * of SunPathAzimuthDegrees; use a negative value for the other side.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Sun Path", meta = (ClampMin = "-60.0", ClampMax = "60.0"))
	float SunPathInclinationDegrees = 20.f;

	/** Where the moon is on its arc compared to the sun, in degrees. 180 = always opposite (full moon look). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Sun Path")
	float MoonOrbitOffsetDegrees = 180.f;

	/**
	 * The moon never goes lower than this many degrees above the horizon: it keeps its compass direction but hangs low
	 * instead of setting. A moon below the horizon lights nothing on the ground, so with a normal orbit it sets exactly
	 * when the sun rises (and rises when the sun sets), and the scene goes dark at the hand-off. The moon's own intensity
	 * curve decides when it is on. -90 = off: the moon sets normally.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Sun Path", meta = (ClampMin = "-90.0", ClampMax = "60.0"))
	float MoonMinElevationDegrees = 20.f;

	/** Only used when the profile has no SunRotationCurve: hour of sunrise for the built-in arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Sun Path", meta = (ClampMin = "0.0", ClampMax = "12.0"))
	float FallbackSunriseHour = 6.5f;

	/** Only used when the profile has no SunRotationCurve: hour of sunset for the built-in arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Sun Path", meta = (ClampMin = "12.0", ClampMax = "24.0"))
	float FallbackSunsetHour = 20.f;

	// ------------------------------------------------------------------
	// Level references (set on the placed instance, see the Engine Setup Guide)
	// ------------------------------------------------------------------

	/** The sun. Must be Movable and marked as Atmosphere Sun Light, index 0. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DayNightCycle|References")
	TObjectPtr<ADirectionalLight> SunDirectionalLight;

	/** The moon. Must be Movable and marked as Atmosphere Sun Light, index 1. Optional. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DayNightCycle|References")
	TObjectPtr<ADirectionalLight> MoonDirectionalLight;

	/** The sky light (ambient). Must be Movable with Real Time Capture on. Optional. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DayNightCycle|References")
	TObjectPtr<ASkyLight> SkyLightActor;

	/**
	 * The Sky Atmosphere actor. A generic actor reference on purpose: the manager looks for the Sky Atmosphere
	 * component on it, and warns at start if there is none. Optional: driven only when the profile has a
	 * MieScatteringScaleCurve.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DayNightCycle|References")
	TObjectPtr<AActor> SkyAtmosphereActor;

	/** The post process volume that carries exposure, saturation and warmth. Must be Infinite Extent (Unbound). Optional. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DayNightCycle|References")
	TObjectPtr<APostProcessVolume> PostProcessVolume;

	/** The look of the day: all the curves. See UDayNightLightingProfile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DayNightCycle|Lighting")
	TObjectPtr<UDayNightLightingProfile> LightingProfile;

	// ------------------------------------------------------------------
	// Events (bind from Blueprint or with AddDynamic in C++)
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightTimeStarted OnTimeStarted;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightTimePaused OnTimePaused;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightTimeUpdated OnTimeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightHourChanged OnHourChanged;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightTimeReached OnTimeReached;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightSunsetStarted OnSunsetStarted;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightNightStarted OnNightStarted;

	UPROPERTY(BlueprintAssignable, Category = "DayNightCycle|Events")
	FOnDayNightDayReset OnDayReset;

private:
	// Clock
	void AdvanceTime(float DeltaSeconds);
	void SyncExternalChanges();
	void SyncPauseState();
	void NotifyTimeJumped(float OldHour);
	void BroadcastCrossedHours(float PrevHour, float EndHour);
	void BroadcastCrossedTargets(float PrevHour, float EndHour);

	// Lighting
	void ResolveReferences();
	void ValidateSetup() const;
	void ApplyLighting();
	float ComputeSunOrbitAngle(float Hour) const;
	FVector ComputeOrbitDirection(float OrbitAngleDegrees) const;
	void UpdateDirectionalLight(UDirectionalLightComponent* Light, const FVector& DirectionToLight, const UCurveFloat* IntensityCurve, float MaxIntensity, float Hour) const;
	void UpdatePostProcess(float Hour) const;

	// Components found on the referenced actors (cached, never serialized)
	UPROPERTY(Transient)
	TObjectPtr<UDirectionalLightComponent> SunLightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDirectionalLightComponent> MoonLightComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkyLightComponent> SkyLightComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphereComponent;

	/** Direction toward the sun, from the last ApplyLighting. */
	FVector CachedSunDirection = FVector::UpVector;

	/** The time this class last wrote or processed. A different CurrentTimeInHours means someone wrote it directly. */
	float LastKnownTimeInHours = 7.f;

	/** The paused state listeners were last told about. A different bIsTimePaused means someone wrote it directly. */
	bool bPauseStateReported = true;

	/** Lighting must be reapplied even though the clock did not move (a property or a reference changed). */
	bool bLightingDirty = true;

	/** Last Mie scale sent to the atmosphere. Changing it rebuilds atmosphere data, so it is sent only when it moved (by more than 1%). */
	float LastAppliedMieScale = -1.f;
};
