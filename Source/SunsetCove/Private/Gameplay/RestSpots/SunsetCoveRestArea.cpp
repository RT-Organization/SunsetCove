// SunsetCoveRestArea.cpp
// SunsetCove - Gameplay area (RestSpots). See Public/Gameplay/RestSpots/SunsetCoveRestArea.h for the role of this class.

#include "Gameplay/RestSpots/SunsetCoveRestArea.h"

#include "CollisionQueryParams.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "WorldCollision.h"

namespace
{
	/**
	 * The navigation system registers the level's navmesh on its first tick, after every BeginPlay, so the BeginPlay
	 * attempt usually finds no navmesh. The retries cover that, and a navmesh that is still being built at runtime
	 * (a level saved with an out-of-date navmesh rebuilds it tile by tile when play starts).
	 */
	constexpr int32 NavmeshRetries = 20;
	constexpr float NavmeshRetryDelay = 1.f;

	/** The clearance sphere floats this high above the navmesh point, so it never touches the sand it stands on. */
	constexpr float ClearanceLift = 30.f;
}

ASunsetCoveRestArea::ASunsetCoveRestArea()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->InitBoxExtent(FVector(1000.f, 1000.f, 200.f));
	Area->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Area->SetCanEverAffectNavigation(false);
	Area->SetGenerateOverlapEvents(false);
	Area->ShapeColor = FColor(255, 200, 110);

#if WITH_EDITORONLY_DATA
	FacingArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
	if (FacingArrow)
	{
		FacingArrow->SetupAttachment(Area);
		FacingArrow->ArrowColor = FColor(255, 140, 0);
		FacingArrow->ArrowSize = 3.f;
	}
#endif
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveRestArea::BeginPlay()
{
	Super::BeginPlay();

	// The spots serve the AI, which runs with authority.
	if (!HasAuthority())
	{
		return;
	}

	RetriesLeft = NavmeshRetries;
	if (!GenerateSpots())
	{
		GetWorldTimerManager().SetTimer(RetryTimer, this, &ASunsetCoveRestArea::RetryGenerate, NavmeshRetryDelay, true);
	}
}

void ASunsetCoveRestArea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RetryTimer);

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Spot generation
// ----------------------------------------------------------------------

void ASunsetCoveRestArea::RetryGenerate()
{
	--RetriesLeft;
	if (!GenerateSpots() && RetriesLeft > 0)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(RetryTimer);

	if (GeneratedSpots.IsEmpty())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Rest area '%s' made no rest spots. Check that a NavMeshBoundsVolume covers its box and that the navmesh is built (Build > Build Paths)."),
			*GetNameSafe(this));
	}
}

bool ASunsetCoveRestArea::GenerateSpots()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavigationSystem || !NavigationSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate))
	{
		return false;
	}

	// A half-built navmesh would leave whole parts of the box without spots: wait for the build to end. The last try
	// takes whatever navmesh exists, because some spots are better than none.
	if (NavigationSystem->IsNavigationBuildInProgress() && RetriesLeft > 0)
	{
		return false;
	}

	if (!SpotType.IsValid() || RestActivities.IsEmpty())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Rest area '%s': set Spot Type and Rest Activities (Details > SunsetCove|RestSpot), otherwise visitors cannot tell what these spots are."),
			*GetNameSafe(this));
	}

	// The grid lives in the box's own axes, so a rotated box works too. Scale is honoured through the scaled extent.
	const FVector HalfSize = Area->GetScaledBoxExtent();
	const FQuat BoxRotation = Area->GetComponentQuat();
	const FVector Center = Area->GetComponentLocation();
	const float Step = FMath::Max(Spacing, 100.f);
	const float FacingYaw = GetActorRotation().Yaw;

	// Search up and down far enough to find the sand anywhere inside the box.
	const FVector QueryExtent(Step * 0.5f, Step * 0.5f, HalfSize.Z + 200.f);

	FCollisionObjectQueryParams Obstacles;
	Obstacles.AddObjectTypesToQuery(ECC_WorldStatic);
	Obstacles.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SunsetCoveRestAreaClearance), false, this);
	const FCollisionShape ClearanceSphere = FCollisionShape::MakeSphere(Clearance);

	// Two spots never closer than half a step, even after the navmesh projection pulled them together.
	const double MinDistanceSquared = FMath::Square(static_cast<double>(Step) * 0.5);

	bool bFoundNavmesh = false;

	// Why candidates were dropped, for the log line at the end (a low count is then easy to explain).
	int32 NoNavmeshCount = 0;
	int32 BlockedCount = 0;
	int32 TooCloseCount = 0;

	for (float X = -HalfSize.X + Step * 0.5f; X <= HalfSize.X; X += Step)
	{
		for (float Y = -HalfSize.Y + Step * 0.5f; Y <= HalfSize.Y; Y += Step)
		{
			if (GeneratedSpots.Num() >= MaxSpots)
			{
				break;
			}

			const FVector Local(
				X + FMath::FRandRange(-Jitter, Jitter) * Step,
				Y + FMath::FRandRange(-Jitter, Jitter) * Step,
				0.f);
			const FVector Candidate = Center + BoxRotation.RotateVector(Local);

			FNavLocation OnNavmesh;
			if (!NavigationSystem->ProjectPointToNavigation(Candidate, OnNavmesh, QueryExtent))
			{
				++NoNavmeshCount;
				continue;
			}
			bFoundNavmesh = true;

			if (Clearance > 0.f)
			{
				const FVector SphereCenter = OnNavmesh.Location + FVector(0.f, 0.f, Clearance + ClearanceLift);
				if (World->OverlapAnyTestByObjectType(SphereCenter, FQuat::Identity, Obstacles, ClearanceSphere, QueryParams))
				{
					++BlockedCount;
					continue;
				}
			}

			const bool bTooClose = GeneratedSpots.ContainsByPredicate([&OnNavmesh, MinDistanceSquared](const USunsetCoveRestSpotComponent* Existing)
			{
				return Existing && FVector::DistSquared2D(Existing->GetComponentLocation(), OnNavmesh.Location) < MinDistanceSquared;
			});
			if (bTooClose)
			{
				++TooCloseCount;
				continue;
			}

			USunsetCoveRestSpotComponent* Spot = NewObject<USunsetCoveRestSpotComponent>(this, NAME_None, RF_Transient);
			Spot->SpotType = SpotType;
			Spot->RestActivities = RestActivities;
			Spot->ApproachOffset = FVector::ZeroVector;	// sand: the visitor rests where it stands
			Spot->SetupAttachment(Area);
			Spot->RegisterComponent();	// runs the spot's BeginPlay, which registers it with the subsystem
			Spot->SetWorldLocationAndRotation(OnNavmesh.Location, FRotator(0.f, FacingYaw + FMath::FRandRange(-FacingJitter, FacingJitter), 0.f));
			GeneratedSpots.Add(Spot);
		}
	}

	if (!bFoundNavmesh)
	{
		return false;
	}

	UE_LOG(LogSunsetCove, Log, TEXT("Rest area '%s': %d %s spots (skipped: %d off the navmesh, %d too close to a prop or rock, %d too close to another spot)."),
		*GetNameSafe(this), GeneratedSpots.Num(), *SpotType.ToString(), NoNavmeshCount, BlockedCount, TooCloseCount);
	return true;
}
