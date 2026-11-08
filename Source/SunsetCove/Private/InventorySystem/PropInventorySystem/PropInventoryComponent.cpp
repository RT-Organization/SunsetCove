// PropInventoryComponent.cpp
// SunsetCove - Inventory System / Prop Inventory. See Public/InventorySystem/PropInventorySystem/PropInventoryComponent.h
// for the role of this class, the states and the rules.

#include "InventorySystem/PropInventorySystem/PropInventoryComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Core/Utils/SunsetCoveViewUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Interfaces/PropInterface.h"
#include "InteractionSystem/InteractionComponent.h"
#include "InventorySystem/PropInventorySystem/PropPlacementHologram.h"

namespace
{
	/** Id of the grab handler in the Interaction System. */
	const FName PropGrabHandlerId(TEXT("PropInventory.Grab"));

	/** One slot per hand. */
	constexpr int32 SlotCount = 2;

	bool HasPhysics(ECollisionEnabled::Type Collision)
	{
		return Collision == ECollisionEnabled::QueryAndPhysics || Collision == ECollisionEnabled::PhysicsOnly;
	}
}

UPropInventoryComponent::UPropInventoryComponent()
{
	// Ticks only while a placement or a throw charge is in progress (see SetState).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	Slots.SetNum(SlotCount);

	SurfaceTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);

	ObstacleObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObstacleObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObstacleObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	ObstacleObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

// ----------------------------------------------------------------------
// UActorComponent
// ----------------------------------------------------------------------

void UPropInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ValidateHandSockets();
}

void UPropInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCurrentAction();
	RestoreAllPawnResponses();

	if (UInteractionComponent* InteractionComponent = Interaction.Get())
	{
		InteractionComponent->UnregisterInteractionHandler(PropGrabHandlerId);
	}
	Interaction.Reset();

	Super::EndPlay(EndPlayReason);
}

void UPropInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (State == EPropInventoryState::Idle)
	{
		return;
	}

	// The prop of the action may have been destroyed by something else (a level script, another system).
	PruneDestroyedProps();
	if (!IsSlotOccupied(LockedSlotIndex))
	{
		CancelCurrentAction();
		return;
	}

	switch (State)
	{
	case EPropInventoryState::Placing:
		UpdatePlacement();
		break;
	case EPropInventoryState::ChargingThrow:
		UpdateThrowCharge(DeltaTime);
		break;
	default:
		break;
	}
}

// ----------------------------------------------------------------------
// Wiring
// ----------------------------------------------------------------------

void UPropInventoryComponent::SetHandMeshes(USceneComponent* FirstPersonArms, USceneComponent* BodyMesh)
{
	FirstPersonHandMesh = FirstPersonArms;
	BodyHandMesh = BodyMesh;
}

void UPropInventoryComponent::RegisterWithInteraction(UInteractionComponent* InInteraction)
{
	if (!InInteraction)
	{
		return;
	}

	UInteractionComponent* Previous = Interaction.Get();
	if (Previous && Previous != InInteraction)
	{
		Previous->UnregisterInteractionHandler(PropGrabHandlerId);
	}

	Interaction = InInteraction;
	InInteraction->RegisterInteractionHandler(
		PropGrabHandlerId,
		FInteractionCanHandleDelegate::CreateUObject(this, &UPropInventoryComponent::CanHandleInteraction),
		FInteractionHandleDelegate::CreateUObject(this, &UPropInventoryComponent::HandleInteraction));
}

void UPropInventoryComponent::SetupInput(UEnhancedInputComponent* EnhancedInput)
{
	if (!EnhancedInput)
	{
		return;
	}

	const auto WarnMissing = [this](const TCHAR* ActionName)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: '%s' has no %s. Set it on the Prop Inventory component of the character Blueprint (PropInventory|Input). The matching action does nothing until it is set."),
			*GetNameSafe(GetOwner()), ActionName);
	};

	if (PlaceAction)
	{
		EnhancedInput->BindAction(PlaceAction, ETriggerEvent::Started, this, &UPropInventoryComponent::HandlePlaceStarted);
		EnhancedInput->BindAction(PlaceAction, ETriggerEvent::Completed, this, &UPropInventoryComponent::HandlePlaceCompleted);
		EnhancedInput->BindAction(PlaceAction, ETriggerEvent::Canceled, this, &UPropInventoryComponent::HandlePlaceCanceled);
	}
	else
	{
		WarnMissing(TEXT("Place Action"));
	}

	if (ThrowAction)
	{
		EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Started, this, &UPropInventoryComponent::HandleThrowStarted);
		EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Completed, this, &UPropInventoryComponent::HandleThrowCompleted);
		EnhancedInput->BindAction(ThrowAction, ETriggerEvent::Canceled, this, &UPropInventoryComponent::HandleThrowCanceled);
	}
	else
	{
		WarnMissing(TEXT("Throw Action"));
	}

	if (RotateAction)
	{
		EnhancedInput->BindAction(RotateAction, ETriggerEvent::Triggered, this, &UPropInventoryComponent::HandleRotate);
	}
	else
	{
		WarnMissing(TEXT("Rotate Action"));
	}

	if (DropAction)
	{
		EnhancedInput->BindAction(DropAction, ETriggerEvent::Started, this, &UPropInventoryComponent::HandleDropStarted);
	}
	else
	{
		WarnMissing(TEXT("Drop Action"));
	}
}

