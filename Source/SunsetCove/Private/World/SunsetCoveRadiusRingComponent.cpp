// SunsetCoveRadiusRingComponent.cpp
// SunsetCove - World area. See Public/World/SunsetCoveRadiusRingComponent.h for the role of this class.

#include "World/SunsetCoveRadiusRingComponent.h"

#include "Engine/CollisionProfile.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace
{
	/** Half height of the bounds, so a flat ring is never culled for having no thickness. */
	constexpr float BoundsHalfHeight = 10.f;

	/** Draws the ring each frame it is visible. Values are copied from the component when the render state is built. */
	class FSunsetCoveRadiusRingSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		explicit FSunsetCoveRadiusRingSceneProxy(const USunsetCoveRadiusRingComponent* Component)
			: FPrimitiveSceneProxy(Component)
			, Radius(Component->Radius)
			, Color(Component->RingColor)
			, Thickness(Component->Thickness)
			, Segments(FMath::Clamp(Component->Segments, 8, 256))
		{
			bWillEverBeLit = false;
		}

		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			// Unit axes: the radius is in world units, whatever the scale of the owner.
			const FMatrix& RingToWorld = GetLocalToWorld();
			const FVector Origin = RingToWorld.GetOrigin();
			const FVector AxisX = RingToWorld.GetUnitAxis(EAxis::X);
			const FVector AxisY = RingToWorld.GetUnitAxis(EAxis::Y);

			// Foreground: a flat ring on a slope or under a rock stays readable, like the editor's own gizmos.
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					DrawCircle(Collector.GetPDI(ViewIndex), Origin, AxisX, AxisY, Color, Radius, Segments, SDPG_Foreground, Thickness);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View);
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = false;
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint() const override
		{
			return sizeof(*this) + GetAllocatedSize();
		}

	private:
		float Radius;
		FLinearColor Color;
		float Thickness;
		int32 Segments;
	};
}

USunsetCoveRadiusRingComponent::USunsetCoveRadiusRingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// A drawing aid only: invisible in game, no collision, no shadow, no navmesh influence, not cooked.
	SetHiddenInGame(true);
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CastShadow = false;
	bIsEditorOnly = true;
}

FPrimitiveSceneProxy* USunsetCoveRadiusRingComponent::CreateSceneProxy()
{
	return new FSunsetCoveRadiusRingSceneProxy(this);
}

FBoxSphereBounds USunsetCoveRadiusRingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(Radius, Radius, BoundsHalfHeight), Radius);
}

void USunsetCoveRadiusRingComponent::SetRadius(float NewRadius)
{
	NewRadius = FMath::Max(NewRadius, 0.f);
	if (FMath::IsNearlyEqual(Radius, NewRadius))
	{
		return;
	}

	Radius = NewRadius;
	UpdateBounds();
	MarkRenderStateDirty();
}
