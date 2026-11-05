// SunsetCoveVisitorComponent.cpp
// SunsetCove - Gameplay area (Visitors). See Public/Gameplay/Visitors/SunsetCoveVisitorComponent.h for the role of this class.

#include "Gameplay/Visitors/SunsetCoveVisitorComponent.h"

#include "AI/SunsetCoveAIController.h"
#include "Character/SunsetCoveNPCCharacter.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotComponent.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotSubsystem.h"
#include "Gameplay/Workday/SunsetCoveWorkdaySubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"
#include "World/SunsetCoveEntrance.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugVisitors(
		TEXT("SunsetCove.Debug.Visitors"),
		0,
		TEXT("Writes each visitor's phase above its head and draws its current goal. 0 = off, 1 = on."),
		ECVF_Cheat);

	/** The component ticks only for the debug view, ten times a second. */
	constexpr float DebugTickInterval = 0.1f;

	FString PhaseToString(ESunsetCoveVisitorPhase Phase)
	{
		return UEnum::GetDisplayValueAsText(Phase).ToString();
	}

	/** "Cove.Spot.Sunbed" -> "Sunbed", for short debug labels. */
	FString TagLeaf(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		int32 Dot = INDEX_NONE;
		return Name.FindLastChar(TEXT('.'), Dot) ? Name.RightChop(Dot + 1) : Name;
	}
}

USunsetCoveVisitorComponent::USunsetCoveVisitorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = DebugTickInterval;
	SetIsReplicatedByDefault(false);
}

// ----------------------------------------------------------------------
// UActorComponent
// ----------------------------------------------------------------------

void USunsetCoveVisitorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimers();
	ReleaseSpot();
	UnbindFromController();

	if (USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this))
	{
		Workday->OnShiftEnded.RemoveDynamic(this, &USunsetCoveVisitorComponent::HandleShiftEnded);
	}

	Super::EndPlay(EndPlayReason);
}

void USunsetCoveVisitorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarDebugVisitors.GetValueOnGameThread() > 0)
	{
		DrawDebugState();
	}
}

// ----------------------------------------------------------------------
// Flow
// ----------------------------------------------------------------------

void USunsetCoveVisitorComponent::StartVisit(ASunsetCoveEntrance* InEntrance, FVector InExitLocation)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (Phase != ESunsetCoveVisitorPhase::Idle)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: StartVisit called on a visit that already started. Ignored."), *GetNameSafe(Owner));
		return;
	}

	ASunsetCoveAIController* AI = GetAIController();
	if (!AI)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: no ASunsetCoveAIController possesses this visitor (Class Defaults > Pawn > AI Controller Class). It will stand still."), *GetNameSafe(Owner));
		return;
	}
	BindToController(AI);

	Entrance = InEntrance;
	ExitLocation = InExitLocation;
	DesiredSpotType = RollDesiredSpotType();

	if (USunsetCoveWorkdaySubsystem* Workday = USunsetCoveWorkdaySubsystem::Get(this))
	{
		Workday->OnShiftEnded.AddUniqueDynamic(this, &USunsetCoveVisitorComponent::HandleShiftEnded);
	}

	UE_VLOG(Owner, LogSunsetCove, Log, TEXT("Visit starts. Wants: %s"), DesiredSpotType.IsValid() ? *DesiredSpotType.ToString() : TEXT("anything"));

	const ASunsetCoveEntrance* Gate = Entrance.Get();
	if (!Gate)
	{
		ChooseSpotAndGo();
		return;
	}

	EnterPhase(ESunsetCoveVisitorPhase::ApproachingEntrance);
	if (!RequestMove(Gate->PickApproachLocation(), WaypointAcceptanceRadius))
	{
		HandleMoveFailed(TEXT("no path to the entrance"));
	}
}

void USunsetCoveVisitorComponent::EnterPhase(ESunsetCoveVisitorPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	const ESunsetCoveVisitorPhase OldPhase = Phase;
	Phase = NewPhase;

	UE_VLOG(GetOwner(), LogSunsetCove, Log, TEXT("%s -> %s"), *PhaseToString(OldPhase), *PhaseToString(NewPhase));
	UE_LOG(LogSunsetCove, Verbose, TEXT("%s: %s -> %s"), *GetNameSafe(GetOwner()), *PhaseToString(OldPhase), *PhaseToString(NewPhase));

	OnPhaseChanged.Broadcast(NewPhase, OldPhase);
}