// ----------------------------------------------------------------------
// Input callbacks
// ----------------------------------------------------------------------

void UPropInventoryComponent::HandlePlaceStarted()
{
	BeginPlacement();
}

void UPropInventoryComponent::HandlePlaceCompleted()
{
	// Release: places the prop if the hologram is valid, otherwise just ends the placement.
	ConfirmPlacement();
}

void UPropInventoryComponent::HandlePlaceCanceled()
{
	if (State == EPropInventoryState::Placing)
	{
		CancelCurrentAction();
	}
}

void UPropInventoryComponent::HandleThrowStarted()
{
	BeginThrowCharge();
}

void UPropInventoryComponent::HandleThrowCompleted()
{
	ReleaseThrow();
}

void UPropInventoryComponent::HandleThrowCanceled()
{
	if (State == EPropInventoryState::ChargingThrow)
	{
		CancelCurrentAction();
	}
}

void UPropInventoryComponent::HandleRotate(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();

	// A mouse wheel notch is 1 (a fast scroll can report 2 or more in one frame). A tiny analog value still counts as one step.
	int32 Steps = FMath::RoundToInt(Axis);
	if (Steps == 0 && FMath::Abs(Axis) > 0.1f)
	{
		Steps = Axis > 0.0f ? 1 : -1;
	}

	RotatePlacementSteps(Steps);
}

void UPropInventoryComponent::HandleDropStarted()
{
	DropActiveProp();
}

// ----------------------------------------------------------------------
// Interaction handler (grab)
// ----------------------------------------------------------------------

bool UPropInventoryComponent::CanHandleInteraction(AActor* Target)
{
	return CanGrabProp(Target);
}

bool UPropInventoryComponent::HandleInteraction(AActor* Target)
{
	return TryGrabProp(Target);
}

// ----------------------------------------------------------------------
// Grab
// ----------------------------------------------------------------------

bool UPropInventoryComponent::CanGrabProp(const AActor* Prop) const
{
	if (!IsValid(Prop) || Prop == GetOwner())
	{
		return false;
	}

	if (State != EPropInventoryState::Idle || FindFreeSlotIndex() == INDEX_NONE)
	{
		return false;
	}

	if (!Prop->Implements<UPropInterface>() || IsPropHeld(Prop))
	{
		return false;
	}

	if (IPropInterface::Execute_IsPickupBlocked(Prop, GetOwner()))
	{
		return false;
	}

	return ValidatePropStructure(Prop);
}

bool UPropInventoryComponent::ValidatePropStructure(const AActor* Prop) const
{
	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Prop->GetRootComponent());

	const TCHAR* Problem = nullptr;
	if (!Root)
	{
		Problem = TEXT("its root component is not a primitive component (static mesh, box...). Make the mesh or a collision shape the root of the Blueprint.");
	}
	else if (Root->Mobility != EComponentMobility::Movable)
	{
		Problem = TEXT("its root component is not Movable. Set Mobility to Movable on the root component.");
	}

	if (!Problem)
	{
		return true;
	}

	// One warning per prop: the player may keep aiming at it.
	if (!WarnedProps.Contains(Prop))
	{
		WarnedProps.Add(Prop);
		UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: '%s' cannot be grabbed: %s"), *GetNameSafe(Prop), Problem);
	}
	return false;
}

