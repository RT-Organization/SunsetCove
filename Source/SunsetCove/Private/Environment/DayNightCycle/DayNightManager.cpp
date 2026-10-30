// DayNightManager.cpp
// SunsetCove - Environment area. See Public/Environment/DayNightCycle/DayNightManager.h for the role of this class.

#include "Environment/DayNightCycle/DayNightManager.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Environment/DayNightCycle/DayNightCycleSubsystem.h"
#include "Environment/DayNightCycle/Data/DayNightLightingProfile.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float HoursPerDay = 24.f;

	/** Two clock values closer than this are the same moment (about a third of a game second). */
	constexpr float TimeEpsilon = 1.e-4f;

	/** Two registered hours closer than this are the same hour. */
	constexpr float TargetMatchTolerance = 1.e-3f;

	/** A light whose curve multiplier is below this is hidden, so it costs no shadow rendering while it adds no light. */
	constexpr float HideLightBelowCurveValue = 0.001f;

	/** The atmosphere is updated only when the Mie scale moved by more than this fraction of its value. */
	constexpr float MieScaleRelativeTolerance = 0.01f;

	/** Any hour value into [0, 24). 24 becomes 0, -1 becomes 23. */
	float WrapHour(float Hour)
	{
		float Wrapped = FMath::Fmod(Hour, HoursPerDay);
		if (Wrapped < 0.f)
		{
			Wrapped += HoursPerDay;
		}
		return Wrapped;
	}

	/**
	 * True if the clock ran through TargetHour while going from PrevHour to EndHour.
	 * PrevHour is in [0, 24). EndHour is PrevHour plus the elapsed hours and is NOT wrapped, so it can pass 24.
	 * TargetHour is in [0, 24). The start is excluded and the end included: a target hit exactly fires once.
	 */
	bool CrossedHour(float PrevHour, float EndHour, float TargetHour)
	{
		const bool bThisDay = TargetHour > PrevHour && TargetHour <= EndHour;
		const bool bNextDay = (TargetHour + HoursPerDay) > PrevHour && (TargetHour + HoursPerDay) <= EndHour;
		return bThisDay || bNextDay;
	}

	/** True if the curve has at least one key. A curve asset with no keys evaluates to 0 at every hour. */
	bool HasKeys(const UCurveFloat* Curve)
	{
		return Curve && Curve->FloatCurve.GetNumKeys() > 0;
	}

	bool HasKeys(const UCurveLinearColor* Curve)
	{
		return Curve && (Curve->FloatCurves[0].GetNumKeys() > 0 || Curve->FloatCurves[1].GetNumKeys() > 0 || Curve->FloatCurves[2].GetNumKeys() > 0);
	}

	/**
	 * The curve itself if it can be sampled, null if it is unassigned or empty. Null means "this channel is not driven".
	 * This is what keeps an empty curve asset from writing 0 into the world (0 saturation = a gray world).
	 */
	const UCurveFloat* UsableCurve(const UCurveFloat* Curve)
	{
		return HasKeys(Curve) ? Curve : nullptr;
	}

	const UCurveLinearColor* UsableColorCurve(const UCurveLinearColor* Curve)
	{
		return HasKeys(Curve) ? Curve : nullptr;
	}
}

ADayNightManager::ADayNightManager()
{
	// The tick also runs while the clock is paused: it is what notices a value written by hand (see SyncExternalChanges).
	// It does almost nothing in that case (a few comparisons).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ADayNightManager::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Registered here, not in BeginPlay: every actor runs PostInitializeComponents before any actor runs BeginPlay,
	// so other systems can already look the manager up from their own BeginPlay.
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (UDayNightCycleSubsystem* Subsystem = World->GetSubsystem<UDayNightCycleSubsystem>())
		{
			Subsystem->RegisterManager(this);
		}
	}
}

