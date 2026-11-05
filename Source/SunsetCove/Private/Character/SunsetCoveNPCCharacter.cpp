// SunsetCoveNPCCharacter.cpp
// SunsetCove - Character area. See Public/Character/SunsetCoveNPCCharacter.h for the role of this class.

#include "Character/SunsetCoveNPCCharacter.h"

#include "AI/SunsetCoveAIController.h"
#include "CameraSystem/CameraManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Movement/BaseCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Ease in and out (smoothstep), so sitting down and standing up start and end softly. */
	float EaseInOut(float Alpha)
	{
		Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
		return Alpha * Alpha * (3.f - 2.f * Alpha);
	}

	/** The facing marker is a darker shade of the body colour. */
	constexpr float NoseShade = 0.35f;
}

ASunsetCoveNPCCharacter::ASunsetCoveNPCCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The framework constructor has already created the rig: CameraManager, FirstPersonMesh, StateMachine and the
	// framework movement component. Only what an AI body needs differently is changed here.

	// AI from the first frame, whether the NPC is placed in the level or spawned by a spawner.
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ASunsetCoveAIController::StaticClass();

	// Nobody looks through an NPC. The camera manager builds its spring arm and camera in InitializeComponent, which the
	// engine only calls for components that ask for it, so asking for nothing skips the rig. Every camera call of the
	// framework is safe without a rig (it is the same state as on a dedicated server).
	if (UCameraManagerComponent* CameraManagerComponent = GetCameraManager())
	{
		CameraManagerComponent->bWantsInitializeComponent = false;
	}

	// Third person only: the framework then turns the body toward where it walks.
	DefaultPerspective = ECharacterPerspective::ThirdPerson;
	bAllowPerspectiveToggle = false;

	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		// A stroll, not the player's walk. Each NPC Blueprint sets its own speed.
		Movement->MaxWalkSpeed = 150.f;

		// NPCs walk (and, later, swim). They never fly.
		Movement->bEnableFlyDomain = false;

		// Detour crowd avoidance (ASunsetCoveAIController) replaces RVO; the two would fight each other.
		Movement->bUseRVOAvoidance = false;

		// Path following feeds the movement component like player input does (acceleration). See the header.
		if (FNavMovementProperties* NavMovement = Movement->GetNavMovementProperties())
		{
			NavMovement->bUseAccelerationForPaths = true;
		}
	}

	// Placeholder body. Pure visuals: no collision, no navmesh influence, no overlap events.
	PlaceholderRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlaceholderRoot"));
	PlaceholderRoot->SetupAttachment(GetCapsuleComponent());

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(PlaceholderRoot);
	PlaceholderBody->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PlaceholderBody->SetCanEverAffectNavigation(false);
	PlaceholderBody->SetGenerateOverlapEvents(false);
	// Sized for the engine Cylinder (1 m tall and wide, pivot in the middle): 55 cm wide, as tall as the capsule.
	PlaceholderBody->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.76f));

	PlaceholderNose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderNose"));
	PlaceholderNose->SetupAttachment(PlaceholderRoot);
	PlaceholderNose->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PlaceholderNose->SetCanEverAffectNavigation(false);
	PlaceholderNose->SetGenerateOverlapEvents(false);
	// Sized for the engine Cone (points up): turned to point forward, at head height, just outside the body.
	PlaceholderNose->SetRelativeLocationAndRotation(FVector(35.f, 0.f, 62.f), FRotator(-90.f, 0.f, 0.f));
	PlaceholderNose->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.3f));

	// A readable palette for a sunny beach. Values only, not assets: Blueprints may replace it.
	PlaceholderColors = {
		FLinearColor(0.90f, 0.30f, 0.25f),	// coral
		FLinearColor(0.20f, 0.55f, 0.90f),	// sea blue
		FLinearColor(0.95f, 0.75f, 0.20f),	// sun yellow
		FLinearColor(0.35f, 0.75f, 0.40f),	// leaf green
		FLinearColor(0.70f, 0.40f, 0.85f),	// lilac
		FLinearColor(0.95f, 0.55f, 0.15f),	// orange
		FLinearColor(0.25f, 0.80f, 0.80f),	// turquoise
		FLinearColor(0.90f, 0.45f, 0.65f)	// pink
	};

	// SUNSETCOVE GAME LOGIC: components shared by every NPC body (for example an icon widget for the GDD's request
	// icons above the head) are created here. Behavior components (visitor, police...) belong to each Blueprint.
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveNPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetupPlaceholder();
}

void ASunsetCoveNPCCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bBlending)
	{
		return;
	}

	BlendElapsed += DeltaSeconds;
	const float Alpha = (RestBlendTime > 0.f) ? EaseInOut(BlendElapsed / RestBlendTime) : 1.f;

	const FVector Location = FMath::Lerp(BlendStart.GetLocation(), BlendTarget.GetLocation(), Alpha);
	const FQuat Rotation = FQuat::Slerp(BlendStart.GetRotation(), BlendTarget.GetRotation(), Alpha);
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		bBlending = false;
		if (bStandingUp)
		{
			FinishStandingUp();
		}
	}
}

void ASunsetCoveNPCCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASunsetCoveNPCCharacter, ActivityTag);
}

// ----------------------------------------------------------------------
// Activity
// ----------------------------------------------------------------------

