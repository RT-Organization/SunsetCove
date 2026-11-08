// SunsetCoveActionCommit.h
// SunsetCove - Core / Utils. The "when does the item really move" contract between the inventories and whoever plays the animation.
//
// WHY THIS EXISTS
//   Both inventories (props in the hands, tools) change their LOGICAL state the instant the player acts: the hand is
//   occupied, the tool is equipped, the slot is empty. That never waits for an animation.
//   What can wait is the PHYSICAL effect: attaching a prop to the hand socket, detaching it, moving it to the placement
//   transform, launching it, showing or hiding a tool. A first-person arm animation lasts 0.3 - 0.6 s, so the effect looks
//   right only if it lands at a chosen moment of the animation ("the hand closes", "the arm swings forward").
//
// HOW IT WORKS (nothing here knows about animation)
//   Each inventory has a C++ hook (ActionBeginHook). When an action is accepted, the inventory calls it. The listener (the arms
//   animation layer, Public/AnimationSystem/ArmsAnimation/) starts whatever it wants and answers with an FActionCommitTiming:
//     Immediate   The effect happens now. Same as an inventory with no hook bound (the default).
//     AfterDelay  The effect happens DelaySeconds later (a timer).
//     OnNotify    The effect waits for CommitPendingAction() on the inventory (the arms animation calls it from an anim notify).
//                 TimeoutSeconds is a safety net: the effect happens anyway when it runs out, so a missing notify can never
//                 leave an item stuck.
//   A new action, a cancel, the end of play or an unpossess FLUSHES a pending effect at once, so the end state is always consistent.

#pragma once

#include "CoreMinimal.h"
#include "SunsetCoveActionCommit.generated.h"

/** When the physical effect of an inventory action happens, relative to the moment the player acted. */
UENUM(BlueprintType)
enum class EActionCommitMode : uint8
{
	/** Right away (no animation sync). */
	Immediate	UMETA(DisplayName = "Immediate"),
	/** After a fixed number of seconds. */
	AfterDelay	UMETA(DisplayName = "After Delay"),
	/** When the animation tells the inventory (an anim notify), or when the timeout runs out. */
	OnNotify	UMETA(DisplayName = "On Notify")
};

/** The answer of an ActionBeginHook: how the effect of the action must be timed. */
USTRUCT(BlueprintType)
struct FActionCommitTiming
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commit")
	EActionCommitMode Mode = EActionCommitMode::Immediate;

	/** AfterDelay: seconds to wait. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commit", meta = (ClampMin = "0.0", Units = "s"))
	float DelaySeconds = 0.0f;

	/** OnNotify: the effect happens anyway after this many seconds if the notify never comes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Commit", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 1.0f;

	static FActionCommitTiming MakeImmediate() { return FActionCommitTiming(); }
};