void ADayNightManager::BeginPlay()
{
	Super::BeginPlay();

	ResolveReferences();
	ValidateSetup();

	if (bSnapToStartHourOnBeginPlay)
	{
		CurrentTimeInHours = StartHour;
	}
	CurrentTimeInHours = WrapHour(CurrentTimeInHours);
	LastKnownTimeInHours = CurrentTimeInHours;

	// Nobody can be listening yet, so the first state is just recorded, not announced.
	bPauseStateReported = bIsTimePaused;

	LastAppliedMieScale = -1.f;
	ApplyLighting();

	UE_LOG(LogSunsetCove, Verbose, TEXT("DayNightManager '%s': start at %s, %s."), *GetNameSafe(this),
		*GetFormattedTime().ToString(), bIsTimePaused ? TEXT("paused") : TEXT("running"));
}

void ADayNightManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		if (UDayNightCycleSubsystem* Subsystem = World->GetSubsystem<UDayNightCycleSubsystem>())
		{
			Subsystem->UnregisterManager(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADayNightManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SyncExternalChanges();

	if (bIsTimePaused)
	{
		if (bLightingDirty)
		{
			ApplyLighting();
		}
		return;
	}

	AdvanceTime(DeltaSeconds);
}

void ADayNightManager::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Lets the level show the right light as soon as it opens in the editor.
	if (!IsTemplate())
	{
		ResolveReferences();
		ApplyLighting();
	}
}

#if WITH_EDITOR
void ADayNightManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ResolveReferences();
	LastAppliedMieScale = -1.f;
	bLightingDirty = true;

	// Setup problems are reported as soon as the profile or a reference is assigned, not only at BeginPlay.
	const FName ChangedProperty = PropertyChangedEvent.GetPropertyName();
	if (ChangedProperty == GET_MEMBER_NAME_CHECKED(ADayNightManager, LightingProfile)
		|| ChangedProperty == GET_MEMBER_NAME_CHECKED(ADayNightManager, PostProcessVolume)
		|| ChangedProperty == GET_MEMBER_NAME_CHECKED(ADayNightManager, SkyAtmosphereActor))
	{
		ValidateSetup();
	}

	// Nothing ticks in an editor world, so apply right away: the viewport follows the Details panel.
	// In Play In Editor the dirty flag above is picked up by the next tick.
	const UWorld* World = GetWorld();
	if (World && !World->IsGameWorld())
	{
		ApplyLighting();
	}
}
#endif

// ----------------------------------------------------------------------
// Clock control
// ----------------------------------------------------------------------

void ADayNightManager::StartTime()
{
	SetTimePaused(false);
}

void ADayNightManager::PauseTime()
{
	SetTimePaused(true);
}

void ADayNightManager::SetTimePaused(bool bPause)
{
	bIsTimePaused = bPause;
	SyncPauseState();
}

void ADayNightManager::ResetDay()
{
	const float OldHour = CurrentTimeInHours;

	CurrentTimeInHours = WrapHour(StartHour);
	LastKnownTimeInHours = CurrentTimeInHours;
	bIsTimePaused = true;

	ApplyLighting();

	SyncPauseState();
	NotifyTimeJumped(OldHour);
	OnDayReset.Broadcast();

	UE_LOG(LogSunsetCove, Verbose, TEXT("DayNightManager '%s': day reset to %s."), *GetNameSafe(this), *GetFormattedTime().ToString());
}

void ADayNightManager::SetTimeOfDay(float Hour)
{
	const float OldHour = CurrentTimeInHours;

	CurrentTimeInHours = WrapHour(Hour);
	LastKnownTimeInHours = CurrentTimeInHours;

	ApplyLighting();
	NotifyTimeJumped(OldHour);
}

void ADayNightManager::RegisterTimeTarget(float Hour)
{
	const float Target = WrapHour(Hour);
	for (const float Existing : TimeReachedTargets)
	{
		if (FMath::IsNearlyEqual(WrapHour(Existing), Target, TargetMatchTolerance))
		{
			return;
		}
	}
	TimeReachedTargets.Add(Target);
}

