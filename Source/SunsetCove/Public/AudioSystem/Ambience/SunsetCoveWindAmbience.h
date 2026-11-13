// SunsetCoveWindAmbience.h
// SunsetCove - Audio System / Ambience. The wind that always surrounds the player, louder by the sea, quiet under the cliffs.
//
// WHAT IT DOES
//   Place ONE in a gameplay level (not in the main menu). It plays the wind loop (cue Audio.Cue.Ambient.Wind, Ambient category)
//   as a flat 2D sound, so it is "with the player" everywhere, and changes its volume a few times per second from two facts
//   about the listener:
//     1. SHORE   how far the shoreline is (asked to the sea through USunsetCoveSeaSubsystem). At the water: ShoreVolume.
//                Beyond ShoreFarDistance: InlandVolume. Smooth in between. A level without a sea uses InlandVolume.
//     2. SHELTER how enclosed the listener is: ShelterTraceCount horizontal rays around the ear, ShelterTraceLength long. Each ray
//                that hits something counts more the closer the hit (a cliff at 2 m shelters, one at 18 m barely). The average is
//                0 (open beach) .. 1 (walled in by the rocks of the cove). It needs no volumes to place: the landscape and rocks
//                of the artist shelter by themselves (they block the Visibility channel like any mesh).
//   Volume = lerp(InlandVolume, ShoreVolume, Shore) * lerp(1, ShelteredVolumeScale, Shelter), smoothed over VolumeSmoothSeconds.
//   The overall loudness is the cue's Volume in DA_AudioConfig (keep it low: the wind is a detail) and the player's Ambient slider.
//
// WHY AN ACTOR AND NOT A CHARACTER COMPONENT
//   The wind belongs to the place (a cave level has none, an indoor level another one), not to the player: levels choose it
//   by placing it, and the character stays free of ambience code.
//
// NETWORK
//   Single player, local presentation.

#pragma once

#include "CoreMinimal.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveWindAmbience.generated.h"

UCLASS(Blueprintable, ClassGroup = (SunsetCove), meta = (DisplayName = "Sunset Cove Wind Ambience"))
class SUNSETCOVE_API ASunsetCoveWindAmbience : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveWindAmbience();

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** The wind loop (Ambient category, looping, 2D): Audio.Cue.Ambient.Wind (set on BP_WindAmbience). Empty = no wind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (Categories = "Audio.Cue"))
	FGameplayTag WindCue;

	/** Seconds between two volume updates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (ClampMin = "0.02", Units = "s"))
	float UpdateInterval = 0.15f;

	/** Seconds the volume takes to follow a change (walking from the beach into a sheltered corner). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (ClampMin = "0.0", Units = "s"))
	float VolumeSmoothSeconds = 1.5f;

	/** Seconds of the fade-in at the start of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind", meta = (ClampMin = "0.0", Units = "s"))
	float StartFadeSeconds = 3.0f;

	// ---- Shore ----

	/** Volume multiplier at the water's edge (1 = the cue's volume). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shore", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ShoreVolume = 1.0f;

	/** Volume multiplier far from the sea. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shore", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float InlandVolume = 0.35f;

	/** Closer than this to the shoreline (cm): full ShoreVolume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shore", meta = (ClampMin = "0.0", Units = "cm"))
	float ShoreNearDistance = 500.0f;

	/** Farther than this from the shoreline (cm): InlandVolume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shore", meta = (ClampMin = "0.0", Units = "cm"))
	float ShoreFarDistance = 4000.0f;

	// ---- Shelter (cliffs and rocks) ----

	/** Measure how enclosed the listener is (rays around the ear). Off = only the shore distance counts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shelter")
	bool bUseShelter = true;

	/** Rays cast around the listener, evenly spread on the horizontal circle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shelter", meta = (ClampMin = "3", ClampMax = "32", EditCondition = "bUseShelter"))
	int32 ShelterTraceCount = 8;

	/** Length of each ray (cm). A wall farther than this does not shelter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shelter", meta = (ClampMin = "100.0", Units = "cm", EditCondition = "bUseShelter"))
	float ShelterTraceLength = 1500.0f;

	/** Rays start this far above the listener (cm), so low sand dunes and props do not count as shelter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shelter", meta = (Units = "cm", EditCondition = "bUseShelter"))
	float ShelterTraceHeightOffset = 50.0f;

	/** Channel the rays use. Rocks, cliffs and the landscape must block it (Visibility: they do by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shelter", meta = (EditCondition = "bUseShelter"))
	TEnumAsByte<ETraceTypeQuery> ShelterTraceChannel;

	/** Volume multiplier when fully sheltered. 0 = the wind disappears among the high rocks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Shelter", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bUseShelter"))
	float ShelteredVolumeScale = 0.05f;

	/** Draws the shelter rays (green free, red blocked) and prints the values. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wind|Debug")
	bool bDrawDebug = false;

	// ---- Queries ----

	/** Current volume multiplier of the wind (0..2), after smoothing. Other systems can use it as "wind strength" (foliage, foam). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Wind")
	float GetWindStrength() const { return CurrentVolume; }

	/** Last measured shelter, 0 (open) .. 1 (walled in). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Wind")
	float GetShelter() const { return LastShelter; }

private:
	void UpdateWind();

	/** 0 far inland .. 1 at the shoreline. 0 without a sea. */
	float ComputeShoreFactor(const FVector& Listener) const;

	/** 0 open .. 1 enclosed. */
	float ComputeShelter(const FVector& Listener) const;

	bool GetListener(FVector& OutLocation) const;

	FAudioHandle WindHandle;
	FTimerHandle UpdateTimer;
	float CurrentVolume = 0.0f;
	float LastShelter = 0.0f;
	float StartTime = 0.0f;
};
