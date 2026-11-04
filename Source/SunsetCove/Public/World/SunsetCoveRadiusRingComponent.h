// SunsetCoveRadiusRingComponent.h
// SunsetCove - World area. A flat ring drawn in the level editor to show a radius (editor only).
//
// WHAT THIS CLASS IS
//   A visual aid for level design: a circle on the component's XY plane, Radius centimetres wide, drawn in the editor
//   viewport and hidden in game. It has no collision, never touches the navmesh and is stripped from cooked builds.
//   Actors that have a gameplay radius own one and copy that radius into it whenever the actor is edited, for example
//   ASunsetCoveEntrance (ApproachRadius) and ASunsetCoveVisitorSpawner (SpawnRadius).
//
// HOW TO USE IT
//   C++  In the constructor, inside #if WITH_EDITORONLY_DATA:
//          Ring = CreateEditorOnlyDefaultSubobject<USunsetCoveRadiusRingComponent>(TEXT("RadiusRing"));
//        then call Ring->SetRadius(MyRadius) from OnConstruction, so the ring follows the Details panel live.
//   The radius is in world units: actor scale does not change the ring, as it does not change the gameplay radius.
//
// HIERARCHY
//   UPrimitiveComponent -> USunsetCoveRadiusRingComponent (this class)

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "SunsetCoveRadiusRingComponent.generated.h"

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent), hidecategories = (Object, LOD, Lighting, TextureStreaming, Activation, Collision, Physics, Navigation, Cooking, HLOD, Mobile, RayTracing, VirtualTexture))
class SUNSETCOVE_API USunsetCoveRadiusRingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	USunsetCoveRadiusRingComponent();

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface

	/** Changes the radius and redraws the ring. */
	void SetRadius(float NewRadius);

	// ------------------------------------------------------------------
	// Configuration
	// ------------------------------------------------------------------

	/** Ring radius in world centimetres. Owners overwrite it with their own gameplay radius. */
	UPROPERTY(EditAnywhere, Category = "SunsetCove|Ring", meta = (ClampMin = "0.0", Units = "cm"))
	float Radius = 100.f;

	/** Ring colour. */
	UPROPERTY(EditAnywhere, Category = "SunsetCove|Ring")
	FColor RingColor = FColor(255, 200, 0);

	/** Line thickness in world centimetres. 0 = one pixel. */
	UPROPERTY(EditAnywhere, Category = "SunsetCove|Ring", meta = (ClampMin = "0.0", Units = "cm"))
	float Thickness = 2.f;

	/** Straight segments that make up the circle. */
	UPROPERTY(EditAnywhere, Category = "SunsetCove|Ring", meta = (ClampMin = "8", ClampMax = "256"))
	int32 Segments = 48;
};
