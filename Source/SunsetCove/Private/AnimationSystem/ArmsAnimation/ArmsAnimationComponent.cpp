// ArmsAnimationComponent.cpp
// SunsetCove - Animation System / Arms Animation. See Public/AnimationSystem/ArmsAnimation/ArmsAnimationComponent.h

#include "AnimationSystem/ArmsAnimation/ArmsAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationSystem/ArmsAnimation/AnimNotify_ArmsItemCommit.h"
#include "AnimationSystem/ArmsAnimation/ArmsAnimInstance.h"
#include "AnimationSystem/ArmsAnimation/ArmsAnimationSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Tools/SunsetCoveTool.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/ItemIdentityInterface.h"
#include "InventorySystem/PropInventorySystem/PropInventoryComponent.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryComponent.h"
#include "TimerManager.h"

namespace
{
	/** A looping use (the spray) is a montage that repeats this many times: effectively until it is stopped. */
	constexpr int32 EndlessLoopCount = 100000;

	/** Seconds added to the length of a clip for the default timeout of a commit that waits for a notify. */
	constexpr float NotifyTimeoutMargin = 0.1f;

	bool ClipHasCommitNotify(const UAnimSequenceBase* Sequence)
	{
		if (!Sequence)
		{
			return false;
		}
		for (const FAnimNotifyEvent& Event : Sequence->Notifies)
		{
			if (Event.Notify && Event.Notify->IsA<UAnimNotify_ArmsItemCommit>())
			{
				return true;
			}
		}
		return false;
	}

	const TCHAR* SideText(EArmSide Side)
	{
		return Side == EArmSide::Right ? TEXT("R") : TEXT("L");
	}

	FString ActionText(EArmsAnimAction Action)
	{
		FString Name = StaticEnum<EArmsAnimAction>()->GetNameStringByValue(static_cast<int64>(Action));
		int32 ColonIndex = INDEX_NONE;
		if (Name.FindLastChar(TEXT(':'), ColonIndex))
		{
			Name = Name.RightChop(ColonIndex + 1);
		}
		return Name;
	}
}

UArmsAnimationComponent::UArmsAnimationComponent()
{
	// Event driven. No tick.
	PrimaryComponentTick.bCanEverTick = false;
}

// ----------------------------------------------------------------------
// UActorComponent
// ----------------------------------------------------------------------

void UArmsAnimationComponent::BeginPlay()
{
	Super::BeginPlay();

	// The post-process Animation Blueprint is installed by the component, so it does not depend on a setting of the mesh asset.
	if (USkeletalMeshComponent* Mesh = ArmsMesh.Get())
	{
		if (PostProcessAnimClass)
		{
			Mesh->SetOverridePostProcessAnimBP(PostProcessAnimClass, /*ReinitAnimInstances=*/true);
		}
	}

	ValidateSetup();

	// Whatever the inventories already hold at start (a prop placed in the hand by a test, a starting tool) is shown.
	ResyncArm(EArmSide::Right);
	ResyncArm(EArmSide::Left);
}

void UArmsAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPropInventoryComponent* Props = PropInventory.Get())
	{
		Props->ActionBeginHook.Unbind();
	}
	if (UToolInventoryComponent* Tools = ToolInventory.Get())
	{
		Tools->ActionBeginHook.Unbind();
	}

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Wiring
// ----------------------------------------------------------------------

void UArmsAnimationComponent::SetArmsMesh(USkeletalMeshComponent* InArmsMesh)
{
	ArmsMesh = InArmsMesh;
}

void UArmsAnimationComponent::BindToPropInventory(UPropInventoryComponent* InPropInventory)
{
	PropInventory = InPropInventory;
	if (!InPropInventory)
	{
		return;
	}

	InPropInventory->OnSlotsChanged.AddUniqueDynamic(this, &UArmsAnimationComponent::OnPropSlotsChanged);
	InPropInventory->OnStateChanged.AddUniqueDynamic(this, &UArmsAnimationComponent::OnPropStateChanged);
	InPropInventory->ActionBeginHook.BindUObject(this, &UArmsAnimationComponent::HandlePropActionBegin);
}