bool UPropInventoryComponent::TryGrabProp(AActor* Prop)
{
	// A new action applies the physical effect of the previous one first, so the slots and the world always agree.
	CommitPendingAction();
	PruneDestroyedProps();

	if (!IsValid(Prop))
	{
		return false;
	}

	if (!Prop->Implements<UPropInterface>())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: '%s' is not a prop (it does not implement the Prop Interface), so it cannot be grabbed."), *GetNameSafe(Prop));
		return false;
	}

	if (!CanGrabProp(Prop))
	{
		UE_LOG(LogSunsetCove, Verbose, TEXT("PropInventory: cannot grab '%s' now (hands full, an action in progress, already held, or the prop refuses)."), *GetNameSafe(Prop));
		return false;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Prop->GetRootComponent());
	const int32 SlotIndex = FindFreeSlotIndex();
	if (!Primitive || SlotIndex == INDEX_NONE)
	{
		return false;
	}

	// A prop that was just thrown may still ignore the Pawn channel: give its original response back first.
	RestorePawnResponse(Primitive);

	FPropSlot& Slot = Slots[SlotIndex];
	Slot.Prop = Prop;
	Slot.GrabSequence = ++GrabCounter;
	Slot.SavedCollisionEnabled = Primitive->GetCollisionEnabled();

	// Physics off BEFORE the attach (an attached simulating body fights its parent), then no collision while carried.
	// The prop freezes where it is: it is being picked up, whatever the animation does next.
	Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Primitive->SetSimulatePhysics(false);
	Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// FUTURE (requested by the project owner): instead of attaching at once, the prop should travel toward the hand and attach
	// to the socket when it arrives (see the notes at the top of PropInventoryComponent.h). CommitGrab is the place.
	const EPropHand Hand = HandForSlotIndex(SlotIndex);
	BeginCommit(EPropAction::Grab, Hand, Prop, [this, WeakProp = TWeakObjectPtr<AActor>(Prop), SlotIndex]()
	{
		CommitGrab(WeakProp, SlotIndex);
	});

	// The hand is occupied from this moment, even while the prop has not reached the socket yet.
	OnSlotsChanged.Broadcast();
	return true;
}

void UPropInventoryComponent::CommitGrab(TWeakObjectPtr<AActor> WeakProp, int32 SlotIndex)
{
	AActor* Prop = WeakProp.Get();
	if (!IsValid(Prop) || !Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].Prop != Prop)
	{
		return;
	}

	AttachPropToHand(Prop, SlotIndex);

	IPropInterface::Execute_OnPropGrabbed(Prop, GetOwner());
	OnPropAction.Broadcast(EPropAction::Grab, HandForSlotIndex(SlotIndex), Prop);
}

void UPropInventoryComponent::AttachPropToHand(AActor* Prop, int32 SlotIndex)
{
	USceneComponent* PropRoot = Prop->GetRootComponent();
	USceneComponent* HandMesh = ResolveHandMesh();
	if (!PropRoot || !HandMesh)
	{
		return;
	}

	const bool bRightHand = (SlotIndex == 0);
	const FName SocketName = bRightHand ? RightHandSocketName : LeftHandSocketName;
	const FVector HoldLocation = IPropInterface::Execute_GetPropHoldLocationOffset(Prop, bRightHand);
	const FRotator HoldRotation = IPropInterface::Execute_GetPropHoldRotationOffset(Prop, bRightHand);

	if (!SocketName.IsNone() && HandMesh->DoesSocketExist(SocketName))
	{
		// Standard pipeline: snap to the socket, then the per-prop grip tweak relative to it.
		Prop->AttachToComponent(HandMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
		PropRoot->SetRelativeLocationAndRotation(HoldLocation, HoldRotation);
		return;
	}

	// Already reported at start (ValidateHandSockets). Say it again only if that check could not run.
	if (!bWarnedMissingSocket)
	{
		bWarnedMissingSocket = true;
		UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: socket or bone '%s' does not exist on '%s'. Props are held at the fallback offset of the hand mesh instead. The name must match the socket / bone of the arms (see Docs/InventorySystem/PropInventorySystem/Engine_Setup_Guide.md)."),
			*SocketName.ToString(), *GetNameSafe(HandMesh));
	}

	// The fallback acts as a stand-in hand socket: the root sits relative to it exactly as it would relative to the real one.
	Prop->AttachToComponent(HandMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	const FTransform FallbackSocket(HandFallbackRotation, bRightHand ? RightHandFallbackLocation : LeftHandFallbackLocation);
	const FTransform RootInSocket(HoldRotation, HoldLocation);
	const FTransform RootInHandMesh = RootInSocket * FallbackSocket;
	PropRoot->SetRelativeLocationAndRotation(RootInHandMesh.GetLocation(), RootInHandMesh.GetRotation());
}

void UPropInventoryComponent::ValidateHandSockets()
{
	const USceneComponent* HandMesh = ResolveHandMesh();
	if (!HandMesh)
	{
		return;
	}

	TArray<FString> Missing;
	if (RightHandSocketName.IsNone() || !HandMesh->DoesSocketExist(RightHandSocketName))
	{
		Missing.Add(RightHandSocketName.ToString());
	}
	if (LeftHandSocketName.IsNone() || !HandMesh->DoesSocketExist(LeftHandSocketName))
	{
		Missing.Add(LeftHandSocketName.ToString());
	}

	if (Missing.Num() == 0)
	{
		return;
	}

	bWarnedMissingSocket = true;
	UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: hand socket(s) or bone(s) [%s] not found on '%s' (a skeletal mesh asset must be set, and the socket / bone must be in the arms FBX). Props are held at the fallback offset until it is fixed. The names are set in PropInventory|Hands."),
		*FString::Join(Missing, TEXT(", ")), *GetNameSafe(HandMesh));
}

