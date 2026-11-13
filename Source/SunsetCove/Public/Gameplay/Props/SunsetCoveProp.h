// SunsetCoveProp.h
// SunsetCove - Gameplay area (Props). The base class of every carriable object: sunbed, umbrella, towel...
//
// WHAT THIS CLASS IS
//   Everything a prop needs to work with the Prop Inventory, built in C++. To make a prop, create a Blueprint whose
//   parent class is "Sunset Cove Prop", then set the Skeletal Mesh Asset of the PropMesh component. That is all: no root
//   to restructure, no collision to set up, no socket to add, no interface to implement.
//
// HIERARCHY
//   AActor + IPropInterface -> ASunsetCoveProp (this class) -> BP_Sunbed, BP_Umbrella ... (Blueprints)
//   Rest spots (USunsetCoveRestSpotComponent) are added to the Blueprint as children of PropMesh or CollisionBody.
//
// COMPONENTS
//   CollisionBody (root)   An invisible box (USunsetCovePivotBoxComponent, Core/Components/). It is the ROOT, so it carries the
//                          physics, the collision and the attachment to the hand, as the Prop Inventory needs (Movable primitive
//                          with simple collision). The interaction trace hits it, so it is what the player aims at.
//                          Its size comes from the mesh (bAutoFitCollisionToMesh) so you never size it by hand.
//                          ORIGIN AT THE BASE: the actor origin is the center of the BOTTOM of the box (Origin = Bottom), and the
//                          mesh is moved so its base is on it. Dragged into a level, snapped to the floor (End), spawned at a
//                          point or placed by the player, the prop stands ON the surface. Whatever pivot the FBX has, nothing to
//                          fix in Blender or by hand. (CollisionBody > Origin = Center brings back a centered origin.)
//   PropMesh               The skeletal mesh of the prop, a child of the box. It has NO collision: it only shows and
//                          animates. Rigid physics on the box moves it like one solid object, the skeleton never
//                          ragdolls.
//
// WHY A SKELETON
//   Props animate when they are put down (an umbrella opens, a sunbed unfolds). The animator builds that skeleton in
//   Blender, including the bone used to hold the prop, and it arrives in Unreal with the FBX. Nothing about the grip is
//   set up in Unreal.
//
// THE GRIP (how the prop sits in the hand)
//   GripSocketName names a bone (or a socket) of the prop's skeleton. When the prop is grabbed, the system places the
//   prop so that THIS bone lands exactly on the hand socket of the character arms (Prop_R / Prop_L, also from Blender).
//   So the grip is decided in Blender: put the grip bone where the hand holds the prop, with the same axes as the hand
//   socket, and the prop sits right with no offset. HoldLocationOffset / HoldRotationOffset are a small per-prop fine
//   tune on top of it, in the space of the hand socket, for when something has to be nudged without going back to
//   Blender. A prop can have a different grip for the left hand (LeftGripSocketName); if it is empty or missing, the
//   same grip is used for both hands.
//   Without a grip bone the root of the prop (the center of the base of the box) is placed on the hand socket, and one warning says so.
//
// ANIMATION
//   The system tells the prop what happens to it. Two ways to react, use either:
//     * Optional animation assets: PickedUpAnimation (CLOSE) and PlacedAnimation (OPEN) are played once on PropMesh. They use a
//       single animation node on the mesh, so they replace an Animation Blueprint on PropMesh. Leave them empty if you use an
//       Animation Blueprint. The flow:
//         pickup    the prop stays where it lies and plays CLOSE; the Prop Inventory waits for it (OnPropGrabStarted returns its
//                   length) and only then attaches the prop to the hand. The rest spots are switched off at the start.
//         carried   the prop is closed. A dropped or thrown prop stays closed and lands closed.
//         placed    the prop is moved to the hologram spot and plays OPEN (OnPropPlaced); the rest spots are switched on by the character.
//       A prop that is already closed (dropped) is picked up again with no CLOSE. OpenPoseAnimation / ClosedPoseAnimation are optional
//       one-frame poses used for the starting pose, for snapping to the final pose when the pickup is interrupted, and for the
//       placement hologram (which must show the OPEN prop).
//     * Blueprint events: BP_OnGrabStarted, BP_OnGrabbed, BP_OnDropped, BP_OnPlaced, BP_OnThrown fire in the Blueprint child. Use
//       them to drive an Animation Blueprint, play sounds or particles.
//
// PHYSICS
//   The box simulates physics only while loose after a drop or a throw, or from the start if bStartSimulatingPhysics is on.
//   By default a prop in the level is solid and stays where it was put, until the player picks it up.
//
// NETWORK
//   Single player game: nothing replicates.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ItemIdentityInterface.h"
#include "Interfaces/PropInterface.h"
#include "SunsetCoveProp.generated.h"

