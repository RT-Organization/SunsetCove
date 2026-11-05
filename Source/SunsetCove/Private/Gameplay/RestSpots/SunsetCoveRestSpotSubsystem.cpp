// SunsetCoveRestSpotSubsystem.cpp
// SunsetCove - Gameplay area (RestSpots). See Public/Gameplay/RestSpots/SunsetCoveRestSpotSubsystem.h for the role of this class.

#include "Gameplay/RestSpots/SunsetCoveRestSpotSubsystem.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugRestSpots(
		TEXT("SunsetCove.Debug.RestSpots"),
		0,
		TEXT("Draws every rest spot: green free, yellow claimed, red occupied, grey disabled. 0 = off, 1 = on."),
		ECVF_Cheat);

	/** Weight of one claimed neighbour against the random part of the score (0..1). */
	constexpr float CrowdingPenalty = 0.35f;

	bool MatchesType(const USunsetCoveRestSpotComponent& Spot, const FGameplayTag& Type)
	{
		return !Type.IsValid() || Spot.SpotType.MatchesTag(Type);
	}

	bool IsExcluded(const USunsetCoveRestSpotComponent* Spot, const FSunsetCoveRestSpotQuery& Query)
	{
		return Query.Excluded.ContainsByPredicate([Spot](const TWeakObjectPtr<USunsetCoveRestSpotComponent>& Entry)
		{
			return Entry.Get() == Spot;
		});
	}
}

USunsetCoveRestSpotSubsystem* USunsetCoveRestSpotSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<USunsetCoveRestSpotSubsystem>() : nullptr;
}

bool USunsetCoveRestSpotSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Spots only matter where visitors play: the game and Play In Editor.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ----------------------------------------------------------------------
// Tick (debug view only)
// ----------------------------------------------------------------------

void USunsetCoveRestSpotSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CVarDebugRestSpots.GetValueOnGameThread() > 0)
	{
		DrawDebugSpots();
	}
}

TStatId USunsetCoveRestSpotSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USunsetCoveRestSpotSubsystem, STATGROUP_Tickables);
}

// ----------------------------------------------------------------------
// Registry
// ----------------------------------------------------------------------

void USunsetCoveRestSpotSubsystem::RegisterSpot(USunsetCoveRestSpotComponent* Spot)
{
	if (Spot)
	{
		Spots.AddUnique(Spot);
	}
}

void USunsetCoveRestSpotSubsystem::UnregisterSpot(USunsetCoveRestSpotComponent* Spot)
{
	Spots.RemoveAll([Spot](const TWeakObjectPtr<USunsetCoveRestSpotComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Spot;
	});
}

// ----------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------

void USunsetCoveRestSpotSubsystem::GatherFreeSpots(const FSunsetCoveRestSpotQuery& Query, TArray<USunsetCoveRestSpotComponent*>& OutSpots) const
{
	for (const TWeakObjectPtr<USunsetCoveRestSpotComponent>& Entry : Spots)
	{
		USunsetCoveRestSpotComponent* Spot = Entry.Get();
		if (Spot && Spot->IsFree() && MatchesType(*Spot, Query.PreferredType) && !IsExcluded(Spot, Query))
		{
			OutSpots.Add(Spot);
		}
	}
}

USunsetCoveRestSpotComponent* USunsetCoveRestSpotSubsystem::FindFreeSpot(const FSunsetCoveRestSpotQuery& Query) const
{
	TArray<USunsetCoveRestSpotComponent*> Candidates;
	GatherFreeSpots(Query, Candidates);

	// The wanted kind is full: any kind will do, if the visitor accepts it.
	if (Candidates.IsEmpty() && Query.PreferredType.IsValid() && Query.bAllowOtherTypes)
	{
		FSunsetCoveRestSpotQuery AnyType = Query;
		AnyType.PreferredType = FGameplayTag();
		GatherFreeSpots(AnyType, Candidates);
	}

	USunsetCoveRestSpotComponent* Best = nullptr;
	float BestScore = -FLT_MAX;

	for (USunsetCoveRestSpotComponent* Candidate : Candidates)
	{
		// Random first, so a group spreads out; crowded spots lose points, so people keep some distance.
		float Score = FMath::FRand();
		if (Query.PersonalSpaceRadius > 0.f)
		{
			Score -= CrowdingPenalty * CountClaimedNeighbours(Candidate, Query.PersonalSpaceRadius);
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

int32 USunsetCoveRestSpotSubsystem::CountSpots(FGameplayTag Type, bool bOnlyFree) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<USunsetCoveRestSpotComponent>& Entry : Spots)
	{
		const USunsetCoveRestSpotComponent* Spot = Entry.Get();
		if (Spot && MatchesType(*Spot, Type) && (!bOnlyFree || Spot->IsFree()))
		{
			++Count;
		}
	}
	return Count;
}

int32 USunsetCoveRestSpotSubsystem::CountClaimedNeighbours(const USunsetCoveRestSpotComponent* Candidate, float Radius) const
{
	const FVector Center = Candidate->GetComponentLocation();
	const double RadiusSquared = FMath::Square(static_cast<double>(Radius));

	int32 Count = 0;
	for (const TWeakObjectPtr<USunsetCoveRestSpotComponent>& Entry : Spots)
	{
		const USunsetCoveRestSpotComponent* Other = Entry.Get();
		if (Other && Other != Candidate && Other->GetClaimant() && FVector::DistSquared(Center, Other->GetComponentLocation()) < RadiusSquared)
		{
			++Count;
		}
	}
	return Count;
}

// ----------------------------------------------------------------------
// Debug
// ----------------------------------------------------------------------

void USunsetCoveRestSpotSubsystem::DrawDebugSpots() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TWeakObjectPtr<USunsetCoveRestSpotComponent>& Entry : Spots)
	{
		const USunsetCoveRestSpotComponent* Spot = Entry.Get();
		if (!Spot)
		{
			continue;
		}

		FColor Color = FColor::Green;
		if (!Spot->IsSpotEnabled())
		{
			Color = FColor(128, 128, 128);
		}
		else if (Spot->IsOccupied())
		{
			Color = FColor::Red;
		}
		else if (Spot->GetClaimant())
		{
			Color = FColor::Yellow;
		}

		const FTransform Feet = Spot->GetRestFeetTransform();
		const FVector Location = Feet.GetLocation() + FVector(0.f, 0.f, 5.f);
		DrawDebugSphere(World, Location, 20.f, 8, Color);
		DrawDebugDirectionalArrow(World, Location, Location + Feet.GetRotation().GetForwardVector() * 60.f, 25.f, Color);

		const FVector Approach = Spot->GetApproachLocation() + FVector(0.f, 0.f, 5.f);
		if (!Approach.Equals(Location, 1.f))
		{
			DrawDebugLine(World, Location, Approach, Color);
			DrawDebugPoint(World, Approach, 8.f, Color);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}
