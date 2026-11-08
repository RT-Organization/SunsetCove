// PropInventoryComponent.h
// SunsetCove - Inventory System / Prop Inventory. The two hands of the player: grab, carry, place, throw and drop props.
//
// WHAT THIS CLASS IS
//   The "Prop Inventory (attrezzature in mano)" of the GDD. It is NOT a backpack: a prop that is held occupies a hand
//   (physically attached to a socket of the arms). There are 2 slots, one per hand. A prop is any actor that implements
//   IPropInterface (Public/Interfaces/PropInterface.h). Nothing else is needed on the actor, apart from the setup
//   described there (the root component is a Movable primitive).
//
// THE FOUR ACTIONS (default keys; the player can remap them, so only actions are named in code and docs)
//   GRAB   Interact action (E). It does not come from this class: the Interaction System aims, and calls this class
//          through a registered handler (RegisterWithInteraction -> TryGrabProp). This class has no grab input.
//   PLACE  Hold the Place action (left mouse): a hologram of the prop follows the view, green on a valid spot and red
//          on an invalid one. The Rotate action (mouse wheel) turns it in RotationStepDegrees steps. RELEASE the Place
//          action to put the prop down at the hologram, if the spot is valid. Releasing on an invalid spot does nothing
//          and the prop stays in the hand.
//   THROW  Hold the Throw action (right mouse): the power bar charges, rises to the peak and falls again (ping pong, the
//          GDD's "fills and falls"). RELEASE to throw. The speed goes from MinThrowSpeed (bar empty) to MaxThrowSpeed
//          (bar at the peak).
//   DROP   Press the Drop action (Q): the prop falls from the hand, pushed gently forward (like Minecraft).
//
// WHICH PROP DOES AN ACTION USE (two full hands)
//   The one grabbed LAST. Grabbing fills the right hand first, then the left. When the active prop leaves the hand, the
//   one grabbed before it becomes active.
//
// STATES (EPropInventoryState)
//   Idle -> Placing (Place pressed) -> Idle (Place released, or cancelled)
//   Idle -> ChargingThrow (Throw pressed) -> Idle (Throw released, or cancelled)
//   Throw pressed while Placing cancels the placement and starts the charge. Drop cancels whatever is in progress and
//   drops the active prop. Grab is refused unless the state is Idle. The component ticks only while it is not Idle.
//
// HOW A PROP IS HELD, THROWN AND PLACED
//   Grab:   physics off, collision off, AttachToComponent on the hand socket (Prop_R / Prop_L on the arms skeleton).
//   Place:  detach, move to the hologram transform, restore collision, physics off (bSimulatePhysicsAfterPlace).
//   Throw / Drop: detach, collision back on (with physics), physics on, velocity set directly (mass independent).
//   For a short time after leaving the hand the prop ignores the Pawn channel, so it does not hit the thrower.
//
// LOGICAL STATE VERSUS PHYSICAL EFFECT (commit timing)
//   The slots, the hands and OnSlotsChanged change the INSTANT the player acts: the hand is occupied (Grab) or free (Place, Throw,
//   Drop) whatever an animation does. The PHYSICAL effect (attach to the socket, detach, move to the hologram, launch) can be
//   delayed to a chosen moment of an animation. The C++ hook ActionBeginHook is asked when an action is accepted and answers with an
//   FActionCommitTiming (Immediate, AfterDelay, OnNotify). With no hook bound everything is immediate, exactly as before.
//   A new action, CancelCurrentAction, EndPlay and the unpossess flush a pending effect at once. CommitPendingAction() lets the
//   animation (a notify) release an OnNotify wait. See Core/Utils/SunsetCoveActionCommit.h.
//
// FUTURE (requested by the project owner, not done yet): the prop must not teleport to the hand. On a Grab it should TRAVEL
//   toward the hand (a short move of the prop) and attach to the socket when it arrives. The place to do it is CommitGrab: the
//   logical grab has already happened, so a travel can run between the hook (animation starts) and the attach. Same idea for the
//   Pickup of the Tool Inventory.
//
// HOOKS FOR LATER WORK (nothing here needs changing to add them)
//   UI         OnThrowChargeChanged (0..1) feeds the charge bar; OnStateChanged shows and hides it.
//   ANIMATION  ActionBeginHook starts the one-shot animation of Grab / Place / Throw / Drop and sets when the effect lands (the arms
//              animation layer, Public/AnimationSystem/ArmsAnimation/, does it). The Animation Blueprint can read the state
//              through GetState, IsHoldingAnyProp and IsHandOccupied.
//   TOOLS      The Tool Inventory (Public/InventorySystem/ToolInventorySystem/) is refused while IsHoldingAnyProp is true
//              (GDD). The character tells it from OnSlotsChanged; the two systems do not include each other.
//
// WIRING (done by ASunsetCoveCharacter, the composition root)
//   SetHandMeshes             tells the component which meshes carry the hand sockets.
//   RegisterWithInteraction   plugs the grab into the Interaction System.
//   SetupInput                binds Place, Throw, Rotate and Drop.
//
// NETWORK
//   Single player game: everything runs locally, nothing replicates.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InventorySystem/PropInventorySystem/PropInventoryTypes.h"
#include "TimerManager.h"
#include "PropInventoryComponent.generated.h"

