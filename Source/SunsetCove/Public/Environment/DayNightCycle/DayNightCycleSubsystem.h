// DayNightCycleSubsystem.h
// SunsetCove - Environment area (Day/Night Cycle). The phone book entry of the day/night manager.
//
// WHAT THIS IS FOR
//   Other systems (workday manager, HUD, NPC brain) need a way to reach the ADayNightManager of the level without
//   holding a hard reference to a placed actor, and without scanning the world for it. This subsystem is that
//   lookup. It holds one weak pointer and nothing else: no clock, no lighting, no events of its own.
//
// HOW TO USE IT
//   C++        ADayNightManager* Clock = UDayNightCycleSubsystem::GetDayNightManager(this);
//              if (Clock) { Clock->OnHourChanged.AddDynamic(this, &UMyClass::HandleHourChanged); }
//   Blueprint  "Get Day Night Manager" (needs no world pin, any actor or component works as the context).
//   The result is null in a level with no manager. Always check it.
//
// LIFETIME
//   One instance per world, created and destroyed by the engine. The manager registers itself in
//   PostInitializeComponents and unregisters in EndPlay, so the lookup works from any BeginPlay.
//   Only one manager per world is supported. A second one is reported in the log and ignored by this lookup.
//
// HIERARCHY
//   UWorldSubsystem -> UDayNightCycleSubsystem (this class)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DayNightCycleSubsystem.generated.h"

class ADayNightManager;

UCLASS()
class SUNSETCOVE_API UDayNightCycleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The active manager of this world, or null if the level has none. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle")
	ADayNightManager* GetManager() const;

	/** Shortcut for any object that lives in a world: the active manager, or null. */
	UFUNCTION(BlueprintPure, Category = "DayNightCycle", meta = (WorldContext = "WorldContextObject", DisplayName = "Get Day Night Manager"))
	static ADayNightManager* GetDayNightManager(const UObject* WorldContextObject);

	/** Called by ADayNightManager only. */
	void RegisterManager(ADayNightManager* NewManager);

	/** Called by ADayNightManager only. Ignored if the given manager is not the registered one. */
	void UnregisterManager(ADayNightManager* OldManager);

private:
	TWeakObjectPtr<ADayNightManager> Manager;
};