void USunsetCoveVisitorComponent::ArriveAtEntrance()
{
	EnterPhase(ESunsetCoveVisitorPhase::AtEntrance);

	// SUNSETCOVE GAME LOGIC: pay for access, borrow items, queue, or wait at a closed barrier. For now the visitor only
	// stops for a moment, so the step stays visible.
	const float Pause = FMath::FRandRange(FMath::Min(EntrancePauseMin, EntrancePauseMax), FMath::Max(EntrancePauseMin, EntrancePauseMax));
	if (Pause > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(FlowTimer, this, &USunsetCoveVisitorComponent::ChooseSpotAndGo, Pause, false);
	}
	else
	{
		ChooseSpotAndGo();
	}
}

void USunsetCoveVisitorComponent::ChooseSpotAndGo()
{
	// A leave is pending (shift over, random delay): StartLeaving takes over soon, start nothing new.
	if (bLeaveRequested || Phase == ESunsetCoveVisitorPhase::Leaving || Phase == ESunsetCoveVisitorPhase::Left)
	{
		return;
	}

	// Too many unreachable spots in a row: wait and look again later instead of testing paths to every spot now.
	if (SpotsTriedThisSearch >= MaxSpotsPerSearch)
	{
		BeginWaitingForSpot();
		return;
	}

	USunsetCoveRestSpotComponent* Spot = nullptr;
	if (const USunsetCoveRestSpotSubsystem* Spots = USunsetCoveRestSpotSubsystem::Get(this))
	{
		FSunsetCoveRestSpotQuery Query;
		Query.Origin = GetOwner()->GetActorLocation();
		Query.PreferredType = DesiredSpotType;
		Query.bAllowOtherTypes = bAcceptOtherSpotTypes;
		Query.PersonalSpaceRadius = PersonalSpaceRadius;
		Query.Excluded = SkippedSpots;
		Spot = Spots->FindFreeSpot(Query);
	}

	if (!Spot || !ClaimSpot(Spot))
	{
		BeginWaitingForSpot();
		return;
	}

	++SpotsTriedThisSearch;
	MoveToClaimedSpot();
}

void USunsetCoveVisitorComponent::MoveToClaimedSpot()
{
	USunsetCoveRestSpotComponent* Spot = ClaimedSpot.Get();
	if (!Spot)
	{
		ChooseSpotAndGo();
		return;
	}

	EnterPhase(ESunsetCoveVisitorPhase::GoingToSpot);
	MoveRetries = 0;

	if (!RequestMove(Spot->GetApproachLocation(), SpotAcceptanceRadius))
	{
		// No path at all: trying the same spot again will not help. Recalculate the choice.
		UE_VLOG(GetOwner(), LogSunsetCove, Log, TEXT("No path to spot %s, skipping it."), *GetNameSafe(Spot->GetOwner()));
		SkippedSpots.AddUnique(Spot);
		ReleaseSpot();
		ChooseSpotAndGo();
	}
}

void USunsetCoveVisitorComponent::BeginWaitingForSpot()
{
	EnterPhase(ESunsetCoveVisitorPhase::WaitingForSpot);

	const float Now = GetWorld()->GetTimeSeconds();
	if (WaitingSince < 0.f)
	{
		WaitingSince = Now;
	}

	if (Now - WaitingSince >= MaxWaitForSpot)
	{
		UE_LOG(LogSunsetCove, Log, TEXT("%s: no reachable rest spot for %.0f s, leaving the cove."), *GetNameSafe(GetOwner()), Now - WaitingSince);
		Leave(ESunsetCoveVisitorLeaveReason::NoSpotAvailable);
		return;
	}

	// SUNSETCOVE GAME LOGIC: show the "no place for me" request icon here (GDD: requests above the head).
	GetWorld()->GetTimerManager().SetTimer(FlowTimer, this, &USunsetCoveVisitorComponent::RetrySpotSearch, SpotRetryInterval, false);
}

void USunsetCoveVisitorComponent::RetrySpotSearch()
{
	// Things change while waiting (visitors leave, props move): every spot gets a fresh chance.
	SkippedSpots.Reset();
	SpotsTriedThisSearch = 0;
	ChooseSpotAndGo();
}

