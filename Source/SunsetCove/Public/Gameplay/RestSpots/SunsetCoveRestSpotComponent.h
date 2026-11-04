// SunsetCoveRestSpotComponent.h
// SunsetCove - Gameplay area (RestSpots). One place where one visitor can rest: a sunbed, a seat under an umbrella, a
// patch of sand.
//
// WHAT THIS CLASS IS
//   A marker that any prop Blueprint can carry (a sunbed has one, an umbrella two, a towel one). Its transform says where
//   the visitor's FEET go while resting (on a sunbed: on the mattress) and which way the visitor faces (its X axis).
//   It registers itself with USunsetCoveRestSpotSubsystem at BeginPlay; visitors ask that subsystem for free spots.
//
// CLAIM, OCCUPY, RELEASE (one visitor per spot)
//   A visitor CLAIMS a spot before walking there, so nobody else picks it on the way. It OCCUPIES the spot while
//   resting on it, and RELEASES it when it leaves. If the spot goes away while claimed (the prop is picked up,
//   disabled or destroyed), the claim is revoked and the claimant is told through the callback it gave to Claim.
//
// APPROACH POINT
//   Visitors walk to the approach point (ApproachOffset, relative to the spot, yaw only) and blend from there onto the
//   spot. A sunbed cuts its own footprint out of the navmesh, so its spot cannot be reached directly; the approach
//   point beside it can. Sand spots use a zero offset: the visitor rests where it stands.
//
// PROPS THAT MOVE (GRAB AND PLACE, LATER)
//   SUNSETCOVE GAME LOGIC: the grab-and-place system calls SetSpotEnabled(false) when the player picks a prop up and
//   SetSpotEnabled(true) once it is placed again. Disabling a claimed spot sends its visitor to another one.
//
// SETUP
//   Add the component to a prop Blueprint, put it on the seat at feet height, turn its X axis to where the visitor
//   should face, then set Spot Type (Cove.Spot.*) and Rest Activities (NPC.Activity.Rest.*). Each time a visitor uses
//   the spot it picks one of the activities at random.
//
// NETWORK
//   Claims exist where the AI runs (with authority). Nothing here replicates.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveRestSpotComponent.generated.h"

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API USunsetCoveRestSpotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USunsetCoveRestSpotComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration (on the prop Blueprint, or on a placed instance)
	// ------------------------------------------------------------------

	/** What kind of place this is (Cove.Spot.Sunbed, Cove.Spot.Umbrella, Cove.Spot.Sand). Visitors want some kinds more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (Categories = "Cove.Spot"))
	FGameplayTag SpotType;

	/** What a visitor does here (NPC.Activity.Rest.Sit, .Lay, .Stand). One is picked at random on every visit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (Categories = "NPC.Activity"))
	FGameplayTagContainer RestActivities;

	/** Where the visitor stands before sitting or lying down, relative to this component (yaw only, no scale). On the navmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot")
	FVector ApproachOffset = FVector(0.f, 100.f, 0.f);

	/** False: the spot starts disabled (for example a prop that is still in its delivery box). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot")
	bool bStartEnabled = true;

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/** World transform for the feet of the resting visitor: this component's location, upright, facing its X axis. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	FTransform GetRestFeetTransform() const;

	/** World location of the approach point (not projected on the navmesh). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	FVector GetApproachLocation() const;

	/** False while the spot is disabled (prop carried, broken...). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	bool IsSpotEnabled() const { return bSpotEnabled; }

	/** Enabled and claimed by nobody. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	bool IsFree() const { return bSpotEnabled && !ClaimedBy.IsValid(); }

	/** A visitor is resting on it right now. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	bool IsOccupied() const { return bOccupied && ClaimedBy.IsValid(); }

	/** The object that claimed the spot (a visitor component), or null. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	UObject* GetClaimant() const { return ClaimedBy.Get(); }

	/** One of RestActivities at random, or an empty tag if none is set. */
	FGameplayTag PickRestActivity() const;

	// ------------------------------------------------------------------
	// Commands
	// ------------------------------------------------------------------

	/** Turns the spot on or off. Turning a claimed spot off revokes the claim (its visitor goes elsewhere). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|RestSpot")
	void SetSpotEnabled(bool bEnabled);

	/**
	 * Reserves the spot for Claimant. OnLost runs if the claim is revoked later (spot disabled or removed); it is never
	 * called for a normal Release. Returns false if the spot is disabled or claimed by someone else.
	 */
	bool Claim(UObject* Claimant, FSimpleDelegate OnLost);

	/** Marks the spot as being rested on (true) or only reserved (false). Only the claimant can change it. */
	void SetOccupied(const UObject* Claimant, bool bNewOccupied);

	/** Frees the spot. Only the claimant can release it; anything else is ignored. */
	void Release(const UObject* Claimant);

private:
	/** Clears the claim and tells the claimant (a revoked claim, not a normal release). */
	void RevokeClaim();

	/** Who reserved the spot. Weak: a destroyed visitor frees the spot by itself. */
	TWeakObjectPtr<UObject> ClaimedBy;

	/** Told when the claim is revoked. Set by Claim, cleared by Release. */
	FSimpleDelegate OnClaimLost;

	bool bOccupied = false;
	bool bSpotEnabled = true;
	bool bRegistered = false;
};
