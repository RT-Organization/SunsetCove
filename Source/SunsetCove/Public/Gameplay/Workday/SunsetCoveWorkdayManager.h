// SunsetCoveWorkdayManager.h
// SunsetCove - Gameplay area (Workday). The first piece of the workday manager the GDD asks for: it links the
// Day/Night clock to the keeper's shift.
//
// WHAT THIS CLASS DOES (AND NOTHING ELSE, FOR NOW)
//   1. At BeginPlay it can start the shift (USunsetCoveWorkdaySubsystem) and let the clock run (ADayNightManager).
//   2. When the clock runs through ShiftEndHour it ends the shift (Miro: "SHIFT OVER"): spawners stop and visitors leave.
//   That keeps the one-way dependency of the Day/Night Cycle sheet: this manager LISTENS to the clock and CALLS its
//   control functions; the clock knows nothing about visitors.
//
// WHAT IT WILL DO LATER (GDD, "Day/Night Cycle + Gamemode: Giorno lavorativo")
//   SUNSETCOVE GAME LOGIC: start the day from the button near the barrier instead of BeginPlay, the closing warning
//   (18:00), closing the arrivals before the shift is over, the evening tidy-up, ResetDay after resting in the cave.
//
// SETUP (place one per gameplay level, next to BP_DayNightManager)
//   Defaults: start the shift and the clock at BeginPlay, shift over at 19:00 (the GDD closing hour).
//   With no Day/Night manager in the level only the shift start works; end it from the console
//   (SunsetCove.Workday.EndShift).
//
// HIERARCHY
//   AActor -> ASunsetCoveWorkdayManager (this class). Placed directly; no Blueprint needed until it gets visuals.
//
// NETWORK
//   Single player, like the Day/Night manager. The shift is decided where the game has authority.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SunsetCoveWorkdayManager.generated.h"

class ADayNightManager;

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveWorkdayManager : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveWorkdayManager();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	// ------------------------------------------------------------------
	// Configuration (Details panel of the placed instance)
	// ------------------------------------------------------------------

	/** Start the shift as soon as the level begins (until the barrier button exists). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Workday")
	bool bStartShiftOnBeginPlay = true;

	/** Let the Day/Night clock run when the shift starts (the clock waits paused at 07:00 otherwise). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Workday")
	bool bStartClockWithShift = true;

	/** End the shift when the clock runs through ShiftEndHour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Workday")
	bool bEndShiftAtHour = true;

	/** Hour of "shift over", 19.0 = 19:00 (GDD closing hour). A jump of the clock past it does not count, by design of the clock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Workday", meta = (EditCondition = "bEndShiftAtHour", ClampMin = "0.0", ClampMax = "24.0"))
	float ShiftEndHour = 19.f;

private:
	UFUNCTION()
	void HandleTimeReached(float TargetHour);

	/** The level's clock, found through UDayNightCycleSubsystem at BeginPlay. */
	TWeakObjectPtr<ADayNightManager> Clock;
};