void USunsetCoveVisitorComponent::ArriveAtSpot()
{
	// The shift ended on the way: no point sitting down just to stand up again.
	if (bLeaveRequested)
	{
		StartLeaving();
		return;
	}

	USunsetCoveRestSpotComponent* Spot = ClaimedSpot.Get();
	if (!Spot || Spot->GetClaimant() != this)
	{
		ClaimedSpot.Reset();
		ChooseSpotAndGo();
		return;
	}

	WaitingSince = -1.f;
	SpotsTriedThisSearch = 0;
	SkippedSpots.Reset();

	StandFeetLocation = GetFeetLocation();
	Spot->SetOccupied(this, true);
	EnterPhase(ESunsetCoveVisitorPhase::Resting);

	if (ASunsetCoveNPCCharacter* NPC = GetNPC())
	{
		NPC->BeginRest(Spot->GetRestFeetTransform(), Spot->PickRestActivity());
	}

	// SUNSETCOVE GAME LOGIC: resting is where satisfaction grows or drops (shade, neighbours, litter) and where the
	// Miro activities (swim, fish, walk and chat) can start from, coming back to this spot afterwards.
}

void USunsetCoveVisitorComponent::Leave(ESunsetCoveVisitorLeaveReason Reason)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// Not started, already going, or gone.
	if (Phase == ESunsetCoveVisitorPhase::Idle || bLeaveRequested || Phase == ESunsetCoveVisitorPhase::Leaving || Phase == ESunsetCoveVisitorPhase::Left)
	{
		return;
	}

	bLeaveRequested = true;
	LeaveReason = Reason;

	const float Delay = (Reason == ESunsetCoveVisitorLeaveReason::ShiftOver && LeaveDelayMax > 0.f) ? FMath::FRandRange(0.f, LeaveDelayMax) : 0.f;
	UE_VLOG(Owner, LogSunsetCove, Log, TEXT("Leave requested (%s) in %.1f s"), *UEnum::GetDisplayValueAsText(Reason).ToString(), Delay);

	if (Delay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(LeaveTimer, this, &USunsetCoveVisitorComponent::StartLeaving, Delay, false);
	}
	else
	{
		StartLeaving();
	}
}

void USunsetCoveVisitorComponent::StartLeaving()
{
	if (Phase == ESunsetCoveVisitorPhase::Leaving || Phase == ESunsetCoveVisitorPhase::Left)
	{
		return;
	}

	// Whatever was going on (a stop at the barrier, a retry, a search, a pending leave delay) is over.
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	Timers.ClearTimer(FlowTimer);
	Timers.ClearTimer(LeaveTimer);
	StopCurrentMove();

	const bool bWasResting = (Phase == ESunsetCoveVisitorPhase::Resting);
	ReleaseSpot();
	EnterPhase(ESunsetCoveVisitorPhase::Leaving);
	MoveRetries = 0;

	ASunsetCoveNPCCharacter* NPC = GetNPC();
	if (NPC && (bWasResting || NPC->IsResting()))
	{
		NPC->EndRest(StandFeetLocation, FSimpleDelegate::CreateUObject(this, &USunsetCoveVisitorComponent::WalkToExit));
	}
	else
	{
		WalkToExit();
	}
}

void USunsetCoveVisitorComponent::WalkToExit()
{
	if (Phase != ESunsetCoveVisitorPhase::Leaving)
	{
		return;
	}

	if (!RequestMove(ExitLocation, WaypointAcceptanceRadius))
	{
		HandleMoveFailed(TEXT("no path to the exit"));
	}
}

void USunsetCoveVisitorComponent::FinishVisit()
{
	if (Phase == ESunsetCoveVisitorPhase::Left)
	{
		return;
	}

	ClearTimers();
	StopCurrentMove();
	ReleaseSpot();
	EnterPhase(ESunsetCoveVisitorPhase::Left);
	OnLeftCove.Broadcast(this, LeaveReason);

	// Removed a moment later rather than now: this often runs inside the AI controller's own move callback.
	if (AActor* Owner = GetOwner())
	{
		Owner->SetLifeSpan(0.1f);
	}
}

bool USunsetCoveVisitorComponent::AssignSpot(USunsetCoveRestSpotComponent* Spot)
{
	const AActor* Owner = GetOwner();
	if (!Spot || !Owner || !Owner->HasAuthority())
	{
		return false;
	}

	if (Phase == ESunsetCoveVisitorPhase::Idle || bLeaveRequested || Phase == ESunsetCoveVisitorPhase::Leaving || Phase == ESunsetCoveVisitorPhase::Left)
	{
		return false;
	}

	if (Spot == ClaimedSpot.Get())
	{
		return true;
	}

	if (!Spot->IsFree())
	{
		return false;
	}

	GetWorld()->GetTimerManager().ClearTimer(FlowTimer);
	StopCurrentMove();

	const bool bWasResting = (Phase == ESunsetCoveVisitorPhase::Resting);
	ReleaseSpot();
	if (!ClaimSpot(Spot))
	{
		return false;
	}

	ASunsetCoveNPCCharacter* NPC = GetNPC();
	if (NPC && (bWasResting || NPC->IsResting()))
	{
		EnterPhase(ESunsetCoveVisitorPhase::GoingToSpot);
		NPC->EndRest(StandFeetLocation, FSimpleDelegate::CreateUObject(this, &USunsetCoveVisitorComponent::MoveToClaimedSpot));
	}
	else
	{
		MoveToClaimedSpot();
	}
	return true;
}