class UAnimationAsset;
class USunsetCovePivotBoxComponent;
class USkeletalMeshComponent;

UCLASS(Blueprintable, ClassGroup = (SunsetCove))
class SUNSETCOVE_API ASunsetCoveProp : public AActor, public IPropInterface, public IItemIdentityInterface
{
	GENERATED_BODY()

public:
	ASunsetCoveProp();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	//~ End AActor Interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** The root: invisible box that carries collision, physics and the attachment to the hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Components")
	TObjectPtr<USunsetCovePivotBoxComponent> CollisionBody;

	/** The skeletal mesh (visual only, no collision). Set its Skeletal Mesh Asset in the Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Prop|Components")
	TObjectPtr<USkeletalMeshComponent> PropMesh;

	// ------------------------------------------------------------------
	// Identity
	// ------------------------------------------------------------------

	/**
	 * Id of the prop: Sunbed, Umbrella... It is the <Item> of the animator's file names (AS_Arms_<Item>_<Action>_<Hand>) and the
	 * key of the arms animation data asset. Empty: the id is taken from the Blueprint name without the BP_ prefix and the _C suffix
	 * (BP_Sunbed gives Sunbed), which is the rule the animator follows. Set it by hand for a Blueprint named differently (BP_Sunbed_Proto).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Identity")
	FName PropId = NAME_None;

	// ------------------------------------------------------------------
	// Grip (how the prop is held)
	// ------------------------------------------------------------------

	/**
	 * Bone or socket of the prop's skeleton that is placed on the hand socket when the prop is carried. Created by the
	 * animator in Blender. Used for the right hand, and for the left hand too unless LeftGripSocketName is set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Grip")
	FName GripSocketName = TEXT("Grip_R");

	/** Grip for the left hand (a prop that is held differently). Empty, or not found on the skeleton: GripSocketName is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Grip")
	FName LeftGripSocketName = TEXT("Grip_L");

	/** Fine tune: moves the prop relative to the hand socket (cm), on top of the grip alignment. Zero when the grip is right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Grip", meta = (Units = "cm"))
	FVector HoldLocationOffset = FVector::ZeroVector;

	/** Fine tune: rotates the prop relative to the hand socket, on top of the grip alignment. Zero when the grip is right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Grip")
	FRotator HoldRotationOffset = FRotator::ZeroRotator;

	// ------------------------------------------------------------------
	// Collision and physics
	// ------------------------------------------------------------------

	/** True: the box is sized from the bounds of the mesh and the mesh is placed in it (base on the origin, see CollisionBody > Origin). False: you size it with CollisionBoxExtent and place the mesh yourself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Collision")
	bool bAutoFitCollisionToMesh = true;

	/** Extra size added on the sides and the top of the auto-fitted box (cm), never under the base. A little extra makes the prop easier to aim at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Collision", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bAutoFitCollisionToMesh"))
	float CollisionPadding = 2.0f;

	/** Half size of the box (cm). Used when auto fit is off, and as the starting size while no mesh is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Collision", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "!bAutoFitCollisionToMesh"))
	FVector CollisionBoxExtent = FVector(50.0f, 50.0f, 50.0f);

	/** Mass of the prop (kg). Affects how it is pushed by other physics bodies once it is loose. The throw speed does not depend on it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Physics", meta = (ClampMin = "0.1", Units = "kg"))
	float MassKg = 10.0f;

	/**
	 * True: when the player DROPS the prop (Q) it is turned upright first: no pitch and no roll, only the heading the player looks at,
	 * plus DropRotationOffset. So a sunbed lands flat and an umbrella stands, whatever tilt it had in the hand. False: it keeps the
	 * rotation it had in the hand. (A thrown prop always keeps its tumble.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Drop")
	bool bDropUpright = true;

	/**
	 * Extra rotation applied on top of the upright one when dropping, in the prop's own space (yaw turns it, pitch and roll lay it
	 * down or stand it up). Zero for a prop whose mesh already stands the way it should rest (sunbed: flat, umbrella: standing).
	 * Use it when the closed prop rests on another side, for example Pitch 90 for a closed umbrella that is modeled standing but should lie.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Drop", meta = (EditCondition = "bDropUpright"))
	FRotator DropRotationOffset = FRotator::ZeroRotator;

	/** True: the prop simulates physics from the start (it falls and can be pushed). False: it stays where it is until it is grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Physics")
	bool bStartSimulatingPhysics = false;

	// ------------------------------------------------------------------
	// Rules and animation
	// ------------------------------------------------------------------

	/** False: the player cannot grab this prop (broken, locked, in use). Can be changed at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Rules")
	bool bCanBePickedUp = true;

	/**
	 * The CLOSE animation (AS_Sunbed_Close). Played once on PropMesh, WHERE THE PROP LIES, the moment the player picks it up. The prop
	 * snaps to the hand only after it has finished (and not before the arms Grab commit point). It must end in the closed pose and not
	 * animate the grip bone. Skipped when the prop is already closed (it was dropped or thrown). Optional. Replaces an Animation
	 * Blueprint on the mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Animation")
	TObjectPtr<UAnimationAsset> PickedUpAnimation;

	/**
	 * The OPEN animation (AS_Sunbed_Open). Played once on PropMesh when the prop has been put down at the hologram spot. It must end
	 * in the open pose. Optional. Replaces an Animation Blueprint on the mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Animation")
	TObjectPtr<UAnimationAsset> PlacedAnimation;

	/**
	 * Optional single pose of the OPEN prop (AS_Sunbed_OpenPose, one frame). Used as the starting pose of the prop, to snap it open
	 * when the Open animation is missing, and as the pose of the placement hologram. Empty: the reference pose of the mesh is the open
	 * pose (the animator's rule), and the hologram uses the Open animation's last frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Animation")
	TObjectPtr<UAnimationAsset> OpenPoseAnimation;

	/**
	 * Optional single pose of the CLOSED prop (AS_Sunbed_ClosedPose, one frame). Used to snap the prop closed when the pickup is
	 * interrupted before the Close animation ends, and when there is no Close animation. Empty: the last frame of the Close animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop|Animation")
	TObjectPtr<UAnimationAsset> ClosedPoseAnimation;

	/** True while the prop is open (placed, or lying in the level); false from the moment it starts closing until it is placed again. A dropped or thrown prop stays closed. */
	UFUNCTION(BlueprintPure, Category = "Prop")
	bool IsOpen() const { return bIsOpen; }

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/**
	 * Where the grip is, relative to the root of the prop (location and rotation only). Identity if there is no grip bone.
	 * Useful to check the grip in Blueprint or to attach something else to it.
	 */
	UFUNCTION(BlueprintPure, Category = "Prop")
	FTransform GetGripTransformInRoot(bool bRightHand) const;

