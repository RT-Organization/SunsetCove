// SunsetCoveTool.cpp
// SunsetCove - Gameplay area (Tools). See Public/Gameplay/Tools/SunsetCoveTool.h for the role of this class.

#include "Gameplay/Tools/SunsetCoveTool.h"

#include "Animation/AnimationAsset.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Core/Utils/SunsetCoveHeldItemUtils.h"
#include "Core/Utils/SunsetCoveViewUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Interfaces/ToolTargetInterface.h"

ASunsetCoveTool::ASunsetCoveTool()
{
	PrimaryActorTick.bCanEverTick = false;

	// Root: the invisible box. Same structure as a prop: a Movable primitive with simple collision.
	CollisionBody = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBody"));
	SetRootComponent(CollisionBody);
	CollisionBody->SetMobility(EComponentMobility::Movable);
	CollisionBody->InitBoxExtent(CollisionBoxExtent);
	// PhysicsActor blocks Visibility (the interaction trace) and the world, so the tool can be aimed at and can fall.
	CollisionBody->SetCollisionProfileName(TEXT("PhysicsActor"));
	CollisionBody->SetSimulatePhysics(false);
	CollisionBody->SetGenerateOverlapEvents(false);

	// Visual only: the box is the one that collides.
	ToolMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ToolMesh"));
	ToolMesh->SetupAttachment(CollisionBody);
	ToolMesh->SetMobility(EComponentMobility::Movable);
	ToolMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ToolMesh->SetGenerateOverlapEvents(false);
	ToolMesh->SetCanEverAffectNavigation(false);

	UseTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveTool::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyCollisionSetup();

	if (CollisionBody)
	{
		CollisionBody->SetMassOverrideInKg(NAME_None, MassKg, /*bOverrideMass=*/true);
	}
}

void ASunsetCoveTool::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionBody)
	{
		CollisionBody->SetSimulatePhysics(bStartSimulatingPhysics);
	}

	ValidateSetup();
}

void ASunsetCoveTool::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearHoldTimer();

	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------

void ASunsetCoveTool::ApplyCollisionSetup()
{
	if (!CollisionBody)
	{
		return;
	}

	FVector Extent = CollisionBoxExtent;

	if (bAutoFitCollisionToMesh && ToolMesh)
	{
		// The root is the center of the box, and the center of the box is where the physics turns the tool around. So
		// the mesh is moved until its own center is on the root.
		FVector FittedExtent;
		FVector MeshLocation;
		if (SunsetCoveHeldItem::ComputeFittedBox(ToolMesh, CollisionPadding, FittedExtent, MeshLocation))
		{
			Extent = FittedExtent;
			ToolMesh->SetRelativeLocation(MeshLocation);
		}
	}

	CollisionBody->SetBoxExtent(Extent.ComponentMax(FVector(1.0f)), /*bUpdateOverlaps=*/false);
}

void ASunsetCoveTool::ValidateSetup() const
{
	if (!ToolMesh || !ToolMesh->GetSkeletalMeshAsset())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Tool '%s' has no Skeletal Mesh Asset on its ToolMesh component. Set it in the Blueprint. The tool is invisible until then."),
			*GetName());
		return;
	}

	if (GripSocketName.IsNone() || !ToolMesh->DoesSocketExist(GripSocketName))
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Tool '%s': grip bone or socket '%s' does not exist on skeleton of '%s'. The center of the tool is held on the hand socket instead. Add a bone with that name in Blender, or set Tool|Grip > Grip Socket Name to an existing one."),
			*GetName(), *GripSocketName.ToString(), *GetNameSafe(ToolMesh->GetSkeletalMeshAsset()));
	}

	if (ToolId.IsNone())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Tool '%s' has no Tool Id (Tool|Identity). Set the row name of the tool in the tools table. Tools without an id are never treated as duplicates of each other."),
			*GetName());
	}
}

// ----------------------------------------------------------------------
// Grip
// ----------------------------------------------------------------------

FName ASunsetCoveTool::ResolveGripName() const
{
	return SunsetCoveHeldItem::NameIfSocketExists(ToolMesh, GripSocketName);
}

FTransform ASunsetCoveTool::GetGripTransformInRoot() const
{
	return SunsetCoveHeldItem::GetGripTransformInRoot(ToolMesh, ResolveGripName());
}

FTransform ASunsetCoveTool::GetRootInHandSocketTransform() const
{
	// Wanted: grip (in the hand socket space) = the fine tune offset. The grip is fixed in the root, so the root is the
	// inverse of the grip, moved by the offset. With a zero offset the grip lands exactly on the hand socket.
	return SunsetCoveHeldItem::ComputeRootInHandSocket(GetGripTransformInRoot(), HoldLocationOffset, HoldRotationOffset);
}

// ----------------------------------------------------------------------
// Lifecycle (called by the Tool Inventory)
// ----------------------------------------------------------------------

void ASunsetCoveTool::NotifyPickedUp(AActor* NewHolder)
{
	Holder = NewHolder;
	bInUse = false;
	BP_OnPickedUp(NewHolder);
}

void ASunsetCoveTool::NotifyEquipped()
{
	PlayOptionalAnimation(EquipAnimation);
	BP_OnEquipped(Holder.Get());
}

