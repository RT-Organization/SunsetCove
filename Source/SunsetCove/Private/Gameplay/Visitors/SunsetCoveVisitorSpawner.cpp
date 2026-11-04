// SunsetCoveVisitorSpawner.cpp
// SunsetCove - Gameplay area (Visitors). See Public/Gameplay/Visitors/SunsetCoveVisitorSpawner.h for the role of this class.

#include "Gameplay/Visitors/SunsetCoveVisitorSpawner.h"

#include "Character/SunsetCoveNPCCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/World.h"
#include "Gameplay/Visitors/SunsetCoveVisitorComponent.h"
#include "Gameplay/Workday/SunsetCoveWorkdaySubsystem.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "World/SunsetCoveEntrance.h"
#include "World/SunsetCoveRadiusRingComponent.h"

namespace
{
	/** Random points tried around the spawner before falling back to its own location. */
	constexpr int32 SpawnPointAttempts = 6;

	/** Visitors are placed this much above the navmesh, so the capsule never starts inside the ground. */
	constexpr float SpawnLift = 5.f;

	/** How far up and down a spawn point may be from the navmesh. */
	constexpr float NavmeshSearchHeight = 400.f;
}

ASunsetCoveVisitorSpawner::ASunsetCoveVisitorSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
#if WITH_EDITORONLY_DATA
	Root->bVisualizeComponent = true;

	FacingArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
	if (FacingArrow)
	{
		FacingArrow->SetupAttachment(Root);
		FacingArrow->ArrowColor = FColor(120, 255, 120);
		FacingArrow->ArrowSize = 1.5f;
	}

	SpawnRing = CreateEditorOnlyDefaultSubobject<USunsetCoveRadiusRingComponent>(TEXT("SpawnRing"));
	if (SpawnRing)
	{
		SpawnRing->SetupAttachment(Root);
		SpawnRing->RingColor = FColor(120, 255, 120);
		SpawnRing->Radius = SpawnRadius;
	}
#endif
}

void ASunsetCoveVisitorSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	// Runs on every edit in the level editor, so the ring follows the Details panel live.
	if (SpawnRing)
	{
		SpawnRing->SetRadius(SpawnRadius);
	}
#endif
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveVisitorSpawner::BeginPlay()
{
	Super::BeginPlay();

	// No navmesh query here: the navmesh is registered after BeginPlay (see CachedExitLocation).
	if (!HasAuthority())
	{
		return;
	}

	if (VisitorClasses.IsEmpty())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Visitor spawner '%s': Visitor Classes is empty (Class Defaults > SunsetCove|Visitors). Use BP_VisitorSpawner or fill it on the instance."),
			*GetNameSafe(this));
	}
	if (!Entrance)
	{
		UE_LOG(LogSunsetCove, Log, TEXT("Visitor spawner '%s' has no Entrance: visitors will go straight to a rest spot."), *GetNameSafe(this));
	}

	USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this);
	if (!bSpawnOnlyDuringShift || !Workday)
	{
		StartSpawning();
		return;
	}

	Workday->OnShiftStarted.AddUniqueDynamic(this, &ASunsetCoveVisitorSpawner::HandleShiftStarted);
	Workday->OnShiftEnded.AddUniqueDynamic(this, &ASunsetCoveVisitorSpawner::HandleShiftEnded);
	if (Workday->IsShiftActive())
	{
		StartSpawning();
	}
}

void ASunsetCoveVisitorSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();

	if (USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this))
	{
		Workday->OnShiftStarted.RemoveDynamic(this, &ASunsetCoveVisitorSpawner::HandleShiftStarted);
		Workday->OnShiftEnded.RemoveDynamic(this, &ASunsetCoveVisitorSpawner::HandleShiftEnded);
	}

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Spawning
// ----------------------------------------------------------------------

ASunsetCoveNPCCharacter* ASunsetCoveVisitorSpawner::SpawnVisitor()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return nullptr;
	}

	TArray<TSubclassOf<ASunsetCoveNPCCharacter>> Candidates;
	for (const TSubclassOf<ASunsetCoveNPCCharacter>& VisitorClass : VisitorClasses)
	{
		if (VisitorClass)
		{
			Candidates.Add(VisitorClass);
		}
	}
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	const TSubclassOf<ASunsetCoveNPCCharacter> VisitorClass = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	FVector FeetLocation;
	if (!FindSpawnFeetLocation(FeetLocation))
	{
		if (!bWarnedNoNavmesh)
		{
			bWarnedNoNavmesh = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("Visitor spawner '%s' found no navmesh within %.0f cm. Move it onto the path inside a NavMeshBoundsVolume, or visitors cannot spawn or leave here."),
				*GetNameSafe(this), SpawnRadius + 50.f);
		}
		return nullptr;
	}

	// The actor stands at the capsule centre, half a capsule above the feet.
	const ASunsetCoveNPCCharacter* Defaults = VisitorClass->GetDefaultObject<ASunsetCoveNPCCharacter>();
	const float HalfHeight = (Defaults && Defaults->GetCapsuleComponent()) ? Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector SpawnLocation = FeetLocation + FVector(0.f, 0.f, HalfHeight + SpawnLift);

	// Face up the path, toward the barrier when there is one.
	const FVector LookAt = Entrance ? Entrance->GetApproachLocation() : SpawnLocation + GetActorForwardVector();
	const FRotator SpawnRotation(0.f, (LookAt - SpawnLocation).Rotation().Yaw, 0.f);

	FActorSpawnParameters Params;
	Params.Owner = this;
	// Never inside another visitor: if the spot is taken, this spawn is skipped and the next timer tries again.
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	ASunsetCoveNPCCharacter* Visitor = World->SpawnActor<ASunsetCoveNPCCharacter>(VisitorClass, SpawnLocation, SpawnRotation, Params);
	if (!Visitor)
	{
		return nullptr;
	}

	Visitors.Add(Visitor);
	++SpawnedThisShift;
	Visitor->OnDestroyed.AddUniqueDynamic(this, &ASunsetCoveVisitorSpawner::HandleVisitorDestroyed);

	if (USunsetCoveVisitorComponent* Brain = Visitor->FindComponentByClass<USunsetCoveVisitorComponent>())
	{
		Brain->StartVisit(Entrance, GetExitLocation());
	}
	else
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Visitor spawner '%s': '%s' has no SunsetCove Visitor component (add it in the Components panel of the Blueprint). It will stand still."),
			*GetNameSafe(this), *GetNameSafe(VisitorClass));
	}

	return Visitor;
}

