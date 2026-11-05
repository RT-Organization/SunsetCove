// SunsetCoveRestArea.h
// SunsetCove - Gameplay area (RestSpots). A box of free beach that fills itself with sand rest spots.
//
// WHAT THIS CLASS IS
//   The free part of the beach, for visitors who bring their own towel (Miro: "Walk to desired rest position", the
//   branch without sunbed and umbrella). At BeginPlay it lays a loose grid of USunsetCoveRestSpotComponent over the
//   navmesh inside its box: every Spacing centimetres, shifted at random so it does not look like a grid, each spot
//   facing FacingArrow (toward the sea) give or take FacingJitter. Spots that would sit on top of a prop (sunbed,
//   umbrella pole, rock) are skipped.
//
// WHY GENERATED SPOTS, NOT FREE RANDOM POINTS
//   Every rest place, fixed or generated, is the same component, so claiming, personal space, debug drawing and every
//   later rule (near the water, quiet) work once for all of them. Spots are made once; they are checked again when a
//   visitor claims one (a prop placed later carves the navmesh, and the visitor then fails to reach that spot and picks
//   another).
//
// SETUP (place it in the level)
//   Scale the Area box over the free sand (keep it off the sunbed rows and the entrance), turn the actor so FacingArrow
//   points at the water, and pick the Spot Type and Rest Activities. The navmesh must cover the box.
//
// NETWORK
//   Spots are generated where the game has authority, like the AI that uses them.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveRestArea.generated.h"

class UArrowComponent;
class UBoxComponent;
class USunsetCoveRestSpotComponent;

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveRestArea : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveRestArea();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** The spots made at BeginPlay. Empty before that, and in the editor. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|RestSpot")
	int32 GetGeneratedSpotCount() const { return GeneratedSpots.Num(); }

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** The free sand. Only its extent matters (it has no collision). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot")
	TObjectPtr<UBoxComponent> Area;

#if WITH_EDITORONLY_DATA
	/** Editor only: where visitors resting here look (turn the actor so it points at the water). */
	UPROPERTY(VisibleAnywhere, Category = "SunsetCove|RestSpot")
	TObjectPtr<UArrowComponent> FacingArrow;
#endif

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Kind of the generated spots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (Categories = "Cove.Spot"))
	FGameplayTag SpotType;

	/** What visitors do on these spots (one picked at random per visit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (Categories = "NPC.Activity"))
	FGameplayTagContainer RestActivities;

	/** Distance between two neighbouring spots, before the random shift. Around 3-4 m feels like a real beach. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (ClampMin = "100.0", Units = "cm"))
	float Spacing = 350.f;

	/** Random shift of each spot, as a fraction of Spacing (0 = perfect grid, 0.4 = loose). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (ClampMin = "0.0", ClampMax = "0.45"))
	float Jitter = 0.35f;

	/** Random turn of each spot around FacingArrow, in degrees, both ways. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "deg"))
	float FacingJitter = 35.f;

	/** Free radius around a spot: no prop, rock or wall closer than this (checked against static and dynamic geometry). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (ClampMin = "0.0", Units = "cm"))
	float Clearance = 90.f;

	/** Safety cap, so a huge box cannot create thousands of components. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SunsetCove|RestSpot", meta = (ClampMin = "1"))
	int32 MaxSpots = 150;

private:
	/** Builds the spots. Returns false when the navmesh is not there yet (it is retried a few times). */
	bool GenerateSpots();
	void RetryGenerate();

	UPROPERTY(Transient)
	TArray<TObjectPtr<USunsetCoveRestSpotComponent>> GeneratedSpots;

	FTimerHandle RetryTimer;
	int32 RetriesLeft = 0;
};