void ASunsetCoveTool::NotifyHolstered()
{
	EndUse();
	BP_OnHolstered(Holder.Get());
}

void ASunsetCoveTool::NotifyDropped()
{
	EndUse();

	AActor* PreviousHolder = Holder.Get();
	Holder.Reset();
	BP_OnDropped(PreviousHolder);
}

// ----------------------------------------------------------------------
// Use
// ----------------------------------------------------------------------

bool ASunsetCoveTool::IsOnCooldown() const
{
	const UWorld* World = GetWorld();
	if (!World || UseCooldownSeconds <= 0.0f)
	{
		return false;
	}

	return (World->GetTimeSeconds() - LastUseTime) < UseCooldownSeconds;
}

bool ASunsetCoveTool::BeginUse()
{
	if (!Holder.IsValid() || bInUse)
	{
		return false;
	}

	if (UseMode == EToolUseMode::Press)
	{
		// One click is one use: if nothing can happen, the click is not a use at all.
		AActor* Target = nullptr;
		FHitResult Hit;
		if (!PrepareUse(Target, Hit))
		{
			return false;
		}

		bInUse = true;
		BP_OnUseStarted();
		ExecuteUse(Target, Hit);
		return true;
	}

	// Hold: the use starts with the press, and each repeat checks for itself whether it can act.
	if (IsUseBlocked())
	{
		return false;
	}

	bInUse = true;
	BP_OnUseStarted();
	TryPerformUse();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HoldTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { TryPerformUse(); }),
			FMath::Max(HoldUseInterval, 0.02f),
			/*bInLoop=*/true);
	}
	return true;
}

void ASunsetCoveTool::EndUse()
{
	if (!bInUse)
	{
		return;
	}

	ClearHoldTimer();
	bInUse = false;
	BP_OnUseEnded();
}

bool ASunsetCoveTool::PrepareUse(AActor*& OutTarget, FHitResult& OutHit)
{
	OutTarget = nullptr;

	if (!Holder.IsValid() || IsUseBlocked() || IsOnCooldown())
	{
		return false;
	}

	OutTarget = FindUseTarget(OutHit);
	return !(bRequiresTarget && !OutTarget);
}

void ASunsetCoveTool::ExecuteUse(AActor* Target, const FHitResult& Hit)
{
	if (const UWorld* World = GetWorld())
	{
		LastUseTime = World->GetTimeSeconds();
	}

	PlayOptionalAnimation(UseAnimation);
	PerformUse(Target, Hit);
	OnUsed.Broadcast(this, Target, Hit);
	BP_OnUsed(Target, Hit);
}

bool ASunsetCoveTool::TryPerformUse()
{
	AActor* Target = nullptr;
	FHitResult Hit;
	if (!PrepareUse(Target, Hit))
	{
		return false;
	}

	ExecuteUse(Target, Hit);
	return true;
}

void ASunsetCoveTool::PerformUse_Implementation(AActor* Target, const FHitResult& Hit)
{
	if (Target && Target->Implements<UToolTargetInterface>())
	{
		IToolTargetInterface::Execute_ReceiveToolUse(Target, this, Holder.Get(), Hit);
	}
}

AActor* ASunsetCoveTool::FindUseTarget(FHitResult& OutHit)
{
	OutHit = FHitResult();

	UWorld* World = GetWorld();
	AActor* User = Holder.Get();
	if (!World || !User)
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	SunsetCoveView::GetOwnerViewPoint(User, ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * UseRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ToolUseTrace), /*bTraceComplex=*/false, User);
	Params.AddIgnoredActor(this);
	const ECollisionChannel Channel = UEngineTypes::ConvertToCollisionChannel(UseTraceChannel);

	const bool bHit = UseTraceRadius > 0.0f
		? World->SweepSingleByChannel(OutHit, ViewLocation, TraceEnd, FQuat::Identity, Channel, FCollisionShape::MakeSphere(UseTraceRadius), Params)
		: World->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, Channel, Params);

	AActor* Target = nullptr;
	if (bHit)
	{
		AActor* HitActor = OutHit.GetActor();
		if (HitActor && HitActor->Implements<UToolTargetInterface>()
			&& !IToolTargetInterface::Execute_IsToolUseBlocked(HitActor, this, User, OutHit))
		{
			Target = HitActor;
		}
	}
	else
	{
		// Nothing in range: report the end of the range, so a tool without a target (a spray) still has a place to aim at.
		OutHit.TraceStart = ViewLocation;
		OutHit.TraceEnd = TraceEnd;
		OutHit.Location = TraceEnd;
		OutHit.ImpactPoint = TraceEnd;
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, ViewLocation, bHit ? FVector(OutHit.ImpactPoint) : TraceEnd, Target ? FColor::Green : FColor::Red, false, 0.5f);
		if (bHit)
		{
			DrawDebugSphere(World, OutHit.ImpactPoint, FMath::Max(UseTraceRadius, 3.0f), 8, Target ? FColor::Green : FColor::Orange, false, 0.5f);
		}
	}
#endif

	return Target;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void ASunsetCoveTool::PlayOptionalAnimation(UAnimationAsset* Animation) const
{
	if (ToolMesh && Animation)
	{
		ToolMesh->PlayAnimation(Animation, /*bLooping=*/false);
	}
}

void ASunsetCoveTool::ClearHoldTimer()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
}
