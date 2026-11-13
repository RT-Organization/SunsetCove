// SunsetCoveWaveTypes.h
// SunsetCove - Environment / Water. The wave model of the sea, shared by C++ (water height queries) and the sea material.
//
// THE MODEL: A SUM OF GERSTNER WAVES
//   A Gerstner (trochoidal) wave moves every point of the water surface on a small circle: up and down AND towards the crest,
//   which gives the sharp crests and flat troughs of a real sea (a plain sine only bobs). Summing 3-4 of them with different
//   directions and lengths looks like an ocean swell. It is the same model used by the engine's Water plugin and most games.
//
//   For one wave (direction D, wavelength L, amplitude A, steepness S, N = number of waves):
//       k     = 2 PI / L                        wave number
//       omega = sqrt(g k) * SpeedScale          deep water: long waves travel faster (g = 980 cm/s^2)
//       Q     = S / (k A N)                     how much the points move towards the crest (S = 1 means the crests are sharp)
//       f     = k * dot(D, P.xy) - omega * t
//       offset.xy = Q A D cos(f)
//       offset.z  = A sin(f)
//   The surface is the flat plane at the sea level plus the sum of the offsets of every wave, plus the tide.
//
// THE CONTRACT WITH THE MATERIAL (do not change one side without the other)
//   Each wave is packed into two float4 so the material only does the cheap part (see Docs/Environment/Sea_Setup_Guide.md):
//       WaveA = (D.x, D.y, k, omega)      WaveB = (A, Q * A, 0, 0)
//   The material receives them as SeaWaveA0..3 / SeaWaveB0..3 plus SeaTime (seconds) and SeaTideOffset (cm). The C++ side
//   (ASunsetCoveSea) computes the same values with the functions below, so a floating prop and the drawn water always agree.
//   Only the first MaxMaterialWaves waves are sent to the material; the CPU uses the same ones.

#pragma once

#include "CoreMinimal.h"
#include "SunsetCoveWaveTypes.generated.h"

/** One Gerstner wave of the sea. Directions are world yaw angles (0 = +X); lengths are in cm. */
USTRUCT(BlueprintType)
struct SUNSETCOVE_API FSunsetCoveGerstnerWave
{
	GENERATED_BODY()

	/** Direction the wave travels, world yaw in degrees. Point it towards the beach so the waves come in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (Units = "deg"))
	float DirectionDegrees = 0.0f;

	/** Distance between two crests (cm). Long waves (swell) 2000-6000, short chop 300-800. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "50.0", Units = "cm"))
	float Wavelength = 2000.0f;

	/** Height of the crest above the still water (cm). Keep it below about Wavelength / 20 for a believable sea. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", Units = "cm"))
	float Amplitude = 15.0f;

	/** 0 = round sine waves, 1 = sharp crests (the total of all waves is kept below the point where the surface would fold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Steepness = 0.5f;

	/** Multiplies the physical speed of the wave (1 = deep water speed for its length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0", UIMax = "3.0"))
	float SpeedScale = 1.0f;
};

namespace SunsetCoveWaves
{
	/** Waves sent to the material (SeaWaveA0..3 / SeaWaveB0..3). */
	constexpr int32 MaxMaterialWaves = 4;

	/** Gravity in cm/s^2, for the deep water speed of a wave. */
	constexpr float Gravity = 980.0f;

	/** One wave in the form the material uses. See the contract at the top of this file. */
	struct FPackedWave
	{
		FVector4f A = FVector4f(1.0f, 0.0f, 0.0f, 0.0f); // (D.x, D.y, k, omega)
		FVector4f B = FVector4f(0.0f, 0.0f, 0.0f, 0.0f); // (A, Q * A, 0, 0)
	};

	/** Packs the first MaxMaterialWaves waves (fewer if the array is shorter). Zero-amplitude waves are kept so indices match. */
	SUNSETCOVE_API void PackWaves(const TArray<FSunsetCoveGerstnerWave>& Waves, TArray<FPackedWave>& OutPacked);

	/** Offset of the surface point that rests at XY when the sea is flat, at time Time (cm, z included, tide excluded). */
	SUNSETCOVE_API FVector GetDisplacement(const TArray<FPackedWave>& Packed, const FVector2D& RestXY, float Time);

	/**
	 * Height of the wave surface (relative to the sea level, tide excluded) right above the world point XY.
	 * Gerstner moves points sideways, so the point above XY is found by a few fixed-point steps (3 is plenty for a gentle sea).
	 */
	SUNSETCOVE_API float GetHeightAt(const TArray<FPackedWave>& Packed, const FVector2D& WorldXY, float Time, int32 Iterations = 3);
}
