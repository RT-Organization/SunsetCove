// SunsetCoveSea.cpp
// SunsetCove - Environment / Water. See Public/Environment/Water/SunsetCoveSea.h.

#include "Environment/Water/SunsetCoveSea.h"

#include "AudioSystem/Manager/AudioFrameSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Environment/Water/SunsetCoveSeaSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{

	// Material / collection parameter names (the contract, see SunsetCoveWaveTypes.h and the setup guide).
	const FName SeaTimeParam(TEXT("SeaTime"));
	const FName SeaTideOffsetParam(TEXT("SeaTideOffset"));
	const FName SeaLevelParam(TEXT("SeaLevel"));
	const FName SeaWaveAParams[SunsetCoveWaves::MaxMaterialWaves] = { TEXT("SeaWaveA0"), TEXT("SeaWaveA1"), TEXT("SeaWaveA2"), TEXT("SeaWaveA3") };
	const FName SeaWaveBParams[SunsetCoveWaves::MaxMaterialWaves] = { TEXT("SeaWaveB0"), TEXT("SeaWaveB1"), TEXT("SeaWaveB2"), TEXT("SeaWaveB3") };

	/** A listener jump longer than this (cm) moves the waves emitter at once instead of sliding (respawn, teleport). */
	constexpr float ShoreAudioSnapDistance = 3000.0f;

	FLinearColor ToColor(const FVector4f& Value)
	{
		return FLinearColor(Value.X, Value.Y, Value.Z, Value.W);
	}

	FSunsetCoveGerstnerWave MakeWave(float Direction, float Wavelength, float Amplitude, float Steepness)
	{
		FSunsetCoveGerstnerWave Wave;
		Wave.DirectionDegrees = Direction;
		Wave.Wavelength = Wavelength;
		Wave.Amplitude = Amplitude;
		Wave.Steepness = Steepness;
		return Wave;
	}
}

ASunsetCoveSea::ASunsetCoveSea()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SeaRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SeaRoot"));
	SetRootComponent(SeaRoot);

	// Flat engine plane until the subdivided sea plane is assigned (the setup guide makes SM_SeaPlane).
	SurfaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SurfaceMesh"));
	SurfaceMesh->SetupAttachment(SeaRoot);
	SurfaceMesh->SetCollisionProfileName(TEXT("NoCollision"));
	SurfaceMesh->SetGenerateOverlapEvents(false);
	SurfaceMesh->SetCanEverAffectNavigation(false);
	SurfaceMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		SurfaceMesh->SetStaticMesh(PlaneMesh.Object);
	}

	// A straight 50 m shoreline to start with; the level designer bends it along the beach.
	Shoreline = CreateDefaultSubobject<USplineComponent>(TEXT("Shoreline"));
	Shoreline->SetupAttachment(SeaRoot);
	Shoreline->ClearSplinePoints(/*bUpdateSpline=*/false);
	Shoreline->AddSplinePoint(FVector(0.0f, -2500.0f, 0.0f), ESplineCoordinateSpace::Local, /*bUpdateSpline=*/false);
	Shoreline->AddSplinePoint(FVector(0.0f, 2500.0f, 0.0f), ESplineCoordinateSpace::Local, /*bUpdateSpline=*/true);

	ShoreAudioAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ShoreAudioAnchor"));
	ShoreAudioAnchor->SetupAttachment(SeaRoot);

	// Built-in attenuation of the waves sound (used when the cue / sound asset brings none), sized for the cove: about 50 m from the
	// waterline to the high rocks. Loud at the water, clearly fading over the beach, silent at the rocks.
	ShoreAttenuation.bAttenuate = true;
	ShoreAttenuation.bSpatialize = true;
	ShoreAttenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	ShoreAttenuation.AttenuationShape = EAttenuationShape::Sphere;
	ShoreAttenuation.AttenuationShapeExtents = FVector(500.0f, 0.0f, 0.0f);  // inner radius: full volume within 5 m of the water
	ShoreAttenuation.FalloffDistance = 4500.0f;                             // then fades out over 45 m (silent at the rocks)
	ShoreAttenuation.NonSpatializedRadiusStart = 1200.0f;                   // closer than 12 m: starts to surround the listener
	ShoreAttenuation.NonSpatializedRadiusEnd = 400.0f;                      // closer than 4 m: all around
	ShoreAttenuation.bAttenuateWithLPF = true;                              // far waves sound duller

	// A long swell coming in, a crossing swell, a short chop. Turn DirectionDegrees so the swell travels towards the beach.
	Waves.Add(MakeWave(0.0f, 3500.0f, 18.0f, 0.6f));
	Waves.Add(MakeWave(25.0f, 1800.0f, 9.0f, 0.5f));
	Waves.Add(MakeWave(-35.0f, 700.0f, 3.0f, 0.4f));
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveSea::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	SetupSurface();
	PushWaveParameters();
	PushFrameParameters();
}

