// PropPlacementHologram.h
// SunsetCove - Inventory System / Prop Inventory. The ghost of a prop that shows where it would be placed.
//
// WHAT THIS CLASS IS
//   A transient actor with no collision. UPropInventoryComponent spawns one when the player starts placing a prop and
//   destroys it when the placement ends. It copies the look of the prop (every static and skeletal mesh component, with
//   the same placement relative to the prop root, so a prop made of several meshes is copied whole; a skeletal copy
//   follows the pose of the original) and shows it with the hologram material, tinted green when the spot is valid
//   and red when it is not.
//
// WITHOUT A HOLOGRAM MATERIAL
//   If the component has no HologramMaterial, the copy keeps the materials of the prop and cannot be tinted. Validity is
//   then shown by visibility: the ghost is visible on a valid spot and hidden on an invalid one.
//
// WHO USES IT
//   Only UPropInventoryComponent. It is not meant to be placed in a level or subclassed in Blueprint.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PropPlacementHologram.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

UCLASS(NotBlueprintable, NotPlaceable, Transient)
class SUNSETCOVE_API APropPlacementHologram : public AActor
{
	GENERATED_BODY()

public:
	APropPlacementHologram();

	/**
	 * Builds the ghost from SourceProp. Call once, right after spawning, with the hologram at the transform of the prop
	 * (same scale as the prop root).
	 * @param HologramMaterial  Translucent material with a vector parameter named ColorParameterName. May be null.
	 */
	void InitializeFromProp(const AActor* SourceProp, UMaterialInterface* HologramMaterial, FName ColorParameterName, const FLinearColor& InValidColor, const FLinearColor& InInvalidColor);

	/** Tints the ghost (or shows / hides it when there is no material). Cheap: call it every frame if needed. */
	void SetValid(bool bNewValid);

private:
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> HologramRoot;

	/** The copies of the static and skeletal meshes of the prop. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> MeshCopies;

	/** One dynamic material shared by every mesh copy. Null when no hologram material was given. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HologramMaterialInstance;

	FName ColorParameter;
	FLinearColor ValidColor = FLinearColor::Green;
	FLinearColor InvalidColor = FLinearColor::Red;
};