USceneComponent* UPropInventoryComponent::ResolveHandMesh() const
{
	USceneComponent* Wanted = (HandMeshSource == EPropHandMeshSource::FirstPersonMesh) ? FirstPersonHandMesh.Get() : BodyHandMesh.Get();
	if (Wanted)
	{
		return Wanted;
	}

	// The chosen mesh does not exist: use the other one, then the root of the owner, rather than failing the grab.
	USceneComponent* Other = (HandMeshSource == EPropHandMeshSource::FirstPersonMesh) ? BodyHandMesh.Get() : FirstPersonHandMesh.Get();
	if (Other)
	{
		return Other;
	}

	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

// ----------------------------------------------------------------------
// Placement
// ----------------------------------------------------------------------

bool UPropInventoryComponent::BeginPlacement()
{
	CommitPendingAction();
	PruneDestroyedProps();

	if (State != EPropInventoryState::Idle)
	{
		return false;
	}

	const int32 SlotIndex = FindActiveSlotIndex();
	UWorld* World = GetWorld();
	if (SlotIndex == INDEX_NONE || !World)
	{
		return false;
	}

	AActor* Prop = Slots[SlotIndex].Prop;

	LockedSlotIndex = SlotIndex;
	PlacementYawOffset = 0.0f;
	bPlacementValid = false;

	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(GetOwner(), ViewLocation, ViewRotation);
	PlacementStartYaw = ViewRotation.Yaw;

	// Footprint of the prop in its own (unscaled) space: used to rest it on the ground and to test for obstacles.
	PropLocalBounds = Prop->CalculateComponentsBoundingBoxInLocalSpace(/*bNonColliding=*/true);
	if (!PropLocalBounds.IsValid)
	{
		PropLocalBounds = FBox(FVector(-25.0f), FVector(25.0f));
	}
	PropScale = Prop->GetActorScale3D();

	if (!HologramMaterial && !bWarnedMissingHologramMaterial)
	{
		bWarnedMissingHologramMaterial = true;
		UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: no Hologram Material is set on '%s' (PropInventory|Placement|Hologram). The hologram is shown with the prop's own materials: visible on a valid spot, hidden on an invalid one."),
			*GetNameSafe(GetOwner()));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Hologram = World->SpawnActor<APropPlacementHologram>(APropPlacementHologram::StaticClass(), Prop->GetActorTransform(), SpawnParams);
	if (!Hologram)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("PropInventory: the placement hologram could not be spawned."));
		LockedSlotIndex = INDEX_NONE;
		return false;
	}
	Hologram->InitializeFromProp(Prop, HologramMaterial, HologramColorParameterName, ValidHologramColor, InvalidHologramColor);

	SetState(EPropInventoryState::Placing);

	// Position the hologram now, so the first frame is already right.
	UpdatePlacement();
	return true;
}

