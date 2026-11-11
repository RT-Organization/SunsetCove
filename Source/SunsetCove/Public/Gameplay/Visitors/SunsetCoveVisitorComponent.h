// SunsetCoveVisitorComponent.h
// SunsetCove - Gameplay area (Visitors). The decisions of one visitor: the "common case" of the Miro NPC board.
//
// WHAT THIS CLASS IS
//   The brain of a visitor, as a component on its body (BP_Visitor = ASunsetCoveNPCCharacter + this component). It runs
//   the Miro flow "COMMON CASE TO PROTOTYPE NAVIGATION":
//     spawned at the hidden end of the sandy path -> walks to the barrier ("Approach Entrance") -> goes in (paying
//     comes later) -> claims a rest spot it likes, the sunbed, and walks there ("Walk to desired rest
//     position") -> sits or lies down ("Rest") -> stays until the shift is over ("SHIFT OVER") -> walks back down the
//     path ("Leave Cove") -> removed at the exit, out of the player's sight.
//   On the Miro NPC model this is the "Thinking" layer: picking, pathfinding, updating and recalculating. The body
//   (ASunsetCoveNPCCharacter) is the "Acting" layer. The "Sensing" layer (emotions, sympathies, satisfaction) will be
//   another component next to this one, feeding it preferences.
//
// HIERARCHY
//   UActorComponent -> USunsetCoveVisitorComponent (this class), added in the Components panel of BP_Visitor.
//   The body class is the same for every humanoid NPC; this component is what makes the body a visitor.
//
// HOW IT MOVES
//   Through ASunsetCoveAIController (the body's AI Controller Class): MoveTo, then OnMoveFinished. Every move has a
//   request id, and results of older or aborted moves are ignored.
//
// RECOVERY RULES (GDD, "Percorsi e navigazione NPC")
//   A failed move (blocked, no path, or far slower than it should be) is tried again MaxMoveRetries times, RetryDelay
//   seconds apart. If it still fails:
//     to the barrier   the visitor skips the barrier step and looks for a spot from where it stands;
//     to a spot        the spot is released and skipped by this visitor, and another one is picked;
//     no spot at all   the visitor waits ("Waiting For Spot") and looks again every SpotRetryInterval seconds; after
//                      MaxWaitForSpot it gives up and leaves (the phase tells the problem; request icons come later);
//     to the exit      the visitor is removed where it stands (the GDD's "guided exit"), with a warning in the log so
//                      the level can be fixed.
//   A spot that goes away while claimed (prop picked up, disabled, destroyed) sends the visitor to another spot.
//
// SHIFT OVER
//   The visitor listens to USunsetCoveWorkdaySubsystem::OnShiftEnded and then leaves, whatever it was doing, after a
//   random delay of up to LeaveDelayMax seconds, so the whole beach does not jam the path at once.
//
// SETUP (BP_Visitor, Components panel: add "Sunset Cove Visitor", then its Details)
//   Spot Type Weights: how much this visitor type wants each kind of spot (Cove.Spot.*), for example Sunbed 3 (the only kind today).
//   Everything else has working defaults.
//
// DEBUG
//   Console: SunsetCove.Debug.Visitors 1 writes each visitor's phase above its head and draws its current goal.
//   In Play In Editor, select a visitor: its phase, wish and spot are in the Details panel (SunsetCove|Visitors|State).
//   The Visual Logger records every decision.
//
// NETWORK
//   Decisions run with authority only (the server, or the single player game). Nothing here replicates.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Visitors/SunsetCoveVisitorTypes.h"
#include "SunsetCoveVisitorComponent.generated.h"

class ASunsetCoveAIController;
class ASunsetCoveEntrance;
class ASunsetCoveNPCCharacter;
class USunsetCoveRestSpotComponent;
struct FPathFollowingResult;

