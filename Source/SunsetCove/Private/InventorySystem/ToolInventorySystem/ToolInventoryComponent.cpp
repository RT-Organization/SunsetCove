// ToolInventoryComponent.cpp
// SunsetCove - Inventory System / Tool Inventory. See Public/InventorySystem/ToolInventorySystem/ToolInventoryComponent.h
// for the role of this class, the actions and the rules.

#include "InventorySystem/ToolInventorySystem/ToolInventoryComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Core/Utils/SunsetCoveHeldItemUtils.h"
#include "Core/Utils/SunsetCoveViewUtils.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Gameplay/Tools/SunsetCoveTool.h"
#include "InputAction.h"
#include "InteractionSystem/InteractionComponent.h"

namespace
{
	/** Id of the pickup handler in the Interaction System. */
	const FName ToolPickupHandlerId(TEXT("ToolInventory.Pickup"));

	/** Hard limit of MaxTools (the ClampMax of the property). The HUD wheel stays readable up to here. */
	constexpr int32 MaxSlotCount = 12;

	bool ToolCollisionHasPhysics(ECollisionEnabled::Type Collision)
	{
		return Collision == ECollisionEnabled::QueryAndPhysics || Collision == ECollisionEnabled::PhysicsOnly;
	}
}

UToolInventoryComponent::UToolInventoryComponent()
{
	// Everything is driven by input events: no tick.
	PrimaryComponentTick.bCanEverTick = false;

	WallTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
}

// ----------------------------------------------------------------------
// UActorComponent
// ----------------------------------------------------------------------

void UToolInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// The size comes from MaxTools as set in the Blueprint, so it is fixed here and not in the constructor.
	Slots.Reset();
	Slots.SetNum(FMath::Clamp(MaxTools, 1, MaxSlotCount));

	ValidateHandSockets();

	// Next frame: the arms animation and the other components of the owner have run their BeginPlay by then.
	if (StartingTools.Num() > 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				GiveStartingTools();
			}));
		}
	}
}

void UToolInventoryComponent::GiveStartingTools()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !Owner->HasAuthority())
	{
		return;
	}

	for (const TSubclassOf<ASunsetCoveTool>& ToolClass : StartingTools)
	{
		if (!ToolClass)
		{
			continue;
		}
		if (FindFreeSlotIndex() == INDEX_NONE)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("ToolInventory: more Starting Tools than slots (Max Tools = %d); the rest are ignored."), Slots.Num());
			break;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASunsetCoveTool* Tool = World->SpawnActor<ASunsetCoveTool>(ToolClass, Owner->GetActorTransform(), SpawnParams);
		if (Tool && !TryPickupTool(Tool))
		{
			// Refused (for example a duplicate ToolId): the refusal event already said why; do not leave it lying at the player's feet.
			Tool->Destroy();
		}
	}
}

void UToolInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCurrentAction();
	RestoreAllPawnResponses();

	if (UInteractionComponent* InteractionComponent = Interaction.Get())
	{
		InteractionComponent->UnregisterInteractionHandler(ToolPickupHandlerId);
	}
	Interaction.Reset();

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Wiring
// ----------------------------------------------------------------------

void UToolInventoryComponent::SetHandMeshes(USceneComponent* FirstPersonArms, USceneComponent* BodyMesh)
{
	FirstPersonHandMesh = FirstPersonArms;
	BodyHandMesh = BodyMesh;
}

void UToolInventoryComponent::RegisterWithInteraction(UInteractionComponent* InInteraction)
{
	if (!InInteraction)
	{
		return;
	}

	UInteractionComponent* Previous = Interaction.Get();
	if (Previous && Previous != InInteraction)
	{
		Previous->UnregisterInteractionHandler(ToolPickupHandlerId);
	}

	Interaction = InInteraction;
	InInteraction->RegisterInteractionHandler(
		ToolPickupHandlerId,
		FInteractionCanHandleDelegate::CreateUObject(this, &UToolInventoryComponent::CanHandleInteraction),
		FInteractionHandleDelegate::CreateUObject(this, &UToolInventoryComponent::HandleInteraction));
}

