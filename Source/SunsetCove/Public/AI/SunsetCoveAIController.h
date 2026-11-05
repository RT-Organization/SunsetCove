// SunsetCoveAIController.h
// SunsetCove - AI area. The controller of every SunsetCove NPC: it walks the NPC body along the navmesh.
//
// WHAT THIS CLASS IS
//   The legs of the NPC brain. Behavior components on the pawn (for example USunsetCoveVisitorComponent) decide where to
//   go and call MoveTo; this controller asks the navigation system for a path and follows it, and reports the result
//   through OnMoveFinished. It holds no game rules, so one class serves every NPC type: visitors, police, bodyguards.
//
// HIERARCHY
//   AAIController -> ASunsetCoveAIController (this class)
//   CoreFramework has no AI classes. The NPC body (ASunsetCoveNPCCharacter) names this class as its AI Controller Class,
//   so no Blueprint is needed. Add a Blueprint child only to change the tunables below per NPC type.
//
// CROWD AVOIDANCE
//   The path following component is a UCrowdFollowingComponent (Detour crowd). Visitors weave around each other on
//   the narrow sandy path instead of pushing, and slow down near the goal. RVO avoidance on the movement component
//   stays off (ASunsetCoveNPCCharacter turns it off): RVO and Detour fight each other.
//   The engine allows 50 crowd agents per world (Project Settings > Crowd Manager > Max Agents). NPCs above that walk
//   without avoidance; raise the limit together with the visitor cap of the spawners.
//
// STUCK DETECTION
//   Path following reports a move as Blocked when the NPC moved less than StuckDistance during StuckTime. Behaviors
//   react to that result (try again, pick another goal, leave): see USunsetCoveVisitorComponent.
//
// NETWORK
//   AI controllers exist only where the pawn has authority (the server, or the single player game).

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SunsetCoveAIController.generated.h"

struct FPathFollowingResult;

/** A move requested through MoveTo ended (success, blocked, aborted, invalid...). Native only, bind with AddUObject. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FSunsetCoveMoveFinishedSignature, FAIRequestID /*RequestID*/, const FPathFollowingResult& /*Result*/);

/** Crowd avoidance quality. Same values as the engine's ECrowdAvoidanceQuality, which the editor cannot show. */
UENUM(BlueprintType)
enum class ESunsetCoveAvoidanceQuality : uint8
{
	Low		UMETA(DisplayName = "Low"),
	Medium	UMETA(DisplayName = "Medium"),
	Good	UMETA(DisplayName = "Good"),
	High	UMETA(DisplayName = "High")
};

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveAIController : public AAIController
{
	GENERATED_BODY()

public:
	/** Replaces the path following component with the crowd version. Do not drop the initializer. */
	ASunsetCoveAIController(const FObjectInitializer& ObjectInitializer);

	//~ Begin AAIController Interface
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	//~ End AAIController Interface

	/** Every finished move, with its request id. Ignore ids you did not request: aborted older moves report here too. */
	FSunsetCoveMoveFinishedSignature OnMoveFinished;

	// ------------------------------------------------------------------
	// Configuration (Class Defaults of a Blueprint child, if one is ever needed)
	// ------------------------------------------------------------------

	/** Crowd avoidance quality. Higher looks better and costs more; Medium is plenty for a beach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|AI")
	ESunsetCoveAvoidanceQuality AvoidanceQuality = ESunsetCoveAvoidanceQuality::Medium;

	/** Keeps a little distance from other NPCs (personal space), even when nobody is about to collide. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|AI")
	bool bKeepPersonalSpace = true;

	/** Strength of the personal space push. The engine default is 2. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|AI", meta = (ClampMin = "0.0", EditCondition = "bKeepPersonalSpace"))
	float PersonalSpaceWeight = 2.f;

	/** Stuck detection: a move fails as Blocked when the NPC travels less than this distance during StuckTime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|AI", meta = (ClampMin = "1.0", Units = "cm"))
	float StuckDistance = 25.f;

	/** Stuck detection window. Long enough for crowd jams at the barrier to sort themselves out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|AI", meta = (ClampMin = "0.5", Units = "s"))
	float StuckTime = 4.f;

protected:
	//~ Begin AController Interface
	virtual void OnPossess(APawn* InPawn) override;
	//~ End AController Interface

private:
	/** Pushes the tunables above into the crowd following component. */
	void ApplyCrowdSettings();
};