FVector ASunsetCoveVisitorSpawner::GetExitLocation() const
{
	if (bExitLocationOnNavmesh)
	{
		return CachedExitLocation;
	}

	CachedExitLocation = GetActorLocation();
	if (const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation OnNavmesh;
		if (NavigationSystem->ProjectPointToNavigation(GetActorLocation(), OnNavmesh, FVector(SpawnRadius + 100.f, SpawnRadius + 100.f, NavmeshSearchHeight)))
		{
			CachedExitLocation = OnNavmesh.Location;
			bExitLocationOnNavmesh = true;
		}
	}
	return CachedExitLocation;
}

int32 ASunsetCoveVisitorSpawner::GetVisitorCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<ASunsetCoveNPCCharacter>& Visitor : Visitors)
	{
		if (Visitor.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

bool ASunsetCoveVisitorSpawner::FindSpawnFeetLocation(FVector& OutFeetLocation) const
{
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavigationSystem)
	{
		return false;
	}

	const FVector QueryExtent(SpawnRadius + 50.f, SpawnRadius + 50.f, NavmeshSearchHeight);
	FNavLocation OnNavmesh;

	for (int32 Attempt = 0; Attempt < SpawnPointAttempts; ++Attempt)
	{
		const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);
		if (NavigationSystem->ProjectPointToNavigation(GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f), OnNavmesh, QueryExtent))
		{
			OutFeetLocation = OnNavmesh.Location;
			return true;
		}
	}

	if (NavigationSystem->ProjectPointToNavigation(GetActorLocation(), OnNavmesh, QueryExtent))
	{
		OutFeetLocation = OnNavmesh.Location;
		return true;
	}
	return false;
}

// ----------------------------------------------------------------------
// Schedule
// ----------------------------------------------------------------------

void ASunsetCoveVisitorSpawner::StartSpawning()
{
	if (bSpawning)
	{
		return;
	}

	bSpawning = true;
	SpawnedThisShift = 0;
	ScheduleNextSpawn(FirstSpawnDelay);
}

void ASunsetCoveVisitorSpawner::StopSpawning()
{
	bSpawning = false;
	GetWorldTimerManager().ClearTimer(SpawnTimer);
}

void ASunsetCoveVisitorSpawner::ScheduleNextSpawn(float Delay)
{
	if (bSpawning)
	{
		GetWorldTimerManager().SetTimer(SpawnTimer, this, &ASunsetCoveVisitorSpawner::HandleSpawnTimer, FMath::Max(Delay, 0.05f), false);
	}
}

void ASunsetCoveVisitorSpawner::HandleSpawnTimer()
{
	Visitors.RemoveAll([](const TWeakObjectPtr<ASunsetCoveNPCCharacter>& Visitor) { return !Visitor.IsValid(); });

	const bool bRoomInCove = Visitors.Num() < MaxVisitorsInCove;
	const bool bShiftQuotaLeft = (MaxVisitorsPerShift <= 0) || (SpawnedThisShift < MaxVisitorsPerShift);
	if (bRoomInCove && bShiftQuotaLeft)
	{
		SpawnVisitor();
	}

	// SUNSETCOVE GAME LOGIC: the arrival rate can follow the reputation of the cove or the hour of the day here.
	ScheduleNextSpawn(FMath::FRandRange(FMath::Min(MinSpawnInterval, MaxSpawnInterval), FMath::Max(MinSpawnInterval, MaxSpawnInterval)));
}

void ASunsetCoveVisitorSpawner::HandleShiftStarted()
{
	StartSpawning();
}

void ASunsetCoveVisitorSpawner::HandleShiftEnded()
{
	// Visitors in the cove leave on their own (they listen to the same event).
	StopSpawning();
}

void ASunsetCoveVisitorSpawner::HandleVisitorDestroyed(AActor* DestroyedActor)
{
	Visitors.RemoveAll([DestroyedActor](const TWeakObjectPtr<ASunsetCoveNPCCharacter>& Visitor)
	{
		return !Visitor.IsValid() || Visitor.Get() == DestroyedActor;
	});
}