void ASunsetCoveNPCCharacter::SetActivityTag(FGameplayTag NewActivity)
{
	if (!HasAuthority())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: SetActivityTag called without authority. The activity is decided where the AI runs."), *GetNameSafe(this));
		return;
	}

	if (ActivityTag == NewActivity)
	{
		return;
	}

	const FGameplayTag OldActivity = ActivityTag;
	ActivityTag = NewActivity;

	// The server does not receive its own RepNotify.
	HandleActivityChanged(OldActivity);
}

void ASunsetCoveNPCCharacter::OnRep_ActivityTag(FGameplayTag OldActivity)
{
	HandleActivityChanged(OldActivity);
}

void ASunsetCoveNPCCharacter::HandleActivityChanged(FGameplayTag OldActivity)
{
	ApplyPlaceholderPose();
	OnActivityChanged.Broadcast(ActivityTag, OldActivity);

	// SUNSETCOVE GAME LOGIC: activity side effects shared by every NPC (a towel prop while lying, a sound...).
}

// ----------------------------------------------------------------------
// Resting
// ----------------------------------------------------------------------

void ASunsetCoveNPCCharacter::BeginRest(const FTransform& FeetTransform, FGameplayTag Activity)
{
	if (!HasAuthority())
	{
		return;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();

	// Already resting (a second BeginRest moves the body to the new spot): keep the response saved the first time.
	if (!bResting)
	{
		StandingPawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
	}

	bResting = true;
	bStandingUp = false;
	PendingStoodUp.Unbind();

	// Off the navmesh: no path following, and no movement mode, so gravity and the floor check never pull the body off a
	// sunbed. MOVE_None still counts as the Ground domain for the framework state machine (Ground.Idle).
	if (AController* MyController = GetController())
	{
		MyController->StopMovement();
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// The actor sits at the capsule centre, so the feet transform is raised by the capsule half height. Upright, facing
	// the spot's X axis.
	FTransform Target;
	Target.SetLocation(FeetTransform.GetLocation() + FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight());
	Target.SetRotation(FRotator(0.f, FeetTransform.Rotator().Yaw, 0.f).Quaternion());
	Target.SetScale3D(GetActorScale3D());

	SetActivityTag(Activity);
	StartBodyBlend(Target);
}

void ASunsetCoveNPCCharacter::EndRest(const FVector& StandFeetLocation, FSimpleDelegate OnStoodUp)
{
	if (!HasAuthority() || !bResting)
	{
		OnStoodUp.ExecuteIfBound();
		return;
	}

	SetActivityTag(FGameplayTag());

	PendingStoodUp = MoveTemp(OnStoodUp);
	bStandingUp = true;

	FTransform Target = GetActorTransform();
	Target.SetLocation(StandFeetLocation + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	StartBodyBlend(Target);
}

void ASunsetCoveNPCCharacter::StartBodyBlend(const FTransform& TargetActorTransform)
{
	BlendStart = GetActorTransform();
	BlendTarget = TargetActorTransform;
	BlendElapsed = 0.f;
	bBlending = true;

	// A zero blend time finishes on the next tick, which keeps a single code path for both cases.
}

void ASunsetCoveNPCCharacter::FinishStandingUp()
{
	bResting = false;
	bStandingUp = false;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, StandingPawnResponse);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	// Copied first: the callback may start a new rest, which rebinds PendingStoodUp.
	FSimpleDelegate Callback = MoveTemp(PendingStoodUp);
	PendingStoodUp.Unbind();
	Callback.ExecuteIfBound();
}

// ----------------------------------------------------------------------
// Placeholder body
// ----------------------------------------------------------------------

void ASunsetCoveNPCCharacter::SetupPlaceholder()
{
	if (!PlaceholderRoot)
	{
		return;
	}

	// A real model replaces the placeholder: hide it and never touch it again.
	const USkeletalMeshComponent* BodyMesh = GetMesh();
	bPlaceholderActive = !(BodyMesh && BodyMesh->GetSkeletalMeshAsset());
	PlaceholderRoot->SetVisibility(bPlaceholderActive, true);
	if (!bPlaceholderActive)
	{
		return;
	}

	PlaceholderStandTransform = PlaceholderRoot->GetRelativeTransform();

	if (PlaceholderColors.Num() > 0 && !PlaceholderColorParameter.IsNone())
	{
		const FLinearColor BodyColor = PlaceholderColors[FMath::RandRange(0, PlaceholderColors.Num() - 1)];
		const FLinearColor NoseColor(BodyColor.R * NoseShade, BodyColor.G * NoseShade, BodyColor.B * NoseShade, 1.f);

		// Only meshes that have a material get a dynamic copy of it; a missing material just keeps the engine default.
		if (PlaceholderBody && PlaceholderBody->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* BodyMaterial = PlaceholderBody->CreateDynamicMaterialInstance(0))
			{
				BodyMaterial->SetVectorParameterValue(PlaceholderColorParameter, BodyColor);
			}
		}
		if (PlaceholderNose && PlaceholderNose->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* NoseMaterial = PlaceholderNose->CreateDynamicMaterialInstance(0))
			{
				NoseMaterial->SetVectorParameterValue(PlaceholderColorParameter, NoseColor);
			}
		}
	}

	ApplyPlaceholderPose();
}

void ASunsetCoveNPCCharacter::ApplyPlaceholderPose()
{
	if (!bPlaceholderActive || !PlaceholderRoot)
	{
		return;
	}

	const FTransform* Pose = ActivityTag.IsValid() ? PlaceholderPoses.Find(ActivityTag) : nullptr;
	PlaceholderRoot->SetRelativeTransform(Pose ? *Pose : PlaceholderStandTransform);
}
