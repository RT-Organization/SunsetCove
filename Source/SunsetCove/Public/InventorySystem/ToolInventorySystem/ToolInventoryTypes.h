// ToolInventoryTypes.h
// SunsetCove - Inventory System / Tool Inventory. The enums, the slot struct and the delegate signatures of the system.
//
// Kept apart from the component so that UI and animation code can include the types without including the component.

#pragma once

#include "CoreMinimal.h"
#include "Core/Utils/SunsetCoveActionCommit.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "ToolInventoryTypes.generated.h"

class ASunsetCoveTool;

/** What the Tool Inventory is doing. Using a tool is not a state: ask IsUsingTool. */
UENUM(BlueprintType)
enum class EToolInventoryState : uint8
{
	/** The radial menu is closed. A tool may be equipped (in the hand) or holstered. */
	Idle			UMETA(DisplayName = "Idle"),
	/** The tool menu action is held: the radial menu is open and the look input picks a sector. */
	SelectingTool	UMETA(DisplayName = "Selecting Tool")
};

/** What happened to a tool. Carried by OnToolAction, the hook for the one-shot animations. */
UENUM(BlueprintType)
enum class EToolAction : uint8
{
	/** The tool was picked up from the world and stored. */
	Pickup		UMETA(DisplayName = "Pickup"),
	/** The tool was taken out and is now in the hand. */
	Equip		UMETA(DisplayName = "Equip"),
	/** The tool was put away (the hand is free again). */
	Holster		UMETA(DisplayName = "Holster"),
	/** The player started using the equipped tool. */
	UseStarted	UMETA(DisplayName = "Use Started"),
	/** The player stopped using the equipped tool. */
	UseEnded	UMETA(DisplayName = "Use Ended"),
	/** The tool was dropped on the ground. */
	Drop		UMETA(DisplayName = "Drop")
};

/** Why the Tool Inventory said no. Carried by OnActionRefused so a future UI can show the right message. */
UENUM(BlueprintType)
enum class EToolRefusal : uint8
{
	/** The hands hold props. Tools cannot be picked up, selected or used meanwhile. */
	HandsBusy		UMETA(DisplayName = "Hands Busy"),
	/** Every tool slot is taken. */
	InventoryFull	UMETA(DisplayName = "Inventory Full"),
	/** The player already has this tool (same ToolId). */
	AlreadyOwned	UMETA(DisplayName = "Already Owned"),
	/** The tool refuses to be picked up (bCanBePickedUp is off). */
	ToolBlocked		UMETA(DisplayName = "Tool Blocked"),
	/** The tool menu was asked to open with no tool in the inventory. */
	NoTools			UMETA(DisplayName = "No Tools")
};

/** One slot: the tool it holds and what the tool looked like before it was picked up. */
USTRUCT()
struct FToolSlot
{
	GENERATED_BODY()

	/** The stored tool, or null when the slot is empty. */
	UPROPERTY(Transient)
	TObjectPtr<ASunsetCoveTool> Tool = nullptr;

	/** Collision mode of the tool's root before it was picked up, restored when the tool is dropped. */
	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> SavedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	void Reset()
	{
		Tool = nullptr;
		SavedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	}
};

// ----------------------------------------------------------------------
// Delegate signatures (all BlueprintAssignable on UToolInventoryComponent)
// ----------------------------------------------------------------------

/** Idle / SelectingTool changed. Show or hide the radial menu widget here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FToolInventoryStateChangedSignature, EToolInventoryState, NewState, EToolInventoryState, OldState);

/** The sector under the look direction changed while the radial menu is open. SlotIndex is -1 when no tool is selected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FToolRadialHoverChangedSignature, int32, SlotIndex);

/** The tool in the hand changed. NewTool is null when the tool was holstered, OldTool is null when the hand was empty. SlotIndex is the new slot, -1 if none. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FToolEquippedChangedSignature, ASunsetCoveTool*, NewTool, ASunsetCoveTool*, OldTool, int32, SlotIndex);

/**
 * C++ hook, asked when Pickup, Equip, Holster or Drop is ACCEPTED (the slots and the equipped tool already changed) and BEFORE its
 * physical effect (attach and hide, show, hide, detach). The listener starts its animation and answers with the commit timing of that
 * action. See Core/Utils/SunsetCoveActionCommit.h.
 */
DECLARE_DELEGATE_RetVal_ThreeParams(FActionCommitTiming, FToolActionBeginHook, EToolAction, int32, ASunsetCoveTool*);

/**
 * Something happened to a tool (the logical state has changed; the physical effect may follow slightly later if an ActionBeginHook
 * delays it). To START an animation of Pickup, Equip, Holster or Drop use the ActionBeginHook; UseStarted and UseEnded are for this event.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FToolActionSignature, EToolAction, Action, int32, SlotIndex, ASunsetCoveTool*, Tool);

/** A slot was filled or emptied. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FToolSlotsChangedSignature);

/** The system refused something the player asked for. Tool is the tool involved, or null (for example NoTools). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FToolRefusedSignature, EToolRefusal, Reason, ASunsetCoveTool*, Tool);