void ADayNightManager::UnregisterTimeTarget(float Hour)
{
	const float Target = WrapHour(Hour);
	TimeReachedTargets.RemoveAll([Target](float Existing)
	{
		return FMath::IsNearlyEqual(WrapHour(Existing), Target, TargetMatchTolerance);
	});
}

void ADayNightManager::ClearTimeTargets()
{
	TimeReachedTargets.Reset();
}

void ADayNightManager::SetLightingProfile(UDayNightLightingProfile* NewProfile)
{
	LightingProfile = NewProfile;
	RefreshLighting();
}

void ADayNightManager::RefreshLighting()
{
	LastAppliedMieScale = -1.f;
	ApplyLighting();
}

void ADayNightManager::ResetPostProcessOverrides()
{
	if (!PostProcessVolume)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': no PostProcessVolume assigned, nothing to reset."), *GetNameSafe(this));
		return;
	}

#if WITH_EDITOR
	// Registers the change with the editor, so the level is marked dirty and the clean state gets saved.
	PostProcessVolume->Modify();
#endif

	FPostProcessSettings& Settings = PostProcessVolume->Settings;
	Settings.bOverride_AutoExposureMethod = false;
	Settings.bOverride_AutoExposureMinBrightness = false;
	Settings.bOverride_AutoExposureMaxBrightness = false;
	Settings.bOverride_AutoExposureBias = false;
	Settings.bOverride_ColorSaturation = false;
	Settings.bOverride_WhiteTemp = false;

	UE_LOG(LogSunsetCove, Log, TEXT("DayNightManager '%s': post process overrides switched off on '%s'."), *GetNameSafe(this), *GetNameSafe(PostProcessVolume));
}

// ----------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------

int32 ADayNightManager::GetCurrentHour() const
{
	return FMath::Clamp(FMath::FloorToInt(WrapHour(CurrentTimeInHours)), 0, 23);
}

int32 ADayNightManager::GetCurrentMinute() const
{
	const float Hour = WrapHour(CurrentTimeInHours);
	return FMath::Clamp(FMath::FloorToInt((Hour - FMath::FloorToFloat(Hour)) * 60.f), 0, 59);
}

FText ADayNightManager::GetFormattedTime() const
{
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), GetCurrentHour(), GetCurrentMinute()));
}

EDayNightPhase ADayNightManager::GetCurrentPhase() const
{
	return GetPhaseAtHour(CurrentTimeInHours);
}

EDayNightPhase ADayNightManager::GetPhaseAtHour(float Hour) const
{
	const float Wrapped = WrapHour(Hour);
	if (Wrapped >= StartHour && Wrapped < SunsetStartHour)
	{
		return EDayNightPhase::Day;
	}
	if (Wrapped >= SunsetStartHour && Wrapped < NightStartHour)
	{
		return EDayNightPhase::Sunset;
	}
	return EDayNightPhase::Night;
}

FVector ADayNightManager::GetSunDirection() const
{
	return CachedSunDirection;
}

float ADayNightManager::GetSunElevationDegrees() const
{
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(CachedSunDirection.Z, -1.0, 1.0))));
}

// ----------------------------------------------------------------------
// Clock internals
// ----------------------------------------------------------------------