// ----------------------------------------------------------------------
// Moves
// ----------------------------------------------------------------------

bool USunsetCoveVisitorComponent::RequestMove(const FVector& Goal, float AcceptanceRadius)
{
	ASunsetCoveAIController* AI = GetAIController();
	if (!AI)
	{
		return false;
	}

	GetWorld()->GetTimerManager().ClearTimer(MoveTimeoutTimer);
	CurrentGoal = Goal;
	CurrentAcceptance = AcceptanceRadius;
	CurrentMoveId = FAIRequestID::InvalidRequest;
	++MoveSerial;

	FAIMoveRequest Request(Goal);
	Request.SetAcceptanceRadius(AcceptanceRadius);
	Request.SetUsePathfinding(true);
	Request.SetProjectGoalLocation(true);
	Request.SetAllowPartialPath(false);	// a partial path would leave the visitor somewhere random
	Request.SetCanStrafe(false);
	Request.SetReachTestIncludesAgentRadius(false);

	// MoveTo can finish moves synchronously (the previous move is aborted, or the goal is already reached): those
	// results are handled from the return value below, never through HandleMoveFinished.
	bIssuingMove = true;
	const FPathFollowingRequestResult Result = AI->MoveTo(Request);
	bIssuingMove = false;

	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		CurrentMoveId = Result.MoveId;
		StartMoveTimeout(Goal);
		UE_VLOG_SEGMENT(GetOwner(), LogSunsetCove, Verbose, GetOwner()->GetActorLocation(), Goal, FColor::Cyan, TEXT("Move %u"), CurrentMoveId.GetID());
		return true;
	}

	if (Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// Finished next frame, outside MoveTo, so the flow never re-enters itself. The serial drops it if anything
		// else happens first.
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &USunsetCoveVisitorComponent::HandleImmediateArrival, MoveSerial));
		return true;
	}

	return false;
}

void USunsetCoveVisitorComponent::StopCurrentMove()
{
	GetWorld()->GetTimerManager().ClearTimer(MoveTimeoutTimer);
	++MoveSerial;

	if (!CurrentMoveId.IsValid())
	{
		return;
	}

	// Forgotten first, so the "aborted" result that StopMovement reports is ignored.
	CurrentMoveId = FAIRequestID::InvalidRequest;
	if (ASunsetCoveAIController* AI = GetAIController())
	{
		AI->StopMovement();
	}
}

void USunsetCoveVisitorComponent::HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (bIssuingMove || !CurrentMoveId.IsValid() || RequestID.GetID() != CurrentMoveId.GetID())
	{
		return;
	}

	CurrentMoveId = FAIRequestID::InvalidRequest;
	GetWorld()->GetTimerManager().ClearTimer(MoveTimeoutTimer);

	if (Result.IsSuccess())
	{
		HandleMoveSucceeded();
	}
	else
	{
		HandleMoveFailed(Result.ToString());
	}
}

void USunsetCoveVisitorComponent::HandleImmediateArrival(uint32 Serial)
{
	if (Serial == MoveSerial && !CurrentMoveId.IsValid())
	{
		HandleMoveSucceeded();
	}
}

void USunsetCoveVisitorComponent::HandleMoveSucceeded()
{
	MoveRetries = 0;

	switch (Phase)
	{
	case ESunsetCoveVisitorPhase::ApproachingEntrance:
		ArriveAtEntrance();
		break;
	case ESunsetCoveVisitorPhase::GoingToSpot:
		ArriveAtSpot();
		break;
	case ESunsetCoveVisitorPhase::Leaving:
		FinishVisit();
		break;
	default:
		break;
	}
}

