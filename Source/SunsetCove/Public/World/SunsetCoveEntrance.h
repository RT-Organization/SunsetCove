// SunsetCoveEntrance.h
// SunsetCove - World area. The barrier ("sbarra") where the sandy path meets the beach.
//
// WHAT THIS CLASS IS
//   The point every visitor walks to first (Miro: "Approach Entrance"), on the path side of the barrier. Visitors
//   stop around ApproachPoint for a moment and then go into the cove. Leaving visitors walk back down the same path
//   (the GDD wants one shared path for entering and leaving), so they pass here again on their way out.
//
// WHAT IT WILL DO LATER (not in this slice)
//   SUNSETCOVE GAME LOGIC: the barrier the player opens and closes, the queue, paying for access and borrowing items
//   (Miro: "Pay for Access or Borrowing Items and Services"). Visitors already stop here, so those features plug into
//   USunsetCoveVisitorComponent's "At Entrance" phase without moving anything.
//
// SETUP (place it in the level)
//   Put the actor on the barrier line, the arrow pointing into the cove, and move ApproachPoint onto the path just
//   outside the barrier. ApproachRadius spreads arriving visitors so they do not all aim at one point.
//
// HIERARCHY
//   AActor -> ASunsetCoveEntrance (this class). Placed directly; a Blueprint child only when the barrier gets a mesh.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SunsetCoveEntrance.generated.h"

class UArrowComponent;
class USceneComponent;
class USunsetCoveRadiusRingComponent;

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveEntrance : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveEntrance();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	//~ End AActor Interface

	/** World location where visitors stop before going in. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Entrance")
	FVector GetApproachLocation() const;

	/**
	 * A point near ApproachPoint, within ApproachRadius and on the navmesh, so a group of visitors does not fight for the
	 * same spot. Falls back to ApproachPoint itself when the navmesh cannot be reached.
	 */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Entrance")
	FVector PickApproachLocation() const;

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Entrance")
	TObjectPtr<USceneComponent> Root;

	/** Where visitors stop, on the path side of the barrier. Move it in the viewport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Entrance")
	TObjectPtr<USceneComponent> ApproachPoint;

#if WITH_EDITORONLY_DATA
	/** Editor only: points into the cove. */
	UPROPERTY(VisibleAnywhere, Category = "SunsetCove|Entrance")
	TObjectPtr<UArrowComponent> InwardArrow;

	/** Editor only: shows ApproachRadius around ApproachPoint. */
	UPROPERTY(VisibleAnywhere, Category = "SunsetCove|Entrance")
	TObjectPtr<USunsetCoveRadiusRingComponent> ApproachRing;
#endif

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Arriving visitors pick a point this far from ApproachPoint at most. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|Entrance", meta = (ClampMin = "0.0", Units = "cm"))
	float ApproachRadius = 120.f;
};
