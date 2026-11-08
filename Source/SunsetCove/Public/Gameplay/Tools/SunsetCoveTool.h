// SunsetCoveTool.h
// SunsetCove - Gameplay area (Tools). The base class of every tool: bucket, deodorant spray, toy, garbage bag...
//
// WHAT THIS CLASS IS
//   Everything a tool needs to work with the Tool Inventory, built in C++. To make a tool, create a Blueprint whose parent
//   class is "Sunset Cove Tool", then set the Skeletal Mesh Asset of the ToolMesh component, the ToolId and the display
//   name. That is all: no root to restructure, no collision to set up, no socket to add, no interface to implement.
//   (It is the same idea as ASunsetCoveProp for props.)
//
// HIERARCHY
//   AActor -> ASunsetCoveTool (this class) -> BP_Bucket, BP_DeodorantSpray, BP_Toy, BP_GarbageBag ... (Blueprints)
//
// THE LIFE OF A TOOL
//   In the world    A tool lies in a visitor's spot (spawned there by another system) as a solid object. The player aims
//                   at it and presses Interact: the Tool Inventory picks it up.
//   Stored          A picked up tool is hidden and attached to the hand. The player has up to 4 of them.
//   Equipped        The tool chosen in the radial menu is shown in the hand. Using it (left mouse) calls BeginUse / EndUse.
//   Dropped         The player can drop it from the radial menu: it falls to the ground and is a world tool again.
//   Tools are never consumed: they are used forever (GDD).
//
// COMPONENTS
//   CollisionBody (root)   An invisible box: the thing the interaction trace hits, and what falls when the tool is dropped.
//                          Its size comes from the mesh (bAutoFitCollisionToMesh), so you never size it by hand.
//   ToolMesh               The skeletal mesh (visual only, no collision). Tools may animate, like props do.
//
// THE GRIP (how the tool sits in the hand)
//   GripSocketName names a bone (or a socket) of the tool's skeleton, created by the animator in Blender (default "Grip_R").
//   When the tool is picked up, the system places the tool so that THIS bone lands exactly on the hand socket of the arms
//   (Tool_R, or Prop_R when the arms have no Tool_R). HoldLocationOffset / HoldRotationOffset are a small per-tool fine tune
//   on top of it. Without a grip bone the center of the tool is placed on the hand socket, and one warning says so.
//   Same math and same rules as props (Public/Core/Utils/SunsetCoveHeldItemUtils.h).
//
// USING A TOOL (the part you change per tool)
//   The Tool Inventory calls BeginUse when the player presses the use action and EndUse when it is released.
//     UseMode Press   one use per click (bucket, toy, garbage bag).
//     UseMode Hold    repeats every HoldUseInterval seconds while the action is held (spray).
//   Each use runs:  cooldown check -> find the target (a trace from the player's view, UseRange) -> PerformUse.
//   The default PerformUse calls IToolTargetInterface on the target (Public/Interfaces/), which is how a crab, a child or a
//   smelly visitor reacts to the tool. A tool with its own effect (a spray particle) overrides PerformUse in C++, or reacts
//   to the Blueprint event "On Used". IsUseBlocked lets a tool refuse to be used (broken, empty, not now).
//
// ANIMATION AND UI HOOKS (nothing is drawn or played by the system itself)
//   * Optional animation assets: EquipAnimation and UseAnimation are played once on ToolMesh. They use a single animation
//     node, so they replace an Animation Blueprint on ToolMesh. Leave them empty if you use an Animation Blueprint.
//   * Blueprint events: On Picked Up, On Equipped, On Holstered, On Dropped, On Use Started, On Use Ended, On Used.
//     Use them to drive an Animation Blueprint, play sounds or particles.
//   * ToolId, DisplayName and Icon are what the radial menu widget and the data tables need.
//
// NETWORK
//   Single player game: nothing replicates.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "SunsetCoveTool.generated.h"

class UAnimationAsset;
class UBoxComponent;
class USkeletalMeshComponent;
class UTexture2D;

/** How the use action of a tool works. */
UENUM(BlueprintType)
enum class EToolUseMode : uint8
{
	/** One use per press of the action (bucket, toy, garbage bag). */
	Press	UMETA(DisplayName = "Press"),
	/** Uses repeat every HoldUseInterval seconds while the action is held (spray). */
	Hold	UMETA(DisplayName = "Hold")
};

