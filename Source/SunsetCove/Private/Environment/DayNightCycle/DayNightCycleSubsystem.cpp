// DayNightCycleSubsystem.cpp
// SunsetCove - Environment area. See Public/Environment/DayNightCycle/DayNightCycleSubsystem.h for the role of this class.

#include "Environment/DayNightCycle/DayNightCycleSubsystem.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Environment/DayNightCycle/DayNightManager.h"

ADayNightManager* UDayNightCycleSubsystem::GetManager() const
{
	return Manager.Get();
}

ADayNightManager* UDayNightCycleSubsystem::GetDayNightManager(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UDayNightCycleSubsystem* Subsystem = World ? World->GetSubsystem<UDayNightCycleSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetManager() : nullptr;
}

void UDayNightCycleSubsystem::RegisterManager(ADayNightManager* NewManager)
{
	if (!NewManager)
	{
		return;
	}

	if (Manager.IsValid() && Manager.Get() != NewManager)
	{
		UE_LOG(LogSunsetCove, Warning,
			TEXT("DayNightCycleSubsystem: '%s' is already the day/night manager of this world. '%s' is ignored by the lookup. Keep one ADayNightManager per level."),
			*GetNameSafe(Manager.Get()), *GetNameSafe(NewManager));
		return;
	}

	Manager = NewManager;
}

void UDayNightCycleSubsystem::UnregisterManager(ADayNightManager* OldManager)
{
	if (Manager.Get() == OldManager)
	{
		Manager.Reset();
	}
}
