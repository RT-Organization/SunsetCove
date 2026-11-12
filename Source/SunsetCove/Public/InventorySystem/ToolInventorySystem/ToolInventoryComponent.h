// ToolInventoryComponent.h
// SunsetCove - Inventory System / Tool Inventory. The tools of the player: pick up, choose from a radial menu, use, drop.
//
// WHAT THIS CLASS IS
//   The "Tool Inventory (strumenti)" of the GDD. Not a backpack of props: the player carries up to MaxTools (4) tools, shows
//   ONE of them in the hand at a time (the equipped tool) and uses it forever (tools are never consumed). A tool is an
//   ASunsetCoveTool (Public/Gameplay/Tools/): create tool Blueprints from that class. The Prop Inventory is a separate
//   system with the two hands for props (Public/InventorySystem/PropInventorySystem/); the two never include each other.
//
// THE ACTIONS (default keys; the player can remap them, so only actions are named in code and docs)
//   PICK UP   Interact action (E) on a tool lying in the world. It does not come from this class: the Interaction System aims
//             and calls this class through a registered handler (RegisterWithInteraction -> TryPickupTool).
//             Refused (OnActionRefused tells why) if the hands hold props, all slots are full, or the tool is already owned.
//   SELECT    Hold the Tool Menu action (Tab): the radial menu opens. MOVE THE LOOK INPUT (mouse or right stick) toward the
//             sector of a tool, GTA weapon wheel style, then RELEASE the action: that tool is equipped. No click is needed.
//               - release on the tool that is already equipped  -> it is holstered (the hand is free)
//               - release with nothing selected (a quick tap)   -> the equipped tool is holstered (the hand is free)
//   USE       Hold / press the Use Tool action (left mouse) with a tool equipped and the menu closed. Press tools use once
//             per click, Hold tools repeat while it is held (see ASunsetCoveTool::UseMode).
//   DROP      With the menu open and a tool selected, press the Drop action (Q): that tool falls to the ground gently in front
//             of the player (like Minecraft), and can be picked up again.
//
// HANDS AND PROPS
//   The GDD: "tools cannot be selected while props are in hand". The character tells this component through SetHandsBusy
//   (true while the Prop Inventory holds a prop). While busy: pickup, menu and use are refused, and the moment the hands
//   become busy the menu closes and the equipped tool is holstered and STAYS holstered (the player picks it again later).
//
// HOW A TOOL IS KEPT
//   Pickup:   physics off, collision off, hidden, attached to the hand socket (Tool_R, or Prop_R when the arms have no Tool_R).
//   Equip:    the tool is shown. Holster: it is hidden again. Drop: detach, collision back, physics on, velocity set directly.
//   The grip bone of the tool lands on the hand socket (ASunsetCoveTool::GetRootInHandSocketTransform), like props.
//   For a short time after a drop the tool ignores the Pawn channel, so it does not hit the player.
//
// STATES (EToolInventoryState)
//   Idle <-> SelectingTool (the menu action is held). There is no tick: everything is driven by input events.
//
// LOGICAL STATE VERSUS PHYSICAL EFFECT (commit timing)
//   The slots and the equipped tool change the INSTANT the player acts: the tool is equipped, holstered or dropped whatever an
//   animation does. The PHYSICAL effect (Pickup: attach to the hand and hide; Equip: show; Holster: hide; Drop: detach and launch)
//   can be delayed to a chosen moment of an animation. The C++ hook ActionBeginHook is asked when an action is accepted and answers
//   with an FActionCommitTiming (Immediate, AfterDelay, OnNotify). With no hook bound everything is immediate, as before.
//   While an Equip is pending the tool cannot be used. A new action, CancelCurrentAction, EndPlay and the unpossess flush a pending
//   effect at once; CommitPendingAction() lets the animation (a notify) release an OnNotify wait. A swap (Holster + Equip in one
//   call), a drop of the equipped tool and "hands become busy" hide the old tool at once: only the new Equip is timed.
//   See Core/Utils/SunsetCoveActionCommit.h.
//
// FUTURE (requested by the project owner, not done yet): the tool Pickup should not teleport either: the tool should travel toward the
//   hand and attach when it arrives (same idea as the Grab of the Prop Inventory). The place is the physical part of TryPickupTool.
//
// HOOKS FOR LATER WORK (nothing here needs changing to add them)
//   UI         OnStateChanged shows / hides the radial menu widget. OnRadialHoverChanged gives the sector under the look
//              direction. GetRadialVector / GetRadialSlotAngle / GetSlotTool lay the widget out; icons and names come from
//              the tool actors (Icon, DisplayName). SetHoveredSlot lets the widget pick a sector itself (a mouse cursor).
//              OnActionRefused carries the reason for a "hands busy / inventory full" message.
//   ANIMATION  ActionBeginHook starts the one-shot animation of Pickup / Equip / Holster / Drop and sets when the effect lands.
//              OnToolAction(UseStarted / UseEnded) and OnEquippedToolChanged feed the Use animation and the hold pose (the arms
//              animation layer, Public/AnimationSystem/ArmsAnimation/, does it). The Animation Blueprint can read IsToolEquipped,
//              GetEquippedTool and IsUsingTool.
//
// WIRING (done by ASunsetCoveCharacter, the composition root)
//   SetHandMeshes             tells the component which meshes carry the hand sockets.
//   RegisterWithInteraction   plugs the pickup into the Interaction System.
//   SetupInput                binds the Tool Menu, Use Tool and Drop actions.
//   SetHandsBusy              follows the Prop Inventory.
//   AddRadialInput            receives the look input while the menu is open (the character vetoes the camera look then).
//
// NETWORK
//   Single player game: everything runs locally, nothing replicates.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryTypes.h"
#include "TimerManager.h"
#include "ToolInventoryComponent.generated.h"

