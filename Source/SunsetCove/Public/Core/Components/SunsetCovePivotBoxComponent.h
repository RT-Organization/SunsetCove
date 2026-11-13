// SunsetCovePivotBoxComponent.h
// SunsetCove - Core area (Components). A box collision whose ORIGIN can be the center of its bottom face instead of its center.
//
// WHY THIS EXISTS
//   Props and tools (ASunsetCoveProp, ASunsetCoveTool) use an invisible box as their ROOT: the root must be a primitive, because
//   it carries the physics, the collision and the attachment to the hand. The engine's UBoxComponent is always centered on its
//   origin, so the actor origin of every item was the middle of the item. Everything that puts an actor "at a point of the
//   ground" puts its ORIGIN there, so a sunbed dragged into the level, snapped to the floor (End) or spawned at a spot ended
//   up half inside the floor.
//
//   This component is a UBoxComponent whose box can sit ABOVE its origin (Origin = Bottom, the default): the origin is the
//   center of the bottom face, exactly like a mesh modeled with its pivot at the base. So an item placed at a ground point
//   stands ON the ground, with nothing to compensate anywhere.
//
// WHAT CHANGES COMPARED TO UBoxComponent (everything else is inherited)
//   * Physics: the box element of the body setup is offset by the box half height. The center of mass follows the shape, so
//     a dropped or thrown item still tumbles around its middle.
//   * Bounds (CalcBounds) and the editor wireframe (scene proxy) are offset the same way, so what you see is what collides.
//   * The body setup is never shared with the archetype: the engine shares it by extent only, which would ignore the origin.
//     One small body setup per item is nothing.
//   GetCollisionShape (shape only, no location) is unchanged: callers that place that shape at the component location get
//   a centered box. The engine's own sweeps and overlaps of the component use the body (offset included).
//
// HOW TO USE IT
//   Create it as the root instead of a UBoxComponent; size it with SetBoxExtent as usual (BoxExtent is still the HALF size).
//   GetLocalBox / GetLocalBoxCenter tell where the box is in component space; use them instead of assuming FBox(-Extent, Extent).
//   SunsetCoveHeldItem::ComputeFittedBox (Core/Utils) fits the box to a mesh for either origin.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SunsetCovePivotBoxComponent.generated.h"

/** Where the origin (pivot) of a USunsetCovePivotBoxComponent is on its box. */
UENUM(BlueprintType)
enum class ESunsetCoveBoxOrigin : uint8
{
	/** The center of the bottom face: the item stands on the point it is put at. The default for everything that rests on the ground. */
	Bottom	UMETA(DisplayName = "Bottom (base)"),
	/** The center of the box, like the engine's Box Collision. */
	Center	UMETA(DisplayName = "Center")
};

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent, DisplayName = "Pivot Box Collision"))
class SUNSETCOVE_API USunsetCovePivotBoxComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USunsetCovePivotBoxComponent(const FObjectInitializer& ObjectInitializer);

	/** Where the origin of the component is on the box. Bottom: the box goes up from the origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	ESunsetCoveBoxOrigin Origin = ESunsetCoveBoxOrigin::Bottom;

	/** Changes the origin and rebuilds bounds, wireframe and physics. */
	UFUNCTION(BlueprintCallable, Category = "Shape")
	void SetOrigin(ESunsetCoveBoxOrigin NewOrigin);

	/** Center of the box in component space (unscaled). Zero for Center, (0, 0, half height) for Bottom. */
	UFUNCTION(BlueprintPure, Category = "Shape")
	FVector GetLocalBoxCenter() const { return GetLocalBoxCenterFor(Origin, BoxExtent); }

	/** The box in component space (unscaled). */
	FBox GetLocalBox() const { return FBox(GetLocalBoxCenter() - BoxExtent, GetLocalBoxCenter() + BoxExtent); }

	/** Center of the box in world space. */
	UFUNCTION(BlueprintPure, Category = "Shape")
	FVector GetWorldBoxCenter() const { return GetComponentTransform().TransformPosition(GetLocalBoxCenter()); }

	/** Center of a box of half size Extent for an origin, in component space. Shared with the fitting helper. */
	static FVector GetLocalBoxCenterFor(ESunsetCoveBoxOrigin InOrigin, const FVector& Extent)
	{
		return InOrigin == ESunsetCoveBoxOrigin::Bottom ? FVector(0.0, 0.0, Extent.Z) : FVector::ZeroVector;
	}

	//~ Begin UPrimitiveComponent / UShapeComponent Interface
	virtual void UpdateBodySetup() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent / UShapeComponent Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Bounds, wireframe and the physics shapes of a live body follow the current box. */
	void RefreshShape();
};
