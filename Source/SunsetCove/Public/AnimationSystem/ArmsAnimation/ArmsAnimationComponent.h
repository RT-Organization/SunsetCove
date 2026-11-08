// ArmsAnimationComponent.h
// SunsetCove - Animation System / Arms Animation. Turns what the inventories do into first-person arm animations.
//
// WHAT THIS CLASS IS
//   The animation layer for the arms with props and tools. It LISTENS to the Prop Inventory and the Tool Inventory (their delegates and
//   their ActionBeginHook) and plays the animation of THAT item, in THAT hand, through a data asset (UArmsAnimationSet). The inventory
//   components do not know it exists.
//
// THE PARTS
//   UArmsAnimationSet        data asset: item id -> action -> hand -> animation (DA_ArmsAnimations). A new item is a new entry, no code.
//   UArmsAnimInstance        the graph variables of ABP_FPArms_Items, a POST-PROCESS Animation Blueprint that runs after the base arms
//                            Animation Blueprint and layers the item animation over each arm separately.
//   this component           reads the events, finds the clip, plays it, and says when the item really moves.
//   UAnimNotify_ArmsItemCommit   the notify the animator's frame becomes ("NOW the item moves").
//
// TWO KINDS OF ANIMATION
//   POSES  Hold, PlaceAim, ThrowCharge. They last. The component writes them as variables of UArmsAnimInstance and the graph shows them.
//          ThrowCharge is a pose that ends on its last frame and stays there until the release.
//   ONE-SHOTS  Grab, Place, Throw, Drop, Equip, Holster, Use, Pickup, Open, Close. Each is a MONTAGE built at run time from the
//          sequence (no AM_ asset exists) and played in the slot of the arm (ArmItem_R / ArmItem_L, each in a slot group of its own).
//          The slot source is the held pose, so a montage returns to Hold (or to locomotion after a release) by itself.
//   The free arm keeps its idle / walk / run: the graph only overrides the bones of the arm that holds or uses an item.
//
// WHEN THE ITEM MOVES (commit timing)
//   The inventories change their logical state at once (hand occupied, tool equipped) and ask this component, through the
//   ActionBeginHook, WHEN the physical effect must happen (attach, detach, launch, show, hide). The answer comes from the clip:
//   Immediate, After Delay or On Notify (see FArmsClip and Core/Utils/SunsetCoveActionCommit.h). When the notify is reached this
//   component calls CommitPendingAction() on the inventory that is waiting.
//
// NOTHING MISSING IS AN ERROR
//   No data asset, no item, no action, no hand, no animation, no post-process Animation Blueprint: nothing plays, nothing crashes and
//   nothing is logged per event. The arm keeps its locomotion pose. One line at start says what is not set up. bLogMissingClips lists
//   each missing clip once, for a test session.
//
// WIRING (done by ASunsetCoveCharacter, the composition root)
//   SetArmsMesh, BindToPropInventory, BindToToolInventory. The component never ticks (one next-tick timer when an aim or a charge ends).
//
// TESTING WITHOUT GAMEPLAY (not in shipping builds)
//   Console: SunsetCove.Arms.Play <Item> <Action> <R|L>      plays one clip, or holds one pose, on an arm
//            SunsetCove.Arms.Clear                           back to what the inventories say
//            SunsetCove.Arms.Dump                            prints the state of both arms
//
// NETWORK
//   Single player game: nothing replicates.

#pragma once

#include "CoreMinimal.h"
#include "AnimationSystem/ArmsAnimation/ArmsAnimationTypes.h"
#include "Components/ActorComponent.h"
#include "InventorySystem/PropInventorySystem/PropInventoryTypes.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryTypes.h"
#include "ArmsAnimationComponent.generated.h"

class ASunsetCoveTool;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UArmsAnimInstance;
class UArmsAnimationSet;
class UPropInventoryComponent;
class USkeletalMeshComponent;
class UToolInventoryComponent;

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API UArmsAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArmsAnimationComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration (BP_Character > Components > Arms Animation)
	// ------------------------------------------------------------------

	/** The mapping item > action > hand > animation (DA_ArmsAnimations). Empty: no item animation at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmsAnimation")
	TObjectPtr<UArmsAnimationSet> AnimationSet;

	/**
	 * The post-process Animation Blueprint that layers the items (ABP_FPArms_Items, parent: Arms Anim Instance). It is installed on the arms
	 * mesh at start, so nothing depends on a setting stored in the skeletal mesh asset (a re-import cannot lose it). Leave it empty if you
	 * assigned it as the Post Process Anim Blueprint of the skeletal mesh asset by hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmsAnimation")
	TSubclassOf<UAnimInstance> PostProcessAnimClass;

	/** True: a clip that is missing is listed once in the Output Log (LogSunsetCove). For a test session; off by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmsAnimation|Debug")
	bool bLogMissingClips = false;

	// ------------------------------------------------------------------
	// Wiring (called by the character, C++ only)
	// ------------------------------------------------------------------

	/** The first-person arms mesh. */
	void SetArmsMesh(USkeletalMeshComponent* InArmsMesh);

	/** Listens to the Prop Inventory and takes the ActionBeginHook of it. */
	void BindToPropInventory(UPropInventoryComponent* InPropInventory);

	/** Listens to the Tool Inventory and takes the ActionBeginHook of it. */
	void BindToToolInventory(UToolInventoryComponent* InToolInventory);

	/** Called by UAnimNotify_ArmsItemCommit: the animation reached the frame where the item moves. */
	void HandleCommitNotify(const UAnimSequenceBase* Animation);

	// ------------------------------------------------------------------
	// Test commands (also what the console commands call)
	// ------------------------------------------------------------------

	/**
	 * Plays one clip of an item on an arm (a one-shot, with no inventory behind it) or holds one pose (Hold, PlaceAim, ThrowCharge) until
	 * ClearArms or the next inventory event. Returns false if there is no clip.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmsAnimation|Debug")
	bool PlayItemAnimation(FName ItemId, EArmsAnimAction Action, EArmSide Side);

	/** Stops the test animations and shows again what the inventories say. */
	UFUNCTION(BlueprintCallable, Category = "ArmsAnimation|Debug")
	void ClearArms();

	/** Prints the state of both arms in the Output Log (LogSunsetCove). */
	UFUNCTION(BlueprintCallable, Category = "ArmsAnimation|Debug")
	void DumpStatus() const;

