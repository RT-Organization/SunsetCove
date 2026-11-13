// SunsetCoveFootstepSet.h
// SunsetCove - Audio System / Footsteps. Which sounds a step makes on each kind of ground (DA_Footsteps).
//
// HOW A GROUND KIND IS KNOWN
//   Through PHYSICAL SURFACES (Project Settings > Physics > Physical Surface: Sand, Mud, Rock, Wood, Water). A Physical Material
//   (PM_Sand...) has one Surface Type; it is set on the paint layers of the landscape (Landscape Layer Info > Phys Material) and on
//   meshes (material > Phys Material). The footstep component traces under the feet and reads it. The sea has no collision: the
//   component asks the sea directly (USunsetCoveSeaSubsystem) and uses the Water surface when the feet are under its surface.
//
// WHAT IS IN HERE
//   One row per surface: several step sounds (one is picked at random, never the same twice in a row, so walking does not sound
//   like a loop) + optional landing sounds, a volume and small random pitch / volume variations. Default is used for any surface
//   without a row (and for ground with no physical material: on this beach that is sand).
//   A new kind of ground = a physical surface + a physical material + a row here. No code.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveFootstepSet.generated.h"

class USoundBase;

/** The sounds of one kind of ground. */
USTRUCT(BlueprintType)
struct SUNSETCOVE_API FSunsetCoveFootstepSurface
{
	GENERATED_BODY()

	/** The physical surface this row is for (Project Settings > Physics > Physical Surface). Ignored on the Default row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	/** Step sounds; one is picked at random each step (never the same twice in a row). 3-6 short one-shots work best. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Steps;

	/** Landing sounds after a jump or a fall. Empty = a step sound, louder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Landings;

	/** Volume of this ground (sand is soft, rock is sharper). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Volume = 1.0f;

	/** Random pitch spread per step: pitch x (1 +/- this). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchVariation = 0.06f;

	/** Random volume spread per step: volume x (1 +/- this). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float VolumeVariation = 0.12f;
};

UCLASS(BlueprintType)
class SUNSETCOVE_API USunsetCoveFootstepSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** One row per kind of ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (TitleProperty = "Surface"))
	TArray<FSunsetCoveFootstepSurface> Surfaces;

	/** Used when the ground has no row (or no physical material at all). On the beach: sand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	FSunsetCoveFootstepSurface Default;

	/** Volume category of the steps (the Options slider that controls them). Empty = SFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps", meta = (Categories = "CoreFramework.Audio.Category"))
	FGameplayTag Category;

	/** The row for Surface, or Default. */
	const FSunsetCoveFootstepSurface& FindSurface(EPhysicalSurface Surface) const;
};