void UPropInventoryComponent::UpdatePlacement()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || !IsSlotOccupied(LockedSlotIndex) || !Hologram)
	{
		return;
	}

	AActor* Prop = Slots[LockedSlotIndex].Prop;

	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(Owner, ViewLocation, ViewRotation);

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PropPlacementTrace), /*bTraceComplex=*/false, Owner);
	AddHeldPropsToIgnore(TraceParams);
	TraceParams.AddIgnoredActor(Hologram);
	const ECollisionChannel SurfaceChannel = UEngineTypes::ConvertToCollisionChannel(SurfaceTraceChannel);

	// 1. Find the surface: along the view, or (if the view hits nothing in range) straight down from the end of the range.
	bool bHasSurface = false;
	FVector SurfaceLocation = FVector::ZeroVector;
	FVector SurfaceNormal = FVector::UpVector;
	const AActor* SurfaceActor = nullptr;

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaxPlacementDistance;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, SurfaceChannel, TraceParams))
	{
		bHasSurface = true;
		SurfaceLocation = Hit.ImpactPoint;
		SurfaceNormal = Hit.ImpactNormal;
		SurfaceActor = Hit.GetActor();
	}
	else
	{
		SurfaceLocation = TraceEnd;
		if (GroundSnapTraceLength > 0.0f && World->LineTraceSingleByChannel(Hit, TraceEnd, TraceEnd - FVector::UpVector * GroundSnapTraceLength, SurfaceChannel, TraceParams))
		{
			bHasSurface = true;
			SurfaceLocation = Hit.ImpactPoint;
			SurfaceNormal = Hit.ImpactNormal;
			SurfaceActor = Hit.GetActor();
		}
	}

	// 2. Is the surface flat enough?
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxSurfaceSlopeDegrees));
	const bool bSlopeOk = bHasSurface && SurfaceNormal.Z >= MinNormalZ;

	// 3. Where and how the prop would stand: upright (or tilted to the slope), resting on the surface.
	const float Yaw = (bFollowCameraYaw ? ViewRotation.Yaw : PlacementStartYaw) + PlacementYawOffset;
	const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
	const bool bAlign = bAlignToSurfaceNormal && bHasSurface;
	const FQuat AlignRotation = bAlign ? FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal) : FQuat::Identity;
	const FQuat Rotation = AlignRotation * YawRotation;
	const FVector LiftAxis = bAlign ? SurfaceNormal : FVector::UpVector;
	const float LiftDistance = -PropLocalBounds.Min.Z * PropScale.Z;
	const FVector Location = SurfaceLocation + LiftAxis * LiftDistance;
	const FTransform Candidate(Rotation, Location, PropScale);

	// 4. Is there room? A box the size of the prop, a little smaller and a little above the surface, tested against obstacles.
	bool bObstructed = false;
	FVector BoxCenter = Location;
	FVector BoxHalfExtent = FVector::ZeroVector;
	if (bHasSurface && bSlopeOk && ObstacleObjectTypes.Num() > 0)
	{
		const FVector HalfExtent = PropLocalBounds.GetExtent() * PropScale.GetAbs();
		const float HalfLift = ObstacleBoxBottomLift * 0.5f;
		BoxHalfExtent = FVector(
			FMath::Max(1.0f, HalfExtent.X - ObstacleBoxShrink),
			FMath::Max(1.0f, HalfExtent.Y - ObstacleBoxShrink),
			FMath::Max(1.0f, HalfExtent.Z - ObstacleBoxShrink - HalfLift));
		BoxCenter = Location + Rotation.RotateVector(PropLocalBounds.GetCenter() * PropScale) + Rotation.GetUpVector() * HalfLift;

		FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(PropPlacementOverlap), /*bTraceComplex=*/false, Owner);
		AddHeldPropsToIgnore(OverlapParams);
		OverlapParams.AddIgnoredActor(Hologram);
		if (bIgnoreSupportSurfaceActor && SurfaceActor)
		{
			OverlapParams.AddIgnoredActor(SurfaceActor);
		}

		const FCollisionObjectQueryParams ObjectParams(ObstacleObjectTypes);
		bObstructed = World->OverlapAnyTestByObjectType(BoxCenter, Rotation, ObjectParams, FCollisionShape::MakeBox(BoxHalfExtent), OverlapParams);
	}

	// 5. Does the prop itself accept the spot? (game rules, for example "not on the visitors' path")
	const bool bPropAllows = bHasSurface && bSlopeOk && !bObstructed && !IPropInterface::Execute_IsPlacementBlocked(Prop, Candidate);

	const bool bNewValid = bPropAllows;
	PlacementTransform = Candidate;

	Hologram->SetActorTransform(Candidate, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	Hologram->SetValid(bNewValid);

	if (bNewValid != bPlacementValid)
	{
		bPlacementValid = bNewValid;
		OnPlacementValidityChanged.Broadcast(bPlacementValid);
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const FColor Color = bNewValid ? FColor::Green : FColor::Red;
		DrawDebugPoint(World, SurfaceLocation, 10.0f, bHasSurface ? FColor::Yellow : FColor::Red, false, -1.0f);
		if (BoxHalfExtent.X > 0.0f)
		{
			DrawDebugBox(World, BoxCenter, BoxHalfExtent, Rotation, Color, false, -1.0f, 0, 1.0f);
		}
	}
#endif
}

bool UPropInventoryComponent::ConfirmPlacement()
{
	if (State != EPropInventoryState::Placing)
	{
		return false;
	}

	PruneDestroyedProps();

	// Take what is needed before EndPlacement clears it.
	const int32 SlotIndex = LockedSlotIndex;
	const bool bCanPlace = bPlacementValid && IsSlotOccupied(SlotIndex);
	const FTransform FinalTransform = PlacementTransform;

	// Releasing the action always ends the placement. On an invalid spot nothing else happens: the prop stays in the hand.
	EndPlacement();

	if (!bCanPlace)
	{
		return false;
	}

	BeginPlace(SlotIndex, FinalTransform);
	return true;
}

void UPropInventoryComponent::RotatePlacementSteps(int32 Steps)
{
	if (State != EPropInventoryState::Placing || Steps == 0)
	{
		return;
	}

	PlacementYawOffset = FMath::UnwindDegrees(PlacementYawOffset + Steps * RotationStepDegrees);
}

void UPropInventoryComponent::EndPlacement()
{
	if (IsValid(Hologram))
	{
		Hologram->Destroy();
	}
	Hologram = nullptr;

	const bool bWasValid = bPlacementValid;
	bPlacementValid = false;
	LockedSlotIndex = INDEX_NONE;
	PlacementYawOffset = 0.0f;

	if (bWasValid)
	{
		OnPlacementValidityChanged.Broadcast(false);
	}

	SetState(EPropInventoryState::Idle);
}