void UToolInventoryComponent::SetupInput(UEnhancedInputComponent* EnhancedInput)
{
	if (!EnhancedInput)
	{
		return;
	}

	const auto WarnMissing = [this](const TCHAR* ActionName)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("ToolInventory: '%s' has no %s. Set it on the Tool Inventory component of the character Blueprint (ToolInventory|Input). The matching action does nothing until it is set."),
			*GetNameSafe(GetOwner()), ActionName);
	};

	if (OpenToolMenuAction)
	{
		EnhancedInput->BindAction(OpenToolMenuAction, ETriggerEvent::Started, this, &UToolInventoryComponent::HandleOpenMenuStarted);
		EnhancedInput->BindAction(OpenToolMenuAction, ETriggerEvent::Completed, this, &UToolInventoryComponent::HandleOpenMenuCompleted);
		EnhancedInput->BindAction(OpenToolMenuAction, ETriggerEvent::Canceled, this, &UToolInventoryComponent::HandleOpenMenuCanceled);
	}
	else
	{
		WarnMissing(TEXT("Open Tool Menu Action"));
	}

	if (UseToolAction)
	{
		EnhancedInput->BindAction(UseToolAction, ETriggerEvent::Started, this, &UToolInventoryComponent::HandleUseStarted);
		EnhancedInput->BindAction(UseToolAction, ETriggerEvent::Completed, this, &UToolInventoryComponent::HandleUseEnded);
		EnhancedInput->BindAction(UseToolAction, ETriggerEvent::Canceled, this, &UToolInventoryComponent::HandleUseEnded);
	}
	else
	{
		WarnMissing(TEXT("Use Tool Action"));
	}

	if (DropAction)
	{
		EnhancedInput->BindAction(DropAction, ETriggerEvent::Started, this, &UToolInventoryComponent::HandleDropStarted);
	}
	else
	{
		WarnMissing(TEXT("Drop Action"));
	}
}

void UToolInventoryComponent::SetHandsBusy(bool bNewBusy)
{
	if (bHandsBusy == bNewBusy)
	{
		return;
	}

	bHandsBusy = bNewBusy;

	if (bHandsBusy)
	{
		// The hands are taken by props: no menu, no use, and the tool is put away and stays away. It is hidden at once (no holster
		// animation): the arm is busy with the prop.
		StopUsingTool();
		CloseToolMenu(/*bConfirm=*/false);
		SetEquippedSlot(INDEX_NONE, /*bAllowTimedCommit=*/false);
	}
}

void UToolInventoryComponent::AddRadialInput(const FVector2D& LookInput)
{
	if (State != EToolInventoryState::SelectingTool)
	{
		return;
	}

	FVector2D Delta = LookInput;
	if (bInvertRadialY)
	{
		Delta.Y = -Delta.Y;
	}

	// The vector is a cursor on the wheel: it moves with the look input and stops at the edge, so the sector under it is
	// decided by the DIRECTION the player moved in, not by how far.
	RadialVector += Delta / static_cast<double>(FMath::Max(RadialRadius, 1.0f));
	const double Length = RadialVector.Size();
	if (Length > 1.0)
	{
		RadialVector /= Length;
	}

	UpdateHoverFromVector();
}

// ----------------------------------------------------------------------
// Input callbacks
// ----------------------------------------------------------------------

void UToolInventoryComponent::HandleOpenMenuStarted()
{
	OpenToolMenu();
}

void UToolInventoryComponent::HandleOpenMenuCompleted()
{
	CloseToolMenu(/*bConfirm=*/true);
}

void UToolInventoryComponent::HandleOpenMenuCanceled()
{
	CloseToolMenu(/*bConfirm=*/false);
}

void UToolInventoryComponent::HandleUseStarted()
{
	StartUsingTool();
}

void UToolInventoryComponent::HandleUseEnded()
{
	StopUsingTool();
}

void UToolInventoryComponent::HandleDropStarted()
{
	// The drop action is shared with the Prop Inventory. Tools are dropped only from the open menu, on the selected sector.
	if (State == EToolInventoryState::SelectingTool && HoveredSlotIndex != INDEX_NONE)
	{
		DropToolInSlot(HoveredSlotIndex);
	}
}

// ----------------------------------------------------------------------
// Interaction handler (pickup)
// ----------------------------------------------------------------------

bool UToolInventoryComponent::CanHandleInteraction(AActor* Target)
{
	// Any free-standing tool is interactable, even if it would be refused now: the player aims at it, presses Interact and
	// is told why (OnActionRefused), instead of the tool silently not reacting.
	const ASunsetCoveTool* Tool = Cast<ASunsetCoveTool>(Target);
	return IsValid(Tool) && !Tool->IsHeld() && !IsToolOwned(Tool);
}

bool UToolInventoryComponent::HandleInteraction(AActor* Target)
{
	ASunsetCoveTool* Tool = Cast<ASunsetCoveTool>(Target);
	return Tool && TryPickupTool(Tool);
}