void ADayNightManager::AdvanceTime(float DeltaSeconds)
{
	// Clock speed: StartHour -> MainSpanEndHour takes RealWorldDayDurationMinutes of real time.
	const float SpanHours = FMath::Max(MainSpanEndHour - StartHour, TimeEpsilon);
	const float SpanRealSeconds = FMath::Max(RealWorldDayDurationMinutes, 0.01f) * 60.f;
	const float GameHoursPerRealSecond = SpanHours / SpanRealSeconds;

	// Capped below one day so a long hitch cannot make the crossing tests below ambiguous.
	const float DeltaHours = FMath::Clamp(DeltaSeconds * GameHoursPerRealSecond * TimeSpeedMultiplier, 0.f, HoursPerDay - 1.f);
	if (DeltaHours <= 0.f)
	{
		return;
	}

	const float PrevHour = WrapHour(CurrentTimeInHours);
	const float EndHour = PrevHour + DeltaHours; // not wrapped on purpose: the crossing tests need it that way

	CurrentTimeInHours = WrapHour(EndHour);
	LastKnownTimeInHours = CurrentTimeInHours;

	// Light first, events after: a listener that reads the sky inside a handler sees the final look of this frame.
	ApplyLighting();

	OnTimeUpdated.Broadcast(CurrentTimeInHours);
	BroadcastCrossedHours(PrevHour, EndHour);

	if (CrossedHour(PrevHour, EndHour, WrapHour(SunsetStartHour)))
	{
		OnSunsetStarted.Broadcast();
	}
	if (CrossedHour(PrevHour, EndHour, WrapHour(NightStartHour)))
	{
		OnNightStarted.Broadcast();
	}

	BroadcastCrossedTargets(PrevHour, EndHour);
}

void ADayNightManager::SyncExternalChanges()
{
	// bIsTimePaused written directly (Details panel in Play In Editor, a Blueprint node): announce it as usual.
	SyncPauseState();

	// CurrentTimeInHours written directly: treat it as a jump.
	if (!FMath::IsNearlyEqual(CurrentTimeInHours, LastKnownTimeInHours, TimeEpsilon))
	{
		const float OldHour = LastKnownTimeInHours;
		CurrentTimeInHours = WrapHour(CurrentTimeInHours);
		LastKnownTimeInHours = CurrentTimeInHours;

		ApplyLighting();
		NotifyTimeJumped(OldHour);
	}
}

void ADayNightManager::SyncPauseState()
{
	if (bIsTimePaused == bPauseStateReported)
	{
		return;
	}

	bPauseStateReported = bIsTimePaused;
	if (bIsTimePaused)
	{
		OnTimePaused.Broadcast();
	}
	else
	{
		OnTimeStarted.Broadcast();
	}
}

void ADayNightManager::NotifyTimeJumped(float OldHour)
{
	const float NewHour = WrapHour(CurrentTimeInHours);

	OnTimeUpdated.Broadcast(NewHour);

	const int32 NewWholeHour = FMath::FloorToInt(NewHour);
	if (NewWholeHour != FMath::FloorToInt(WrapHour(OldHour)))
	{
		OnHourChanged.Broadcast(static_cast<float>(NewWholeHour));
	}
}

void ADayNightManager::BroadcastCrossedHours(float PrevHour, float EndHour)
{
	// One event per whole hour crossed, in order. More than one only after a hitch or with a large TimeSpeedMultiplier.
	const int32 LastHour = FMath::FloorToInt(EndHour);
	for (int32 Hour = FMath::FloorToInt(PrevHour) + 1; Hour <= LastHour; ++Hour)
	{
		OnHourChanged.Broadcast(static_cast<float>(Hour % 24));
	}
}

void ADayNightManager::BroadcastCrossedTargets(float PrevHour, float EndHour)
{
	if (TimeReachedTargets.IsEmpty())
	{
		return;
	}

	// Collected first, broadcast after: a listener may register or remove targets from inside its handler.
	// Key = hours after PrevHour (so the targets fire in the order the clock reached them), Value = the target hour.
	TArray<TPair<float, float>> Crossed;
	for (const float RawTarget : TimeReachedTargets)
	{
		const float Target = WrapHour(RawTarget);
		if (CrossedHour(PrevHour, EndHour, Target))
		{
			Crossed.Emplace(WrapHour(Target - PrevHour), Target);
		}
	}

	Crossed.Sort([](const TPair<float, float>& A, const TPair<float, float>& B)
	{
		return A.Key < B.Key;
	});

	for (const TPair<float, float>& Entry : Crossed)
	{
		OnTimeReached.Broadcast(Entry.Value);
	}
}