	// ------------------------------------------------------------------
	// Blueprint events (the prop-side reaction to what the player does)
	// ------------------------------------------------------------------

	/** The pickup started: the prop is still on the ground and starts to close (the Close animation plays now). Holder is the player's pawn. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prop", meta = (DisplayName = "On Grab Started"))
	void BP_OnGrabStarted(AActor* Holder);

	/** The prop was attached to a hand (after the Close animation). Holder is the player's pawn. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prop", meta = (DisplayName = "On Grabbed"))
	void BP_OnGrabbed(AActor* Holder);

	/** The prop was dropped from a hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prop", meta = (DisplayName = "On Dropped"))
	void BP_OnDropped(AActor* Holder);

	/** The prop left the hand and starts flying to the placement spot (the Open animation plays now). Holder is the player's pawn. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prop", meta = (DisplayName = "On Place Started"))
	void BP_OnPlaceStarted(AActor* Holder);

	/** The prop landed at the hologram position (its rest spots are switched on by the character). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prop", meta = (DisplayName = "On Placed"))
	void BP_OnPlaced(AActor* Holder);

	/** The prop was thrown. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Prop", meta = (DisplayName = "On Thrown"))
	void BP_OnThrown(AActor* Holder);

	// ------------------------------------------------------------------
	// IPropInterface (implemented here, nothing to do in the Blueprint)
	// ------------------------------------------------------------------

	virtual bool IsPickupBlocked_Implementation(AActor* Picker) const override;
	virtual FVector GetPropHoldLocationOffset_Implementation(bool bRightHand) const override;
	virtual FRotator GetPropHoldRotationOffset_Implementation(bool bRightHand) const override;
	virtual bool GetPropDropRotation_Implementation(float ViewYaw, FQuat& OutRotation) const override;
	virtual bool GetPropPlacementBounds_Implementation(FBox& OutLocalBounds) const override;
	virtual bool GetPropPlacementPreviewPose_Implementation(UAnimationAsset*& OutAsset, float& OutTime) const override;
	virtual float OnPropGrabStarted_Implementation(AActor* Holder) override;
	virtual float OnPropPlaceStarted_Implementation(AActor* Holder) override;
	virtual void OnPropGrabbed_Implementation(AActor* Holder) override;
	virtual void OnPropDropped_Implementation(AActor* Holder) override;
	virtual void OnPropPlaced_Implementation(AActor* Holder) override;
	virtual void OnPropThrown_Implementation(AActor* Holder) override;

	// ------------------------------------------------------------------
	// IItemIdentityInterface
	// ------------------------------------------------------------------

	/** PropId, or the Blueprint name without BP_ / _C when PropId is empty. */
	virtual FName GetItemId_Implementation() const override;

private:
	/** Sizes the box from the mesh and centers the mesh in it. Runs from OnConstruction, so it is visible in the Blueprint viewport. */
	void ApplyCollisionSetup();

	/** The transform of the root relative to the hand socket: the inverse of the grip, then the fine tune offset. */
	FTransform ComputeRootInHandSocket(bool bRightHand) const;

	/** The grip bone to use for a hand, or None if there is none. */
	FName ResolveGripName(bool bRightHand) const;

	/** Logs, once, what is missing for the prop to be held right (no mesh, no grip bone). */
	void ValidateSetup() const;

	void PlayOptionalAnimation(UAnimationAsset* Animation) const;

	/** Shows a single pose on PropMesh at once and holds it (Time 0 for a pose asset, the last frame for a full animation). Null does nothing. */
	void ApplyPose(UAnimationAsset* Pose, bool bLastFrame) const;

	/** Length in seconds of an animation asset, or 0 when it is null. */
	static float GetAnimationLength(const UAnimationAsset* Animation);

	/** The Close animation is playing on PropMesh right now (the pickup has started and has not been attached yet). */
	bool bIsClosing = false;

	/** See IsOpen. */
	bool bIsOpen = true;
};
