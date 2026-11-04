// SunsetCoveWorkdayManager.cpp
// SunsetCove - Gameplay area (Workday). See Public/Gameplay/Workday/SunsetCoveWorkdayManager.h for the role of this class.

#include "Gameplay/Workday/SunsetCoveWorkdayManager.h"

#include "Components/SceneComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Environment/DayNightCycle/DayNightCycleSubsystem.h"
#include "Environment/DayNightCycle/DayNightManager.h"
#include "Gameplay/Workday/SunsetCoveWorkdaySubsystem.h"

namespace
{
	/** Two hours closer than this are the same hour (the clock uses the same tolerance for its targets). */
	constexpr float HourTolerance = 1.e-3f;
}

ASunsetCoveWorkdayManager::ASunsetCoveWorkdayManager()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
#if WITH_EDITORONLY_DATA
	Root->bVisualizeComponent = true;
#endif
}

void ASunsetCoveWorkdayManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this);
	if (!Workday)
	{
		return;
	}

	// The clock registers itself in PostInitializeComponents, so it can be found from any BeginPlay.
	ADayNightManager* DayNight = UDayNightCycleSubsystem::GetDayNightManager(this);
	Clock = DayNight;

	if (bEndShiftAtHour)
	{
		if (DayNight)
		{
			DayNight->RegisterTimeTarget(ShiftEndHour);
			DayNight->OnTimeReached.AddUniqueDynamic(this, &ASunsetCoveWorkdayManager::HandleTimeReached);
		}
		else
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("WorkdayManager '%s': no Day/Night manager in this level, so the shift never ends on its own. Place BP_DayNightManager, or use SunsetCove.Workday.EndShift."),
				*GetNameSafe(this));
		}
	}

	if (bStartShiftOnBeginPlay)
	{
		Workday->StartShift();

		if (bStartClockWithShift && DayNight)
		{
			DayNight->StartTime();
		}
	}

	// SUNSETCOVE GAME LOGIC: OnDayReset (sleeping in the cave) starts the next day; the closing warning (18:00) is
	// another RegisterTimeTarget handled in HandleTimeReached.
}

void ASunsetCoveWorkdayManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The time target stays registered: the clock merges equal hours, so removing it could silence another system
	// that asked for the same hour. A target nobody listens to costs nothing.
	if (ADayNightManager* DayNight = Clock.Get())
	{
		DayNight->OnTimeReached.RemoveDynamic(this, &ASunsetCoveWorkdayManager::HandleTimeReached);
	}
	Clock.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASunsetCoveWorkdayManager::HandleTimeReached(float TargetHour)
{
	// Other systems register their own hours on the same clock: react to ours only.
	if (!bEndShiftAtHour || !FMath::IsNearlyEqual(FMath::Fmod(TargetHour, 24.f), FMath::Fmod(ShiftEndHour, 24.f), HourTolerance))
	{
		return;
	}

	if (USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this))
	{
		Workday->EndShift();
	}
}