// ----------------------------------------------------------------------
// Lighting
// ----------------------------------------------------------------------

void ADayNightManager::ResolveReferences()
{
	SunLightComponent = SunDirectionalLight ? SunDirectionalLight->FindComponentByClass<UDirectionalLightComponent>() : nullptr;
	MoonLightComponent = MoonDirectionalLight ? MoonDirectionalLight->FindComponentByClass<UDirectionalLightComponent>() : nullptr;
	SkyLightComponent = SkyLightActor ? SkyLightActor->FindComponentByClass<USkyLightComponent>() : nullptr;
	SkyAtmosphereComponent = SkyAtmosphereActor ? SkyAtmosphereActor->FindComponentByClass<USkyAtmosphereComponent>() : nullptr;
}

void ADayNightManager::ValidateSetup() const
{
	const FString Self = GetNameSafe(this);

	if (!SunDirectionalLight)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': SunDirectionalLight is not assigned. The sun will not move."), *Self);
	}
	if (SunDirectionalLight && SunDirectionalLight == MoonDirectionalLight)
	{
		UE_LOG(LogSunsetCove, Error, TEXT("DayNightManager '%s': Sun and Moon are the same actor. Use two directional lights."), *Self);
	}
	if (!LightingProfile)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': no LightingProfile. The sun moves along the fallback arc and nothing else is driven."), *Self);
	}
	if (!MoonDirectionalLight || !SkyLightActor || !SkyAtmosphereActor || !PostProcessVolume)
	{
		UE_LOG(LogSunsetCove, Log, TEXT("DayNightManager '%s': some optional references are empty (moon, sky light, atmosphere or post process). They are skipped."), *Self);
	}

	auto CheckMovable = [&Self](const USceneComponent* Component, const TCHAR* Label)
	{
		if (Component && Component->Mobility != EComponentMobility::Movable)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': %s is not Movable, so it cannot change at runtime. Set its Mobility to Movable."), *Self, Label);
		}
	};
	CheckMovable(SunLightComponent, TEXT("the sun"));
	CheckMovable(MoonLightComponent, TEXT("the moon"));
	CheckMovable(SkyLightComponent, TEXT("the sky light"));

	if (SkyAtmosphereActor && !SkyAtmosphereComponent)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': '%s' is assigned as SkyAtmosphereActor but it has no Sky Atmosphere component."), *Self, *GetNameSafe(SkyAtmosphereActor));
	}

	if (PostProcessVolume && !PostProcessVolume->bUnbound)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': post process volume '%s' is not Infinite Extent (Unbound), so its effect only exists inside the box."), *Self, *GetNameSafe(PostProcessVolume));
	}

	if (MainSpanEndHour <= StartHour)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': MainSpanEndHour (%.2f) must be after StartHour (%.2f). The clock speed is wrong."), *Self, MainSpanEndHour, StartHour);
	}
	if (!(StartHour < SunsetStartHour && SunsetStartHour < NightStartHour && NightStartHour <= HoursPerDay))
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': keep StartHour < SunsetStartHour < NightStartHour <= 24. The phases and sunset/night events will be wrong."), *Self);
	}

	if (const UDayNightLightingProfile* Profile = LightingProfile)
	{
		// A curve that is assigned but has no keys reads 0 everywhere. It is ignored, and reported here.
		auto WarnIfEmpty = [&Self](bool bAssigned, bool bHasKeysNow, const TCHAR* Label)
		{
			if (bAssigned && !bHasKeysNow)
			{
				UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': %s is assigned but has no keys, so it is ignored (an empty curve would read 0). Add keys, or clear the slot."), *Self, Label);
			}
		};
		WarnIfEmpty(Profile->SunRotationCurve != nullptr, HasKeys(Profile->SunRotationCurve.Get()), TEXT("SunRotationCurve"));
		WarnIfEmpty(Profile->SunIntensityCurve != nullptr, HasKeys(Profile->SunIntensityCurve.Get()), TEXT("SunIntensityCurve"));
		WarnIfEmpty(Profile->SunColorCurve != nullptr, HasKeys(Profile->SunColorCurve.Get()), TEXT("SunColorCurve"));
		WarnIfEmpty(Profile->MoonIntensityCurve != nullptr, HasKeys(Profile->MoonIntensityCurve.Get()), TEXT("MoonIntensityCurve"));
		WarnIfEmpty(Profile->SkyLightIntensityCurve != nullptr, HasKeys(Profile->SkyLightIntensityCurve.Get()), TEXT("SkyLightIntensityCurve"));
		WarnIfEmpty(Profile->MieScatteringScaleCurve != nullptr, HasKeys(Profile->MieScatteringScaleCurve.Get()), TEXT("MieScatteringScaleCurve"));
		WarnIfEmpty(Profile->ExposureMinOffsetCurve != nullptr, HasKeys(Profile->ExposureMinOffsetCurve.Get()), TEXT("ExposureMinOffsetCurve"));
		WarnIfEmpty(Profile->ExposureMaxOffsetCurve != nullptr, HasKeys(Profile->ExposureMaxOffsetCurve.Get()), TEXT("ExposureMaxOffsetCurve"));
		WarnIfEmpty(Profile->ExposureBiasCurve != nullptr, HasKeys(Profile->ExposureBiasCurve.Get()), TEXT("ExposureBiasCurve"));
		WarnIfEmpty(Profile->SaturationCurve != nullptr, HasKeys(Profile->SaturationCurve.Get()), TEXT("SaturationCurve"));
		WarnIfEmpty(Profile->WhiteTemperatureCurve != nullptr, HasKeys(Profile->WhiteTemperatureCurve.Get()), TEXT("WhiteTemperatureCurve"));

		// The exposure range is written in EV100 only if the project uses the extended luminance range.
		if (UsableCurve(Profile->ExposureMinOffsetCurve.Get()) || UsableCurve(Profile->ExposureMaxOffsetCurve.Get()))
		{
			const TConsoleVariableData<int32>* ExtendRange = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
			if (ExtendRange && ExtendRange->GetValueOnGameThread() == 0)
			{
				UE_LOG(LogSunsetCove, Warning, TEXT("DayNightManager '%s': the exposure range curves are in EV100, but 'Extend default luminance range in Auto Exposure settings' is off in the project. The numbers will be read as luminance instead."), *Self);
			}
		}
	}
}

