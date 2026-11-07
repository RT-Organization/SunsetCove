// SunsetCoveProp.cpp
// SunsetCove - Gameplay area (Props). See Public/Gameplay/Props/SunsetCoveProp.h for the role of this class.

#include "Gameplay/Props/SunsetCoveProp.h"

#include "Animation/AnimationAsset.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Core/Utils/SunsetCoveHeldItemUtils.h"
#include "Engine/SkeletalMesh.h"

ASunsetCoveProp::ASunsetCoveProp()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root: the invisible box. A Movable primitive with simple collision is exactly what the Prop Inventory requires.
	CollisionBody = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBody"));
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
		// The root is the center of the box, and the center of the box is where the physics turns the prop around. So
		// the mesh is moved until its own center is on the root.
		FVector FittedExtent;
		FVector MeshLocation;
		if (SunsetCoveHeldItem::ComputeFittedBox(PropMesh, CollisionPadding, FittedExtent, MeshLocation))
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

void ASunsetCoveProp::OnPropGrabbed_Implementation(AActor* Holder)
{
	PlayOptionalAnimation(PickedUpAnimation);
	BP_OnGrabbed(Holder);
}

void ASunsetCoveProp::OnPropDropped_Implementation(AActor* Holder)
{
	BP_OnDropped(Holder);
}

void ASunsetCoveProp::OnPropPlaced_Implementation(AActor* Holder)
{
	PlayOptionalAnimation(PlacedAnimation);
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