class APropPlacementHologram;
class UEnhancedInputComponent;
class UInputAction;
class UInteractionComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
struct FCollisionQueryParams;
struct FInputActionValue;

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API UPropInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPropInventoryComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration: input actions (BP_Character > Components > Prop Inventory)
	// ------------------------------------------------------------------

	/** Hold to show the hologram, release to place. Digital (bool), no triggers. Left mouse button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Input")
	TObjectPtr<UInputAction> PlaceAction;

	/** Hold to charge the throw, release to throw. Digital (bool), no triggers. Right mouse button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Input")
	TObjectPtr<UInputAction> ThrowAction;

	/** Turns the hologram while placing. Axis1D (float): the mouse wheel; each notch is one rotation step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Input")
	TObjectPtr<UInputAction> RotateAction;

	/** Lets the active prop fall from the hand. Digital (bool). Q. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Input")
	TObjectPtr<UInputAction> DropAction;

	// ------------------------------------------------------------------
	// Configuration: hands
	// ------------------------------------------------------------------

	/** Which mesh carries the hand sockets. The first-person arms for this game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Hands")
	EPropHandMeshSource HandMeshSource = EPropHandMeshSource::FirstPersonMesh;

	/**
	 * Socket OR bone of the right hand on the arms skeleton. It normally comes from Blender (the animator adds it to the
	 * arms, and it arrives with the FBX), so this name must match the one used there. A bone works as well as a socket.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Hands")
	FName RightHandSocketName = TEXT("Prop_R");

	/** Socket or bone of the left hand on the arms skeleton (same rules as the right hand). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Hands")
	FName LeftHandSocketName = TEXT("Prop_L");

	/**
	 * Used only while the hand socket does not exist yet (arms not made, socket not added): where the prop sits relative to the
	 * hand mesh component (cm). Lets the system be tested before the arms are ready. A warning is logged once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Hands", meta = (Units = "cm"))
	FVector RightHandFallbackLocation = FVector(45.0f, 25.0f, -30.0f);

	/** Fallback location of the left hand (see RightHandFallbackLocation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Hands", meta = (Units = "cm"))
	FVector LeftHandFallbackLocation = FVector(45.0f, -25.0f, -30.0f);

	/** Fallback rotation of a prop relative to the hand mesh component, both hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Hands")
	FRotator HandFallbackRotation = FRotator::ZeroRotator;

	// ------------------------------------------------------------------
	// Configuration: placement
	// ------------------------------------------------------------------

	/** How far from the camera a prop can be placed (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement", meta = (ClampMin = "50.0", Units = "cm"))
	float MaxPlacementDistance = 350.0f;

	/** When the view ray hits nothing within range, a ray goes down from its end this far to find the ground (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float GroundSnapTraceLength = 400.0f;

	/** Steepest surface a prop can be placed on (degrees from flat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxSurfaceSlopeDegrees = 35.0f;

	/** Degrees the hologram turns for each step of the Rotate action (one mouse wheel notch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement", meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg"))
	float RotationStepDegrees = 15.0f;

	/** True: the prop's heading follows where the camera looks, and the Rotate action adds to it. False: the heading is fixed when placing starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement")
	bool bFollowCameraYaw = true;

	/** True: the prop tilts to the slope of the surface. False: it always stands upright. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement")
	bool bAlignToSurfaceNormal = false;

	/** Trace channel used to find the surface to place on, and to keep a released prop out of walls. Surfaces must BLOCK it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement")
	TEnumAsByte<ETraceTypeQuery> SurfaceTraceChannel;

	/** What counts as an obstacle for the box test at the hologram (walls, rocks, other props, people). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObstacleObjectTypes;

	/** The obstacle box is this much smaller than the prop on every side (cm), so props may touch without being blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float ObstacleBoxShrink = 4.0f;

	/** The obstacle box starts this high above the surface (cm), so bumpy ground does not count as an obstacle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float ObstacleBoxBottomLift = 8.0f;

	/** True: the actor the prop would stand on (the ground) is left out of the obstacle test. Turn off if the beach is one big mesh with walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement")
	bool bIgnoreSupportSurfaceActor = true;

	/** True: a placed prop keeps simulating physics (it can be pushed or blown over). False: it stays where it was put. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement")
	bool bSimulatePhysicsAfterPlace = false;

	/** Translucent material of the hologram. It needs a vector parameter named HologramColorParameterName. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement|Hologram")
	TObjectPtr<UMaterialInterface> HologramMaterial;

	/** Name of the color parameter of HologramMaterial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement|Hologram")
	FName HologramColorParameterName = TEXT("HologramColor");

	/** Hologram color on a spot where the prop can be placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement|Hologram")
	FLinearColor ValidHologramColor = FLinearColor(0.1f, 1.0f, 0.2f, 1.0f);

	/** Hologram color on a spot where the prop cannot be placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Placement|Hologram")
	FLinearColor InvalidHologramColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f);

	// ------------------------------------------------------------------
	// Configuration: throw
	// ------------------------------------------------------------------

	/** Seconds for the power bar to go from empty to the peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Throw", meta = (ClampMin = "0.05", Units = "s"))
	float ChargeTimeToPeak = 0.8f;

	/** True: after the peak the bar falls back to empty and rises again, until the action is released. False: it stops at the peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Throw")
	bool bPingPongCharge = true;

	/** Launch speed with the bar empty (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Throw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinThrowSpeed = 400.0f;

	/** Launch speed with the bar at the peak (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Throw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxThrowSpeed = 1600.0f;

	/** Degrees the throw direction is raised above where the camera looks, for a more natural arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Throw", meta = (ClampMin = "-30.0", ClampMax = "60.0", Units = "deg"))
	float ThrowPitchOffsetDegrees = 8.0f;

	/** Tumble of a thrown prop around a random axis (degrees per second). 0 = no spin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Throw", meta = (ClampMin = "0.0", Units = "deg/s"))
	float ThrowSpinDegreesPerSecond = 360.0f;

	// ------------------------------------------------------------------
	// Configuration: drop
	// ------------------------------------------------------------------

	/** Forward speed of a dropped prop (cm/s). Small: the prop is let go, not thrown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Drop", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DropForwardSpeed = 250.0f;

	/** Upward speed of a dropped prop (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Drop", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DropUpSpeed = 100.0f;

	// ------------------------------------------------------------------
	// Configuration: leaving the hand (throw and drop)
	// ------------------------------------------------------------------

	/** 0 = the prop ignores the player's speed. 1 = it adds the player's velocity (running, then throwing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Release", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InheritOwnerVelocityScale = 1.0f;

	/** A released prop ignores the Pawn channel this long (seconds), so it does not bump into the player. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Release", meta = (ClampMin = "0.0", Units = "s"))
	float ReleasePawnIgnoreSeconds = 0.35f;

	/** If a wall is between the camera and the hand, the prop is released this far in front of the wall (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Release", meta = (ClampMin = "0.0", Units = "cm"))
	float ReleaseWallMargin = 20.0f;

	// ------------------------------------------------------------------
	// Configuration: debug
	// ------------------------------------------------------------------

	/** Draws the placement box and the surface hit in the world. For tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PropInventory|Debug")
	bool bDrawDebug = false;

	// ------------------------------------------------------------------
	// Wiring (called by the character, C++ only)
	// ------------------------------------------------------------------

	/**
	 * Optional. Called when an action (Grab, Place, Throw, Drop) is accepted, before its physical effect. The listener starts its
	 * animation and returns when the effect must happen. Unbound: the effect is immediate. One listener only (the animation layer).
	 */
	FPropActionBeginHook ActionBeginHook;

	/** Tells the component which meshes carry the hand sockets. Either can be null. */
	void SetHandMeshes(USceneComponent* FirstPersonArms, USceneComponent* BodyMesh);

	/** Plugs the grab into the Interaction System as a handler. Call once, after both components exist. */
	void RegisterWithInteraction(UInteractionComponent* InInteraction);

	/** Binds Place, Throw, Rotate and Drop. Call it from the character's SetupSunsetCoveInput. */
	void SetupInput(UEnhancedInputComponent* EnhancedInput);

	// ------------------------------------------------------------------
	// Commands (also usable from Blueprint, for example by a future interaction or a test)
	// ------------------------------------------------------------------

	/** Puts Prop in a free hand. False if it is not a valid prop, the hands are full, a Place or Throw is in progress, or the prop refuses. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	bool TryGrabProp(AActor* Prop);

	/** Starts placing the active prop (hologram on). False if there is no prop or the state is not Idle. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	bool BeginPlacement();

	/** Ends placing. Puts the prop down if the hologram is valid. Returns true if it was placed. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	bool ConfirmPlacement();

	/** Turns the hologram by Steps rotation steps (negative = the other way). Only while placing. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	void RotatePlacementSteps(int32 Steps);

	/** Starts charging the throw of the active prop. Cancels a placement in progress. False if there is no prop. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	bool BeginThrowCharge();

	/** Throws the active prop with the current charge. Returns true if it was thrown. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	bool ReleaseThrow();

	/** Lets the active prop fall from the hand. Cancels a placement or a charge first. Returns true if a prop was dropped. */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	bool DropActiveProp();

	/**
	 * Ends a placement or a throw charge without doing anything with the prop, and applies a pending physical effect at once
	 * (see the commit timing notes at the top). Safe to call at any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	void CancelCurrentAction();

	/**
	 * Applies the pending physical effect now (the prop attaches, detaches or launches). The arms animation calls it from its commit
	 * notify. Does nothing if nothing is pending. Safe to call at any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "PropInventory")
	void CommitPendingAction();

	/** True while an accepted action waits for its physical effect (a delay or a notify). */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	bool IsCommitPending() const { return PendingCommit.bActive; }

	// ------------------------------------------------------------------
	// Queries (for the Animation Blueprint, the UI and other systems)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PropInventory")
	EPropInventoryState GetState() const { return State; }

	/** True if at least one hand holds a prop. The Tool Inventory must be refused while this is true. */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	bool IsHoldingAnyProp() const;

	UFUNCTION(BlueprintPure, Category = "PropInventory")
	bool IsHandOccupied(EPropHand Hand) const;

	/** The prop in a hand, or null. */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	AActor* GetPropInHand(EPropHand Hand) const;

	/** The prop that Place, Throw and Drop would use (the last grabbed), or null. */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	AActor* GetActiveProp() const;

	/** The hand of the active prop. Right if there is none: check GetActiveProp first. */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	EPropHand GetActiveHand() const;

	/** Current throw charge, 0 to 1. 0 when not charging. */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	float GetThrowCharge() const { return ThrowCharge; }

	/** True while placing and the hologram is on a valid spot. */
	UFUNCTION(BlueprintPure, Category = "PropInventory")
	bool IsPlacementValid() const { return bPlacementValid; }

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	/** Idle / Placing / ChargingThrow changed. */
	UPROPERTY(BlueprintAssignable, Category = "PropInventory")
	FPropInventoryStateChangedSignature OnStateChanged;

	/** An action took effect (Grab, Place, Throw, Drop). The hook for one-shot animations. */
	UPROPERTY(BlueprintAssignable, Category = "PropInventory")
	FPropActionSignature OnPropAction;

	/** The hologram became valid or invalid. */
	UPROPERTY(BlueprintAssignable, Category = "PropInventory")
	FPropPlacementValidityChangedSignature OnPlacementValidityChanged;

	/** The throw charge changed (0..1). The hook for the charge bar. */
	UPROPERTY(BlueprintAssignable, Category = "PropInventory")
	FPropThrowChargeChangedSignature OnThrowChargeChanged;

	/** A hand was filled or emptied. */
	UPROPERTY(BlueprintAssignable, Category = "PropInventory")
	FPropSlotsChangedSignature OnSlotsChanged;

