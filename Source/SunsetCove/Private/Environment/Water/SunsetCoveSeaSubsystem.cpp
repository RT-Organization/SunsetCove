// SunsetCoveSeaSubsystem.cpp
// SunsetCove - Environment / Water. See Public/Environment/Water/SunsetCoveSeaSubsystem.h.

#include "Environment/Water/SunsetCoveSeaSubsystem.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Environment/Water/SunsetCoveSea.h"

ASunsetCoveSea* USunsetCoveSeaSubsystem::GetSea(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const USunsetCoveSeaSubsystem* Subsystem = World ? World->GetSubsystem<USunsetCoveSeaSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetActiveSea() : nullptr;
}

void USunsetCoveSeaSubsystem::RegisterSea(ASunsetCoveSea* Sea)
{
	if (!Sea)
	{
		return;
	}

	ASunsetCoveSea* Current = ActiveSea.Get();
	if (Current && Current != Sea)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Sea: '%s' is a second sea in this world; '%s' stays the active one. Keep one sea per level."),
			*Sea->GetName(), *Current->GetName());
		return;
	}
	ActiveSea = Sea;
}

void USunsetCoveSeaSubsystem::UnregisterSea(ASunsetCoveSea* Sea)
{
	if (ActiveSea.Get() == Sea)
	{
		ActiveSea.Reset();
	}
}
