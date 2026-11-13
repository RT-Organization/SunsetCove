// SunsetCoveSea.h
// SunsetCove - Environment / Water. The sea in front of the cove: surface, waves, tide, shoreline and the sound of the waves.
//
// WHAT THIS CLASS IS (a base to grow, see Docs/Environment/Sea_Architecture.md)
//   One actor per level, placed in front of the beach. It is the single source of truth of "where is the water":
//     * the WAVES (a sum of Gerstner waves, Environment/Water/SunsetCoveWaveTypes.h) and the TIDE (the whole sea slowly rising and
//       falling), computed in C++ every frame and handed to the sea material, so the drawn water and gameplay queries agree;
//     * the SHORELINE: a spline the level designer draws where the water meets the sand. It answers "how far is the shore"
//       (wind ambience, visitors) and guides the sound of the waves;
//     * the SOUND OF THE WAVES: one looping cue (Audio.Cue.Ambient.ShoreWaves) on an emitter that slides along the shoreline to the
//       point closest to the listener. Walking along the long beach the waves stay beside you; walking inland they fade with the
//       attenuation of the cue (a "line" sound source made of one moving point, the standard technique for rivers and shores).
//
// COMPONENTS
//   SeaRoot           the actor origin = the still water level (sea level without tide).
//   SurfaceMesh       the water surface. Default: the engine plane scaled to SurfaceSize (flat, for blocking out). For real waves
//                     use a finely subdivided plane (see the setup guide) and the sea material (M_Sea / MI_Sea).
//   Shoreline         spline along the waterline. Edit its points in the level (select the actor, click the spline points).
//   ShoreAudioAnchor  moved at runtime; the waves sound is attached to it.
//
// THE MATERIAL CONTRACT (parameter names, set every frame on a dynamic instance of the surface material and, if given, on the
// Material Parameter Collection): SeaTime, SeaTideOffset, SeaLevel, SeaWaveA0..3, SeaWaveB0..3. Formula in SunsetCoveWaveTypes.h.
// A material without these parameters still works (it just does not move).
//
// QUERIES (for gameplay)  GetWaterHeightAt, IsUnderwater, GetDistanceToShore, GetClosestShorePoint, GetSeaLevel.
// Other systems reach the sea through USunsetCoveSeaSubsystem::GetSea(this).
//
// EDITOR
//   With Animate In Editor on, the waves move in the editor viewport too (same code), so the material can be tuned without Play.
//
// NETWORK
//   Single player. Everything is local presentation computed from the game time.

#pragma once