class ASunsetCoveTool;
class UEnhancedInputComponent;
class UInputAction;
class UInteractionComponent;
class UPrimitiveComponent;
class USceneComponent;

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API UToolInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolInventoryComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration: input actions (BP_Character > Components > Tool Inventory)
	// ------------------------------------------------------------------

	/** Hold to open the radial menu, release to confirm. Digital (bool), no triggers. Tab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Input")
	TObjectPtr<UInputAction> OpenToolMenuAction;

	/** Uses the equipped tool. Digital (bool), no triggers. Left mouse button: assign the SAME asset as the Prop Inventory Place Action (props and tools never need it at once). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Input")
	TObjectPtr<UInputAction> UseToolAction;

	/** Drops the selected tool while the menu is open. Digital (bool). Q: assign the SAME asset as the Prop Inventory Drop Action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Input")
	TObjectPtr<UInputAction> DropAction;

	// ------------------------------------------------------------------
	// Configuration: slots
	// ------------------------------------------------------------------

	/**
	 * How many tools the player can carry (GDD: about 4). The radial menu has one sector per slot and the HUD wheel builds one slice per
	 * slot, so this number alone decides the wheel layout. Changing it at runtime is not supported.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Slots", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxTools = 4;

	/**
	 * Tools the player already carries when the game starts (GDD MVP: "which tools are initial", still to decide). Each class is
	 * spawned and picked up on the first frame, in order, so the first one lands in slot 0. Extra entries past MaxTools are ignored.
	 * Empty = the player starts with no tools and finds them in the visitors' spots.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Slots")
	TArray<TSubclassOf<ASunsetCoveTool>> StartingTools;

	/** False: a tool with the same ToolId as one already owned is refused (tools are used forever, a second one is useless). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Slots")
	bool bAllowDuplicateTools = false;

	/** True: a tool that is picked up is equipped at once if the hand is empty (no tool equipped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Slots")
	bool bAutoEquipFirstTool = true;

	// ------------------------------------------------------------------
	// Configuration: hand
	// ------------------------------------------------------------------

	/** True: the tool is attached to the first-person arms. False: to the third-person body mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Hand")
	bool bUseFirstPersonArms = true;

	/**
	 * Socket OR bone of the tool hand on the arms skeleton. It normally comes from Blender (the animator adds it to the arms,
	 * and it arrives with the FBX). Optional: when it does not exist, FallbackToolSocketName is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Hand")
	FName ToolSocketName = TEXT("Tool_R");

	/** Used when ToolSocketName does not exist on the arms: the right hand prop socket, which every arms mesh has. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Hand")
	FName FallbackToolSocketName = TEXT("Prop_R");

	/**
	 * Used only while NEITHER socket exists (arms not made, socket not added): where the tool sits relative to the hand mesh
	 * component (cm). Lets the system be tested before the arms are ready. A warning is logged once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Hand", meta = (Units = "cm"))
	FVector FallbackLocation = FVector(45.0f, 25.0f, -30.0f);

	/** Fallback rotation of a tool relative to the hand mesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Hand")
	FRotator FallbackRotation = FRotator::ZeroRotator;

	// ------------------------------------------------------------------
	// Configuration: radial menu
	// ------------------------------------------------------------------

	/**
	 * How far the look input has to travel to push the selection to the edge of the wheel (look input units: mouse counts or
	 * stick deflection, summed). Smaller = a more sensitive wheel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Radial", meta = (ClampMin = "1.0"))
	float RadialRadius = 60.0f;

	/** Selection inside this fraction of the radius (0..1) counts as "nothing selected" (the center of the wheel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Radial", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float RadialDeadZone = 0.35f;

	/** Angle of the center of sector 0, clockwise from straight up (degrees). 0 = slot 0 at the top, the others follow clockwise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Radial", meta = (Units = "deg"))
	float RadialStartAngleDegrees = 0.0f;

	/** Flips the vertical axis of the wheel, for the mouse AND the controller stick (they share the same sign). Turn it on if moving the mouse up selects the bottom sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Radial")
	bool bInvertRadialY = false;

	/**
	 * Controller only: how far the right stick must be pushed (0..1) to pick the sector it points at. The stick selects by
	 * DIRECTION, at once (no cursor to push like the mouse), and the sector stays selected when the stick is released.
	 * Smaller = more sensitive. The mouse keeps using RadialRadius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Radial", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float RadialStickThreshold = 0.25f;

	// ------------------------------------------------------------------
	// Configuration: drop
	// ------------------------------------------------------------------

	/** Forward speed of a dropped tool (cm/s). Small: the tool is let go, not thrown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Drop", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DropForwardSpeed = 250.0f;

	/** Upward speed of a dropped tool (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Drop", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DropUpSpeed = 100.0f;

	/** 0 = the tool ignores the player's speed. 1 = it adds the player's velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Drop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InheritOwnerVelocityScale = 1.0f;

	/** A dropped tool ignores the Pawn channel this long (seconds), so it does not bump into the player. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Drop", meta = (ClampMin = "0.0", Units = "s"))
	float DropPawnIgnoreSeconds = 0.35f;

	/** If a wall is between the camera and the hand, the tool is dropped this far in front of the wall (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Drop", meta = (ClampMin = "0.0", Units = "cm"))
	float DropWallMargin = 20.0f;

	/** Trace channel used to keep a dropped tool out of walls. Walls must BLOCK it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ToolInventory|Drop")
	TEnumAsByte<ETraceTypeQuery> WallTraceChannel;

	// ------------------------------------------------------------------
	// Wiring (called by the character, C++ only)
	// ------------------------------------------------------------------

	/**
	 * Optional. Called when Pickup, Equip, Holster or Drop is accepted, before its physical effect. The listener starts its animation and
	 * returns when the effect must happen. Unbound: the effect is immediate. One listener only (the animation layer).
	 */
	FToolActionBeginHook ActionBeginHook;

	/** Tells the component which meshes carry the hand sockets. Either can be null. */
	void SetHandMeshes(USceneComponent* FirstPersonArms, USceneComponent* BodyMesh);

	/** Plugs the pickup into the Interaction System as a handler. Call once, after both components exist. */
	void RegisterWithInteraction(UInteractionComponent* InInteraction);

	/** Binds the Tool Menu, Use Tool and Drop actions. Call it from the character's SetupSunsetCoveInput. */
	void SetupInput(UEnhancedInputComponent* EnhancedInput);

	/**
	 * True while the hands hold props (set by the character from the Prop Inventory). Turning it on closes the menu, ends a
	 * use and holsters the equipped tool, which stays holstered.
	 */
	void SetHandsBusy(bool bNewBusy);

	/**
	 * Feeds the look input to the radial menu (the delta of the mouse, or the value of the right stick). Does nothing unless the
	 * menu is open. The character calls it from Input_Look and does not move the camera while the menu is open.
	 */
	void AddRadialInput(const FVector2D& LookInput);

	/**
	 * Controller: the raw right stick (-1..1, up = +Y) while the menu is open. Past RadialStickThreshold the selection jumps
	 * to the sector the stick points at; below it nothing changes, so releasing the stick keeps the last sector.
	 */
	void SetRadialStickDirection(const FVector2D& Stick);

	// ------------------------------------------------------------------
	// Commands (also usable from Blueprint, for example by a future UI or a test)
	// ------------------------------------------------------------------

	/** Stores Tool in a free slot. False (with a refusal event) if the hands are busy, the inventory is full or the tool is owned. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool TryPickupTool(ASunsetCoveTool* Tool);

	/** Opens the radial menu. False if the hands are busy, there are no tools or it is already open. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool OpenToolMenu();

	/**
	 * Closes the radial menu. With bConfirm, equips the selected tool (or holsters, see the notes at the top of the file).
	 * Returns true if the menu was open.
	 */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool CloseToolMenu(bool bConfirm);

	/** Selects a sector of the open menu by hand (a UI with its own cursor). Negative or empty slot = nothing selected. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	void SetHoveredSlot(int32 SlotIndex);

	/** Puts the tool of a slot in the hand (the previous one is holstered). False if the slot is empty or the hands are busy. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool EquipSlot(int32 SlotIndex);

	/** Puts the equipped tool away. False if none is equipped. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool HolsterTool();

	/** Starts using the equipped tool (what the Use Tool action does). False if nothing can be used now. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool StartUsingTool();

	/** Stops using the equipped tool. Safe to call at any time. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	void StopUsingTool();

	/** Drops the tool of a slot on the ground, in front of the player. False if the slot is empty. */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	bool DropToolInSlot(int32 SlotIndex);

	/**
	 * Closes the menu without confirming, ends a use and applies a pending physical effect at once (see the commit timing notes at the
	 * top). Safe to call at any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	void CancelCurrentAction();

	/**
	 * Applies the pending physical effect now (the tool attaches, shows, hides or detaches). The arms animation calls it from its commit
	 * notify. Does nothing if nothing is pending. Safe to call at any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "ToolInventory")
	void CommitPendingAction();

	/** True while an accepted action waits for its physical effect (a delay or a notify). */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	bool IsCommitPending() const { return PendingCommit.bActive; }

	// ------------------------------------------------------------------
	// Queries (for the Animation Blueprint, the UI and other systems)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	EToolInventoryState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	bool IsRadialMenuOpen() const { return State == EToolInventoryState::SelectingTool; }

	/** True while the hands hold props and the tools cannot be used. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	bool AreHandsBusy() const { return bHandsBusy; }

	/** Number of slots (MaxTools). */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	int32 GetNumSlots() const { return Slots.Num(); }

	/** How many slots hold a tool. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	int32 GetNumTools() const;

	/** The tool of a slot, or null. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	ASunsetCoveTool* GetSlotTool(int32 SlotIndex) const;

	/** Slot of the tool in the hand, -1 if holstered. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	int32 GetEquippedSlotIndex() const { return EquippedSlotIndex; }

	/** The tool in the hand, or null. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	ASunsetCoveTool* GetEquippedTool() const;

	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	bool IsToolEquipped() const { return GetEquippedTool() != nullptr; }

	/** True between the start and the end of a use of the equipped tool. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	bool IsUsingTool() const { return bUsingTool; }

	/** Slot under the look direction while the menu is open, -1 if none. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	int32 GetHoveredSlotIndex() const { return HoveredSlotIndex; }

	/** Where the selection is on the wheel: X right, Y up, length 0 (center) to 1 (edge). For a cursor in the widget. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	FVector2D GetRadialVector() const { return RadialVector; }

	/** Angle of the center of a sector, clockwise from straight up, in degrees (0..360). To place the icons of the widget. */
	UFUNCTION(BlueprintPure, Category = "ToolInventory")
	float GetRadialSlotAngle(int32 SlotIndex) const;

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	/** Idle / SelectingTool changed. Show and hide the radial menu here. */
	UPROPERTY(BlueprintAssignable, Category = "ToolInventory")
	FToolInventoryStateChangedSignature OnStateChanged;

	/** The sector under the look direction changed (-1 = none). */
	UPROPERTY(BlueprintAssignable, Category = "ToolInventory")
	FToolRadialHoverChangedSignature OnRadialHoverChanged;

	/** The tool in the hand changed (or was holstered). */
	UPROPERTY(BlueprintAssignable, Category = "ToolInventory")
	FToolEquippedChangedSignature OnEquippedToolChanged;

	/** Something happened to a tool (Pickup, Equip, Holster, UseStarted, UseEnded, Drop). The hook for one-shot animations. */
	UPROPERTY(BlueprintAssignable, Category = "ToolInventory")
	FToolActionSignature OnToolAction;

	/** A slot was filled or emptied. */
	UPROPERTY(BlueprintAssignable, Category = "ToolInventory")
	FToolSlotsChangedSignature OnSlotsChanged;

	/** The system said no (hands busy, inventory full...). The hook for the message to the player. */
	UPROPERTY(BlueprintAssignable, Category = "ToolInventory")
	FToolRefusedSignature OnActionRefused;

