// SunsetCoveRestSpotComponent.cpp
// SunsetCove - Gameplay area (RestSpots). See Public/Gameplay/RestSpots/SunsetCoveRestSpotComponent.h for the role of this class.

#include "Gameplay/RestSpots/SunsetCoveRestSpotComponent.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotSubsystem.h"

USunsetCoveRestSpotComponent::USunsetCoveRestSpotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	// A sprite in the level editor, so the spot can be found and moved without opening the prop Blueprint.
	bVisualizeComponent = true;
#endif
}

// ----------------------------------------------------------------------
// UActorComponent
// ----------------------------------------------------------------------

void USunsetCoveRestSpotComponent::BeginPlay()
{
	Super::BeginPlay();

	bSpotEnabled = bStartEnabled;

	if (!SpotType.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Rest spot '%s' on '%s' has no Spot Type (SunsetCove|RestSpot > Spot Type). Visitors that want a specific kind of spot will skip it."),
			*GetName(), *GetNameSafe(GetOwner()));
	}

	if (USunsetCoveRestSpotSubsystem* Subsystem = USunsetCoveRestSpotSubsystem::Get(this))
	{
		Subsystem->RegisterSpot(this);
		bRegistered = true;
	}
}

void USunsetCoveRestSpotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The prop is going away: whoever reserved it must look elsewhere.
	if (ClaimedBy.IsValid())
	{
		RevokeClaim();
	}

	if (bRegistered)
	{
		if (USunsetCoveRestSpotSubsystem* Subsystem = USunsetCoveRestSpotSubsystem::Get(this))
		{
			Subsystem->UnregisterSpot(this);
		}
		bRegistered = false;
	}

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------

FTransform USunsetCoveRestSpotComponent::GetRestFeetTransform() const
{
	// Upright whatever the prop does: a tilted sunbed must not tilt the visitor's capsule.
	return FTransform(FRotator(0.f, GetComponentRotation().Yaw, 0.f), GetComponentLocation());
}

FVector USunsetCoveRestSpotComponent::GetApproachLocation() const
{
	const FRotator YawOnly(0.f, GetComponentRotation().Yaw, 0.f);
	return GetComponentLocation() + YawOnly.RotateVector(ApproachOffset);
}

FGameplayTag USunsetCoveRestSpotComponent::PickRestActivity() const
{
	TArray<FGameplayTag> Activities;
	RestActivities.GetGameplayTagArray(Activities);
	return Activities.Num() > 0 ? Activities[FMath::RandRange(0, Activities.Num() - 1)] : FGameplayTag();
}

// ----------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------

void USunsetCoveRestSpotComponent::SetSpotEnabled(bool bEnabled)
{
	if (bSpotEnabled == bEnabled)
	{
		return;
	}

	bSpotEnabled = bEnabled;

	if (!bSpotEnabled && ClaimedBy.IsValid())
	{
		RevokeClaim();
	}
}

bool USunsetCoveRestSpotComponent::Claim(UObject* Claimant, FSimpleDelegate OnLost)
{
	if (!Claimant || !bSpotEnabled)
	{
		return false;
	}

	if (ClaimedBy.IsValid() && ClaimedBy.Get() != Claimant)
	{
		return false;
	}

	ClaimedBy = Claimant;
	OnClaimLost = MoveTemp(OnLost);
	bOccupied = false;
	return true;
}

void USunsetCoveRestSpotComponent::SetOccupied(const UObject* Claimant, bool bNewOccupied)
{
	if (Claimant && ClaimedBy.Get() == Claimant)
	{
		bOccupied = bNewOccupied;
	}
}

void USunsetCoveRestSpotComponent::Release(const UObject* Claimant)
{
	if (!Claimant || ClaimedBy.Get() != Claimant)
	{
		return;
	}

	ClaimedBy.Reset();
	OnClaimLost.Unbind();
	bOccupied = false;
}

void USunsetCoveRestSpotComponent::RevokeClaim()
{
	// Cleared before the call: the claimant may claim another spot (or this one again) from inside the callback.
	FSimpleDelegate Callback = MoveTemp(OnClaimLost);
	OnClaimLost.Unbind();
	ClaimedBy.Reset();
	bOccupied = false;

	Callback.ExecuteIfBound();
}
