// SunsetCoveEntrance.cpp
// SunsetCove - World area. See Public/World/SunsetCoveEntrance.h for the role of this class.

#include "World/SunsetCoveEntrance.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "World/SunsetCoveRadiusRingComponent.h"

namespace
{
	/** Tries for a navigable point near the approach point before falling back to the point itself. */
	constexpr int32 ApproachPickAttempts = 4;
}

ASunsetCoveEntrance::ASunsetCoveEntrance()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ApproachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ApproachPoint"));
	ApproachPoint->SetupAttachment(Root);
	ApproachPoint->SetRelativeLocation(FVector(-150.f, 0.f, 0.f));
#if WITH_EDITORONLY_DATA
	ApproachPoint->bVisualizeComponent = true;

	InwardArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("InwardArrow"));
	if (InwardArrow)
	{
		InwardArrow->SetupAttachment(Root);
		InwardArrow->ArrowColor = FColor(80, 200, 255);
		InwardArrow->ArrowSize = 2.f;
	}

	ApproachRing = CreateEditorOnlyDefaultSubobject<USunsetCoveRadiusRingComponent>(TEXT("ApproachRing"));
	if (ApproachRing)
	{
		ApproachRing->SetupAttachment(ApproachPoint);
		ApproachRing->RingColor = FColor(80, 200, 255);
		ApproachRing->Radius = ApproachRadius;
	}
#endif
}

void ASunsetCoveEntrance::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	// Runs on every edit in the level editor, so the ring follows the Details panel live.
	if (ApproachRing)
	{
		ApproachRing->SetRadius(ApproachRadius);
	}
#endif
}

FVector ASunsetCoveEntrance::GetApproachLocation() const
{
	return ApproachPoint ? ApproachPoint->GetComponentLocation() : GetActorLocation();
}

FVector ASunsetCoveEntrance::PickApproachLocation() const
{
	const FVector Base = GetApproachLocation();
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavigationSystem || ApproachRadius <= 0.f)
	{
		return Base;
	}

	const FVector QueryExtent(ApproachRadius, ApproachRadius, 300.f);
	for (int32 Attempt = 0; Attempt < ApproachPickAttempts; ++Attempt)
	{
		const FVector2D Offset = FMath::RandPointInCircle(ApproachRadius);
		FNavLocation OnNavmesh;
		if (NavigationSystem->ProjectPointToNavigation(Base + FVector(Offset.X, Offset.Y, 0.f), OnNavmesh, QueryExtent))
		{
			return OnNavmesh.Location;
		}
	}

	return Base;
}
