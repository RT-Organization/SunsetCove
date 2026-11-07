// PropInterface.h
// SunsetCove - Interfaces area. Marks an actor as a "prop": something the player can carry in the hands.
//
// WHAT THIS IS
//   The marker the Prop Inventory (Public/InventorySystem/PropInventorySystem/) looks for. An actor that implements
//   this interface IS a prop: it can be grabbed, carried, placed, thrown and dropped. An actor without it is ignored.
//   Sunbeds, umbrellas and every future carriable object implement it.
//
//   This interface lives OUTSIDE the inventory on purpose. The dependency is one way: the inventory knows the
//   interface, the interface never knows the inventory (no hands, no slots, no inventory types in here). Other
//   systems (visitors, rest spots, wind) can use the same marker without including anything of the inventory.
//
// HOW A PROP IMPLEMENTS IT
//   Blueprint:  Class Settings > Interfaces > Add > "Prop Interface". Every function below is optional: a prop that
//               overrides nothing still works, because the defaults are the permissive ones.
//   C++:        class AMyProp : public AActor, public IPropInterface { ... } and override the _Implementation functions.
//
// WHY THE FUNCTIONS ARE "BLOCKED" CHECKS AND NOT "CAN DO" CHECKS
//   A Blueprint that adds an interface but does not implement a function that returns a value returns the default
//   value of the type (false, zero). Every question below is phrased so that the default answer means "no objection":
//   a prop that implements nothing can be picked up and placed anywhere valid.
//
// CALLING IT FROM C++
//   Interface events must be called through the Execute_ functions (they work for Blueprint and C++ implementers):
//     if (Actor->Implements<UPropInterface>()) { IPropInterface::Execute_OnPropGrabbed(Actor, Holder); }
//
// PROP REQUIREMENTS (checked by the Prop Inventory, which logs a warning and refuses the grab if they are not met)
//   The ROOT component of the actor must be a primitive component (static mesh, box...) with Mobility = Movable and
//   simple collision, so the system can attach it to a hand and switch physics on when it is dropped or thrown.
//
// THE EASY WAY
//   ASunsetCoveProp (Public/Gameplay/Props/) implements this interface and builds all of the above by itself: a collision
//   box root, a skeletal mesh and the grip bone. Derive prop Blueprints from it instead of implementing this by hand.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "PropInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UPropInterface : public UInterface
{
	GENERATED_BODY()
};

class SUNSETCOVE_API IPropInterface
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// Questions the inventory asks the prop (default: no objection)
	// ------------------------------------------------------------------

	/** Return true to refuse the grab (broken, locked, in use by a visitor...). Picker is the actor that wants it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	bool IsPickupBlocked(AActor* Picker) const;
	virtual bool IsPickupBlocked_Implementation(AActor* Picker) const { return false; }

	/**
	 * Return true to refuse the placement at this world transform, on top of the checks of the inventory (distance, slope,
	 * obstacles). This is the hook for game rules such as "do not block the main path of the visitors".
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	bool IsPlacementBlocked(const FTransform& WorldTransform) const;
	virtual bool IsPlacementBlocked_Implementation(const FTransform& WorldTransform) const { return false; }

	/**
	 * Where the ROOT of the prop sits relative to the hand socket while it is carried (location, cm). The inventory snaps
	 * the root to the hand socket and then applies this relative location and the rotation below. ASunsetCoveProp computes
	 * it from its grip bone, so the grip lands exactly on the hand socket. Zero by default (root on the socket).
	 * bRightHand tells which hand carries it, for props with a different grip per hand.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	FVector GetPropHoldLocationOffset(bool bRightHand) const;
	virtual FVector GetPropHoldLocationOffset_Implementation(bool bRightHand) const { return FVector::ZeroVector; }

	/** The rotation that goes with GetPropHoldLocationOffset. Zero by default. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	FRotator GetPropHoldRotationOffset(bool bRightHand) const;
	virtual FRotator GetPropHoldRotationOffset_Implementation(bool bRightHand) const { return FRotator::ZeroRotator; }

	// ------------------------------------------------------------------
	// Notifications from the inventory (prop-side reactions: sound, particles, physics tweaks, rest spots...)
	// Holder is the actor that carries or carried the prop. All default to doing nothing.
	// ------------------------------------------------------------------

	/** The prop was attached to a hand. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	void OnPropGrabbed(AActor* Holder);
	virtual void OnPropGrabbed_Implementation(AActor* Holder) {}

	/** The prop was let fall from a hand (drop action). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	void OnPropDropped(AActor* Holder);
	virtual void OnPropDropped_Implementation(AActor* Holder) {}

	/** The prop was put down at the hologram position. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	void OnPropPlaced(AActor* Holder);
	virtual void OnPropPlaced_Implementation(AActor* Holder) {}

	/** The prop left the hand with a throw. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Prop")
	void OnPropThrown(AActor* Holder);
	virtual void OnPropThrown_Implementation(AActor* Holder) {}
};