private:
	// ---- Input callbacks ----
	void HandleOpenMenuStarted();
	void HandleOpenMenuCompleted();
	void HandleOpenMenuCanceled();
	void HandleUseStarted();
	void HandleUseEnded();
	void HandleDropStarted();

	// ---- Interaction handler (pickup) ----
	bool CanHandleInteraction(AActor* Target);
	bool HandleInteraction(AActor* Target);

	// ---- Slots ----
	/** Spawns and picks up StartingTools (authority only). Runs one frame after BeginPlay, when every component has started. */
	void GiveStartingTools();
	bool IsSlotOccupied(int32 SlotIndex) const;
	int32 FindFreeSlotIndex() const;
	bool IsToolOwned(const ASunsetCoveTool* Tool) const;
	bool OwnsToolWithId(FName ToolId) const;
	/** Clears the slots whose tool was destroyed meanwhile. */
	void PruneDestroyedTools();
	void Refuse(EToolRefusal Reason, ASunsetCoveTool* Tool);

	// ---- Hand ----
	USceneComponent* ResolveHandMesh() const;
	/** The hand socket to attach to: ToolSocketName, then FallbackToolSocketName, otherwise None. */
	FName ResolveToolSocket(const USceneComponent* HandMesh) const;
	/** Logs once, at start, which hand socket is missing on the hand mesh (so a wrong name is found before the first pickup). */
	void ValidateHandSockets();
	void AttachToolToHand(ASunsetCoveTool* Tool);

	// ---- Equip ----
	/**
	 * Puts the tool of NewIndex in the hand (INDEX_NONE = nothing): holsters the current one first. Broadcasts the events.
	 * bAllowTimedCommit false: the physical effect (show / hide) is immediate and the hook is not asked (a drop, hands that become busy).
	 */
	void SetEquippedSlot(int32 NewIndex, bool bAllowTimedCommit = true);

	// ---- Commit timing ----
	/** Asks the hook for the timing, then runs Commit now or later. Any earlier pending effect is flushed first. */
	void BeginCommit(EToolAction Action, int32 SlotIndex, ASunsetCoveTool* Tool, TFunction<void()> Commit);
	/** The physical part of a Drop: detach, keep out of walls, show, launch. */
	void CommitDrop(TWeakObjectPtr<ASunsetCoveTool> WeakTool, ECollisionEnabled::Type SavedCollision, const FVector& LinearVelocity);

	/** An accepted action whose physical effect has not happened yet. */
	struct FPendingCommit
	{
		TFunction<void()> Commit;
		FTimerHandle Timer;
		bool bActive = false;
	};

	// ---- Radial menu ----
	void UpdateHoverFromVector();
	void SetHoveredSlotInternal(int32 NewIndex);
	void ResetRadial();

	// ---- State ----
	void SetState(EToolInventoryState NewState);

	// ---- Temporary Pawn-ignore after a drop ----
	void ApplyTemporaryPawnIgnore(UPrimitiveComponent* Primitive);
	/** Restores the Pawn response of Primitive (and of any destroyed primitive). Null restores only the destroyed ones. */
	void RestorePawnResponse(UPrimitiveComponent* Primitive);
	void RestoreAllPawnResponses();

	/** A tool that ignores the Pawn channel until its timer ends. */
	struct FPendingPawnIgnore
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		TEnumAsByte<ECollisionResponse> OriginalResponse = ECR_Block;
		FTimerHandle Timer;
	};

	// ---- Data ----
	UPROPERTY(Transient)
	TArray<FToolSlot> Slots;

	TWeakObjectPtr<USceneComponent> FirstPersonHandMesh;
	TWeakObjectPtr<USceneComponent> BodyHandMesh;
	TWeakObjectPtr<UInteractionComponent> Interaction;

	TArray<FPendingPawnIgnore> PendingPawnIgnores;

	FPendingCommit PendingCommit;

	EToolInventoryState State = EToolInventoryState::Idle;

	/** Slot of the tool in the hand, INDEX_NONE if holstered. */
	int32 EquippedSlotIndex = INDEX_NONE;

	/** Slot under the look direction while the menu is open. */
	int32 HoveredSlotIndex = INDEX_NONE;

	/** Where the selection is on the wheel (X right, Y up, length 0..1). */
	FVector2D RadialVector = FVector2D::ZeroVector;

	/** True after a drop from the menu: releasing the menu action with nothing selected must not holster the equipped tool. */
	bool bConfirmSuppressed = false;

	bool bHandsBusy = false;
	bool bUsingTool = false;
	bool bWarnedMissingSocket = false;
};