// ----------------------------------------------------------------------
// Pickup
// ----------------------------------------------------------------------

bool UToolInventoryComponent::TryPickupTool(ASunsetCoveTool* Tool)
{
	// A new action applies the physical effect of the previous one first, so the slots and the world always agree.
	CommitPendingAction();
	PruneDestroyedTools();

	if (!IsValid(Tool) || IsToolOwned(Tool) || Tool->IsHeld())
	{
		return false;
	}

	if (bHandsBusy)
	{
		Refuse(EToolRefusal::HandsBusy, Tool);
		return false;
	}

	if (!Tool->bCanBePickedUp)
	{
		Refuse(EToolRefusal::ToolBlocked, Tool);
		return false;
	}

	if (!bAllowDuplicateTools && OwnsToolWithId(Tool->ToolId))
	{
		Refuse(EToolRefusal::AlreadyOwned, Tool);
		return false;
	}

	const int32 SlotIndex = FindFreeSlotIndex();
	if (SlotIndex == INDEX_NONE)
	{
		Refuse(EToolRefusal::InventoryFull, Tool);
		return false;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Tool->GetRootComponent());

	// A tool that was just dropped may still ignore the Pawn channel: give its original response back first.
	RestorePawnResponse(Primitive);

	FToolSlot& Slot = Slots[SlotIndex];
	Slot.Tool = Tool;
	Slot.SavedCollisionEnabled = Primitive ? Primitive->GetCollisionEnabled() : ECollisionEnabled::QueryAndPhysics;

	if (Primitive)
	{
		// Physics off BEFORE the attach (an attached simulating body fights its parent), then no collision while carried.
		Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Primitive->SetSimulatePhysics(false);
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// FUTURE (requested by the project owner): instead of attaching at once, the tool should travel toward the hand and attach when it
	// arrives (see the notes at the top of ToolInventoryComponent.h). This lambda is the place.
	auto PhysicalPickup = [this, WeakTool = TWeakObjectPtr<ASunsetCoveTool>(Tool)]()
	{
		ASunsetCoveTool* PickedTool = WeakTool.Get();
		if (!IsValid(PickedTool))
		{
			return;
		}
		AttachToolToHand(PickedTool);
		PickedTool->SetActorHiddenInGame(true);
		PickedTool->NotifyPickedUp(GetOwner());
	};

	const bool bAutoEquip = bAutoEquipFirstTool && EquippedSlotIndex == INDEX_NONE && State == EToolInventoryState::Idle;
	if (bAutoEquip)
	{
		// The Equip that follows right away is the animation of this moment, and it shows the tool: the tool goes into the hand now.
		PhysicalPickup();
	}
	else
	{
		BeginCommit(EToolAction::Pickup, SlotIndex, Tool, PhysicalPickup);
	}

	// The tool is in the inventory from this moment, even while it has not reached the hand yet.
	OnToolAction.Broadcast(EToolAction::Pickup, SlotIndex, Tool);
	OnSlotsChanged.Broadcast();

	if (bAutoEquip)
	{
		SetEquippedSlot(SlotIndex);
	}

	return true;
}

void UToolInventoryComponent::AttachToolToHand(ASunsetCoveTool* Tool)
{
	USceneComponent* ToolRoot = Tool->GetRootComponent();
	USceneComponent* HandMesh = ResolveHandMesh();
	if (!ToolRoot || !HandMesh)
	{
		return;
	}

	// Where the root of the tool must be relative to the hand socket so that the grip bone of the tool lands on the socket.
	const FTransform RootInSocket = Tool->GetRootInHandSocketTransform();
	const FName SocketName = ResolveToolSocket(HandMesh);

	if (!SocketName.IsNone())
	{
		Tool->AttachToComponent(HandMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
		// The offset is in cm of the world; the hand socket space may be scaled (x100 arms), so it is divided by that scale.
		const FVector SocketScale = HandMesh->GetSocketTransform(SocketName, RTS_World).GetScale3D();
		ToolRoot->SetRelativeLocationAndRotation(SunsetCoveHeldItem::WorldOffsetToRelative(RootInSocket.GetLocation(), SocketScale), RootInSocket.GetRotation());
		return;
	}

	// Already reported at start (ValidateHandSockets). Say it again only if that check could not run.
	if (!bWarnedMissingSocket)
	{
		bWarnedMissingSocket = true;
		UE_LOG(LogSunsetCove, Warning, TEXT("ToolInventory: neither socket or bone '%s' nor '%s' exists on '%s'. Tools are held at the fallback offset of the hand mesh instead. See Docs/InventorySystem/ToolInventorySystem/Engine_Setup_Guide.md."),
			*ToolSocketName.ToString(), *FallbackToolSocketName.ToString(), *GetNameSafe(HandMesh));
	}

	// The fallback acts as a stand-in hand socket: the root sits relative to it exactly as it would relative to the real one.
	Tool->AttachToComponent(HandMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	const FTransform FallbackSocket(FallbackRotation, FallbackLocation);
	const FTransform RootInHandMesh = RootInSocket * FallbackSocket;
	ToolRoot->SetRelativeLocationAndRotation(SunsetCoveHeldItem::WorldOffsetToRelative(RootInHandMesh.GetLocation(), HandMesh->GetComponentScale()), RootInHandMesh.GetRotation());
}

FName UToolInventoryComponent::ResolveToolSocket(const USceneComponent* HandMesh) const
{
	if (!HandMesh)
	{
		return NAME_None;
	}

	if (!ToolSocketName.IsNone() && HandMesh->DoesSocketExist(ToolSocketName))
	{
		return ToolSocketName;
	}

	if (!FallbackToolSocketName.IsNone() && HandMesh->DoesSocketExist(FallbackToolSocketName))
	{
		return FallbackToolSocketName;
	}

	return NAME_None;
}

void UToolInventoryComponent::ValidateHandSockets()
{
	const USceneComponent* HandMesh = ResolveHandMesh();
	if (!HandMesh)
	{
		return;
	}

	if (!ToolSocketName.IsNone() && HandMesh->DoesSocketExist(ToolSocketName))
	{
		return;
	}

	if (!FallbackToolSocketName.IsNone() && HandMesh->DoesSocketExist(FallbackToolSocketName))
	{
		UE_LOG(LogSunsetCove, Log, TEXT("ToolInventory: socket or bone '%s' not found on '%s', so tools are held on '%s'. Add '%s' to the arms in Blender if the tools need their own hand point."),
			*ToolSocketName.ToString(), *GetNameSafe(HandMesh), *FallbackToolSocketName.ToString(), *ToolSocketName.ToString());
		return;
	}

	bWarnedMissingSocket = true;
	UE_LOG(LogSunsetCove, Warning, TEXT("ToolInventory: neither '%s' nor '%s' was found on '%s' (a skeletal mesh asset must be set, and the socket / bone must be in the arms FBX). Tools are held at the fallback offset until it is fixed. The names are set in ToolInventory|Hand."),
		*ToolSocketName.ToString(), *FallbackToolSocketName.ToString(), *GetNameSafe(HandMesh));
}

USceneComponent* UToolInventoryComponent::ResolveHandMesh() const
{
	USceneComponent* Wanted = bUseFirstPersonArms ? FirstPersonHandMesh.Get() : BodyHandMesh.Get();
	if (Wanted)
	{
		return Wanted;
	}

	// The chosen mesh does not exist: use the other one, then the root of the owner, rather than failing the pickup.
	USceneComponent* Other = bUseFirstPersonArms ? BodyHandMesh.Get() : FirstPersonHandMesh.Get();
	if (Other)
	{
		return Other;
	}

	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

// ----------------------------------------------------------------------
// Equip and holster
// ----------------------------------------------------------------------

bool UToolInventoryComponent::EquipSlot(int32 SlotIndex)
{
	PruneDestroyedTools();

	if (bHandsBusy)
	{
		Refuse(EToolRefusal::HandsBusy, GetSlotTool(SlotIndex));
		return false;
	}

	if (!IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	SetEquippedSlot(SlotIndex);
	return true;
}

bool UToolInventoryComponent::HolsterTool()
{
	PruneDestroyedTools();

	if (EquippedSlotIndex == INDEX_NONE)
	{
		return false;
	}

	SetEquippedSlot(INDEX_NONE);
	return true;
}

void UToolInventoryComponent::SetEquippedSlot(int32 NewIndex, bool bAllowTimedCommit)
{
	// A new action applies the physical effect of the previous one first (a tool still waiting to be shown or hidden).
	CommitPendingAction();

	if (NewIndex != INDEX_NONE && !IsSlotOccupied(NewIndex))
	{
		NewIndex = INDEX_NONE;
	}

	if (NewIndex == EquippedSlotIndex)
	{
		return;
	}

	// Whatever was being used stops with the swap.
	StopUsingTool();

	ASunsetCoveTool* OldTool = GetEquippedTool();
	const int32 OldIndex = EquippedSlotIndex;

	if (OldTool)
	{
		EquippedSlotIndex = INDEX_NONE;

		auto PhysicalHolster = [WeakTool = TWeakObjectPtr<ASunsetCoveTool>(OldTool)]()
		{
			if (ASunsetCoveTool* HolsteredTool = WeakTool.Get(); IsValid(HolsteredTool))
			{
				HolsteredTool->SetActorHiddenInGame(true);
				HolsteredTool->NotifyHolstered();
			}
		};

		// A swap hides the old tool at once: only the Equip of the new one is animated and timed.
		if (bAllowTimedCommit && NewIndex == INDEX_NONE)
		{
			BeginCommit(EToolAction::Holster, OldIndex, OldTool, PhysicalHolster);
		}
		else
		{
			PhysicalHolster();
		}
		OnToolAction.Broadcast(EToolAction::Holster, OldIndex, OldTool);
	}
	EquippedSlotIndex = INDEX_NONE;

	ASunsetCoveTool* NewTool = nullptr;
	if (NewIndex != INDEX_NONE)
	{
		NewTool = Slots[NewIndex].Tool;
		EquippedSlotIndex = NewIndex;

		auto PhysicalEquip = [WeakTool = TWeakObjectPtr<ASunsetCoveTool>(NewTool)]()
		{
			if (ASunsetCoveTool* EquippedTool = WeakTool.Get(); IsValid(EquippedTool))
			{
				EquippedTool->SetActorHiddenInGame(false);
				EquippedTool->NotifyEquipped();
			}
		};

		if (bAllowTimedCommit)
		{
			BeginCommit(EToolAction::Equip, NewIndex, NewTool, PhysicalEquip);
		}
		else
		{
			PhysicalEquip();
		}
		OnToolAction.Broadcast(EToolAction::Equip, NewIndex, NewTool);
	}

	OnEquippedToolChanged.Broadcast(NewTool, OldTool, EquippedSlotIndex);
}

// ----------------------------------------------------------------------
// Radial menu
// ----------------------------------------------------------------------

bool UToolInventoryComponent::OpenToolMenu()
{
	PruneDestroyedTools();

	if (State != EToolInventoryState::Idle)
	{
		return false;
	}

	if (bHandsBusy)
	{
		Refuse(EToolRefusal::HandsBusy, nullptr);
		return false;
	}

	if (GetNumTools() == 0)
	{
		Refuse(EToolRefusal::NoTools, nullptr);
		return false;
	}

	// A tool in use stops when the menu opens.
	StopUsingTool();
	ResetRadial();
	SetState(EToolInventoryState::SelectingTool);
	return true;
}

bool UToolInventoryComponent::CloseToolMenu(bool bConfirm)
{
	if (State != EToolInventoryState::SelectingTool)
	{
		return false;
	}

	PruneDestroyedTools();

	// Take what is needed before the menu state is cleared.
	const int32 Selected = HoveredSlotIndex;
	const bool bSuppressed = bConfirmSuppressed;

	ResetRadial();
	SetState(EToolInventoryState::Idle);

	if (!bConfirm)
	{
		return true;
	}

	if (Selected != INDEX_NONE && IsSlotOccupied(Selected))
	{
		// The tool that is already in the hand: choosing it again puts it away.
		SetEquippedSlot(Selected == EquippedSlotIndex ? INDEX_NONE : Selected);
	}
	else if (!bSuppressed)
	{
		// Nothing selected (a quick tap, or the center of the wheel): the hand is freed.
		SetEquippedSlot(INDEX_NONE);
	}

	return true;
}

void UToolInventoryComponent::SetHoveredSlot(int32 SlotIndex)
{
	if (State != EToolInventoryState::SelectingTool)
	{
		return;
	}

	SetHoveredSlotInternal(IsSlotOccupied(SlotIndex) ? SlotIndex : INDEX_NONE);
}

void UToolInventoryComponent::UpdateHoverFromVector()
{
	int32 NewHover = INDEX_NONE;

	const double Length = RadialVector.Size();
	const int32 NumSlots = Slots.Num();
	if (NumSlots > 0 && Length > KINDA_SMALL_NUMBER && Length >= static_cast<double>(RadialDeadZone))
	{
		// Angle clockwise from straight up (X is right, Y is up).
		const double Angle = FMath::RadiansToDegrees(FMath::Atan2(RadialVector.X, RadialVector.Y));
		const double SectorWidth = 360.0 / NumSlots;

		double Relative = FMath::Fmod(Angle - static_cast<double>(RadialStartAngleDegrees) + SectorWidth * 0.5, 360.0);
		if (Relative < 0.0)
		{
			Relative += 360.0;
		}

		const int32 Sector = FMath::Clamp(FMath::FloorToInt(Relative / SectorWidth), 0, NumSlots - 1);

		// An empty sector selects nothing.
		if (IsSlotOccupied(Sector))
		{
			NewHover = Sector;
		}
	}

	SetHoveredSlotInternal(NewHover);
}

void UToolInventoryComponent::SetHoveredSlotInternal(int32 NewIndex)
{
	if (HoveredSlotIndex == NewIndex)
	{
		return;
	}

	HoveredSlotIndex = NewIndex;
	OnRadialHoverChanged.Broadcast(HoveredSlotIndex);
}

void UToolInventoryComponent::ResetRadial()
{
	RadialVector = FVector2D::ZeroVector;
	bConfirmSuppressed = false;
	SetHoveredSlotInternal(INDEX_NONE);
}

float UToolInventoryComponent::GetRadialSlotAngle(int32 SlotIndex) const
{
	const int32 NumSlots = FMath::Max(Slots.Num(), 1);
	double Angle = FMath::Fmod(static_cast<double>(RadialStartAngleDegrees) + SlotIndex * (360.0 / NumSlots), 360.0);
	if (Angle < 0.0)
	{
		Angle += 360.0;
	}
	return static_cast<float>(Angle);
}

// ----------------------------------------------------------------------
// Use
// ----------------------------------------------------------------------

bool UToolInventoryComponent::StartUsingTool()
{
	PruneDestroyedTools();

	// A tool that is still being shown (an Equip waiting for its animation) cannot be used yet.
	if (State != EToolInventoryState::Idle || bHandsBusy || bUsingTool || PendingCommit.bActive)
	{
		return false;
	}

	ASunsetCoveTool* Tool = GetEquippedTool();
	if (!Tool || !Tool->BeginUse())
	{
		return false;
	}

	bUsingTool = true;
	OnToolAction.Broadcast(EToolAction::UseStarted, EquippedSlotIndex, Tool);
	return true;
}

void UToolInventoryComponent::StopUsingTool()
{
	if (!bUsingTool)
	{
		return;
	}

	bUsingTool = false;

	if (ASunsetCoveTool* Tool = GetEquippedTool())
	{
		Tool->EndUse();
		OnToolAction.Broadcast(EToolAction::UseEnded, EquippedSlotIndex, Tool);
	}
}

// ----------------------------------------------------------------------
// Drop
// ----------------------------------------------------------------------

bool UToolInventoryComponent::DropToolInSlot(int32 SlotIndex)
{
	CommitPendingAction();
	PruneDestroyedTools();

	if (!IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	ASunsetCoveTool* Tool = Slots[SlotIndex].Tool;

	// A tool in the hand is put away first, so its use ends and the events come in order (Holster, then Drop). It is hidden at once:
	// only the Drop is animated and timed.
	if (SlotIndex == EquippedSlotIndex)
	{
		SetEquippedSlot(INDEX_NONE, /*bAllowTimedCommit=*/false);
	}

	const ECollisionEnabled::Type SavedCollision = Slots[SlotIndex].SavedCollisionEnabled;

	// Logical part: the slot is free from this moment. The tool stays attached (hidden) until the commit.
	Slots[SlotIndex].Reset();

	// Dropping from the open menu: the sector is empty now, and letting go of the menu action must not put the equipped tool away.
	if (State == EToolInventoryState::SelectingTool)
	{
		RadialVector = FVector2D::ZeroVector;
		SetHoveredSlotInternal(INDEX_NONE);
		bConfirmSuppressed = true;
	}

	// The speed is decided now (the heading of the player at the drop); where the tool leaves from is decided at the commit.
	AActor* Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(Owner, ViewLocation, ViewRotation);

	// Forward along the heading only: looking down must not push the tool into the floor.
	const FVector Forward = FRotator(0.0f, ViewRotation.Yaw, 0.0f).Vector();
	const FVector OwnerVelocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
	const FVector LinearVelocity = Forward * DropForwardSpeed + FVector::UpVector * DropUpSpeed + OwnerVelocity * InheritOwnerVelocityScale;

	BeginCommit(EToolAction::Drop, SlotIndex, Tool, [this, WeakTool = TWeakObjectPtr<ASunsetCoveTool>(Tool), SavedCollision, LinearVelocity]()
	{
		CommitDrop(WeakTool, SavedCollision, LinearVelocity);
	});

	OnToolAction.Broadcast(EToolAction::Drop, SlotIndex, Tool);
	OnSlotsChanged.Broadcast();
	return true;
}

void UToolInventoryComponent::CommitDrop(TWeakObjectPtr<ASunsetCoveTool> WeakTool, ECollisionEnabled::Type SavedCollision, const FVector& LinearVelocity)
{
	ASunsetCoveTool* Tool = WeakTool.Get();
	if (!IsValid(Tool))
	{
		return;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Tool->GetRootComponent());
	Tool->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	AActor* Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(Owner, ViewLocation, ViewRotation);

	// If a wall is between the camera and the hand, the tool is dropped in front of the wall, not inside it.
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ToolDropTrace), /*bTraceComplex=*/false, Owner);
		Params.AddIgnoredActor(Tool);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, ViewLocation, Tool->GetActorLocation(), UEngineTypes::ConvertToCollisionChannel(WallTraceChannel), Params))
		{
			Tool->SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * DropWallMargin, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	Tool->SetActorHiddenInGame(false);

	if (Primitive)
	{
		// A dropped tool must fall: it needs collision WITH physics, whatever it had before it was picked up.
		Primitive->SetCollisionEnabled(ToolCollisionHasPhysics(SavedCollision) ? SavedCollision : ECollisionEnabled::QueryAndPhysics);
		ApplyTemporaryPawnIgnore(Primitive);
		Primitive->SetSimulatePhysics(true);
		Primitive->WakeRigidBody();
		// Velocity is set directly (not an impulse), so the drop does not depend on the mass of the tool.
		Primitive->SetPhysicsLinearVelocity(LinearVelocity);
		Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	Tool->NotifyDropped();
}

// ----------------------------------------------------------------------
// Commit timing
// ----------------------------------------------------------------------

void UToolInventoryComponent::BeginCommit(EToolAction Action, int32 SlotIndex, ASunsetCoveTool* Tool, TFunction<void()> Commit)
{
	// Never two pending effects: the callers flushed the previous one, this is only a guard.
	CommitPendingAction();

	FActionCommitTiming Timing;
	if (ActionBeginHook.IsBound())
	{
		Timing = ActionBeginHook.Execute(Action, SlotIndex, Tool);
	}

	UWorld* World = GetWorld();
	float WaitSeconds = 0.0f;
	if (Timing.Mode == EActionCommitMode::AfterDelay)
	{
		WaitSeconds = Timing.DelaySeconds;
	}
	else if (Timing.Mode == EActionCommitMode::OnNotify)
	{
		WaitSeconds = Timing.TimeoutSeconds;
	}

	if (!World || WaitSeconds <= 0.0f)
	{
		Commit();
		return;
	}

	PendingCommit.Commit = MoveTemp(Commit);
	PendingCommit.bActive = true;
	World->GetTimerManager().SetTimer(PendingCommit.Timer, FTimerDelegate::CreateUObject(this, &UToolInventoryComponent::CommitPendingAction), WaitSeconds, /*bLoop=*/false);
}

void UToolInventoryComponent::CommitPendingAction()
{
	if (!PendingCommit.bActive)
	{
		return;
	}

	// Clear first: the effect may start another action (and the timer must not fire a second time).
	PendingCommit.bActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingCommit.Timer);
	}

	TFunction<void()> Commit = MoveTemp(PendingCommit.Commit);
	PendingCommit.Commit = nullptr;
	if (Commit)
	{
		Commit();
	}
}

// ----------------------------------------------------------------------
// Cancel
// ----------------------------------------------------------------------

void UToolInventoryComponent::CancelCurrentAction()
{
	// A pending physical effect cannot outlive the action that asked for it (cancel, end of play, unpossess).
	CommitPendingAction();

	StopUsingTool();
	CloseToolMenu(/*bConfirm=*/false);
}

// ----------------------------------------------------------------------
// Temporary Pawn-ignore after a drop
// ----------------------------------------------------------------------

void UToolInventoryComponent::ApplyTemporaryPawnIgnore(UPrimitiveComponent* Primitive)
{
	UWorld* World = GetWorld();
	if (!Primitive || !World || DropPawnIgnoreSeconds <= 0.0f)
	{
		return;
	}

	// Never stack two entries for the same primitive: the original response would be lost.
	RestorePawnResponse(Primitive);

	FPendingPawnIgnore& Entry = PendingPawnIgnores.AddDefaulted_GetRef();
	Entry.Primitive = Primitive;
	Entry.OriginalResponse = Primitive->GetCollisionResponseToChannel(ECC_Pawn);
	Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	const TWeakObjectPtr<UPrimitiveComponent> WeakPrimitive(Primitive);
	World->GetTimerManager().SetTimer(
		Entry.Timer,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakPrimitive]()
		{
			RestorePawnResponse(WeakPrimitive.Get());
		}),
		DropPawnIgnoreSeconds,
		/*bInLoop=*/false);
}