void USunsetCoveVisitorComponent::HandleMoveFailed(const FString& Reason)
{
	UE_VLOG(GetOwner(), LogSunsetCove, Log, TEXT("Move failed in %s: %s (retry %d of %d)"), *PhaseToString(Phase), *Reason, MoveRetries, MaxMoveRetries);

	// First rule of recovery: recalculate the path to the same goal a couple of times. Crowds clear up.
	if (MoveRetries < MaxMoveRetries)
	{
		++MoveRetries;
		GetWorld()->GetTimerManager().SetTimer(FlowTimer, this, &USunsetCoveVisitorComponent::RetryCurrentMove, FMath::Max(RetryDelay, 0.01f), false);
		return;
	}
	MoveRetries = 0;

	switch (Phase)
	{
	case ESunsetCoveVisitorPhase::ApproachingEntrance:
		UE_LOG(LogSunsetCove, Log, TEXT("%s: cannot reach the entrance (%s). Looking for a spot from here."), *GetNameSafe(GetOwner()), *Reason);
		ChooseSpotAndGo();
		break;

	case ESunsetCoveVisitorPhase::GoingToSpot:
		// Second rule: change the destination.
		if (USunsetCoveRestSpotComponent* Spot = ClaimedSpot.Get())
		{
			SkippedSpots.AddUnique(Spot);
		}
		ReleaseSpot();
		ChooseSpotAndGo();
		break;

	case ESunsetCoveVisitorPhase::Leaving:
		// Last rule: the guided exit. The visitor must not stay on the beach after the shift.
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: cannot reach the exit (%s). Removed where it stands. Check the navmesh between the beach and the visitor spawner."),
			*GetNameSafe(GetOwner()), *Reason);
		FinishVisit();
		break;

	default:
		break;
	}
}

void USunsetCoveVisitorComponent::RetryCurrentMove()
{
	FVector Goal = CurrentGoal;

	// A spot may have moved a little since the last try (a prop nudged): ask it again.
	if (Phase == ESunsetCoveVisitorPhase::GoingToSpot)
	{
		const USunsetCoveRestSpotComponent* Spot = ClaimedSpot.Get();
		if (!Spot)
		{
			ChooseSpotAndGo();
			return;
		}
		Goal = Spot->GetApproachLocation();
	}

	if (!RequestMove(Goal, CurrentAcceptance))
	{
		HandleMoveFailed(TEXT("no path"));
	}
}

void USunsetCoveVisitorComponent::StartMoveTimeout(const FVector& Goal)
{
	const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), Goal);
	const float Timeout = MoveTimeoutExtra + MoveTimeoutFactor * Distance / FMath::Max(GetWalkSpeed(), 50.f);
	GetWorld()->GetTimerManager().SetTimer(MoveTimeoutTimer, this, &USunsetCoveVisitorComponent::HandleMoveTimeout, Timeout, false);
}

void USunsetCoveVisitorComponent::HandleMoveTimeout()
{
	if (!CurrentMoveId.IsValid())
	{
		return;
	}

	StopCurrentMove();
	HandleMoveFailed(TEXT("took too long"));
}

// ----------------------------------------------------------------------
// Spots
// ----------------------------------------------------------------------

bool USunsetCoveVisitorComponent::ClaimSpot(USunsetCoveRestSpotComponent* Spot)
{
	if (!Spot || !Spot->Claim(this, FSimpleDelegate::CreateUObject(this, &USunsetCoveVisitorComponent::HandleSpotLost)))
	{
		return false;
	}

	ClaimedSpot = Spot;
	return true;
}

void USunsetCoveVisitorComponent::ReleaseSpot()
{
	if (USunsetCoveRestSpotComponent* Spot = ClaimedSpot.Get())
	{
		Spot->Release(this);
	}
	ClaimedSpot.Reset();
}

void USunsetCoveVisitorComponent::HandleSpotLost()
{
	// The spot already dropped the claim.
	ClaimedSpot.Reset();

	if (bLeaveRequested || Phase == ESunsetCoveVisitorPhase::Leaving || Phase == ESunsetCoveVisitorPhase::Left)
	{
		return;
	}

	// Next frame: the spot is in the middle of being disabled or destroyed.
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &USunsetCoveVisitorComponent::RecoverFromLostSpot));
}

