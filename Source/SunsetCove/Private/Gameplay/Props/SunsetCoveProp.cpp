// SunsetCoveProp.cpp
// SunsetCove - Gameplay area (Props). See Public/Gameplay/Props/SunsetCoveProp.h for the role of this class.

#include "Gameplay/Props/SunsetCoveProp.h"

#include "Animation/AnimationAsset.h"
#include "Core/Components/SunsetCovePivotBoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Core/Utils/SunsetCoveHeldItemUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotComponent.h"

ASunsetCoveProp::ASunsetCoveProp()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root: the invisible box. A Movable primitive with simple collision is exactly what the Prop Inventory requires.
	CollisionBody = CreateDefaultSubobject<USunsetCovePivotBoxComponent>(TEXT("CollisionBody"));
	SetRootComponent(CollisionBody);
	CollisionBody->SetMobility(EComponentMobility::Movable);
	CollisionBody->InitBoxExtent(CollisionBoxExtent);
	// PhysicsActor blocks Visibility (the interaction trace) and the world, so the prop can be aimed at and can fall.
	CollisionBody->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionBody->SetSimulatePhysics(false);
	CollisionBody->SetGenerateOverlapEvents(false);

	// Visual only: the box is the one that collides.
	PropMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PropMesh"));
	PropMesh->SetupAttachment(CollisionBody);
	PropMesh->SetMobility(EComponentMobility::Movable);
	PropMesh->SetCollisionProfileName(TEXT("NoCollision"));
	PropMesh->SetGenerateOverlapEvents(false);
	PropMesh->SetCanEverAffectNavigation(false);
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveProp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyCollisionSetup();

	if (CollisionBody)
	{
		CollisionBody->SetMassOverrideInKg(NAME_None, MassKg, /*bOverrideMass=*/true);
	}
}

void ASunsetCoveProp::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionBody)
	{
		CollisionBody->SetSimulatePhysics(bStartSimulatingPhysics);
	}

	// A prop in the level lies open. With an open pose asset the start does not depend on the reference pose of the mesh.
	bIsOpen = true;
	bIsClosing = false;
	ApplyPose(OpenPoseAnimation, /*bLastFrame=*/false);

	ValidateSetup();
}

// ----------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------

void ASunsetCoveProp::ApplyCollisionSetup()
{
	if (!CollisionBody)
	{
		return;
	}

	FVector Extent = CollisionBoxExtent;

	if (bAutoFitCollisionToMesh && PropMesh)
	{
		// The root is the center of the BOTTOM of the box (the base of the prop, CollisionBody->Origin), so the mesh is moved until
		// its base is on the root: the prop stands on the point it is put at. Physics still turns it around the center of the box.
		FVector FittedExtent;
		FVector MeshLocation;
		if (SunsetCoveHeldItem::ComputeFittedBox(PropMesh, CollisionPadding, CollisionBody->Origin, FittedExtent, MeshLocation))
		{
			Extent = FittedExtent;
			PropMesh->SetRelativeLocation(MeshLocation);
		}
	}

	CollisionBody->SetBoxExtent(Extent.ComponentMax(FVector(1.0f)), /*bUpdateOverlaps=*/false);
}

void ASunsetCoveProp::ValidateSetup() const
{
	if (!PropMesh || !PropMesh->GetSkeletalMeshAsset())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Prop '%s' has no Skeletal Mesh Asset on its PropMesh component. Set it in the Blueprint. The prop is invisible until then."),
			*GetName());
		return;
	}

	if (GripSocketName.IsNone() || !PropMesh->DoesSocketExist(GripSocketName))
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Prop '%s': grip bone or socket '%s' does not exist on skeleton of '%s'. The center of the prop is held on the hand socket instead. Add a bone with that name in Blender, or set Prop|Grip > Grip Socket Name to an existing one."),
			*GetName(), *GripSocketName.ToString(), *GetNameSafe(PropMesh->GetSkeletalMeshAsset()));
	}
}

// ----------------------------------------------------------------------
// Grip
// ----------------------------------------------------------------------

