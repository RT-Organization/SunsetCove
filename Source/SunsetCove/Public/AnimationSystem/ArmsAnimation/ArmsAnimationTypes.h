// ArmsAnimationTypes.h
// SunsetCove - Animation System / Arms Animation. The enums and structs of the first-person arms animation layer.
//
// Kept apart from the component, the data asset and the Animation Instance so that each of them includes only what it needs.
// See ArmsAnimationComponent.h for the role of the whole system.

#pragma once

#include "CoreMinimal.h"
#include "Core/Utils/SunsetCoveActionCommit.h"
#include "ArmsAnimationTypes.generated.h"

class UAnimSequence;

/** The two arms. The right arm carries Prop_R / Tool_R, the left arm carries Prop_L (Tool_L is reserved). */
UENUM(BlueprintType)
enum class EArmSide : uint8
{
	Right	UMETA(DisplayName = "Right"),
	Left	UMETA(DisplayName = "Left")
};

/**
 * Everything the animator delivers per item and per hand (see Docs/InventorySystem/Guida_Animatore.md, section 9).
 * The names are the <Action> of the file name AS_Arms_<Item>_<Action>_<Hand>: do not rename a value without telling the animator.
 */
UENUM(BlueprintType)
enum class EArmsAnimAction : uint8
{
	// ---- Poses (they last: a pose is held until something changes it) ----
	/** The item is in the hand. A loop. */
	Hold		UMETA(DisplayName = "Hold"),
	/** A prop is being placed (the hologram is visible). A loop. Optional. */
	PlaceAim	UMETA(DisplayName = "Place Aim"),
	/** The throw is charging. NOT a loop: it ends in the pose of the charge and stays there until the release. */
	ThrowCharge	UMETA(DisplayName = "Throw Charge"),

	// ---- One-shots (they happen and end; they are played as montages) ----
	Grab		UMETA(DisplayName = "Grab"),
	Place		UMETA(DisplayName = "Place"),
	Throw		UMETA(DisplayName = "Throw"),
	Drop		UMETA(DisplayName = "Drop"),
	Equip		UMETA(DisplayName = "Equip"),
	Holster		UMETA(DisplayName = "Holster"),
	/** Use of a tool. A loop while the use action is held when the clip says so (the spray). */
	Use			UMETA(DisplayName = "Use"),
	Pickup		UMETA(DisplayName = "Pickup"),
	/** The radial tool menu opened (generic item id "ToolMenu"). Optional. */
	Open		UMETA(DisplayName = "Open"),
	/** The radial tool menu closed (generic item id "ToolMenu"). Optional. */
	Close		UMETA(DisplayName = "Close")
};

/** The pose an arm holds in the Animation Blueprint (a Blend Poses by Enum selects it). */
UENUM(BlueprintType)
enum class EArmHeldPose : uint8
{
	/** The arm shows the locomotion pose of the base Animation Blueprint. */
	None		UMETA(DisplayName = "None"),
	Hold		UMETA(DisplayName = "Hold"),
	PlaceAim	UMETA(DisplayName = "Place Aim"),
	ThrowCharge	UMETA(DisplayName = "Throw Charge")
};

namespace ArmsAnim
{
	/** Hold, PlaceAim and ThrowCharge are poses; everything else is a one-shot. */
	inline bool IsPoseAction(EArmsAnimAction Action)
	{
		return Action == EArmsAnimAction::Hold || Action == EArmsAnimAction::PlaceAim || Action == EArmsAnimAction::ThrowCharge;
	}

	/** The actions where an item changes hands: the commit timing applies to them. */
	inline bool IsTimedAction(EArmsAnimAction Action)
	{
		switch (Action)
		{
		case EArmsAnimAction::Grab:
		case EArmsAnimAction::Place:
		case EArmsAnimAction::Throw:
		case EArmsAnimAction::Drop:
		case EArmsAnimAction::Equip:
		case EArmsAnimAction::Holster:
		case EArmsAnimAction::Pickup:
			return true;
		default:
			return false;
		}
	}

	/** The item leaves the hand: the arm keeps its pose until the animation of the release blends out. */
	inline bool IsReleaseAction(EArmsAnimAction Action)
	{
		return Action == EArmsAnimAction::Place || Action == EArmsAnimAction::Throw
			|| Action == EArmsAnimAction::Drop || Action == EArmsAnimAction::Holster;
	}

	/** The item id used for the radial tool menu clips (AS_Arms_ToolMenu_Open_R). It belongs to no tool. */
	inline FName ToolMenuItemId() { return FName(TEXT("ToolMenu")); }
}

/** One animation of one arm, and how it is played. */
USTRUCT(BlueprintType)
struct FArmsClip
{
	GENERATED_BODY()

	/** The animation (AS_Arms_<Item>_<Action>_<Hand>). Empty = no animation: nothing plays, nothing is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip")
	TObjectPtr<UAnimSequence> Sequence;

	/** One-shots: seconds to blend in. Ignored by the poses (their blend times are in the Animation Blueprint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip", meta = (ClampMin = "0.0", Units = "s"))
	float BlendIn = 0.15f;

	/** One-shots: seconds to blend out. Ignored by the poses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip", meta = (ClampMin = "0.0", Units = "s"))
	float BlendOut = 0.2f;

	/** One-shots: playback speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip", meta = (ClampMin = "0.05"))
	float PlayRate = 1.0f;

	/** Use only: the clip repeats while the use action is held and stops when it is released (the spray). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip")
	bool bLoopWhileActive = false;

	/**
	 * Grab, Place, Throw, Drop, Equip, Holster, Pickup: WHEN the item really moves (attaches, detaches, appears, launches).
	 * Immediate: the moment the player acts. After Delay: Commit Delay Seconds later. On Notify: when the "Arms Item Commit" notify
	 * placed on the animation is reached (the best choice: it follows the animation if its speed changes).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip|Commit")
	EActionCommitMode CommitMode = EActionCommitMode::Immediate;

	/** After Delay: seconds between the player's action and the effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip|Commit", meta = (ClampMin = "0.0", Units = "s", EditCondition = "CommitMode == EActionCommitMode::AfterDelay"))
	float CommitDelaySeconds = 0.0f;

	/** On Notify: the effect happens anyway after this long if the notify is never reached. 0 = the length of the clip plus 0.1 s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip|Commit", meta = (ClampMin = "0.0", Units = "s", EditCondition = "CommitMode == EActionCommitMode::OnNotify"))
	float CommitTimeoutSeconds = 0.0f;
};

/** The same action for both arms. */
USTRUCT(BlueprintType)
struct FArmsActionClips
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clips")
	FArmsClip Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clips")
	FArmsClip Left;

	const FArmsClip& Get(EArmSide Side) const { return Side == EArmSide::Right ? Right : Left; }
	FArmsClip& Get(EArmSide Side) { return Side == EArmSide::Right ? Right : Left; }
};

/** Every animation of one item (Sunbed, Bucket...), by action. */
USTRUCT(BlueprintType)
struct FArmsItemAnimations
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TMap<EArmsAnimAction, FArmsActionClips> Actions;
};
