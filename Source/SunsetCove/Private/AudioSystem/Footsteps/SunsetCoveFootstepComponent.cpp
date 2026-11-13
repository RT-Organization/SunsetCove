// SunsetCoveFootstepComponent.cpp
// SunsetCove - Audio System / Footsteps. See Public/AudioSystem/Footsteps/SunsetCoveFootstepComponent.h.

#include "AudioSystem/Footsteps/SunsetCoveFootstepComponent.h"

#include "AudioSystem/Core/AudioGameplayTags.h"
#include "AudioSystem/Footsteps/SunsetCoveFootstepSet.h"
#include "AudioSystem/Manager/AudioFrameSubsystem.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Environment/Water/SunsetCoveSea.h"
#include "Environment/Water/SunsetCoveSeaSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
	/** A move longer than this in one frame is a teleport (respawn, travel): it does not count as walking. */
	constexpr float TeleportDistance = 300.0f;
}

USunsetCoveFootstepComponent::USunsetCoveFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	GroundTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
}

void USunsetCoveFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	CharacterOwner = Character;
	if (Character)
	{
		Character->LandedDelegate.AddUniqueDynamic(this, &USunsetCoveFootstepComponent::HandleLanded);
	}
	else
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Footsteps: '%s' is not a Character; automatic steps and landings are off (PlayFootstep still works)."), *GetNameSafe(GetOwner()));
	}

	if (const AActor* Owner = GetOwner())
	{
		LastLocation = Owner->GetActorLocation();
		bHasLastLocation = true;
	}
}

void USunsetCoveFootstepComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACharacter* Character = CharacterOwner.Get())
	{
		Character->LandedDelegate.RemoveDynamic(this, &USunsetCoveFootstepComponent::HandleLanded);
	}
	Super::EndPlay(EndPlayReason);
}

// ----------------------------------------------------------------------
// Rhythm
// ----------------------------------------------------------------------

float USunsetCoveFootstepComponent::GetRunAlpha(float Speed) const
{
	return RunSpeed > WalkSpeed ? FMath::Clamp((Speed - WalkSpeed) / (RunSpeed - WalkSpeed), 0.0f, 1.0f) : 1.0f;
}

void USunsetCoveFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Character = CharacterOwner.Get();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	const FVector Location = Character->GetActorLocation();
	const FVector Previous = bHasLastLocation ? LastLocation : Location;
	LastLocation = Location;
	bHasLastLocation = true;

	// The highest fall speed of the current fall decides the landing.
	if (Movement->IsFalling())
	{
		MaxFallSpeed = FMath::Max(MaxFallSpeed, static_cast<float>(-Movement->Velocity.Z));
		return;
	}

	if (!bAutoFootsteps || !Movement->IsMovingOnGround())
	{
		return;
	}

	const float Moved = FVector::Dist2D(Location, Previous);
	if (Moved > TeleportDistance)
	{
		return;
	}

	const float Speed = Movement->Velocity.Size2D();
	const float Stride = FMath::Lerp(WalkStrideLength, RunStrideLength, GetRunAlpha(Speed));
	if (Speed < MinSpeed)
	{
		// Standing still: the next step comes soon after starting to walk again (a foot moves first), not a full stride later.
		DistanceSinceStep = FMath::Max(DistanceSinceStep, Stride * 0.6f);
		return;
	}

	DistanceSinceStep += Moved;
	if (DistanceSinceStep >= Stride)
	{
		DistanceSinceStep = FMath::Fmod(DistanceSinceStep, Stride);
		PlayFootstep(/*bLanding=*/false, 1.0f);
	}
}

void USunsetCoveFootstepComponent::HandleLanded(const FHitResult& Hit)
{
	const float FallSpeed = MaxFallSpeed;
	MaxFallSpeed = 0.0f;
	DistanceSinceStep = 0.0f;

	if (!bPlayLandingSound || FallSpeed < LandingMinFallSpeed)
	{
		return;
	}

	const float Range = FMath::Max(LandingFullVolumeFallSpeed - LandingMinFallSpeed, 1.0f);
	PlayFootstep(/*bLanding=*/true, FMath::Lerp(0.55f, 1.0f, FMath::Clamp((FallSpeed - LandingMinFallSpeed) / Range, 0.0f, 1.0f)));
}

// ----------------------------------------------------------------------
// Playing a step
// ----------------------------------------------------------------------