void ASunsetCoveSea::BeginPlay()
{
	Super::BeginPlay();

	if (USunsetCoveSeaSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<USunsetCoveSeaSubsystem>() : nullptr)
	{
		Subsystem->RegisterSea(this);
	}

	SeaTime = 0.0f;
	SetupSurface();
	PushWaveParameters();
	PushFrameParameters();
	StartShoreAudio();
}

void ASunsetCoveSea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopShoreAudio();

	if (USunsetCoveSeaSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<USunsetCoveSeaSubsystem>() : nullptr)
	{
		Subsystem->UnregisterSea(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ASunsetCoveSea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SeaTime += DeltaSeconds * WaveTimeScale;
	TideOffset = TideAmplitude * FMath::Sin(UE_TWO_PI * SeaTime / FMath::Max(TidePeriodSeconds, 1.0f));
	PushFrameParameters();

	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		UpdateShoreAudio(DeltaSeconds);
	}
}

#if WITH_EDITOR
void ASunsetCoveSea::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetupSurface();
	PushWaveParameters();
	PushFrameParameters();
}
#endif

// ----------------------------------------------------------------------
// Surface and material parameters
// ----------------------------------------------------------------------

void ASunsetCoveSea::SetupSurface()
{
	if (!SurfaceMesh)
	{
		return;
	}

	// The mesh is 100 x 100 cm (engine Plane or SM_SeaPlane): scale it to the wanted size.
	SurfaceMesh->SetRelativeScale3D(FVector(FMath::Max(SurfaceSize.X, 100.0) / 100.0, FMath::Max(SurfaceSize.Y, 100.0) / 100.0, 1.0));

	if (SurfaceMaterial)
	{
		// Reuses the existing dynamic instance when it already has this parent.
		SurfaceMID = SurfaceMesh->CreateDynamicMaterialInstance(0, SurfaceMaterial);
	}
	else
	{
		SurfaceMID = nullptr;
	}
}

void ASunsetCoveSea::PushWaveParameters()
{
	SunsetCoveWaves::PackWaves(Waves, PackedWaves);

	UMaterialParameterCollectionInstance* Collection = (ParameterCollection && GetWorld()) ? GetWorld()->GetParameterCollectionInstance(ParameterCollection) : nullptr;

	for (int32 Index = 0; Index < SunsetCoveWaves::MaxMaterialWaves; ++Index)
	{
		// Missing waves are sent as zero amplitude, so the material can always sum four.
		const SunsetCoveWaves::FPackedWave Packed = PackedWaves.IsValidIndex(Index) ? PackedWaves[Index] : SunsetCoveWaves::FPackedWave();
		if (SurfaceMID)
		{
			SurfaceMID->SetVectorParameterValue(SeaWaveAParams[Index], ToColor(Packed.A));
			SurfaceMID->SetVectorParameterValue(SeaWaveBParams[Index], ToColor(Packed.B));
		}
		if (Collection)
		{
			Collection->SetVectorParameterValue(SeaWaveAParams[Index], ToColor(Packed.A));
			Collection->SetVectorParameterValue(SeaWaveBParams[Index], ToColor(Packed.B));
		}
	}
}

void ASunsetCoveSea::PushFrameParameters()
{
	const float SeaLevel = GetSeaLevel();

	if (SurfaceMID)
	{
		SurfaceMID->SetScalarParameterValue(SeaTimeParam, SeaTime);
		SurfaceMID->SetScalarParameterValue(SeaTideOffsetParam, TideOffset);
		SurfaceMID->SetScalarParameterValue(SeaLevelParam, SeaLevel);
	}

	UMaterialParameterCollectionInstance* Collection = (ParameterCollection && GetWorld()) ? GetWorld()->GetParameterCollectionInstance(ParameterCollection) : nullptr;
	if (Collection)
	{
		const bool bOk = Collection->SetScalarParameterValue(SeaTimeParam, SeaTime)
			& Collection->SetScalarParameterValue(SeaTideOffsetParam, TideOffset)
			& Collection->SetScalarParameterValue(SeaLevelParam, SeaLevel);
		if (!bOk && !bWarnedNoCollectionParams)
		{
			bWarnedNoCollectionParams = true;
			UE_LOG(LogSunsetCove, Warning, TEXT("Sea '%s': the collection '%s' lacks some parameters. It needs scalars SeaTime, SeaTideOffset, SeaLevel and vectors SeaWaveA0..3, SeaWaveB0..3 (Docs/Environment/Sea_Setup_Guide.md)."),
				*GetName(), *ParameterCollection->GetName());
		}
	}
}

void ASunsetCoveSea::SetWaves(const TArray<FSunsetCoveGerstnerWave>& NewWaves)
{
	Waves = NewWaves;
	PushWaveParameters();
}

// ----------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------

float ASunsetCoveSea::GetWaterHeightAt(const FVector& Location) const
{
	return GetSeaLevel() + SunsetCoveWaves::GetHeightAt(PackedWaves, FVector2D(Location.X, Location.Y), SeaTime);
}