void UToolInventoryComponent::RestorePawnResponse(UPrimitiveComponent* Primitive)
{
	UWorld* World = GetWorld();

	for (int32 Index = PendingPawnIgnores.Num() - 1; Index >= 0; --Index)
	{
		FPendingPawnIgnore& Entry = PendingPawnIgnores[Index];
		UPrimitiveComponent* Pending = Entry.Primitive.Get();

		// Matches the asked primitive, or is a leftover of a primitive that no longer exists.
		if (Pending == nullptr || Pending == Primitive)
		{
			if (Pending)
			{
				Pending->SetCollisionResponseToChannel(ECC_Pawn, Entry.OriginalResponse);
			}
			if (World)
			{
				World->GetTimerManager().ClearTimer(Entry.Timer);
			}
			PendingPawnIgnores.RemoveAtSwap(Index);
		}
	}
}

void UToolInventoryComponent::RestoreAllPawnResponses()
{
	UWorld* World = GetWorld();

	for (FPendingPawnIgnore& Entry : PendingPawnIgnores)
	{
		if (UPrimitiveComponent* Pending = Entry.Primitive.Get())
		{
			Pending->SetCollisionResponseToChannel(ECC_Pawn, Entry.OriginalResponse);
		}
		if (World)
		{
			World->GetTimerManager().ClearTimer(Entry.Timer);
		}
	}
	PendingPawnIgnores.Reset();
}