void UPropInventoryComponent::BeginPlace(int32 SlotIndex, const FTransform& WorldTransform)
{
	if (!IsSlotOccupied(SlotIndex))
	{
		return;
	}

	AActor* Prop = Slots[SlotIndex].Prop;
	const ECollisionEnabled::Type SavedCollision = Slots[SlotIndex].SavedCollisionEnabled;
	const EPropHand Hand = HandForSlotIndex(SlotIndex);

	// Logical part: the hand is free from this moment. The prop is still attached until the commit.
	ClearSlot(SlotIndex);

	BeginCommit(EPropAction::Place, Hand, Prop, [this, WeakProp = TWeakObjectPtr<AActor>(Prop), SavedCollision, Hand, WorldTransform]()
	{
		CommitPlace(WeakProp, SavedCollision, Hand, WorldTransform);
	});

	OnSlotsChanged.Broadcast();
}

void UPropInventoryComponent::CommitPlace(TWeakObjectPtr<AActor> WeakProp, ECollisionEnabled::Type SavedCollision, EPropHand Hand, const FTransform& WorldTransform)
{
	AActor* Prop = WeakProp.Get();
	if (!IsValid(Prop))
	{
		return;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Prop->GetRootComponent());
	ECollisionEnabled::Type RestoredCollision = SavedCollision;

	Prop->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Prop->SetActorTransform(WorldTransform, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	if (Primitive)
	{
		// A prop that must keep simulating needs a collision mode with physics, whatever it had before.
		if (bSimulatePhysicsAfterPlace && !HasPhysics(RestoredCollision))
		{
			RestoredCollision = ECollisionEnabled::QueryAndPhysics;
		}

		Primitive->SetCollisionEnabled(RestoredCollision);
		Primitive->SetSimulatePhysics(bSimulatePhysicsAfterPlace);
		if (bSimulatePhysicsAfterPlace)
		{
			Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	IPropInterface::Execute_OnPropPlaced(Prop, GetOwner());
	OnPropAction.Broadcast(EPropAction::Place, Hand, Prop);
}

// ----------------------------------------------------------------------
// Throw
// ----------------------------------------------------------------------

bool UPropInventoryComponent::BeginThrowCharge()
{
	CommitPendingAction();
	PruneDestroyedProps();

	// Throw wins over a placement in progress: the placement ends without placing, the charge starts.
	if (State == EPropInventoryState::Placing)
	{
		EndPlacement();
	}

	if (State != EPropInventoryState::Idle)
	{
		return false;
	}

	const int32 SlotIndex = FindActiveSlotIndex();
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	LockedSlotIndex = SlotIndex;
	ThrowElapsed = 0.0f;
	ThrowCharge = 0.0f;

	SetState(EPropInventoryState::ChargingThrow);
	OnThrowChargeChanged.Broadcast(0.0f);
	return true;
}

void UPropInventoryComponent::UpdateThrowCharge(float DeltaTime)
{
	ThrowElapsed += DeltaTime;

	const float Phase = ThrowElapsed / FMath::Max(ChargeTimeToPeak, KINDA_SMALL_NUMBER);

	// Ping pong: 0 -> 1 in ChargeTimeToPeak seconds, then 1 -> 0, and so on. Otherwise it rises and holds at the peak.
	const float NewCharge = bPingPongCharge
		? 1.0f - FMath::Abs(FMath::Fmod(Phase, 2.0f) - 1.0f)
		: FMath::Min(Phase, 1.0f);

	if (!FMath::IsNearlyEqual(NewCharge, ThrowCharge))
	{
		ThrowCharge = NewCharge;
		OnThrowChargeChanged.Broadcast(ThrowCharge);
	}
}

bool UPropInventoryComponent::ReleaseThrow()
{
	if (State != EPropInventoryState::ChargingThrow)
	{
		return false;
	}

	PruneDestroyedProps();

	const int32 SlotIndex = LockedSlotIndex;
	const float Charge = ThrowCharge;
	EndThrowCharge();

	if (!IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	AActor* Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(Owner, ViewLocation, ViewRotation);

	FRotator ThrowRotation = ViewRotation;
	ThrowRotation.Pitch += ThrowPitchOffsetDegrees;

	const float Speed = FMath::Lerp(MinThrowSpeed, MaxThrowSpeed, Charge);
	const FVector OwnerVelocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
	const FVector LinearVelocity = ThrowRotation.Vector() * Speed + OwnerVelocity * InheritOwnerVelocityScale;
	const FVector AngularVelocity = FMath::VRand() * ThrowSpinDegreesPerSecond;

	BeginRelease(SlotIndex, EPropAction::Throw, LinearVelocity, AngularVelocity);
	return true;
}

void UPropInventoryComponent::EndThrowCharge()
{
	const bool bHadCharge = ThrowCharge > 0.0f;
	ThrowCharge = 0.0f;
	ThrowElapsed = 0.0f;
	LockedSlotIndex = INDEX_NONE;

	// Tells the charge bar to go back to empty before the state change hides it.
	if (bHadCharge)
	{
		OnThrowChargeChanged.Broadcast(0.0f);
	}

	SetState(EPropInventoryState::Idle);
}

// ----------------------------------------------------------------------
// Drop
// ----------------------------------------------------------------------

bool UPropInventoryComponent::DropActiveProp()
{
	// Drop wins over everything in progress.
	CancelCurrentAction();
	PruneDestroyedProps();

	const int32 SlotIndex = FindActiveSlotIndex();
	if (SlotIndex == INDEX_NONE)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(Owner, ViewLocation, ViewRotation);

	// Forward along the heading only: looking down must not push the prop into the floor.
	const FVector Forward = FRotator(0.0f, ViewRotation.Yaw, 0.0f).Vector();
	const FVector OwnerVelocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
	const FVector LinearVelocity = Forward * DropForwardSpeed + FVector::UpVector * DropUpSpeed + OwnerVelocity * InheritOwnerVelocityScale;

	BeginRelease(SlotIndex, EPropAction::Drop, LinearVelocity, FVector::ZeroVector);
	return true;
}

void UPropInventoryComponent::BeginRelease(int32 SlotIndex, EPropAction Action, const FVector& LinearVelocity, const FVector& AngularVelocityDegrees)
{
	if (!IsSlotOccupied(SlotIndex))
	{
		return;
	}

	AActor* Prop = Slots[SlotIndex].Prop;
	const ECollisionEnabled::Type SavedCollision = Slots[SlotIndex].SavedCollisionEnabled;
	const EPropHand Hand = HandForSlotIndex(SlotIndex);

	// Logical part: the hand is free from this moment. The prop is still attached until the commit. The speed was decided now
	// (at the release); the place the prop leaves from is decided at the commit, where the hand is by then.
	ClearSlot(SlotIndex);

	BeginCommit(Action, Hand, Prop, [this, WeakProp = TWeakObjectPtr<AActor>(Prop), SavedCollision, Hand, Action, LinearVelocity, AngularVelocityDegrees]()
	{
		CommitRelease(WeakProp, SavedCollision, Hand, Action, LinearVelocity, AngularVelocityDegrees);
	});

	OnSlotsChanged.Broadcast();
}

void UPropInventoryComponent::CommitRelease(TWeakObjectPtr<AActor> WeakProp, ECollisionEnabled::Type SavedCollision, EPropHand Hand, EPropAction Action, const FVector& LinearVelocity, const FVector& AngularVelocityDegrees)
{
	AActor* Prop = WeakProp.Get();
	if (!IsValid(Prop))
	{
		return;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Prop->GetRootComponent());

	Prop->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// If a wall is between the camera and the hand, release the prop in front of the wall, not inside it.
	if (UWorld* World = GetWorld())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		SunsetCoveView::GetOwnerViewPoint(GetOwner(), ViewLocation, ViewRotation);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(PropReleaseTrace), /*bTraceComplex=*/false, GetOwner());
		Params.AddIgnoredActor(Prop);
		AddHeldPropsToIgnore(Params);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, ViewLocation, Prop->GetActorLocation(), UEngineTypes::ConvertToCollisionChannel(SurfaceTraceChannel), Params))
		{
			Prop->SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * ReleaseWallMargin, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	if (Primitive)
	{
		// A released prop must fall: it needs collision WITH physics, whatever it had before it was grabbed.
		Primitive->SetCollisionEnabled(HasPhysics(SavedCollision) ? SavedCollision : ECollisionEnabled::QueryAndPhysics);
		ApplyTemporaryPawnIgnore(Primitive);
		Primitive->SetSimulatePhysics(true);
		Primitive->WakeRigidBody();
		// Velocity is set directly (not an impulse), so the throw does not depend on the mass of the prop.
		Primitive->SetPhysicsLinearVelocity(LinearVelocity);
		Primitive->SetPhysicsAngularVelocityInDegrees(AngularVelocityDegrees);
	}

	if (Action == EPropAction::Throw)
	{
		IPropInterface::Execute_OnPropThrown(Prop, GetOwner());
	}
	else
	{
		IPropInterface::Execute_OnPropDropped(Prop, GetOwner());
	}
	OnPropAction.Broadcast(Action, Hand, Prop);
}

// ----------------------------------------------------------------------
// Commit timing
// ----------------------------------------------------------------------

void UPropInventoryComponent::BeginCommit(EPropAction Action, EPropHand Hand, AActor* Prop, TFunction<void()> Commit)
{
	// Never two pending effects: the caller flushed the previous one, this is only a guard.
	CommitPendingAction();

	FActionCommitTiming Timing;
	if (ActionBeginHook.IsBound())
	{
		Timing = ActionBeginHook.Execute(Action, Hand, Prop);
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
	World->GetTimerManager().SetTimer(PendingCommit.Timer, FTimerDelegate::CreateUObject(this, &UPropInventoryComponent::CommitPendingAction), WaitSeconds, /*bLoop=*/false);
}

void UPropInventoryComponent::CommitPendingAction()
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

void UPropInventoryComponent::CancelCurrentAction()
{
	// A pending physical effect cannot outlive the action that asked for it (cancel, end of play, unpossess).
	CommitPendingAction();

	switch (State)
	{
	case EPropInventoryState::Placing:
		EndPlacement();
		break;
	case EPropInventoryState::ChargingThrow:
		EndThrowCharge();
		break;
	default:
		break;
	}
}

// ----------------------------------------------------------------------
// Temporary Pawn-ignore after release
// ----------------------------------------------------------------------

void UPropInventoryComponent::ApplyTemporaryPawnIgnore(UPrimitiveComponent* Primitive)
{
	UWorld* World = GetWorld();
	if (!Primitive || !World || ReleasePawnIgnoreSeconds <= 0.0f)
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
		ReleasePawnIgnoreSeconds,
		/*bInLoop=*/false);
}

void UPropInventoryComponent::RestorePawnResponse(UPrimitiveComponent* Primitive)
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

void UPropInventoryComponent::RestoreAllPawnResponses()
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

bool UPropInventoryComponent::IsHoldingAnyProp() const
{
	return FindActiveSlotIndex() != INDEX_NONE;
}

bool UPropInventoryComponent::IsHandOccupied(EPropHand Hand) const
{
	return IsSlotOccupied(SlotIndexFor(Hand));
}

AActor* UPropInventoryComponent::GetPropInHand(EPropHand Hand) const
{
	const int32 SlotIndex = SlotIndexFor(Hand);
	return IsSlotOccupied(SlotIndex) ? Slots[SlotIndex].Prop.Get() : nullptr;
}

AActor* UPropInventoryComponent::GetActiveProp() const
{
	const int32 SlotIndex = FindActiveSlotIndex();
	return SlotIndex != INDEX_NONE ? Slots[SlotIndex].Prop.Get() : nullptr;
}

EPropHand UPropInventoryComponent::GetActiveHand() const
{
	const int32 SlotIndex = FindActiveSlotIndex();
	return SlotIndex != INDEX_NONE ? HandForSlotIndex(SlotIndex) : EPropHand::Right;
}

// ----------------------------------------------------------------------
// Slots and state
// ----------------------------------------------------------------------

bool UPropInventoryComponent::IsSlotOccupied(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) && IsValid(Slots[SlotIndex].Prop);
}

int32 UPropInventoryComponent::FindFreeSlotIndex() const
{
	// Right hand first, then left.
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!IsSlotOccupied(Index))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UPropInventoryComponent::FindActiveSlotIndex() const
{
	// The prop grabbed last is the active one.
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (IsSlotOccupied(Index) && (Best == INDEX_NONE || Slots[Index].GrabSequence > Slots[Best].GrabSequence))
		{
			Best = Index;
		}
	}
	return Best;
}