void UArmsAnimationComponent::BindToToolInventory(UToolInventoryComponent* InToolInventory)
{
	ToolInventory = InToolInventory;
	if (!InToolInventory)
	{
		return;
	}

	InToolInventory->OnSlotsChanged.AddUniqueDynamic(this, &UArmsAnimationComponent::OnToolSlotsChanged);
	InToolInventory->OnEquippedToolChanged.AddUniqueDynamic(this, &UArmsAnimationComponent::OnToolEquippedChanged);
	InToolInventory->OnToolAction.AddUniqueDynamic(this, &UArmsAnimationComponent::OnToolAction);
	InToolInventory->OnStateChanged.AddUniqueDynamic(this, &UArmsAnimationComponent::OnToolStateChanged);
	InToolInventory->ActionBeginHook.BindUObject(this, &UArmsAnimationComponent::HandleToolActionBegin);
}

UArmsAnimInstance* UArmsAnimationComponent::GetItemsInstance() const
{
	USkeletalMeshComponent* Mesh = ArmsMesh.Get();
	if (!Mesh)
	{
		return nullptr;
	}

	if (UArmsAnimInstance* PostProcess = Cast<UArmsAnimInstance>(Mesh->GetPostProcessInstance()))
	{
		return PostProcess;
	}

	// The layer may live in the main Animation Blueprint instead (a fallback that needs no post-process instance).
	return Cast<UArmsAnimInstance>(Mesh->GetAnimInstance());
}

FName UArmsAnimationComponent::GetItemIdOf(const AActor* Item)
{
	if (IsValid(Item) && Item->Implements<UItemIdentityInterface>())
	{
		return IItemIdentityInterface::Execute_GetItemId(Item);
	}
	return NAME_None;
}

// ----------------------------------------------------------------------
// Hooks of the inventories: the animation starts here
// ----------------------------------------------------------------------

FActionCommitTiming UArmsAnimationComponent::HandlePropActionBegin(EPropAction Action, EPropHand Hand, AActor* Prop)
{
	EArmsAnimAction AnimAction = EArmsAnimAction::Grab;
	bool bLeavesHand = false;
	switch (Action)
	{
	case EPropAction::Grab:
		AnimAction = EArmsAnimAction::Grab;
		break;
	case EPropAction::Place:
		AnimAction = EArmsAnimAction::Place;
		bLeavesHand = true;
		break;
	case EPropAction::Throw:
		AnimAction = EArmsAnimAction::Throw;
		bLeavesHand = true;
		break;
	case EPropAction::Drop:
		AnimAction = EArmsAnimAction::Drop;
		bLeavesHand = true;
		break;
	}

	return BeginOneShot(SideOf(Hand), AnimAction, Prop, bLeavesHand, /*bFromPropInventory=*/true);
}

FActionCommitTiming UArmsAnimationComponent::HandleToolActionBegin(EToolAction Action, int32 /*SlotIndex*/, ASunsetCoveTool* Tool)
{
	// Tools are held in the right hand (Tool_R). Tool_L is reserved.
	EArmsAnimAction AnimAction = EArmsAnimAction::Pickup;
	bool bLeavesHand = false;
	switch (Action)
	{
	case EToolAction::Pickup:
		AnimAction = EArmsAnimAction::Pickup;
		break;
	case EToolAction::Equip:
		AnimAction = EArmsAnimAction::Equip;
		break;
	case EToolAction::Holster:
		AnimAction = EArmsAnimAction::Holster;
		bLeavesHand = true;
		break;
	case EToolAction::Drop:
		AnimAction = EArmsAnimAction::Drop;
		break;
	default:
		return FActionCommitTiming::MakeImmediate();
	}

	return BeginOneShot(EArmSide::Right, AnimAction, Tool, bLeavesHand, /*bFromPropInventory=*/false);
}