/** The tool was used. Target is the actor the use reached (null if the tool needs no target and none was aimed at). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FToolUsedSignature, ASunsetCoveTool*, Tool, AActor*, Target, const FHitResult&, Hit);

UCLASS(Blueprintable, ClassGroup = (SunsetCove))
class SUNSETCOVE_API ASunsetCoveTool : public AActor
{
	GENERATED_BODY()

public:
	ASunsetCoveTool();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** The root: invisible box that carries collision, physics and the attachment to the hand. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tool|Components")
	TObjectPtr<UBoxComponent> CollisionBody;

	/** The skeletal mesh (visual only, no collision). Set its Skeletal Mesh Asset in the Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tool|Components")
	TObjectPtr<USkeletalMeshComponent> ToolMesh;

	// ------------------------------------------------------------------
	// Identity (for the UI and the data tables)
	// ------------------------------------------------------------------

	/** Unique id of the tool. Use the row name of the tool in the "Strumenti" table of SunsetCove_Dati_Oggetti. Two tools with the same id count as the same tool. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Identity")
	FName ToolId = NAME_None;

	/** Name shown to the player (radial menu, messages). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Identity")
	FText DisplayName;

	/** Icon of the radial menu. Soft reference: the widget loads it when it needs it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Identity")
	TSoftObjectPtr<UTexture2D> Icon;

	// ------------------------------------------------------------------
	// Grip (how the tool is held)
	// ------------------------------------------------------------------

	/** Bone or socket of the tool's skeleton that is placed on the hand socket when the tool is carried. Created by the animator in Blender. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Grip")
	FName GripSocketName = TEXT("Grip_R");

	/** Fine tune: moves the tool relative to the hand socket (cm), on top of the grip alignment. Zero when the grip is right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Grip", meta = (Units = "cm"))
	FVector HoldLocationOffset = FVector::ZeroVector;

	/** Fine tune: rotates the tool relative to the hand socket, on top of the grip alignment. Zero when the grip is right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Grip")
	FRotator HoldRotationOffset = FRotator::ZeroRotator;

	// ------------------------------------------------------------------
	// Collision and physics (while the tool lies in the world or after a drop)
	// ------------------------------------------------------------------

	/** True: the box is sized from the bounds of the mesh and the mesh is centered in it. False: you size it with CollisionBoxExtent and place the mesh yourself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Collision")
	bool bAutoFitCollisionToMesh = true;

	/** Extra size added on every side of the auto-fitted box (cm). A little extra makes the tool easier to aim at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Collision", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bAutoFitCollisionToMesh"))
	float CollisionPadding = 2.0f;

	/** Half size of the box (cm). Used when auto fit is off, and as the starting size while no mesh is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Collision", meta = (ClampMin = "1.0", Units = "cm", EditCondition = "!bAutoFitCollisionToMesh"))
	FVector CollisionBoxExtent = FVector(15.0f, 15.0f, 15.0f);

	/** Mass of the tool (kg). Affects how it is pushed by other physics bodies once it is loose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Physics", meta = (ClampMin = "0.1", Units = "kg"))
	float MassKg = 2.0f;

	/** True: the tool simulates physics from the start (it falls and can be pushed). False: it stays where it is until it is picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Physics")
	bool bStartSimulatingPhysics = false;

	// ------------------------------------------------------------------
	// Rules
	// ------------------------------------------------------------------

	/** False: the player cannot pick this tool up (locked, not available yet). Can be changed at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Rules")
	bool bCanBePickedUp = true;

	// ------------------------------------------------------------------
	// Use
	// ------------------------------------------------------------------

	/** Press: one use per click. Hold: repeats while the action is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use")
	EToolUseMode UseMode = EToolUseMode::Press;

	/** Minimum time between two uses (seconds). 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use", meta = (ClampMin = "0.0", Units = "s"))
	float UseCooldownSeconds = 0.4f;

	/** Hold mode only: seconds between two repeated uses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use", meta = (ClampMin = "0.02", Units = "s", EditCondition = "UseMode == EToolUseMode::Hold"))
	float HoldUseInterval = 0.2f;

	/** How far from the camera the tool reaches (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use", meta = (ClampMin = "0.0", Units = "cm"))
	float UseRange = 250.0f;

	/** Radius of the aim trace (cm). 0 = a thin line. A little radius makes small targets (crabs) easier to hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use", meta = (ClampMin = "0.0", Units = "cm"))
	float UseTraceRadius = 15.0f;

	/** Trace channel of the aim trace. Targets must BLOCK it (characters and static meshes do by default on Visibility). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use")
	TEnumAsByte<ETraceTypeQuery> UseTraceChannel;

	/** True: a use without a valid target does nothing at all (no cooldown, no event). False: it is a use anyway (a spray works on empty air). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use")
	bool bRequiresTarget = true;

	/** Draws the aim trace and what it hit. For tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Use")
	bool bDrawDebug = false;

	// ------------------------------------------------------------------
	// Animation (optional assets, see the notes at the top of this file)
	// ------------------------------------------------------------------

	/** Played once on ToolMesh when the tool is taken out. Optional. Replaces an Animation Blueprint on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Animation")
	TObjectPtr<UAnimationAsset> EquipAnimation;

	/** Played once on ToolMesh at every use. Optional. Replaces an Animation Blueprint on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool|Animation")
	TObjectPtr<UAnimationAsset> UseAnimation;

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/**
	 * Where the grip is, relative to the root of the tool (location and rotation only). Identity if there is no grip bone.
	 * Useful to check the grip in Blueprint.
	 */
	UFUNCTION(BlueprintPure, Category = "Tool")
	FTransform GetGripTransformInRoot() const;

	/** Where the root of the tool sits relative to the hand socket while it is carried (the grip lands on the socket). The Tool Inventory uses it. */
	UFUNCTION(BlueprintPure, Category = "Tool")
	FTransform GetRootInHandSocketTransform() const;

	/** The actor that holds the tool (the player's pawn), or null while the tool lies in the world. */
	UFUNCTION(BlueprintPure, Category = "Tool")
	AActor* GetHolder() const { return Holder.Get(); }

	/** True while the tool is stored or equipped by someone. */
	UFUNCTION(BlueprintPure, Category = "Tool")
	bool IsHeld() const { return Holder.IsValid(); }

	/** True between BeginUse and EndUse. */
	UFUNCTION(BlueprintPure, Category = "Tool")
	bool IsInUse() const { return bInUse; }

	/** True while the cooldown of the last use has not ended. */
	UFUNCTION(BlueprintPure, Category = "Tool")
	bool IsOnCooldown() const;

	/**
	 * Aims from the player's view and returns the first actor in UseRange that accepts this tool (it implements the tool target
	 * interface and does not refuse). Null if there is none. OutHit is always filled: the hit point, or the end of the range.
	 * Only meaningful while the tool is held.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tool")
	AActor* FindUseTarget(FHitResult& OutHit);

	// ------------------------------------------------------------------
	// Overridable behavior
	// ------------------------------------------------------------------

	/** Return true to refuse a use right now (broken, empty, not allowed here). Default: false. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tool")
	bool IsUseBlocked() const;
	virtual bool IsUseBlocked_Implementation() const { return false; }

	/**
	 * The effect of one use. Target is the accepted target (may be null when bRequiresTarget is off). The default tells the
	 * target (IToolTargetInterface::ReceiveToolUse). Override it for a tool with its own effect.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tool")
	void PerformUse(AActor* Target, const FHitResult& Hit);
	virtual void PerformUse_Implementation(AActor* Target, const FHitResult& Hit);

	// ------------------------------------------------------------------
	// Called by the Tool Inventory (C++ only, not for gameplay code)
	// ------------------------------------------------------------------

	/** Stored in the inventory of InNewHolder. */
	virtual void NotifyPickedUp(AActor* NewHolder);

	/** Taken out into the hand. Plays EquipAnimation. */
	virtual void NotifyEquipped();

	/** Put away. Ends a use in progress. */
	virtual void NotifyHolstered();

	/** Dropped on the ground: no holder any more. Ends a use in progress. */
	virtual void NotifyDropped();

	/**
	 * The use action was pressed. Press mode: performs one use and returns whether it happened. Hold mode: starts repeating
	 * and returns true. False also if the tool has no holder, is blocked or is on cooldown (Press).
	 */
	virtual bool BeginUse();

	/** The use action was released. Does nothing if the tool is not in use. */
	virtual void EndUse();

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	/** The tool had an effect: one use happened. Target may be null. */
	UPROPERTY(BlueprintAssignable, Category = "Tool")
	FToolUsedSignature OnUsed;

	// ------------------------------------------------------------------
	// Blueprint events (the tool-side reaction to what the player does)
	// ------------------------------------------------------------------

	/** The tool was picked up and stored. Holder is the player's pawn. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Picked Up"))
	void BP_OnPickedUp(AActor* InHolder);

	/** The tool was taken out into the hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Equipped"))
	void BP_OnEquipped(AActor* InHolder);

	/** The tool was put away. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Holstered"))
	void BP_OnHolstered(AActor* InHolder);

	/** The tool was dropped on the ground. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Dropped"))
	void BP_OnDropped(AActor* InHolder);

	/** The player started using the tool (accepted by the tool). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Use Started"))
	void BP_OnUseStarted();

	/** The player stopped using the tool. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Use Ended"))
	void BP_OnUseEnded();

	/** One use happened (the effect of a click, or of one repeat in Hold mode). Target may be null. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Tool", meta = (DisplayName = "On Used"))
	void BP_OnUsed(AActor* Target, const FHitResult& Hit);

private:
	/** Sizes the box from the mesh and centers the mesh in it. Runs from OnConstruction, so it is visible in the Blueprint viewport. */
	void ApplyCollisionSetup();

	/** The grip bone to use, or None if the skeleton has none. */
	FName ResolveGripName() const;

	/** Logs, once, what is missing for the tool to be held right (no mesh, no grip bone). */
	void ValidateSetup() const;

	/** The checks of a use: holder, blocked, cooldown, target. True if a use can happen now; OutTarget and OutHit are filled. */
	bool PrepareUse(AActor*& OutTarget, FHitResult& OutHit);

	/** The use itself: starts the cooldown, plays the animation, PerformUse, events. */
	void ExecuteUse(AActor* Target, const FHitResult& Hit);

	/** PrepareUse and ExecuteUse in one. Returns true if a use happened. */
	bool TryPerformUse();

	void PlayOptionalAnimation(UAnimationAsset* Animation) const;
	void ClearHoldTimer();

	TWeakObjectPtr<AActor> Holder;
	FTimerHandle HoldTimer;
	double LastUseTime = -1.0e9;
	bool bInUse = false;
};