/** The visitor moved to another phase of its visit. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSunsetCoveVisitorPhaseChangedSignature, ESunsetCoveVisitorPhase, NewPhase, ESunsetCoveVisitorPhase, OldPhase);

/** The visitor reached the exit (its body is removed right after). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSunsetCoveVisitorLeftSignature, USunsetCoveVisitorComponent*, Visitor, ESunsetCoveVisitorLeaveReason, Reason);

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API USunsetCoveVisitorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USunsetCoveVisitorComponent();

	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Flow (authority only)
	// ------------------------------------------------------------------

	/**
	 * Starts the visit. Called by ASunsetCoveVisitorSpawner right after the spawn.
	 * InEntrance: the barrier to walk to first (may be null: the visitor then looks for a spot at once).
	 * InExitLocation: where the visitor leaves the cove (the spawner, at the hidden end of the path).
	 */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Visitors")
	void StartVisit(ASunsetCoveEntrance* InEntrance, FVector InExitLocation);

	/** Sends the visitor home. Shift Over waits a random delay (LeaveDelayMax); the other reasons leave at once. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Visitors")
	void Leave(ESunsetCoveVisitorLeaveReason Reason);

	/**
	 * Sends the visitor to a given free spot, standing up first if needed. For the GDD feature where the player places
	 * visitors. Returns false if the spot is taken or the visitor is leaving.
	 */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Visitors")
	bool AssignSpot(USunsetCoveRestSpotComponent* Spot);

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SunsetCove|Visitors")
	ESunsetCoveVisitorPhase GetPhase() const { return Phase; }

	/** The kind of spot this visitor wants (Cove.Spot.*), rolled at the start of the visit. Empty = no preference. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Visitors")
	FGameplayTag GetDesiredSpotType() const { return DesiredSpotType; }

	/** The spot reserved or used by this visitor, or null. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Visitors")
	USunsetCoveRestSpotComponent* GetClaimedSpot() const;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|Visitors")
	FSunsetCoveVisitorPhaseChangedSignature OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|Visitors")
	FSunsetCoveVisitorLeftSignature OnLeftCove;

	// ------------------------------------------------------------------
	// Configuration (BP_Visitor, this component's Details)
	// ------------------------------------------------------------------

	/** How much this visitor wants each kind of spot. One kind is rolled per visit, weighted by these numbers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (Categories = "Cove.Spot"))
	TMap<FGameplayTag, float> SpotTypeWeights;

	/** When the wanted kind is full, take any free spot instead of waiting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors")
	bool bAcceptOtherSpotTypes = true;

	/** Spots with other visitors closer than this are less attractive. 0 = does not care. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "cm"))
	float PersonalSpaceRadius = 300.f;

	/** Shortest stop at the barrier (paying will happen here later). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float EntrancePauseMin = 0.5f;

	/** Longest stop at the barrier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float EntrancePauseMax = 1.5f;

	/** How close counts as "arrived" at the barrier and at the exit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "1.0", Units = "cm"))
	float WaypointAcceptanceRadius = 80.f;

	/** How close counts as "arrived" at a spot's approach point. The sit-down blend covers what is left. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "1.0", Units = "cm"))
	float SpotAcceptanceRadius = 30.f;

	/** A failed move is tried again this many times before the visitor changes its plan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0"))
	int32 MaxMoveRetries = 2;

	/** Seconds between two tries of a failed move. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float RetryDelay = 1.5f;

	/** Unreachable spots tried in a row before the visitor stops and waits (keeps one bad frame from testing every spot). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "1"))
	int32 MaxSpotsPerSearch = 5;

	/** While waiting for a spot, seconds between two searches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.5", Units = "s"))
	float SpotRetryInterval = 4.f;

	/** Without a spot for this long, the visitor gives up and leaves. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float MaxWaitForSpot = 60.f;

	/** Shift over: each visitor leaves after a random delay up to this, so they do not all jam the path. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float LeaveDelayMax = 8.f;

	/** A move fails as too slow after (straight distance / walk speed) x this factor + MoveTimeoutExtra. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "1.0"))
	float MoveTimeoutFactor = 3.f;

	/** Fixed part of the move timeout (paths are not straight lines, and crowds slow everyone down). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Visitors", meta = (ClampMin = "0.0", Units = "s"))
	float MoveTimeoutExtra = 15.f;

protected:
	// ------------------------------------------------------------------
	// State (read only, visible on the selected visitor in Play In Editor)
	// ------------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "SunsetCove|Visitors|State")
	ESunsetCoveVisitorPhase Phase = ESunsetCoveVisitorPhase::Idle;

	UPROPERTY(VisibleInstanceOnly, Transient, BlueprintReadOnly, Category = "SunsetCove|Visitors|State")
	FGameplayTag DesiredSpotType;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "SunsetCove|Visitors|State")
	TWeakObjectPtr<USunsetCoveRestSpotComponent> ClaimedSpot;

private:
	// Flow steps, in visit order
	void EnterPhase(ESunsetCoveVisitorPhase NewPhase);
	void ArriveAtEntrance();
	void ChooseSpotAndGo();
	void MoveToClaimedSpot();
	void BeginWaitingForSpot();
	void RetrySpotSearch();
	void ArriveAtSpot();
	void StartLeaving();
	void WalkToExit();
	void FinishVisit();

	// Moves
	bool RequestMove(const FVector& Goal, float AcceptanceRadius);
	void StopCurrentMove();
	void HandleMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void HandleImmediateArrival(uint32 Serial);
	void HandleMoveSucceeded();
	void HandleMoveFailed(const FString& Reason);
	void RetryCurrentMove();
	void StartMoveTimeout(const FVector& Goal);
	void HandleMoveTimeout();

	// Spots
	bool ClaimSpot(USunsetCoveRestSpotComponent* Spot);
	void ReleaseSpot();
	void HandleSpotLost();
	void RecoverFromLostSpot();
	FGameplayTag RollDesiredSpotType() const;

	// Shift
	UFUNCTION()
	void HandleShiftEnded();

	// Helpers
	ASunsetCoveAIController* GetAIController() const;
	ASunsetCoveNPCCharacter* GetNPC() const;
	FVector GetFeetLocation() const;
	float GetWalkSpeed() const;
	void BindToController(ASunsetCoveAIController* Controller);
	void UnbindFromController();
	void ClearTimers();
	void DrawDebugState() const;

	/** The barrier to walk to first (StartVisit). */
	TWeakObjectPtr<ASunsetCoveEntrance> Entrance;

	/** Where the visit ends (StartVisit). */
	FVector ExitLocation = FVector::ZeroVector;

	/** Where the visitor stood before resting: it gets up here again. */
	FVector StandFeetLocation = FVector::ZeroVector;

	/** Spots this visitor could not reach during the current search. */
	TArray<TWeakObjectPtr<USunsetCoveRestSpotComponent>> SkippedSpots;
	int32 SpotsTriedThisSearch = 0;

	/** World time the visitor started waiting for a spot, or -1 when it is not waiting. Kept across retries. */
	float WaitingSince = -1.f;

	// Current move
	FAIRequestID CurrentMoveId;
	FVector CurrentGoal = FVector::ZeroVector;
	float CurrentAcceptance = 50.f;
	int32 MoveRetries = 0;
	uint32 MoveSerial = 0;
	bool bIssuingMove = false;

	// Leaving
	bool bLeaveRequested = false;
	ESunsetCoveVisitorLeaveReason LeaveReason = ESunsetCoveVisitorLeaveReason::ShiftOver;

	// Timers
	FTimerHandle FlowTimer;
	FTimerHandle MoveTimeoutTimer;
	FTimerHandle LeaveTimer;

	// Controller binding
	TWeakObjectPtr<ASunsetCoveAIController> BoundController;
	FDelegateHandle MoveFinishedHandle;
};
