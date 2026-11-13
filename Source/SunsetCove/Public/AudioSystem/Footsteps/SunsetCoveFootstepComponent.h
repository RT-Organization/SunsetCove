// SunsetCoveFootstepComponent.h
// SunsetCove - Audio System / Footsteps. The sound of the player's feet: a step every stride, the ground decides the sound.
//
// WHEN A STEP PLAYS
//   While the character moves ON THE GROUND (not falling, flying or swimming), the horizontal distance it covers is summed; every
//   stride (WalkStrideLength at walking speed up to RunStrideLength at running speed) one step plays. Running is louder than walking,
//   crouching is softer. A landing after a fall faster than LandingMinFallSpeed plays a landing sound. No animation is needed; if
//   one day the body animation should drive the steps, turn bAutoFootsteps off and call PlayFootstep from an anim notify.
//
// WHICH SOUND
//   1. THE SEA: if a sea exists (USunsetCoveSeaSubsystem) and its surface is above the feet by more than MinWaterDepth, the ground
//      is SeaWaterSurface (Water): splashing in the shallows, whatever is under the water. The sea itself has no collision.
//   2. Otherwise a short trace down from the feet returns the PHYSICAL MATERIAL of what is under them: the landscape layer (sand,
//      mud... through the layer's Phys Material) or a mesh (rocks, a wooden deck). Its Surface Type picks the row of FootstepSet.
//   3. Nothing found, or no row for that surface: the Default row (sand on this beach).
//   OnFootstep tells listeners (dust puffs, water splashes, footprints) where and on what the step happened.
//
// HOW IT IS HEARD
//   Through the CoreFramework audio subsystem in the set's Category (SFX by default), so the SFX and Master sliders apply.
//   bPlay2D (on for the player): flat, at a constant level, like your own feet. Off (NPC visitors later): at the feet, 3D, with the
//   attenuation of the sound asset.
//
// OWNER
//   An ACharacter (it reads the Character Movement and the capsule). ASunsetCoveCharacter creates it ("Footsteps").

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "SunsetCoveFootstepComponent.generated.h"

class ACharacter;
class USoundBase;
class USunsetCoveFootstepSet;
struct FSunsetCoveFootstepSurface;

/** A step (or a landing) was played. Surface is the ground kind; Location the point under the feet. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSunsetCoveFootstepSignature, TEnumAsByte<EPhysicalSurface>, Surface, FVector, Location, bool, bLanding);

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API USunsetCoveFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USunsetCoveFootstepComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Sounds
	// ------------------------------------------------------------------

	/** Which sounds each ground makes (DA_Footsteps). Without it no step plays (one warning). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TObjectPtr<USunsetCoveFootstepSet> FootstepSet;

	/** Steps play by themselves from the distance walked. Off = only PlayFootstep calls (anim notifies). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	bool bAutoFootsteps = true;

	/** Flat 2D sound (the local player's own feet). Off = 3D at the feet (other characters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	bool bPlay2D = true;

	// ------------------------------------------------------------------
	// Rhythm and loudness
	// ------------------------------------------------------------------

	/** Distance between two steps when walking (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "20.0", Units = "cm"))
	float WalkStrideLength = 150.0f;

	/** Distance between two steps when running (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "20.0", Units = "cm"))
	float RunStrideLength = 220.0f;

	/** Speed considered "walking" (cm/s): at or below it, walk stride and walk volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "1.0", Units = "cm/s"))
	float WalkSpeed = 250.0f;

	/** Speed considered "running" (cm/s): at or above it, run stride and run volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "1.0", Units = "cm/s"))
	float RunSpeed = 600.0f;

	/** Below this speed (cm/s) no steps are counted (turning on the spot, sliding to a stop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinSpeed = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float WalkVolume = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float RunVolume = 1.0f;

	/** Volume multiplier while crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Rhythm", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrouchVolumeScale = 0.45f;

	// ------------------------------------------------------------------
	// Landing
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Landing")
	bool bPlayLandingSound = true;

	/** A landing sound plays only after falling faster than this (cm/s): a hop down a step is just a step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Landing", meta = (ClampMin = "0.0", Units = "cm/s", EditCondition = "bPlayLandingSound"))
	float LandingMinFallSpeed = 350.0f;

	/** Fall speed (cm/s) at which the landing is at full volume; slower falls are softer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Landing", meta = (ClampMin = "1.0", Units = "cm/s", EditCondition = "bPlayLandingSound"))
	float LandingFullVolumeFallSpeed = 900.0f;

	// ------------------------------------------------------------------
	// Ground detection
	// ------------------------------------------------------------------

	/** How far below the feet the ground is searched (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Ground", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundTraceLength = 60.0f;

	/** Channel of the ground trace. The landscape and meshes block Visibility by default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Ground")
	TEnumAsByte<ETraceTypeQuery> GroundTraceChannel;

	/** The surface used when the feet are in the sea (Project Settings > Physics > Physical Surface "Water"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Ground")
	TEnumAsByte<EPhysicalSurface> SeaWaterSurface = SurfaceType5;

	/** The sea counts only when its surface is this far above the feet (cm): the edge of a wave does not splash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Ground", meta = (ClampMin = "0.0", Units = "cm"))
	float MinWaterDepth = 3.0f;

	/** Draws the ground trace and prints the surface of each step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps|Debug")
	bool bDrawDebug = false;

	// ------------------------------------------------------------------
	// Commands, queries, events
	// ------------------------------------------------------------------

	/** Plays one step (or a landing) on the ground under the feet now. VolumeScale multiplies the usual volume. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Footsteps")
	void PlayFootstep(bool bLanding = false, float VolumeScale = 1.0f);

	/** The ground kind under the feet at the last step. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Footsteps")
	TEnumAsByte<EPhysicalSurface> GetLastSurface() const { return LastSurface; }

	/** A step or a landing was played (for VFX: sand puffs, splashes, footprints). */
	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|Footsteps")
	FSunsetCoveFootstepSignature OnFootstep;

private:
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	/** The ground under the feet: surface kind and the point. */
	EPhysicalSurface DetectSurface(FVector& OutLocation) const;

	/** The bottom of the capsule (or the actor location for a non-character owner). */
	FVector GetFeetLocation() const;

	/** A random sound of List, never the one played last time for that list. */
	USoundBase* PickSound(const TArray<TObjectPtr<USoundBase>>& List, int32& InOutLastIndex) const;

	/** 0 at WalkSpeed or below, 1 at RunSpeed or above. */
	float GetRunAlpha(float Speed) const;

	TWeakObjectPtr<ACharacter> CharacterOwner;
	FVector LastLocation = FVector::ZeroVector;
	float DistanceSinceStep = 0.0f;
	float MaxFallSpeed = 0.0f;
	bool bHasLastLocation = false;
	bool bWarnedNoSet = false;
	TEnumAsByte<EPhysicalSurface> LastSurface = SurfaceType_Default;

	/** Last picked index per (surface, landing) so a sound never repeats twice in a row. */
	mutable TMap<int32, int32> LastPickedIndex;
};