protected:
	// ---- Delegates of the inventories (all UFUNCTION: they are dynamic delegates) ----
	UFUNCTION()
	void OnPropSlotsChanged();

	UFUNCTION()
	void OnPropStateChanged(EPropInventoryState NewState, EPropInventoryState OldState);

	UFUNCTION()
	void OnToolSlotsChanged();

	UFUNCTION()
	void OnToolEquippedChanged(ASunsetCoveTool* NewTool, ASunsetCoveTool* OldTool, int32 SlotIndex);

	UFUNCTION()
	void OnToolAction(EToolAction Action, int32 SlotIndex, ASunsetCoveTool* Tool);

	UFUNCTION()
	void OnToolStateChanged(EToolInventoryState NewState, EToolInventoryState OldState);

private:
	// ---- Hooks of the inventories (C++): the animation starts here and the answer says when the item moves ----
	FActionCommitTiming HandlePropActionBegin(EPropAction Action, EPropHand Hand, AActor* Prop);
	FActionCommitTiming HandleToolActionBegin(EToolAction Action, int32 SlotIndex, ASunsetCoveTool* Tool);

	/** The state of one arm. */
	struct FArmChannel
	{
		/** What the arm holds now (a prop or the equipped tool). */
		TWeakObjectPtr<AActor> Item;
		FName ItemId = NAME_None;
		/** The pose that is wanted (Hold, or PlaceAim / ThrowCharge while those states last). */
		EArmHeldPose Desired = EArmHeldPose::None;
		/** A release animation (Place, Throw, Drop, Holster) is playing: the pose stays until it starts blending out. */
		bool bReleasing = false;
		/** The last one-shot started on this arm. */
		TWeakObjectPtr<UAnimMontage> OneShot;
		/** Counts the one-shots, so a late callback of an interrupted montage cannot touch a newer state. */
		uint32 Generation = 0;
	};

	static int32 IndexOf(EArmSide Side) { return Side == EArmSide::Right ? 0 : 1; }
	static EArmSide SideOf(EPropHand Hand) { return Hand == EPropHand::Right ? EArmSide::Right : EArmSide::Left; }

	/** The Animation Instance that owns the arm slots: the post-process one, or the main one if the layer lives there. Null if neither. */
	UArmsAnimInstance* GetItemsInstance() const;

	/** The id of an item (IItemIdentityInterface). NAME_None if it has none. */
	static FName GetItemIdOf(const AActor* Item);

	/**
	 * Starts the one-shot of an item. Returns the commit timing of the clip (Immediate when there is no clip).
	 * bItemLeavesHand: Place, Throw, Drop of a prop and Holster of a tool: the arm keeps its pose until the animation blends out.
	 */
	FActionCommitTiming BeginOneShot(EArmSide Side, EArmsAnimAction Action, const AActor* Item, bool bItemLeavesHand, bool bFromPropInventory);

	/** Plays a clip in the slot of an arm as a dynamic montage. Null if it could not play. */
	UAnimMontage* PlayClip(EArmSide Side, const FArmsClip& Clip, bool bLoop);

	/** Re-reads what the inventories say about an arm and applies the pose. Does nothing while the arm is releasing. */
	void ResyncArm(EArmSide Side);

	/** Writes the pose of an arm (Channel.Desired and the sequences of its item) to the Animation Instance. */
	void ApplyArm(EArmSide Side);

	/** An aim or a charge ended: back to Hold, unless a release took over or a new aim started. Runs on the next tick. */
	void EndAimPoses();

	/** One line at start about what is not set up (a missing set, mesh, post-process Animation Blueprint or slot). */
	void ValidateSetup();

	void LogMissingClip(FName ItemId, EArmsAnimAction Action, EArmSide Side);

	FArmChannel Channels[2];

	TWeakObjectPtr<USkeletalMeshComponent> ArmsMesh;
	TWeakObjectPtr<UPropInventoryComponent> PropInventory;
	TWeakObjectPtr<UToolInventoryComponent> ToolInventory;

	/** The clip an inventory waits for the commit notify of (matched by sequence when the notify arrives). */
	TWeakObjectPtr<const UAnimSequenceBase> PropPendingSequence;
	TWeakObjectPtr<const UAnimSequenceBase> ToolPendingSequence;

	/** The loop of a tool in use (the spray), stopped when the use action is released. */
	TWeakObjectPtr<UAnimMontage> UseMontage;
	float UseBlendOut = 0.2f;

	TSet<uint32> LoggedMissingClips;
	bool bAimActive = false;
	bool bWarnedMissingSlot = false;
	bool bWarnedMissingNotify = false;
};
