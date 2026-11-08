// PropInventoryTypes.h
// SunsetCove - Inventory System / Prop Inventory. The enums, the slot struct and the delegate signatures of the system.
//
// Kept apart from the component so that UI and animation code can include the types without including the component.

#pragma once

#include "CoreMinimal.h"
#include "Core/Utils/SunsetCoveActionCommit.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "PropInventoryTypes.generated.h"

/** The two hands. Each hand holds at most one prop (GDD: 2 slots, one per hand). */
UENUM(BlueprintType)
enum class EPropHand : uint8
{
	Right	UMETA(DisplayName = "Right Hand"),
	Left	UMETA(DisplayName = "Left Hand")
};

/** What the Prop Inventory is doing. Only Idle accepts a new Place or Throw. */
UENUM(BlueprintType)
enum class EPropInventoryState : uint8
{
	/** Nothing in progress. The hands may be empty or full. */
	Idle			UMETA(DisplayName = "Idle"),
	/** The place action is held: the hologram follows the view and rotates with the rotate action. */
	Placing			UMETA(DisplayName = "Placing"),
	/** The throw action is held: the power bar charges. */
	ChargingThrow	UMETA(DisplayName = "Charging Throw")
};

/** The four things the player can do with a prop. Carried by OnPropAction, the hook for the one-shot animations. */
UENUM(BlueprintType)
enum class EPropAction : uint8
{
	Grab	UMETA(DisplayName = "Grab"),
	Place	UMETA(DisplayName = "Place"),
	Throw	UMETA(DisplayName = "Throw"),
	Drop	UMETA(DisplayName = "Drop")
};

/** Which mesh of the character carries the hand sockets. */
UENUM(BlueprintType)
enum class EPropHandMeshSource : uint8
{
	/** The first-person arms (FirstPersonMesh of the framework character). The default for this first-person game. */
	FirstPersonMesh	UMETA(DisplayName = "First Person Arms"),
	/** The third-person body mesh (GetMesh()). */
	BodyMesh		UMETA(DisplayName = "Body Mesh")
};

/** One hand: the prop it holds and what the prop looked like before it was grabbed. */
USTRUCT()
struct FPropSlot
{
	GENERATED_BODY()

	/** The carried prop, or null when the hand is empty. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> Prop = nullptr;

	/** Grab counter at the time of the grab. The highest number among the full hands is the "active" prop. */
	UPROPERTY(Transient)
	int32 GrabSequence = 0;

	/** Collision mode of the prop's root before it was grabbed, restored when the prop is placed. */
	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> SavedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	void Reset()
	{
		Prop = nullptr;
		GrabSequence = 0;
		SavedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	}
};

// ----------------------------------------------------------------------
// Delegate signatures (all BlueprintAssignable on UPropInventoryComponent)
// ----------------------------------------------------------------------

/** The state of the inventory changed (Idle, Placing, ChargingThrow). Show or hide the charge bar and the placement UI here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPropInventoryStateChangedSignature, EPropInventoryState, NewState, EPropInventoryState, OldState);

/**
 * An action took PHYSICAL effect: the prop is now attached (Grab), put down (Place), or left the hand (Throw, Drop). With the
 * default commit timing that is the instant the player acted; with an ActionBeginHook it can be a little later (the logical state,
 * the slots and OnSlotsChanged have already changed by then). To START an animation use the ActionBeginHook, not this.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPropActionSignature, EPropAction, Action, EPropHand, Hand, AActor*, Prop);

/**
 * C++ hook, asked when an action has been ACCEPTED (the slots already changed) and BEFORE its physical effect (attach, detach, launch).
 * The listener starts its animation and answers with the commit timing of that action. See Core/Utils/SunsetCoveActionCommit.h.
 */
DECLARE_DELEGATE_RetVal_ThreeParams(FActionCommitTiming, FPropActionBeginHook, EPropAction, EPropHand, AActor*);

/** The hologram changed between valid (can be placed here) and invalid. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPropPlacementValidityChangedSignature, bool, bIsValid);

/** The throw charge changed, 0 to 1. Fires every frame while charging, and once with 0 when the charge ends. Drive the charge bar with it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPropThrowChargeChangedSignature, float, Charge);

/** A hand was filled or emptied. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPropSlotsChangedSignature);