void USunsetCoveVisitorComponent::RecoverFromLostSpot()
{
	if (bLeaveRequested || ClaimedSpot.IsValid()
		|| Phase == ESunsetCoveVisitorPhase::Leaving || Phase == ESunsetCoveVisitorPhase::Left)
	{
		return;
	}

	UE_VLOG(GetOwner(), LogSunsetCove, Log, TEXT("Spot lost while %s, picking another one."), *PhaseToString(Phase));
	GetWorld()->GetTimerManager().ClearTimer(FlowTimer);
	StopCurrentMove();
	SpotsTriedThisSearch = 0;

	ASunsetCoveNPCCharacter* NPC = GetNPC();
	if (NPC && NPC->IsResting())
	{
		EnterPhase(ESunsetCoveVisitorPhase::WaitingForSpot);
		NPC->EndRest(StandFeetLocation, FSimpleDelegate::CreateUObject(this, &USunsetCoveVisitorComponent::ChooseSpotAndGo));
		return;
	}

	ChooseSpotAndGo();
}

FGameplayTag USunsetCoveVisitorComponent::RollDesiredSpotType() const
{
	float Total = 0.f;
	for (const TPair<FGameplayTag, float>& Entry : SpotTypeWeights)
	{
		if (Entry.Key.IsValid() && Entry.Value > 0.f)
		{
			Total += Entry.Value;
		}
	}

	if (Total <= 0.f)
	{
		return FGameplayTag();
	}

	float Roll = FMath::FRandRange(0.f, Total);
	FGameplayTag Last;
	for (const TPair<FGameplayTag, float>& Entry : SpotTypeWeights)
	{
		if (!Entry.Key.IsValid() || Entry.Value <= 0.f)
		{
			continue;
		}

		Last = Entry.Key;
		Roll -= Entry.Value;
		if (Roll <= 0.f)
		{
			return Entry.Key;
		}
	}
	return Last;
}

// ----------------------------------------------------------------------
// Shift
// ----------------------------------------------------------------------

void USunsetCoveVisitorComponent::HandleShiftEnded()
{
	Leave(ESunsetCoveVisitorLeaveReason::ShiftOver);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

USunsetCoveRestSpotComponent* USunsetCoveVisitorComponent::GetClaimedSpot() const
{
	return ClaimedSpot.Get();
}

ASunsetCoveAIController* USunsetCoveVisitorComponent::GetAIController() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Cast<ASunsetCoveAIController>(Pawn->GetController()) : nullptr;
}

ASunsetCoveNPCCharacter* USunsetCoveVisitorComponent::GetNPC() const
{
	return Cast<ASunsetCoveNPCCharacter>(GetOwner());
}

FVector USunsetCoveVisitorComponent::GetFeetLocation() const
{
	const AActor* Owner = GetOwner();
	return Owner->GetActorLocation() - FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight());
}

float USunsetCoveVisitorComponent::GetWalkSpeed() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	return Movement ? Movement->GetMaxSpeed() : 150.f;
}

void USunsetCoveVisitorComponent::BindToController(ASunsetCoveAIController* Controller)
{
	if (BoundController.Get() == Controller)
	{
		return;
	}

	UnbindFromController();
	MoveFinishedHandle = Controller->OnMoveFinished.AddUObject(this, &USunsetCoveVisitorComponent::HandleMoveFinished);
	BoundController = Controller;
}

void USunsetCoveVisitorComponent::UnbindFromController()
{
	if (ASunsetCoveAIController* Controller = BoundController.Get())
	{
		Controller->OnMoveFinished.Remove(MoveFinishedHandle);
	}
	BoundController.Reset();
	MoveFinishedHandle.Reset();
}

void USunsetCoveVisitorComponent::ClearTimers()
{
	if (const UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(FlowTimer);
		Timers.ClearTimer(MoveTimeoutTimer);
		Timers.ClearTimer(LeaveTimer);
	}
}

void USunsetCoveVisitorComponent::DrawDebugState() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	const float Duration = DebugTickInterval * 1.5f;
	const FVector Location = Owner->GetActorLocation();

	FString Label = PhaseToString(Phase);
	if (DesiredSpotType.IsValid())
	{
		Label += FString::Printf(TEXT("\nwants %s"), *TagLeaf(DesiredSpotType));
	}
	DrawDebugString(World, Location + FVector(0.f, 0.f, Owner->GetSimpleCollisionHalfHeight() + 40.f), Label, nullptr, FColor::White, Duration, true, 1.f);

	if (CurrentMoveId.IsValid())
	{
		DrawDebugLine(World, Location, CurrentGoal, FColor::Cyan, false, Duration, 0, 1.5f);
	}

	if (const USunsetCoveRestSpotComponent* Spot = ClaimedSpot.Get())
	{
		DrawDebugLine(World, Location, Spot->GetComponentLocation(), FColor::Yellow, false, Duration, 0, 0.5f);
	}
#endif // ENABLE_DRAW_DEBUG
}