FName ASunsetCoveProp::ResolveGripName(bool bRightHand) const
{
	if (!PropMesh)
	{
		return NAME_None;
	}

	if (!bRightHand)
	{
		const FName LeftGrip = SunsetCoveHeldItem::NameIfSocketExists(PropMesh, LeftGripSocketName);
		if (!LeftGrip.IsNone())
		{
			return LeftGrip;
		}
	}

	return SunsetCoveHeldItem::NameIfSocketExists(PropMesh, GripSocketName);
}

FTransform ASunsetCoveProp::GetGripTransformInRoot(bool bRightHand) const
{
	// Actor space is the space of the root (the root is the box). Scale is dropped: the grip is a place and an orientation.
	return SunsetCoveHeldItem::GetGripTransformInRoot(PropMesh, ResolveGripName(bRightHand));
}

FTransform ASunsetCoveProp::ComputeRootInHandSocket(bool bRightHand) const
{
	// Wanted: grip (in the hand socket space) = the fine tune offset. The grip is fixed in the root, so the root is the
	// inverse of the grip, moved by the offset. With a zero offset the grip lands exactly on the hand socket.
	return SunsetCoveHeldItem::ComputeRootInHandSocket(GetGripTransformInRoot(bRightHand), HoldLocationOffset, HoldRotationOffset);
}

// ----------------------------------------------------------------------
// IPropInterface
// ----------------------------------------------------------------------

bool ASunsetCoveProp::IsPickupBlocked_Implementation(AActor* /*Picker*/) const
{
	return !bCanBePickedUp;
}

FVector ASunsetCoveProp::GetPropHoldLocationOffset_Implementation(bool bRightHand) const
{
	return ComputeRootInHandSocket(bRightHand).GetLocation();
}

FRotator ASunsetCoveProp::GetPropHoldRotationOffset_Implementation(bool bRightHand) const
{
	return ComputeRootInHandSocket(bRightHand).Rotator();
}

FName ASunsetCoveProp::GetItemId_Implementation() const
{
	if (!PropId.IsNone())
	{
		return PropId;
	}

	// BP_Sunbed_C -> Sunbed. A native class without these decorations is returned as it is.
	FString Name = GetClass()->GetName();
	Name.RemoveFromStart(TEXT("BP_"));
	Name.RemoveFromEnd(TEXT("_C"));
	return FName(*Name);
}

bool ASunsetCoveProp::GetPropDropRotation_Implementation(float ViewYaw, FQuat& OutRotation) const
{
	if (!bDropUpright)
	{
		return false;
	}

	// Upright (only the heading), then the per-prop offset in the prop's own space.
	OutRotation = FRotator(0.0f, ViewYaw, 0.0f).Quaternion() * DropRotationOffset.Quaternion();
	return true;
}

bool ASunsetCoveProp::GetPropPlacementBounds_Implementation(FBox& OutLocalBounds) const
{
	if (!CollisionBody)
	{
		return false;
	}

	// The base of the prop is the bottom of its collision box. The box is fitted once, from the reference (open) pose of the mesh, so
	// it does not depend on the pose the prop is carried in. The padding that makes the box easier to aim at is not part of the prop:
	// it is taken off, so the prop stands ON the ground and not a little above it. With the default origin (Bottom) the root is the
	// base itself, so the footprint starts at Z = 0 and the Prop Inventory puts the root right on the surface.
	OutLocalBounds = SunsetCoveHeldItem::ComputeFootprint(CollisionBody, bAutoFitCollisionToMesh ? CollisionPadding : 0.0f);
	return OutLocalBounds.IsValid != 0;
}

bool ASunsetCoveProp::GetPropPlacementPreviewPose_Implementation(UAnimationAsset*& OutAsset, float& OutTime) const
{
	// The hologram shows the prop as it will be once put down: open. The prop itself is closed while it is carried.
	if (OpenPoseAnimation)
	{
		OutAsset = OpenPoseAnimation;
		OutTime = 0.0f;
		return true;
	}

	if (PlacedAnimation)
	{
		OutAsset = PlacedAnimation;
		OutTime = GetAnimationLength(PlacedAnimation);
		return true;
	}

	return false;
}