void ADayNightManager::ApplyLighting()
{
	bLightingDirty = false;

	const float Hour = WrapHour(CurrentTimeInHours);

	// Geometry: where the sun and the moon are.
	const float SunAngle = ComputeSunOrbitAngle(Hour);
	const FVector ToSun = ComputeOrbitDirection(SunAngle);
	const FVector ToMoon = ComputeOrbitDirection(SunAngle + MoonOrbitOffsetDegrees);
	CachedSunDirection = ToSun;

	const UDayNightLightingProfile* Profile = LightingProfile;

	// A curve that is unassigned, or assigned but empty, comes back null here and is simply not driven.
	const UCurveFloat* SunIntensityCurve = Profile ? UsableCurve(Profile->SunIntensityCurve.Get()) : nullptr;
	const UCurveFloat* MoonIntensityCurve = Profile ? UsableCurve(Profile->MoonIntensityCurve.Get()) : nullptr;
	const UCurveLinearColor* SunColorCurve = Profile ? UsableColorCurve(Profile->SunColorCurve.Get()) : nullptr;
	const UCurveFloat* SkyLightIntensityCurve = Profile ? UsableCurve(Profile->SkyLightIntensityCurve.Get()) : nullptr;
	const UCurveFloat* MieScaleCurve = Profile ? UsableCurve(Profile->MieScatteringScaleCurve.Get()) : nullptr;

	// Sun and moon: rotation always, intensity only when the profile has a usable curve for it.
	UpdateDirectionalLight(SunLightComponent, ToSun, SunIntensityCurve, Profile ? Profile->SunMaxIntensity : 0.f, Hour);
	UpdateDirectionalLight(MoonLightComponent, ToMoon, MoonIntensityCurve, Profile ? Profile->MoonMaxIntensity : 0.f, Hour);

	if (SunColorCurve && SunLightComponent)
	{
		SunLightComponent->SetLightColor(SunColorCurve->GetLinearColorValue(Hour));
	}

	// Sky light (ambient).
	if (SkyLightIntensityCurve && SkyLightComponent)
	{
		const float Multiplier = FMath::Max(0.f, SkyLightIntensityCurve->GetFloatValue(Hour));
		SkyLightComponent->SetIntensity(Multiplier * Profile->SkyLightMaxIntensity);
	}

	// Sky atmosphere. The sky color itself follows the sun and moon lights (set up as Atmosphere Sun Light in the
	// level); the only thing driven here is the Mie scale, and only when it moved enough to matter.
	if (MieScaleCurve && SkyAtmosphereComponent)
	{
		const float MieScale = MieScaleCurve->GetFloatValue(Hour);
		const bool bMoved = FMath::Abs(MieScale - LastAppliedMieScale) > FMath::Abs(MieScale) * MieScaleRelativeTolerance + 1.e-7f;
		if (bMoved)
		{
			SkyAtmosphereComponent->SetMieScatteringScale(MieScale);
			LastAppliedMieScale = MieScale;
		}
	}

	UpdatePostProcess(Hour);
}