private:
	// ---- Input callbacks ----
	void HandlePlaceStarted();
	void HandlePlaceCompleted();
	void HandlePlaceCanceled();
	void HandleThrowStarted();
	void HandleThrowCompleted();
	void HandleThrowCanceled();
	void HandleRotate(const FInputActionValue& Value);
	void HandleDropStarted();

	// ---- Interaction handler (grab) ----
	bool CanHandleInteraction(AActor* Target);
	bool HandleInteraction(AActor* Target);

	// ---- Grab rules ----
	bool CanGrabProp(const AActor* Prop) const;
	/** True if the root of the prop is a Movable primitive. Logs one warning per prop that is not. */
	bool ValidatePropStructure(const AActor* Prop) const;

	// ---- Slots ----
	static int32 SlotIndexFor(EPropHand Hand) { return Hand == EPropHand::Right ? 0 : 1; }
	static EPropHand HandForSlotIndex(int32 SlotIndex) { return SlotIndex == 0 ? EPropHand::Right : EPropHand::Left; }
	bool IsSlotOccupied(int32 SlotIndex) const;
	int32 FindFreeSlotIndex() const;
	int32 FindActiveSlotIndex() const;
	bool IsPropHeld(const AActor* Prop) const;
	/** Clears the slots whose prop was destroyed meanwhile. */
	void PruneDestroyedProps();
	void ClearSlot(int32 SlotIndex);
	/** Adds every carried prop to the ignore list of a trace or overlap, so the player's own props never block it. */
	void AddHeldPropsToIgnore(FCollisionQueryParams& Params) const;

	// ---- Hands ----
	USceneComponent* ResolveHandMesh() const;
	/** Logs once, at start, which hand sockets are missing on the hand mesh (so a wrong name is found before the first grab). */
	void ValidateHandSockets();
	void AttachPropToHand(AActor* Prop, int32 SlotIndex);

	// ---- State ----
	void SetState(EPropInventoryState NewState);

	// ---- Placement ----
	void UpdatePlacement();
	void EndPlacement();
	/** Logical part of a Place: the hand is free at once. The physical part (CommitPlace) follows the commit timing. */
	void BeginPlace(int32 SlotIndex, const FTransform& WorldTransform);
	void CommitPlace(TWeakObjectPtr<AActor> WeakProp, ECollisionEnabled::Type SavedCollision, EPropHand Hand, const FTransform& WorldTransform);

	// ---- Throw and drop ----
	void UpdateThrowCharge(float DeltaTime);
	void EndThrowCharge();
	/** Logical part of a Throw or Drop: the hand is free at once. The physical part (CommitRelease) follows the commit timing. */
	void BeginRelease(int32 SlotIndex, EPropAction Action, const FVector& LinearVelocity, const FVector& AngularVelocityDegrees);
	/** Detaches the prop from its hand and hands it to the physics engine. Shared by Throw and Drop. */
	void CommitRelease(TWeakObjectPtr<AActor> WeakProp, ECollisionEnabled::Type SavedCollision, EPropHand Hand, EPropAction Action, const FVector& LinearVelocity, const FVector& AngularVelocityDegrees);

	// ---- Grab (physical part) ----
	/** Attaches the prop to the hand socket and tells the prop and the listeners. */
	void CommitGrab(TWeakObjectPtr<AActor> WeakProp, int32 SlotIndex);

	// ---- Commit timing ----
	/** Asks the hook for the timing, then runs Commit now or later. Any earlier pending effect has been flushed before. */
	void BeginCommit(EPropAction Action, EPropHand Hand, AActor* Prop, TFunction<void()> Commit);

	/** An accepted action whose physical effect has not happened yet. */
	struct FPendingCommit
	{
		TFunction<void()> Commit;
		FTimerHandle Timer;
		bool bActive = false;
	};

	// ---- Temporary Pawn-ignore after release ----
	void ApplyTemporaryPawnIgnore(UPrimitiveComponent* Primitive);
	/** Restores the Pawn response of Primitive (and of any destroyed primitive). Null restores only the destroyed ones. */
	void RestorePawnResponse(UPrimitiveComponent* Primitive);
	void RestoreAllPawnResponses();

	/** A prop that ignores the Pawn channel until its timer ends. */
	struct FPendingPawnIgnore
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		TEnumAsByte<ECollisionResponse> OriginalResponse = ECR_Block;
		FTimerHandle Timer;
	};

	// ---- Data ----
	/** Index 0 = right hand, 1 = left hand. */
	UPROPERTY(Transient)
	TArray<FPropSlot> Slots;

	UPROPERTY(Transient)
	TObjectPtr<APropPlacementHologram> Hologram;

	TWeakObjectPtr<USceneComponent> FirstPersonHandMesh;
	TWeakObjectPtr<USceneComponent> BodyHandMesh;
	TWeakObjectPtr<UInteractionComponent> Interaction;

	TArray<FPendingPawnIgnore> PendingPawnIgnores;

	FPendingCommit PendingCommit;

	/** Props that already got a "bad structure" warning (so the log is not flooded while the player aims at one). */
	mutable TSet<TWeakObjectPtr<const AActor>> WarnedProps;

	EPropInventoryState State = EPropInventoryState::Idle;

	/** The slot that Place or Throw works on, fixed when the action starts. */
	int32 LockedSlotIndex = INDEX_NONE;

	int32 GrabCounter = 0;

	// Placement
	float PlacementYawOffset = 0.0f;
	float PlacementStartYaw = 0.0f;
	bool bPlacementValid = false;
	FTransform PlacementTransform;
	FBox PropLocalBounds = FBox(ForceInit);
	FVector PropScale = FVector::OneVector;

	// Throw
	float ThrowCharge = 0.0f;
	float ThrowElapsed = 0.0f;

	bool bWarnedMissingSocket = false;
	bool bWarnedMissingHologramMaterial = false;
};