bool ASunsetCoveSea::IsInsideSeaArea(const FVector& Location) const
{
	// The surface mesh is centered on the actor and scaled to SurfaceSize.
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Location);
	return FMath::Abs(Local.X) <= SurfaceSize.X * 0.5 && FMath::Abs(Local.Y) <= SurfaceSize.Y * 0.5;
}

bool ASunsetCoveSea::IsUnderwater(const FVector& Location, float Margin) const
{
	return IsInsideSeaArea(Location) && Location.Z < GetWaterHeightAt(Location) - Margin;
}

FVector ASunsetCoveSea::GetClosestShorePoint(const FVector& Location) const
{
	if (!Shoreline || Shoreline->GetNumberOfSplinePoints() == 0)
	{
		return Location;
	}

	// Searched at the height of the spline, so the answer is the horizontally closest point.
	const FVector AtSplineHeight(Location.X, Location.Y, Shoreline->GetComponentLocation().Z);
	FVector Closest = Shoreline->FindLocationClosestToWorldLocation(AtSplineHeight, ESplineCoordinateSpace::World);
	Closest.Z = GetSeaLevel();
	return Closest;
}

float ASunsetCoveSea::GetDistanceToShore(const FVector& Location) const
{
	if (!Shoreline || Shoreline->GetNumberOfSplinePoints() == 0)
	{
		return TNumericLimits<float>::Max();
	}
	return FVector::Dist2D(Location, GetClosestShorePoint(Location));
}

// ----------------------------------------------------------------------
// Shore audio
// ----------------------------------------------------------------------

bool ASunsetCoveSea::GetListenerLocation(FVector& OutLocation) const
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

void ASunsetCoveSea::UpdateShoreAudio(float DeltaSeconds)
{
	// The sound may still be loading when it is started: the attenuation is checked as soon as it exists.
	if (!bShoreAttenuationChecked && ShoreAudioHandle.IsValid())
	{
		TryApplyShoreAttenuation();
	}

	FVector Listener;
	if (!ShoreAudioAnchor || !GetListenerLocation(Listener))
	{
		return;
	}

	FVector Target = GetClosestShorePoint(Listener);
	Target.Z = GetSeaLevel() + ShoreAudioHeight;

	const FVector Current = ShoreAudioAnchor->GetComponentLocation();
	const bool bSnap = !bShoreAnchorPlaced || DeltaSeconds <= 0.0f || FVector::Dist(Current, Target) > ShoreAudioSnapDistance;
	ShoreAudioAnchor->SetWorldLocation(bSnap ? Target : FMath::VInterpTo(Current, Target, DeltaSeconds, ShoreAudioFollowSpeed));
	bShoreAnchorPlaced = true;

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const FVector Emitter = ShoreAudioAnchor->GetComponentLocation();
		DrawDebugSphere(GetWorld(), Emitter, 60.0f, 12, FColor::Cyan, false, -1.0f);
		DrawDebugLine(GetWorld(), Emitter, Listener, FColor::Cyan, false, -1.0f, 0, 1.0f);
	}
#endif
}

void ASunsetCoveSea::StartShoreAudio()
{
	if (!bPlayShoreAudio || ShoreAudioHandle.IsValid())
	{
		return;
	}

	UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this);
	const FGameplayTag Cue = ShoreWavesCue;
	if (!Audio || !Cue.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("Sea '%s': no waves sound (%s). Set Sea | Shore Audio > Shore Waves Cue (Audio.Cue.Ambient.ShoreWaves; BP_Sea has it)."),
			*GetName(), Audio ? TEXT("cue tag missing") : TEXT("no audio subsystem"));
		return;
	}

	// Place the emitter before the sound starts, so it does not begin at the actor origin.
	bShoreAnchorPlaced = false;
	UpdateShoreAudio(0.0f);
	bShoreAttenuationChecked = false;
	ShoreAudioHandle = Audio->PlayCueAttached(Cue, ShoreAudioAnchor);
	TryApplyShoreAttenuation();
}

void ASunsetCoveSea::TryApplyShoreAttenuation()
{
	UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this);
	UAudioComponent* Component = Audio ? Audio->GetHandleComponent(ShoreAudioHandle) : nullptr;
	if (!Component)
	{
		return; // Still loading: tried again next frame.
	}
	bShoreAttenuationChecked = true;

	// The cue's Attenuation Override or the sound asset's own attenuation exists: it wins.
	// (Not bAllowSpatialization: it is true by default on every Audio Component, attenuation or not, so it says nothing.)
	if (Component->GetAttenuationSettingsToApply())
	{
		return;
	}

	// No attenuation anywhere (the sound would be equally loud everywhere): restart it with the built-in shore attenuation. It
	// started this frame, so the restart is not audible.
	Component->bOverrideAttenuation = true;
	Component->AttenuationOverrides = ShoreAttenuation;
	Component->bAllowSpatialization = true;
	Component->Play();
}

void ASunsetCoveSea::StopShoreAudio()
{
	if (UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(this))
	{
		Audio->StopSound(ShoreAudioHandle, 1.0f);
	}
	ShoreAudioHandle = FAudioHandle();
	bShoreAttenuationChecked = false;
}