FActionCommitTiming UArmsAnimationComponent::BeginOneShot(EArmSide Side, EArmsAnimAction Action, const AActor* Item, bool bItemLeavesHand, bool bFromPropInventory)
{
	FArmChannel& Channel = Channels[IndexOf(Side)];

	// A new one-shot starts on this arm: whatever a previous release was waiting for no longer applies.
	++Channel.Generation;
	Channel.bReleasing = false;

	const FName ItemId = GetItemIdOf(Item);
	FArmsClip Clip;
	if (!AnimationSet || ItemId.IsNone() || !AnimationSet->FindClip(ItemId, Action, Side, Clip))
	{
		if (AnimationSet && !ItemId.IsNone())
		{
			LogMissingClip(ItemId, Action, Side);
		}
		return FActionCommitTiming::MakeImmediate();
	}

	UAnimMontage* Montage = PlayClip(Side, Clip, /*bLoop=*/false);
	if (!Montage)
	{
		return FActionCommitTiming::MakeImmediate();
	}
	Channel.OneShot = Montage;

	if (bItemLeavesHand)
	{
		// The slot of the arm is empty from the point of view of the inventory, but the arm must not snap to locomotion under its own
		// release animation: the pose stays until the animation starts blending out, then the arm is re-read from the inventory.
		Channel.bReleasing = true;

		if (UArmsAnimInstance* Instance = GetItemsInstance())
		{
			FOnMontageBlendingOutStarted OnBlendOut = FOnMontageBlendingOutStarted::CreateWeakLambda(this,
				[this, Side, Generation = Channel.Generation](UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
				{
					FArmChannel& Arm = Channels[IndexOf(Side)];
					if (Arm.Generation != Generation)
					{
						return;
					}
					Arm.bReleasing = false;
					ResyncArm(Side);
				});
			Instance->Montage_SetBlendingOutDelegate(OnBlendOut, Montage);
		}
	}

	// ---- When the item really moves ----
	FActionCommitTiming Timing;
	switch (Clip.CommitMode)
	{
	case EActionCommitMode::AfterDelay:
		Timing.Mode = EActionCommitMode::AfterDelay;
		Timing.DelaySeconds = Clip.CommitDelaySeconds;
		break;

	case EActionCommitMode::OnNotify:
	{
		Timing.Mode = EActionCommitMode::OnNotify;
		Timing.TimeoutSeconds = Clip.CommitTimeoutSeconds > 0.0f
			? Clip.CommitTimeoutSeconds
			: Clip.Sequence->GetPlayLength() / FMath::Max(Clip.PlayRate, 0.05f) + NotifyTimeoutMargin;

		(bFromPropInventory ? PropPendingSequence : ToolPendingSequence) = Clip.Sequence.Get();

		if (!bWarnedMissingNotify && !ClipHasCommitNotify(Clip.Sequence))
		{
			bWarnedMissingNotify = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimation: '%s' is set to commit On Notify but has no 'Arms Item Commit' notify. The item will move only when the timeout runs out (%.2f s). Add the notify on the frame where the item must move (Docs/AnimationSystem/Engine_Setup_Guide.md). This warning is shown once."),
				*GetNameSafe(Clip.Sequence), Timing.TimeoutSeconds);
		}
		break;
	}

	default:
		break;
	}

	return Timing;
}

UAnimMontage* UArmsAnimationComponent::PlayClip(EArmSide Side, const FArmsClip& Clip, bool bLoop)
{
	UArmsAnimInstance* Instance = GetItemsInstance();
	if (!Instance || !Clip.Sequence || !AnimationSet)
	{
		return nullptr;
	}

	const FName SlotName = AnimationSet->GetSlotName(Side);

	// The engine does not check the slot: a montage in a slot that is not on the skeleton silently shows nothing.
	const USkeletalMesh* MeshAsset = ArmsMesh.IsValid() ? ArmsMesh->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* Skeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	if (Skeleton && !Skeleton->ContainsSlotName(SlotName))
	{
		if (!bWarnedMissingSlot)
		{
			bWarnedMissingSlot = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimation: the slot '%s' is not on the skeleton '%s', so item animations cannot show. Use 'Ensure Skeleton Slots' on the Arms Animation Set, then save the skeleton. This warning is shown once."),
				*SlotName.ToString(), *Skeleton->GetName());
		}
		return nullptr;
	}

	return Instance->PlaySlotAnimationAsDynamicMontage(Clip.Sequence, SlotName, Clip.BlendIn, Clip.BlendOut, Clip.PlayRate, bLoop ? EndlessLoopCount : 1);
}

// ----------------------------------------------------------------------
// Commit notify
// ----------------------------------------------------------------------

void UArmsAnimationComponent::HandleCommitNotify(const UAnimSequenceBase* Animation)
{
	if (!Animation)
	{
		return;
	}

	if (PropPendingSequence.Get() == Animation)
	{
		PropPendingSequence.Reset();
		if (UPropInventoryComponent* Props = PropInventory.Get())
		{
			Props->CommitPendingAction();
		}
		return;
	}

	if (ToolPendingSequence.Get() == Animation)
	{
		ToolPendingSequence.Reset();
		if (UToolInventoryComponent* Tools = ToolInventory.Get())
		{
			Tools->CommitPendingAction();
		}
	}
}