// ----------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------

int32 UToolInventoryComponent::GetNumTools() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (IsSlotOccupied(Index))
		{
			++Count;
		}
	}
	return Count;
}

ASunsetCoveTool* UToolInventoryComponent::GetSlotTool(int32 SlotIndex) const
{
	return IsSlotOccupied(SlotIndex) ? Slots[SlotIndex].Tool.Get() : nullptr;
}

ASunsetCoveTool* UToolInventoryComponent::GetEquippedTool() const
{
	return GetSlotTool(EquippedSlotIndex);
}

// ----------------------------------------------------------------------
// Slots and state
// ----------------------------------------------------------------------

bool UToolInventoryComponent::IsSlotOccupied(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) && IsValid(Slots[SlotIndex].Tool);
}

int32 UToolInventoryComponent::FindFreeSlotIndex() const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!IsSlotOccupied(Index))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool UToolInventoryComponent::IsToolOwned(const ASunsetCoveTool* Tool) const
{
	for (const FToolSlot& Slot : Slots)
	{
		if (Tool && Slot.Tool.Get() == Tool)
		{
			return true;
		}
	}
	return false;
}

bool UToolInventoryComponent::OwnsToolWithId(FName ToolId) const
{
	// A tool without an id is never a duplicate of another one.
	if (ToolId.IsNone())
	{
		return false;
	}

	for (const FToolSlot& Slot : Slots)
	{
		if (IsValid(Slot.Tool) && Slot.Tool->ToolId == ToolId)
		{
			return true;
		}
	}
	return false;
}

void UToolInventoryComponent::PruneDestroyedTools()
{
	bool bChanged = false;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		FToolSlot& Slot = Slots[Index];
		if (Slot.Tool && !IsValid(Slot.Tool))
		{
			if (Index == EquippedSlotIndex)
			{
				EquippedSlotIndex = INDEX_NONE;
				bUsingTool = false;
			}
			if (Index == HoveredSlotIndex)
			{
				SetHoveredSlotInternal(INDEX_NONE);
			}
			Slot.Reset();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnSlotsChanged.Broadcast();
	}
}

void UToolInventoryComponent::Refuse(EToolRefusal Reason, ASunsetCoveTool* Tool)
{
	UE_LOG(LogSunsetCove, Verbose, TEXT("ToolInventory: refused (%d) for tool '%s'."), static_cast<int32>(Reason), *GetNameSafe(Tool));
	OnActionRefused.Broadcast(Reason, Tool);
}

void UToolInventoryComponent::SetState(EToolInventoryState NewState)
{
	if (State == NewState)
	{
		return;
	}

	const EToolInventoryState OldState = State;
	State = NewState;
	OnStateChanged.Broadcast(NewState, OldState);
}