bool UPropInventoryComponent::IsPropHeld(const AActor* Prop) const
{
	for (const FPropSlot& Slot : Slots)
	{
		if (Prop && Slot.Prop.Get() == Prop)
		{
			return true;
		}
	}
	return false;
}

void UPropInventoryComponent::PruneDestroyedProps()
{
	bool bChanged = false;
	for (FPropSlot& Slot : Slots)
	{
		if (Slot.Prop && !IsValid(Slot.Prop))
		{
			Slot.Reset();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnSlotsChanged.Broadcast();
	}
}

void UPropInventoryComponent::ClearSlot(int32 SlotIndex)
{
	if (Slots.IsValidIndex(SlotIndex))
	{
		Slots[SlotIndex].Reset();
	}
}

void UPropInventoryComponent::AddHeldPropsToIgnore(FCollisionQueryParams& Params) const
{
	for (const FPropSlot& Slot : Slots)
	{
		if (IsValid(Slot.Prop))
		{
			Params.AddIgnoredActor(Slot.Prop);
		}
	}
}

void UPropInventoryComponent::SetState(EPropInventoryState NewState)
{
	if (State == NewState)
	{
		return;
	}

	const EPropInventoryState OldState = State;
	State = NewState;

	// Idle needs no per-frame work.
	SetComponentTickEnabled(State != EPropInventoryState::Idle);

	OnStateChanged.Broadcast(NewState, OldState);
}