float ADayNightManager::ComputeSunOrbitAngle(float Hour) const
{
	if (const UCurveFloat* RotationCurve = LightingProfile ? UsableCurve(LightingProfile->SunRotationCurve.Get()) : nullptr)
	{
		return RotationCurve->GetFloatValue(Hour);
	}

	// Fallback arc, used until art authors SunRotationCurve. Straight lines in time:
	//   sunrise -> sunset : 0 to 180 degrees (over the day)
	//   sunset -> sunrise : 180 to 360 degrees (over the night, which wraps past midnight)
	const float DayLength = FMath::Max(FallbackSunsetHour - FallbackSunriseHour, 0.1f);
	const float NightLength = HoursPerDay - DayLength;

	if (Hour >= FallbackSunriseHour && Hour <= FallbackSunsetHour)
	{
		return 180.f * (Hour - FallbackSunriseHour) / DayLength;
	}

	const float HoursSinceSunset = (Hour >= FallbackSunsetHour) ? (Hour - FallbackSunsetHour) : (Hour + HoursPerDay - FallbackSunsetHour);
	return 180.f + 180.f * HoursSinceSunset / FMath::Max(NightLength, 0.1f);
}

FVector ADayNightManager::ComputeOrbitDirection(float OrbitAngleDegrees) const
{
	// The sun moves on a circle. Angle 0 is the rising point on the horizon, 90 the top, 180 the setting point.
	//   East  = horizontal direction the sun rises from (SunPathAzimuthDegrees)
	//   Side  = horizontal direction 90 degrees to the left of East (in yaw)
	//   Top   = "up" of the circle, leaned toward Side by SunPathInclinationDegrees
	// The sun is at East * cos(angle) + Top * sin(angle). Both vectors are unit length and perpendicular,
	// so the result is a unit vector, and the horizon crossings stay at angle 0 and 180 whatever the tilt.
	const FVector East = FRotator(0.f, SunPathAzimuthDegrees, 0.f).Vector();
	const FVector Side = FRotator(0.f, SunPathAzimuthDegrees + 90.f, 0.f).Vector();

	const float TiltRad = FMath::DegreesToRadians(SunPathInclinationDegrees);
	const FVector Top = FVector::UpVector * FMath::Cos(TiltRad) + Side * FMath::Sin(TiltRad);

	const float AngleRad = FMath::DegreesToRadians(OrbitAngleDegrees);
	return East * FMath::Cos(AngleRad) + Top * FMath::Sin(AngleRad);
}