// ----------------------------------------------------------------------
// Poses: what the arms hold
// ----------------------------------------------------------------------

void UArmsAnimationComponent::OnPropSlotsChanged()
{
	ResyncArm(EArmSide::Right);
	ResyncArm(EArmSide::Left);
}

void UArmsAnimationComponent::OnToolSlotsChanged()
{
	ResyncArm(EArmSide::Right);
}

void UArmsAnimationComponent::OnToolEquippedChanged(ASunsetCoveTool* /*NewTool*/, ASunsetCoveTool* /*OldTool*/, int32 /*SlotIndex*/)
{
	ResyncArm(EArmSide::Right);
}

void UArmsAnimationComponent::OnPropStateChanged(EPropInventoryState NewState, EPropInventoryState /*OldState*/)
{
	UPropInventoryComponent* Props = PropInventory.Get();
	if (!Props)
	{
		return;
	}

	if (NewState == EPropInventoryState::Placing || NewState == EPropInventoryState::ChargingThrow)
	{
		if (!Props->IsHoldingAnyProp())
		{
			return;
		}

		// The prop that Place and Throw act on is the active one: its hand shows the aim or the charge.
		const EArmSide Side = SideOf(Props->GetActiveHand());
		FArmChannel& Channel = Channels[IndexOf(Side)];
		if (Channel.bReleasing || Channel.ItemId.IsNone())
		{
			return;
		}

		Channel.Desired = (NewState == EPropInventoryState::Placing) ? EArmHeldPose::PlaceAim : EArmHeldPose::ThrowCharge;
		bAimActive = true;
		ApplyArm(Side);
		return;
	}

	// Idle again. A Place or a Throw follows in the same call and takes over the arm, or a new aim starts at once (Placing then
	// ChargingThrow): so the return to Hold waits for the next tick and looks at what is true then.
	if (bAimActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UArmsAnimationComponent::EndAimPoses);
		}
	}
}

void UArmsAnimationComponent::EndAimPoses()
{
	const UPropInventoryComponent* Props = PropInventory.Get();
	if (Props && Props->GetState() != EPropInventoryState::Idle)
	{
		return;
	}

	bAimActive = false;
	for (const EArmSide Side : { EArmSide::Right, EArmSide::Left })
	{
		FArmChannel& Channel = Channels[IndexOf(Side)];
		if (!Channel.bReleasing && (Channel.Desired == EArmHeldPose::PlaceAim || Channel.Desired == EArmHeldPose::ThrowCharge))
		{
			Channel.Desired = EArmHeldPose::Hold;
			ApplyArm(Side);
		}
	}
}

void UArmsAnimationComponent::ResyncArm(EArmSide Side)
{
	FArmChannel& Channel = Channels[IndexOf(Side)];

	if (Channel.bReleasing)
	{
		// Safety: if the release animation is gone without telling (the Animation Instance was rebuilt), the arm is not stuck.
		const UArmsAnimInstance* Instance = GetItemsInstance();
		const UAnimMontage* Montage = Channel.OneShot.Get();
		if (Instance && Montage && Instance->Montage_IsActive(Montage))
		{
			return;
		}
		Channel.bReleasing = false;
	}

	// The inventories are the truth. A prop wins over a tool: the right hand cannot hold both.
	AActor* Truth = nullptr;
	if (const UPropInventoryComponent* Props = PropInventory.Get())
	{
		Truth = Props->GetPropInHand(Side == EArmSide::Right ? EPropHand::Right : EPropHand::Left);
	}
	if (!Truth && Side == EArmSide::Right)
	{
		if (const UToolInventoryComponent* Tools = ToolInventory.Get())
		{
			if (!Tools->AreHandsBusy())
			{
				Truth = Tools->GetEquippedTool();
			}
		}
	}

	if (Truth != Channel.Item.Get())
	{
		Channel.Item = Truth;
		Channel.ItemId = GetItemIdOf(Truth);
		Channel.Desired = (Truth && !Channel.ItemId.IsNone()) ? EArmHeldPose::Hold : EArmHeldPose::None;
	}
	else if (!Truth)
	{
		Channel.ItemId = NAME_None;
		Channel.Desired = EArmHeldPose::None;
	}

	ApplyArm(Side);
}