void USunsetCoveFootstepComponent::PlayFootstep(bool bLanding, float VolumeScale)
{
	if (!FootstepSet)
	{
		if (!bWarnedNoSet)
		{
			bWarnedNoSet = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("Footsteps: '%s' has no Footstep Set (Footsteps component > Footstep Set = DA_Footsteps). No step sounds until then."), *GetNameSafe(GetOwner()));
		}
		return;
	}

	FVector Ground;
	const EPhysicalSurface Surface = DetectSurface(Ground);
	LastSurface = Surface;

	const FSunsetCoveFootstepSurface& Row = FootstepSet->FindSurface(Surface);
	const bool bUseLandings = bLanding && Row.Landings.Num() > 0;
	const TArray<TObjectPtr<USoundBase>>& List = bUseLandings ? Row.Landings : Row.Steps;

	int32& LastIndex = LastPickedIndex.FindOrAdd(static_cast<int32>(Surface) * 2 + (bUseLandings ? 1 : 0), INDEX_NONE);
	USoundBase* Sound = PickSound(List, LastIndex);
	if (!Sound)
	{
		return;
	}

	// Loudness: walk -> run, crouch softer; a landing without its own sound uses a step played louder.
	const ACharacter* Character = CharacterOwner.Get();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const float Speed = Movement ? Movement->Velocity.Size2D() : WalkSpeed;
	float Volume = bLanding ? RunVolume * (bUseLandings ? 1.0f : 1.3f) : FMath::Lerp(WalkVolume, RunVolume, GetRunAlpha(Speed));
	if (Movement && Movement->IsCrouching())
	{
		Volume *= CrouchVolumeScale;
	}
	Volume *= VolumeScale * Row.Volume * (1.0f + FMath::FRandRange(-Row.VolumeVariation, Row.VolumeVariation));
	const float Pitch = 1.0f + FMath::FRandRange(-Row.PitchVariation, Row.PitchVariation);

	if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
	{
		const FGameplayTag Category = FootstepSet->Category.IsValid() ? FootstepSet->Category : FAudioTags::CategorySFX();
		if (bPlay2D)
		{
			Audio->PlaySound2D(Sound, Category, Volume, Pitch);
		}
		else
		{
			Audio->PlaySoundAtLocation(Sound, Category, Ground, FRotator::ZeroRotator, Volume, Pitch);
		}
	}

	OnFootstep.Broadcast(Surface, Ground, bLanding);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug && GetWorld())
	{
		const UEnum* SurfaceEnum = StaticEnum<EPhysicalSurface>();
		const FString SurfaceName = SurfaceEnum ? SurfaceEnum->GetDisplayNameTextByValue(Surface).ToString() : FString::FromInt(Surface);
		DrawDebugString(GetWorld(), Ground + FVector(0.0f, 0.0f, 20.0f), FString::Printf(TEXT("%s%s %.2f"), bLanding ? TEXT("LAND ") : TEXT(""), *SurfaceName, Volume),
			nullptr, FColor::Yellow, 1.0f, true);
	}
#endif
}

USoundBase* USunsetCoveFootstepComponent::PickSound(const TArray<TObjectPtr<USoundBase>>& List, int32& InOutLastIndex) const
{
	const int32 Count = List.Num();
	if (Count == 0)
	{
		return nullptr;
	}

	int32 Index = FMath::RandRange(0, Count - 1);
	if (Count > 1 && Index == InOutLastIndex)
	{
		Index = (Index + 1 + FMath::RandRange(0, Count - 2)) % Count; // any other one
	}
	InOutLastIndex = Index;
	return List[Index];
}

// ----------------------------------------------------------------------
// Ground
// ----------------------------------------------------------------------

FVector USunsetCoveFootstepComponent::GetFeetLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	const ACharacter* Character = CharacterOwner.Get();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	return Owner->GetActorLocation() - FVector(0.0f, 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f);
}

EPhysicalSurface USunsetCoveFootstepComponent::DetectSurface(FVector& OutLocation) const
{
	const FVector Feet = GetFeetLocation();
	OutLocation = Feet;

	// 1. The sea has no collision: ask it whether the feet are under its surface.
	if (const ASunsetCoveSea* Sea = USunsetCoveSeaSubsystem::GetSea(this))
	{
		if (Sea->IsInsideSeaArea(Feet))
		{
			const float WaterZ = Sea->GetWaterHeightAt(Feet);
			if (WaterZ - Feet.Z > MinWaterDepth)
			{
				OutLocation.Z = WaterZ;
				return SeaWaterSurface;
			}
		}
	}

	// 2. What is under the feet: landscape layer or mesh, through its physical material.
	UWorld* World = GetWorld();
	if (!World)
	{
		return SurfaceType_Default;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootstepTrace), /*bTraceComplex=*/false, GetOwner());
	Params.bReturnPhysicalMaterial = true;

	const FVector Start = Feet + FVector(0.0f, 0.0f, 10.0f);
	const FVector End = Feet - FVector(0.0f, 0.0f, GroundTraceLength);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, UEngineTypes::ConvertToCollisionChannel(GroundTraceChannel), Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 1.0f);
	}
#endif

	if (!bHit)
	{
		return SurfaceType_Default;
	}

	OutLocation = Hit.ImpactPoint;
	return UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
}