float ASunsetCoveProp::OnPropGrabStarted_Implementation(AActor* Holder)
{
	// The pickup is a LOGICAL fact from now on: visitors must stop using the prop while it closes, so its rest spots go off here
	// and not at the attach. (The character also switches them off at the attach: harmless, and it enables them again on Place.)
	TInlineComponentArray<USunsetCoveRestSpotComponent*> RestSpots(this);
	for (USunsetCoveRestSpotComponent* RestSpot : RestSpots)
	{
		if (RestSpot)
		{
			RestSpot->SetSpotEnabled(false);
		}
	}

	float WaitSeconds = 0.0f;

	// A prop that is already closed (it was dropped or thrown) has nothing to close: it is attached at once.
	if (bIsOpen)
	{
		bIsOpen = false;

		const float CloseLength = GetAnimationLength(PickedUpAnimation);
		if (PropMesh && PickedUpAnimation && CloseLength > 0.0f)
		{
			PlayOptionalAnimation(PickedUpAnimation);
			bIsClosing = true;
			WaitSeconds = CloseLength;
		}
		else
		{
			// No Close animation: the prop is simply closed (the pose asset, if there is one) and carried at once.
			ApplyPose(ClosedPoseAnimation, /*bLastFrame=*/true);
		}
	}

	BP_OnGrabStarted(Holder);
	return WaitSeconds;
}

void ASunsetCoveProp::OnPropGrabbed_Implementation(AActor* Holder)
{
	// Whatever happened (the Close ended, or an interruption flushed the pickup in the middle of it) the prop is carried closed.
	if (bIsClosing)
	{
		bIsClosing = false;
		ApplyPose(ClosedPoseAnimation ? ClosedPoseAnimation.Get() : PickedUpAnimation.Get(), /*bLastFrame=*/true);
	}

	BP_OnGrabbed(Holder);
}

void ASunsetCoveProp::OnPropDropped_Implementation(AActor* Holder)
{
	BP_OnDropped(Holder);
}

float ASunsetCoveProp::OnPropPlaceStarted_Implementation(AActor* Holder)
{
	// The prop left the hand and flies to the placement spot: it opens WHILE it flies. (A prop that was never closed, because it has no
	// animations, has nothing to open.) The length is what the flight must last at least, so the prop lands as it finishes opening.
	float WaitSeconds = 0.0f;

	if (!bIsOpen)
	{
		bIsOpen = true;
		bIsClosing = false;

		const float OpenLength = GetAnimationLength(PlacedAnimation);
		if (PropMesh && PlacedAnimation && OpenLength > 0.0f)
		{
			PlayOptionalAnimation(PlacedAnimation);
			WaitSeconds = OpenLength;
		}
		else
		{
			ApplyPose(OpenPoseAnimation, /*bLastFrame=*/false);
		}
	}

	BP_OnPlaceStarted(Holder);
	return WaitSeconds;
}

void ASunsetCoveProp::OnPropPlaced_Implementation(AActor* Holder)
{
	// Landed at the hologram spot. (The Open animation started when the flight started: OnPropPlaceStarted.)
	BP_OnPlaced(Holder);
}

void ASunsetCoveProp::OnPropThrown_Implementation(AActor* Holder)
{
	BP_OnThrown(Holder);
}

void ASunsetCoveProp::PlayOptionalAnimation(UAnimationAsset* Animation) const
{
	if (PropMesh && Animation)
	{
		PropMesh->PlayAnimation(Animation, /*bLooping=*/false);
	}
}

void ASunsetCoveProp::ApplyPose(UAnimationAsset* Pose, bool bLastFrame) const
{
	if (!PropMesh || !Pose)
	{
		return;
	}

	// A single animation node, stopped on the wanted frame: the pose is held, nothing plays.
	PropMesh->PlayAnimation(Pose, /*bLooping=*/false);
	PropMesh->SetPosition(bLastFrame ? GetAnimationLength(Pose) : 0.0f, /*bFireNotifies=*/false);
	PropMesh->Stop();
}

float ASunsetCoveProp::GetAnimationLength(const UAnimationAsset* Animation)
{
	return Animation ? Animation->GetPlayLength() : 0.0f;
}