void ADayNightManager::UpdateDirectionalLight(UDirectionalLightComponent* Light, const FVector& DirectionToLight,
	const UCurveFloat* IntensityCurve, float MaxIntensity, float Hour) const
{
	if (!Light)
	{
		return;
	}

	// A directional light shines along its forward axis, so it looks away from the place it is seen at.
	Light->SetWorldRotation((-DirectionToLight).Rotation());

	if (IntensityCurve)
	{
		const float Multiplier = FMath::Max(0.f, IntensityCurve->GetFloatValue(Hour));
		Light->SetIntensity(Multiplier * MaxIntensity);

		// A light that adds nothing still pays for its shadow maps: hide it while it is off.
		// Visibility is touched only when it changes, because it rebuilds the light's render state.
		const bool bShouldBeVisible = Multiplier > HideLightBelowCurveValue;
		if (Light->IsVisible() != bShouldBeVisible)
		{
			Light->SetVisibility(bShouldBeVisible);
		}
	}
}

void ADayNightManager::UpdatePostProcess(float Hour) const
{
	if (!PostProcessVolume || !LightingProfile)
	{
		return;
	}

	const UDayNightLightingProfile* Profile = LightingProfile;
	FPostProcessSettings& Settings = PostProcessVolume->Settings;

	// Auto exposure method (optional).
	switch (Profile->ExposureMode)
	{
	case EDayNightExposureMode::Histogram:
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Histogram;
		break;
	case EDayNightExposureMode::Basic:
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = AEM_Basic;
		break;
	case EDayNightExposureMode::LeaveAsIs:
	default:
		break;
	}

	// Auto exposure range, in EV100. A lower EV100 is a brighter image, so Min is how much a dark scene may brighten
	// and Max is how much a bright scene may darken. The curves are offsets from the calibration value.
	const UCurveFloat* MinOffsetCurve = UsableCurve(Profile->ExposureMinOffsetCurve.Get());
	const UCurveFloat* MaxOffsetCurve = UsableCurve(Profile->ExposureMaxOffsetCurve.Get());

	float MinEV = 0.f;
	float MaxEV = 0.f;
	if (MinOffsetCurve)
	{
		MinEV = Profile->DayExposureEV100 + MinOffsetCurve->GetFloatValue(Hour);
	}
	if (MaxOffsetCurve)
	{
		MaxEV = Profile->DayExposureEV100 + MaxOffsetCurve->GetFloatValue(Hour);
	}
	if (MinOffsetCurve && MaxOffsetCurve && MaxEV < MinEV)
	{
		MaxEV = MinEV; // never invert the range
	}
	if (MinOffsetCurve)
	{
		Settings.bOverride_AutoExposureMinBrightness = true;
		Settings.AutoExposureMinBrightness = MinEV;
	}
	if (MaxOffsetCurve)
	{
		Settings.bOverride_AutoExposureMaxBrightness = true;
		Settings.AutoExposureMaxBrightness = MaxEV;
	}

	// Exposure compensation, on top of the exposure above.
	if (const UCurveFloat* BiasCurve = UsableCurve(Profile->ExposureBiasCurve.Get()))
	{
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = BiasCurve->GetFloatValue(Hour);
	}

	if (const UCurveFloat* SaturationCurve = UsableCurve(Profile->SaturationCurve.Get()))
	{
		const float Saturation = FMath::Max(0.f, SaturationCurve->GetFloatValue(Hour));
		Settings.bOverride_ColorSaturation = true;
		Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	}

	if (const UCurveFloat* TemperatureCurve = UsableCurve(Profile->WhiteTemperatureCurve.Get()))
	{
		Settings.bOverride_WhiteTemp = true;
		Settings.WhiteTemp = FMath::Clamp(TemperatureCurve->GetFloatValue(Hour), 1500.f, 15000.f);
	}
}
