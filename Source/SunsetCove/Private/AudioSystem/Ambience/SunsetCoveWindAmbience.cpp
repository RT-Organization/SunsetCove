// SunsetCoveWindAmbience.cpp
// SunsetCove - Audio System / Ambience. See Public/AudioSystem/Ambience/SunsetCoveWindAmbience.h.

#include "AudioSystem/Ambience/SunsetCoveWindAmbience.h"

#include "AudioSystem/Manager/AudioFrameSubsystem.h"
#include "CollisionQueryParams.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Environment/Water/SunsetCoveSea.h"
#include "Environment/Water/SunsetCoveSeaSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{

	/** Lowest volume sent to the sound: practically silent, but the loop keeps running (no restart when it comes back). */
	constexpr float MinAudibleVolume = 0.001f;
}

ASunsetCoveWindAmbience::ASunsetCoveWindAmbience()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ShelterTraceChannel = UEngineTypes::ConvertToTraceType(ECC_Visibility);
}

void ASunsetCoveWindAmbience::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this);
	const FGameplayTag Cue = WindCue;
	if (!World || !Audio || !Cue.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Wind ambience '%s': nothing to play (%s). Set Wind > Wind Cue (Audio.Cue.Ambient.Wind; BP_WindAmbience has it)."),
			*GetName(), Audio ? TEXT("cue tag missing") : TEXT("no audio subsystem"));
		return;
	}

	// The loop starts silent and fades in with the first updates (StartFadeSeconds).
	WindHandle = Audio->PlayCue2D(Cue);
	Audio->SetHandleVolume(WindHandle, MinAudibleVolume, KINDA_SMALL_NUMBER);
	CurrentVolume = 0.0f;
	StartTime = World->GetTimeSeconds();

	World->GetTimerManager().SetTimer(UpdateTimer, this, &ASunsetCoveWindAmbience::UpdateWind, UpdateInterval, /*bLoop=*/true);
}

void ASunsetCoveWindAmbience::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
	{
		Audio->StopSound(WindHandle, 1.0f);
	}
	WindHandle = FAudioHandle();

	Super::EndPlay(EndPlayReason);
}

void ASunsetCoveWindAmbience::UpdateWind()
{
	UWorld* World = GetWorld();
	UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this);
	FVector Listener;
	if (!World || !Audio || !GetListener(Listener))
	{
		return;
	}

	const float Shore = ComputeShoreFactor(Listener);
	LastShelter = bUseShelter ? ComputeShelter(Listener) : 0.0f;

	float Target = FMath::Lerp(InlandVolume, ShoreVolume, Shore) * FMath::Lerp(1.0f, ShelteredVolumeScale, LastShelter);

	// Fade in at the start of the level.
	if (StartFadeSeconds > 0.0f)
	{
		Target *= FMath::Clamp((World->GetTimeSeconds() - StartTime) / StartFadeSeconds, 0.0f, 1.0f);
	}

	// Exponential smoothing: about VolumeSmoothSeconds to follow a change, whatever the update rate.
	const float Alpha = VolumeSmoothSeconds > 0.0f ? 1.0f - FMath::Exp(-UpdateInterval / VolumeSmoothSeconds) : 1.0f;
	CurrentVolume = FMath::Lerp(CurrentVolume, Target, Alpha);

	// The fade over one interval makes the steps between two updates inaudible.
	Audio->SetHandleVolume(WindHandle, FMath::Max(CurrentVolume, MinAudibleVolume), UpdateInterval);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugString(World, Listener + FVector(0.0f, 0.0f, 80.0f),
			FString::Printf(TEXT("Wind %.2f (shore %.2f, shelter %.2f)"), CurrentVolume, Shore, LastShelter), nullptr, FColor::White, UpdateInterval, true);
	}
#endif
}

float ASunsetCoveWindAmbience::ComputeShoreFactor(const FVector& Listener) const
{
	const ASunsetCoveSea* Sea = USunsetCoveSeaSubsystem::GetSea(this);
	if (!Sea)
	{
		return 0.0f;
	}

	const float Distance = Sea->GetDistanceToShore(Listener);
	if (ShoreFarDistance <= ShoreNearDistance)
	{
		return Distance <= ShoreNearDistance ? 1.0f : 0.0f;
	}

	const float T = FMath::Clamp((Distance - ShoreNearDistance) / (ShoreFarDistance - ShoreNearDistance), 0.0f, 1.0f);
	return 1.0f - FMath::SmoothStep(0.0f, 1.0f, T);
}

float ASunsetCoveWindAmbience::ComputeShelter(const FVector& Listener) const
{
	UWorld* World = GetWorld();
	if (!World || ShelterTraceCount <= 0 || ShelterTraceLength <= 0.0f)
	{
		return 0.0f;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WindShelterTrace), /*bTraceComplex=*/false, this);
	if (const APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		Params.AddIgnoredActor(PlayerController->GetPawn());
	}
	const ECollisionChannel Channel = UEngineTypes::ConvertToCollisionChannel(ShelterTraceChannel);
	const FVector Start = Listener + FVector(0.0f, 0.0f, ShelterTraceHeightOffset);

	float Blocked = 0.0f;
	for (int32 Index = 0; Index < ShelterTraceCount; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / ShelterTraceCount;
		const FVector End = Start + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * ShelterTraceLength;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params);
		if (bHit)
		{
			// A close wall shelters a lot, a far one a little.
			Blocked += 1.0f - FMath::Clamp(Hit.Distance / ShelterTraceLength, 0.0f, 1.0f);
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			DrawDebugLine(World, Start, bHit ? Hit.ImpactPoint : End, bHit ? FColor::Red : FColor::Green, false, UpdateInterval, 0, 1.0f);
		}
#endif
	}

	return Blocked / ShelterTraceCount;
}

bool ASunsetCoveWindAmbience::GetListener(FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return false;
	}

	FVector Front;
	FVector Right;
	PlayerController->GetAudioListenerPosition(OutLocation, Front, Right);
	return true;
}
