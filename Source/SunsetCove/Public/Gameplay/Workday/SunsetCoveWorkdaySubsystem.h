// SunsetCoveWorkdaySubsystem.h
// SunsetCove - Gameplay area (Workday). The keeper's shift: "the cove is open" and "shift over".
//
// WHAT THIS CLASS IS
//   One flag and two events. While the shift is active visitors arrive; when it ends (Miro: "SHIFT OVER") the
//   spawners stop and every visitor walks home. This class does not decide WHEN that happens. Today the decision comes
//   from ASunsetCoveWorkdayManager (from the Day/Night clock) or from the console; later from the barrier, the GameMode
//   or the player. Everything that reacts to the shift listens here, so the source can change without touching them.
//
// WHY A SUBSYSTEM
//   Spawners, visitors and (later) the HUD find it without a reference to a placed actor, and the console commands
//   work in any level, even one with no workday manager.
//
// HOW TO USE IT
//   C++        USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this);
//              Workday->OnShiftEnded.AddDynamic(this, &UMyClass::HandleShiftEnded);
//   Blueprint  Get World Subsystem (SunsetCove Workday Subsystem), then bind On Shift Ended.
//   Console    SunsetCove.Workday.StartShift / SunsetCove.Workday.EndShift (also in Play In Editor).
//
// LIFETIME
//   One per game world (Game and Play In Editor), created and destroyed by the engine. The shift starts inactive.
//
// HIERARCHY
//   UWorldSubsystem -> USunsetCoveWorkdaySubsystem (this class)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SunsetCoveWorkdaySubsystem.generated.h"

/** The shift started or ended. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSunsetCoveShiftEventSignature);

UCLASS()
class SUNSETCOVE_API USunsetCoveWorkdaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the world WorldContextObject lives in, or null outside a game world. */
	static USunsetCoveWorkdaySubsystem* Get(const UObject* WorldContextObject);

	//~ Begin UWorldSubsystem Interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem Interface

	/** Opens the cove: visitors start arriving. Fires OnShiftStarted if the shift was not active. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Workday")
	void StartShift();

	/** Shift over: arrivals stop and every visitor leaves. Fires OnShiftEnded if the shift was active. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Workday")
	void EndShift();

	UFUNCTION(BlueprintPure, Category = "SunsetCove|Workday")
	bool IsShiftActive() const { return bShiftActive; }

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|Workday")
	FSunsetCoveShiftEventSignature OnShiftStarted;

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|Workday")
	FSunsetCoveShiftEventSignature OnShiftEnded;

private:
	bool bShiftActive = false;
};
