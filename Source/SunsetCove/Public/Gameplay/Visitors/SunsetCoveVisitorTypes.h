// SunsetCoveVisitorTypes.h
// SunsetCove - Gameplay area (Visitors). Plain types shared by the visitor classes.

#pragma once

#include "CoreMinimal.h"
#include "SunsetCoveVisitorTypes.generated.h"

/**
 * Where a visitor is in its day at the cove. One value at a time, in this order for a normal visit.
 * The Miro board names are given so the code and the design talk about the same steps.
 */
UENUM(BlueprintType)
enum class ESunsetCoveVisitorPhase : uint8
{
	/** Spawned but not started yet (StartVisit has not been called). */
	Idle				UMETA(DisplayName = "Idle"),

	/** Walking up the sandy path to the barrier. Miro: "Approach Entrance". */
	ApproachingEntrance	UMETA(DisplayName = "Approaching Entrance"),

	/** Standing at the barrier. Miro: "Pay for Access or Borrowing Items and Services" (later; for now they just enter). */
	AtEntrance			UMETA(DisplayName = "At Entrance"),

	/** Walking to the rest spot it claimed. Miro: "Walk to desired rest position". */
	GoingToSpot			UMETA(DisplayName = "Going To Spot"),

	/** No free spot it can reach. It waits near where it is and asks again every few seconds. */
	WaitingForSpot		UMETA(DisplayName = "Waiting For Spot"),

	/** Sitting, lying or standing on its spot. Miro: "Rest (Sit or Lay)". */
	Resting				UMETA(DisplayName = "Resting"),

	/** Walking back down the path to the exit. Miro: "SHIFT OVER" then "Leave Cove". */
	Leaving				UMETA(DisplayName = "Leaving"),

	/** Reached the exit. The body is removed right after this phase is announced. */
	Left				UMETA(DisplayName = "Left")
};

/** Why a visitor leaves. Kept as a reason, so later systems (satisfaction, reputation) can tell the cases apart. */
UENUM(BlueprintType)
enum class ESunsetCoveVisitorLeaveReason : uint8
{
	/** The workday ended (Miro: "SHIFT OVER"). */
	ShiftOver			UMETA(DisplayName = "Shift Over"),

	/** It waited too long without finding a spot it could reach. */
	NoSpotAvailable		UMETA(DisplayName = "No Spot Available"),

	/** It could not walk anywhere useful (stuck recovery gave up). This is the GDD's "guided exit". */
	Stuck				UMETA(DisplayName = "Stuck"),

	/** Asked by game code (for example the player sends the visitor away). */
	Requested			UMETA(DisplayName = "Requested")
};
