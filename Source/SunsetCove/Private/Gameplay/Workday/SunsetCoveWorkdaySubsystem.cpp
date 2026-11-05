// SunsetCoveWorkdaySubsystem.cpp
// SunsetCove - Gameplay area (Workday). See Public/Gameplay/Workday/SunsetCoveWorkdaySubsystem.h for the role of this class.

#include "Gameplay/Workday/SunsetCoveWorkdaySubsystem.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	FAutoConsoleCommandWithWorld StartShiftCommand(
		TEXT("SunsetCove.Workday.StartShift"),
		TEXT("Starts the keeper's shift: visitor spawners start sending visitors."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(World))
			{
				Workday->StartShift();
			}
		}),
		ECVF_Cheat);

	FAutoConsoleCommandWithWorld EndShiftCommand(
		TEXT("SunsetCove.Workday.EndShift"),
		TEXT("Shift over: visitor spawners stop and every visitor walks out of the cove."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(World))
			{
				Workday->EndShift();
			}
		}),
		ECVF_Cheat);
}

USunsetCoveWorkdaySubsystem* USunsetCoveWorkdaySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<USunsetCoveWorkdaySubsystem>() : nullptr;
}

bool USunsetCoveWorkdaySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USunsetCoveWorkdaySubsystem::StartShift()
{
	if (bShiftActive)
	{
		return;
	}

	bShiftActive = true;
	UE_LOG(LogSunsetCove, Log, TEXT("Workday: shift started."));
	OnShiftStarted.Broadcast();
}

void USunsetCoveWorkdaySubsystem::EndShift()
{
	if (!bShiftActive)
	{
		return;
	}

	bShiftActive = false;
	UE_LOG(LogSunsetCove, Log, TEXT("Workday: shift over, visitors leave the cove."));
	OnShiftEnded.Broadcast();
}