#include "CoreMinimal.h"
#include "Environment/Water/SunsetCoveWaveTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AudioSystem/Core/AudioTypes.h"
#include "Sound/SoundAttenuation.h"
#include "SunsetCoveSea.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, ClassGroup = (SunsetCove), meta = (DisplayName = "Sunset Cove Sea"))
class SUNSETCOVE_API ASunsetCoveSea : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveSea();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return bAnimateInEditor; }
	//~ End AActor Interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea|Components")
	TObjectPtr<USceneComponent> SeaRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea|Components")
	TObjectPtr<UStaticMeshComponent> SurfaceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea|Components")
	TObjectPtr<USplineComponent> Shoreline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sea|Components")
	TObjectPtr<USceneComponent> ShoreAudioAnchor;

	// ------------------------------------------------------------------
	// Surface
	// ------------------------------------------------------------------

	/** The sea material (Single Layer Water shading model, see the setup guide). A dynamic instance of it is driven every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Surface")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Size of the water surface (cm). The surface mesh is scaled to it, assuming a 100 x 100 cm mesh (engine Plane, or the setup guide's SM_SeaPlane). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Surface", meta = (Units = "cm"))
	FVector2D SurfaceSize = FVector2D(100000.0, 100000.0);

	/** Optional Material Parameter Collection (MPC_Sea) that receives the same values, for other materials (wet sand, foam on rocks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Surface")
	TObjectPtr<UMaterialParameterCollection> ParameterCollection;

	// ------------------------------------------------------------------
	// Waves and tide
	// ------------------------------------------------------------------

	/** The waves (the first 4 are used). Defaults: a slow swell coming in, a crossing swell, a short chop. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Waves", meta = (TitleProperty = "Wavelength"))
	TArray<FSunsetCoveGerstnerWave> Waves;

	/** Speed of the whole sea animation. 1 = real speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Waves", meta = (ClampMin = "0.0", UIMax = "3.0"))
	float WaveTimeScale = 1.0f;

	/** How far the whole sea rises above and sinks below its still level (cm). 0 = no tide. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Tide", meta = (ClampMin = "0.0", Units = "cm"))
	float TideAmplitude = 10.0f;

	/** Seconds of one full tide cycle (up and down). Real tides take 12 h; a few minutes reads as "the sea breathes". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Tide", meta = (ClampMin = "1.0", Units = "s"))
	float TidePeriodSeconds = 240.0f;

	// ------------------------------------------------------------------
	// Shore audio
	// ------------------------------------------------------------------

	/** Looping sound of the waves breaking (Ambient category): Audio.Cue.Ambient.ShoreWaves (set on BP_Sea). Empty = no sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Shore Audio", meta = (Categories = "Audio.Cue"))
	FGameplayTag ShoreWavesCue;

	/** False: no waves sound (a level with its own ambience). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Shore Audio")
	bool bPlayShoreAudio = true;

	/** Height of the waves sound above the sea level (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Shore Audio", meta = (Units = "cm"))
	float ShoreAudioHeight = 30.0f;

	/** How fast the emitter slides along the shore after the listener (higher = snappier). A jump longer than 30 m snaps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Shore Audio", meta = (ClampMin = "0.1"))
	float ShoreAudioFollowSpeed = 4.0f;

	/**
	 * How the waves fade with distance, used when the cue (Attenuation Override) and the sound asset have no attenuation of their
	 * own. Defaults (a 50 m cove): full volume within 5 m of the waterline, fading out over 45 m, enveloping within 12 m, duller far
	 * away. An ATT_ShoreWaves asset on the cue replaces these.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Shore Audio")
	FSoundAttenuationSettings ShoreAttenuation;

	// ------------------------------------------------------------------
	// Editor and debug
	// ------------------------------------------------------------------

	/** Animate the waves in the editor viewport (no Play needed to tune the material). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Editor")
	bool bAnimateInEditor = true;

	/** Draws the shore audio emitter and its link to the listener. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sea|Debug")
	bool bDrawDebug = false;

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/** World Z of the still water now: the actor height plus the tide. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	float GetSeaLevel() const { return GetActorLocation().Z + TideOffset; }

	/** World Z of the wavy surface right above / below Location (waves and tide included). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	float GetWaterHeightAt(const FVector& Location) const;

	/** True if Location is above / below the water area (the Surface Size rectangle around the actor). Outside it there is no sea. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	bool IsInsideSeaArea(const FVector& Location) const;

	/** True if Location is inside the sea area and below the wavy surface by more than Margin cm. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	bool IsUnderwater(const FVector& Location, float Margin = 0.0f) const;

	/** Point of the shoreline closest to Location (world, Z = sea level). Location itself if the shoreline has no points. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	FVector GetClosestShorePoint(const FVector& Location) const;

	/** Horizontal distance from Location to the shoreline (cm). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	float GetDistanceToShore(const FVector& Location) const;

	/** The animation clock of the sea (seconds, scaled by WaveTimeScale). The material receives the same value. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Sea")
	float GetSeaTime() const { return SeaTime; }

	/** Replaces the waves at runtime (calm morning, windy afternoon) and updates the material. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Sea")
	void SetWaves(const TArray<FSunsetCoveGerstnerWave>& NewWaves);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Material instance + scale of the surface mesh. */
	void SetupSurface();

	/** Packs the waves and sends them (vectors) to the material and the collection. */
	void PushWaveParameters();

	/** Sends the per-frame values (time, tide, sea level). */
	void PushFrameParameters();

	/** Moves the emitter of the waves sound along the shoreline towards the listener. */
	void UpdateShoreAudio(float DeltaSeconds);

	void StartShoreAudio();
	void StopShoreAudio();

	/** Gives the waves sound ShoreAttenuation if neither the cue nor the asset has one (once the sound exists). */
	void TryApplyShoreAttenuation();

	/** The first local player's listener position. False when there is none (editor). */
	bool GetListenerLocation(FVector& OutLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	TArray<SunsetCoveWaves::FPackedWave> PackedWaves;

	FAudioHandle ShoreAudioHandle;
	bool bShoreAttenuationChecked = false;

	float SeaTime = 0.0f;
	float TideOffset = 0.0f;
	bool bShoreAnchorPlaced = false;
	bool bWarnedNoCollectionParams = false;
};
