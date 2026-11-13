// SunsetCovePivotBoxComponent.cpp
// SunsetCove - Core area (Components). See Public/Core/Components/SunsetCovePivotBoxComponent.h.

#include "Core/Components/SunsetCovePivotBoxComponent.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BoxElem.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "SceneView.h"

USunsetCovePivotBoxComponent::USunsetCovePivotBoxComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The engine shares the body setup with the archetype when the EXTENT matches, without looking at the origin. An own body
	// setup per component keeps the offset right in every case (a Blueprint that changes the origin, an older archetype).
	bUseArchetypeBodySetup = false;
}

void USunsetCovePivotBoxComponent::SetOrigin(ESunsetCoveBoxOrigin NewOrigin)
{
	if (Origin == NewOrigin)
	{
		return;
	}

	Origin = NewOrigin;
	RefreshShape();
}

void USunsetCovePivotBoxComponent::UpdateBodySetup()
{
	// Never use (or modify) the archetype's body setup, see the constructor.
	bUseArchetypeBodySetup = false;
	if (ShapeBodySetup && ShapeBodySetup->GetOuter() != this)
	{
		ShapeBodySetup = nullptr;
	}

	// The engine builds a centered box element sized from BoxExtent...
	Super::UpdateBodySetup();

	// ...then the element is moved so the box sits where Origin says. Center of mass follows the shape.
	if (ShapeBodySetup && ShapeBodySetup->AggGeom.BoxElems.Num() == 1)
	{
		const FVector Extent = BoxExtent.ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
		ShapeBodySetup->AggGeom.BoxElems[0].Center = GetLocalBoxCenterFor(Origin, Extent);
	}
}

FBoxSphereBounds USunsetCovePivotBoxComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(GetLocalBox()).TransformBy(LocalToWorld);
}

void USunsetCovePivotBoxComponent::RefreshShape()
{
	UpdateBounds();
	MarkRenderStateDirty();
	UpdateBodySetup();

	if (bPhysicsStateCreated)
	{
		// A new element transform needs new physics shapes (UpdateBodyScale only resizes when the scale changes).
		RecreatePhysicsState();
	}
}

#if WITH_EDITOR
void USunsetCovePivotBoxComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(USunsetCovePivotBoxComponent, Origin))
	{
		RefreshShape();
	}
}
#endif

FPrimitiveSceneProxy* USunsetCovePivotBoxComponent::CreateSceneProxy()
{
	/** The editor wireframe of the box, drawn where the box really is (offset from the origin). */
	class FPivotBoxSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		FPivotBoxSceneProxy(const USunsetCovePivotBoxComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
			, bDrawOnlyIfSelected(InComponent->bDrawOnlyIfSelected)
			, BoxExtent(InComponent->GetUnscaledBoxExtent())
			, BoxCenter(InComponent->GetLocalBoxCenter())
			, BoxColor(InComponent->ShapeColor)
			, LineThickness(InComponent->LineThickness)
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
			const FMatrix& LocalToWorld = GetLocalToWorld();
			const FVector WorldCenter = LocalToWorld.TransformPosition(BoxCenter);

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FLinearColor DrawColor = GetViewSelectionColor(BoxColor, *Views[ViewIndex], IsSelected(), IsHovered(), false, IsIndividuallySelected());
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
					DrawOrientedWireBox(PDI, WorldCenter, LocalToWorld.GetScaledAxis(EAxis::X), LocalToWorld.GetScaledAxis(EAxis::Y), LocalToWorld.GetScaledAxis(EAxis::Z),
						BoxExtent, DrawColor, SDPG_World, LineThickness);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			const bool bProxyVisible = !bDrawOnlyIfSelected || IsSelected();
			const bool bShowForCollision = View->Family->EngineShowFlags.Collision && IsCollisionEnabled();

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = (IsShown(View) && bProxyVisible) || bShowForCollision;
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

	private:
		const uint32 bDrawOnlyIfSelected : 1;
		const FVector BoxExtent;
		const FVector BoxCenter;
		const FColor BoxColor;
		const float LineThickness;
	};

	return new FPivotBoxSceneProxy(this);
}