void UArmsAnimationComponent::ApplyArm(EArmSide Side)
{
	UArmsAnimInstance* Instance = GetItemsInstance();
	if (!Instance)
	{
		return;
	}

	const FArmChannel& Channel = Channels[IndexOf(Side)];

	UAnimSequence* Hold = nullptr;
	UAnimSequence* PlaceAim = nullptr;
	UAnimSequence* ThrowCharge = nullptr;

	if (AnimationSet && Channel.Desired != EArmHeldPose::None && !Channel.ItemId.IsNone())
	{
		FArmsClip Clip;
		if (AnimationSet->FindClip(Channel.ItemId, EArmsAnimAction::Hold, Side, Clip))
		{
			Hold = Clip.Sequence;
		}
		else
		{
			LogMissingClip(Channel.ItemId, EArmsAnimAction::Hold, Side);
		}

		if (AnimationSet->FindClip(Channel.ItemId, EArmsAnimAction::PlaceAim, Side, Clip))
		{
			PlaceAim = Clip.Sequence;
		}
		if (AnimationSet->FindClip(Channel.ItemId, EArmsAnimAction::ThrowCharge, Side, Clip))
		{
			ThrowCharge = Clip.Sequence;
		}
	}

	// SetArm downgrades a pose whose sequence is missing, so the graph never plays nothing.
	Instance->SetArm(Side, Channel.Desired, Hold, PlaceAim, ThrowCharge);
}

// ----------------------------------------------------------------------
// Tool use and the radial menu
// ----------------------------------------------------------------------

void UArmsAnimationComponent::OnToolAction(EToolAction Action, int32 /*SlotIndex*/, ASunsetCoveTool* Tool)
{
	// Pickup, Equip, Holster and Drop start in the ActionBeginHook (they have a commit moment). Only Use is here.
	if (!AnimationSet || !Tool)
	{
		return;
	}

	if (Action == EToolAction::UseStarted)
	{
		const FName ItemId = GetItemIdOf(Tool);
		FArmsClip Clip;
		if (ItemId.IsNone() || !AnimationSet->FindClip(ItemId, EArmsAnimAction::Use, EArmSide::Right, Clip))
		{
			if (!ItemId.IsNone())
			{
				LogMissingClip(ItemId, EArmsAnimAction::Use, EArmSide::Right);
			}
			return;
		}

		UAnimMontage* Montage = PlayClip(EArmSide::Right, Clip, Clip.bLoopWhileActive);
		if (Clip.bLoopWhileActive)
		{
			UseMontage = Montage;
			UseBlendOut = Clip.BlendOut;
		}
	}
	else if (Action == EToolAction::UseEnded)
	{
		// A repeating use stops when the use action is released. A single use finishes by itself.
		UAnimMontage* Montage = UseMontage.Get();
		UseMontage.Reset();
		if (Montage)
		{
			if (UArmsAnimInstance* Instance = GetItemsInstance())
			{
				Instance->Montage_Stop(UseBlendOut, Montage);
			}
		}
	}
}

void UArmsAnimationComponent::OnToolStateChanged(EToolInventoryState NewState, EToolInventoryState OldState)
{
	// Optional: the arm relaxes when the radial menu opens and comes back when it closes (generic id "ToolMenu").
	if (!AnimationSet)
	{
		return;
	}

	EArmsAnimAction Action = EArmsAnimAction::Open;
	if (NewState == EToolInventoryState::SelectingTool)
	{
		Action = EArmsAnimAction::Open;
	}
	else if (OldState == EToolInventoryState::SelectingTool)
	{
		Action = EArmsAnimAction::Close;
	}
	else
	{
		return;
	}

	FArmsClip Clip;
	if (AnimationSet->FindClip(ArmsAnim::ToolMenuItemId(), Action, EArmSide::Right, Clip))
	{
		PlayClip(EArmSide::Right, Clip, /*bLoop=*/false);
	}
}

// ----------------------------------------------------------------------
// Setup check and logging
// ----------------------------------------------------------------------

