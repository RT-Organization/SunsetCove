// SunsetCoveAIController.cpp
// SunsetCove - AI area. See Public/AI/SunsetCoveAIController.h for the role of this class.

#include "AI/SunsetCoveAIController.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
	/** Samples taken during StuckTime. More samples = the same window, judged more finely. */
	constexpr int32 StuckSampleCount = 8;

	ECrowdAvoidanceQuality::Type ToEngineQuality(ESunsetCoveAvoidanceQuality Quality)
	{
		switch (Quality)
		{
		case ESunsetCoveAvoidanceQuality::Low:
			return ECrowdAvoidanceQuality::Low;
		case ESunsetCoveAvoidanceQuality::Good:
			return ECrowdAvoidanceQuality::Good;
		case ESunsetCoveAvoidanceQuality::High:
			return ECrowdAvoidanceQuality::High;
		case ESunsetCoveAvoidanceQuality::Medium:
		default:
			return ECrowdAvoidanceQuality::Medium;
		}
	}
}

ASunsetCoveAIController::ASunsetCoveAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	// The engine defaults already fit an NPC: no player state, and the control rotation follows the pawn, which the
	// movement component turns toward where it walks.
}

// ----------------------------------------------------------------------
// AController / AAIController
// ----------------------------------------------------------------------

void ASunsetCoveAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ApplyCrowdSettings();

	// SUNSETCOVE GAME LOGIC: per-NPC AI setup that needs the body (perception senses, team) goes here. Behavior
	// (where to go and when) stays in the behavior components of the pawn.
}

void ASunsetCoveAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	UE_VLOG(this, LogSunsetCove, Verbose, TEXT("Move %u finished: %s"), RequestID.GetID(), *Result.ToString());
	OnMoveFinished.Broadcast(RequestID, Result);
}

// ----------------------------------------------------------------------
// Internals
// ----------------------------------------------------------------------

void ASunsetCoveAIController::ApplyCrowdSettings()
{
	UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
	if (!PathFollowing)
	{
		return;
	}

	PathFollowing->SetBlockDetection(StuckDistance, FMath::Max(StuckTime / StuckSampleCount, 0.05f), StuckSampleCount);
	PathFollowing->SetBlockDetectionState(true);

	UCrowdFollowingComponent* Crowd = Cast<UCrowdFollowingComponent>(PathFollowing);
	if (!Crowd)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: the path following component is not a crowd component, so NPCs will not avoid each other. Do not replace PathFollowingComponent in a Blueprint child."), *GetNameSafe(this));
		return;
	}

	// Each setter updates the crowd agent only when its value really changes, so every call asks for the update.
	Crowd->SetCrowdAvoidanceQuality(ToEngineQuality(AvoidanceQuality), true);
	Crowd->SetCrowdSeparation(bKeepPersonalSpace, true);
	Crowd->SetCrowdSeparationWeight(PersonalSpaceWeight, true);
	Crowd->SetCrowdSlowdownAtGoal(true, true);
}