void UArmsAnimationComponent::ValidateSetup()
{
	if (!ArmsMesh.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimation: no arms mesh was given by the character, so there are no item animations."));
		return;
	}

	if (!AnimationSet)
	{
		UE_LOG(LogSunsetCove, Log, TEXT("ArmsAnimation: no Animation Set on '%s' (Arms Animation component), so items play no arm animation. Assign DA_ArmsAnimations."), *GetNameSafe(GetOwner()));
		return;
	}

	if (!GetItemsInstance())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimation: the arms mesh of '%s' has no Animation Instance of class Arms Anim Instance (no post-process ABP, or its parent class is wrong). Set Post Process Anim Class on the Arms Animation component to ABP_FPArms_Items. Items play no arm animation until then."),
			*GetNameSafe(GetOwner()));
		return;
	}

	const USkeletalMesh* MeshAsset = ArmsMesh->GetSkeletalMeshAsset();
	const USkeleton* Skeleton = MeshAsset ? MeshAsset->GetSkeleton() : nullptr;
	if (Skeleton && (!Skeleton->ContainsSlotName(AnimationSet->RightSlotName) || !Skeleton->ContainsSlotName(AnimationSet->LeftSlotName)))
	{
		bWarnedMissingSlot = true;
		UE_LOG(LogSunsetCove, Warning, TEXT("ArmsAnimation: the slots '%s' and '%s' are not both on the skeleton '%s'. Use 'Ensure Skeleton Slots' on the Arms Animation Set, then save the skeleton."),
			*AnimationSet->RightSlotName.ToString(), *AnimationSet->LeftSlotName.ToString(), *Skeleton->GetName());
	}
}

void UArmsAnimationComponent::LogMissingClip(FName ItemId, EArmsAnimAction Action, EArmSide Side)
{
	if (!bLogMissingClips)
	{
		return;
	}

	const uint32 Key = HashCombine(GetTypeHash(ItemId), HashCombine(GetTypeHash(static_cast<uint8>(Action)), GetTypeHash(static_cast<uint8>(Side))));
	bool bAlreadyLogged = false;
	LoggedMissingClips.Add(Key, &bAlreadyLogged);
	if (!bAlreadyLogged)
	{
		UE_LOG(LogSunsetCove, Log, TEXT("ArmsAnimation: no clip for %s / %s / %s (AS_Arms_%s_%s_%s). Nothing plays."),
			*ItemId.ToString(), *ActionText(Action), SideText(Side), *ItemId.ToString(), *ActionText(Action), SideText(Side));
	}
}

// ----------------------------------------------------------------------
// Test commands
// ----------------------------------------------------------------------

bool UArmsAnimationComponent::PlayItemAnimation(FName ItemId, EArmsAnimAction Action, EArmSide Side)
{
	if (!AnimationSet)
	{
		return false;
	}

	if (ArmsAnim::IsPoseAction(Action))
	{
		// A pose held by hand, until ClearArms or the next inventory event.
		FArmChannel& Channel = Channels[IndexOf(Side)];
		Channel.Item.Reset();
		Channel.ItemId = ItemId;
		Channel.bReleasing = false;
		Channel.Desired = (Action == EArmsAnimAction::Hold) ? EArmHeldPose::Hold
			: (Action == EArmsAnimAction::PlaceAim) ? EArmHeldPose::PlaceAim : EArmHeldPose::ThrowCharge;
		ApplyArm(Side);

		FArmsClip Clip;
		return AnimationSet->FindClip(ItemId, Action, Side, Clip);
	}

	FArmsClip Clip;
	if (!AnimationSet->FindClip(ItemId, Action, Side, Clip))
	{
		return false;
	}
	return PlayClip(Side, Clip, /*bLoop=*/false) != nullptr;
}

void UArmsAnimationComponent::ClearArms()
{
	if (UArmsAnimInstance* Instance = GetItemsInstance())
	{
		Instance->Montage_Stop(0.1f);
	}

	for (const EArmSide Side : { EArmSide::Right, EArmSide::Left })
	{
		FArmChannel& Channel = Channels[IndexOf(Side)];
		Channel.Item.Reset();
		Channel.ItemId = NAME_None;
		Channel.Desired = EArmHeldPose::None;
		Channel.bReleasing = false;
		++Channel.Generation;
		ResyncArm(Side);
	}
}

void UArmsAnimationComponent::DumpStatus() const
{
	const UArmsAnimInstance* Instance = GetItemsInstance();
	UE_LOG(LogSunsetCove, Log, TEXT("ArmsAnimation: set '%s', instance '%s', pending commit: prop %s, tool %s."),
		*GetNameSafe(AnimationSet), *GetNameSafe(Instance),
		PropPendingSequence.IsValid() ? *GetNameSafe(PropPendingSequence.Get()) : TEXT("none"),
		ToolPendingSequence.IsValid() ? *GetNameSafe(ToolPendingSequence.Get()) : TEXT("none"));

	const UEnum* PoseEnum = StaticEnum<EArmHeldPose>();
	for (const EArmSide Side : { EArmSide::Right, EArmSide::Left })
	{
		const FArmChannel& Channel = Channels[IndexOf(Side)];
		UE_LOG(LogSunsetCove, Log, TEXT("  %s arm: item '%s' (%s), wanted pose %s, shown pose %s, releasing %s"),
			SideText(Side), *Channel.ItemId.ToString(), *GetNameSafe(Channel.Item.Get()),
			*PoseEnum->GetNameStringByValue(static_cast<int64>(Channel.Desired)),
			Instance ? *PoseEnum->GetNameStringByValue(static_cast<int64>(Instance->GetArmPose(Side))) : TEXT("(no instance)"),
			Channel.bReleasing ? TEXT("yes") : TEXT("no"));
	}
}

#if !UE_BUILD_SHIPPING

namespace
{
	UArmsAnimationComponent* FindLocalArmsAnimation(UWorld* World)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		return Pawn ? Pawn->FindComponentByClass<UArmsAnimationComponent>() : nullptr;
	}

	void ArmsPlayCommand(const TArray<FString>& Args, UWorld* World)
	{
		UArmsAnimationComponent* Component = FindLocalArmsAnimation(World);
		if (!Component)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("SunsetCove.Arms.Play: the player pawn has no Arms Animation component (start the game first)."));
			return;
		}

		if (Args.Num() != 3)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("Usage: SunsetCove.Arms.Play <Item> <Action> <R|L>   for example: SunsetCove.Arms.Play Sunbed Grab R"));
			return;
		}

		const int64 ActionValue = StaticEnum<EArmsAnimAction>()->GetValueByNameString(Args[1]);
		if (ActionValue == INDEX_NONE)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("SunsetCove.Arms.Play: unknown action '%s' (Hold, PlaceAim, ThrowCharge, Grab, Place, Throw, Drop, Equip, Holster, Use, Pickup, Open, Close)."), *Args[1]);
			return;
		}

		const bool bRight = Args[2].Equals(TEXT("R"), ESearchCase::IgnoreCase);
		const bool bLeft = Args[2].Equals(TEXT("L"), ESearchCase::IgnoreCase);
		if (!bRight && !bLeft)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("SunsetCove.Arms.Play: the hand must be R or L, not '%s'."), *Args[2]);
			return;
		}

		const bool bPlayed = Component->PlayItemAnimation(FName(*Args[0]), static_cast<EArmsAnimAction>(ActionValue), bRight ? EArmSide::Right : EArmSide::Left);
		UE_LOG(LogSunsetCove, Log, TEXT("SunsetCove.Arms.Play %s %s %s: %s"), *Args[0], *Args[1], *Args[2], bPlayed ? TEXT("playing") : TEXT("no clip (check the Animation Set, the post-process ABP and the slots)"));
	}

	FAutoConsoleCommandWithWorldAndArgs ArmsPlayConsoleCommand(
		TEXT("SunsetCove.Arms.Play"),
		TEXT("Plays one arm animation of an item without gameplay. Usage: SunsetCove.Arms.Play <Item> <Action> <R|L>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ArmsPlayCommand));

	FAutoConsoleCommandWithWorld ArmsClearConsoleCommand(
		TEXT("SunsetCove.Arms.Clear"),
		TEXT("Stops the test arm animations and shows what the inventories say."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UArmsAnimationComponent* Component = FindLocalArmsAnimation(World))
			{
				Component->ClearArms();
			}
		}));

	FAutoConsoleCommandWithWorld ArmsDumpConsoleCommand(
		TEXT("SunsetCove.Arms.Dump"),
		TEXT("Prints the state of both arms (item, wanted pose, shown pose) to the Output Log."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UArmsAnimationComponent* Component = FindLocalArmsAnimation(World))
			{
				Component->DumpStatus();
			}
		}));
}

#endif // !UE_BUILD_SHIPPING
