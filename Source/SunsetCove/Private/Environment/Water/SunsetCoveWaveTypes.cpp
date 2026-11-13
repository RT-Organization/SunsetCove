// SunsetCoveWaveTypes.cpp
// SunsetCove - Environment / Water. See Public/Environment/Water/SunsetCoveWaveTypes.h (the formulas and the material contract).

#include "Environment/Water/SunsetCoveWaveTypes.h"

namespace SunsetCoveWaves
{
	void PackWaves(const TArray<FSunsetCoveGerstnerWave>& Waves, TArray<FPackedWave>& OutPacked)
	{
		OutPacked.Reset();
		const int32 Count = FMath::Min(Waves.Num(), MaxMaterialWaves);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FSunsetCoveGerstnerWave& Wave = Waves[Index];
			const float Radians = FMath::DegreesToRadians(Wave.DirectionDegrees);
			const float Wavelength = FMath::Max(Wave.Wavelength, 1.0f);
			const float K = UE_TWO_PI / Wavelength;
			const float Omega = FMath::Sqrt(Gravity * K) * FMath::Max(Wave.SpeedScale, 0.0f);
			const float Amplitude = FMath::Max(Wave.Amplitude, 0.0f);

			// Q = S / (k A N): with the sum of all steepness values <= 1 the surface never folds over itself.
			const float Q = (Amplitude > KINDA_SMALL_NUMBER) ? FMath::Clamp(Wave.Steepness, 0.0f, 1.0f) / (K * Amplitude * Count) : 0.0f;

			FPackedWave& Packed = OutPacked.AddDefaulted_GetRef();
			Packed.A = FVector4f(FMath::Cos(Radians), FMath::Sin(Radians), K, Omega);
			Packed.B = FVector4f(Amplitude, Q * Amplitude, 0.0f, 0.0f);
		}
	}

	FVector GetDisplacement(const TArray<FPackedWave>& Packed, const FVector2D& RestXY, float Time)
	{
		FVector Offset = FVector::ZeroVector;
		for (const FPackedWave& Wave : Packed)
		{
			const float Phase = Wave.A.Z * (Wave.A.X * RestXY.X + Wave.A.Y * RestXY.Y) - Wave.A.W * Time;
			const float Cos = FMath::Cos(Phase);
			Offset.X += Wave.B.Y * Wave.A.X * Cos;
			Offset.Y += Wave.B.Y * Wave.A.Y * Cos;
			Offset.Z += Wave.B.X * FMath::Sin(Phase);
		}
		return Offset;
	}

	float GetHeightAt(const TArray<FPackedWave>& Packed, const FVector2D& WorldXY, float Time, int32 Iterations)
	{
		if (Packed.Num() == 0)
		{
			return 0.0f;
		}

		// Find the rest point whose displaced position lands on WorldXY: Rest = WorldXY - Offset(Rest), iterated.
		FVector2D Rest = WorldXY;
		for (int32 Step = 0; Step < FMath::Max(Iterations, 1); ++Step)
		{
			const FVector Offset = GetDisplacement(Packed, Rest, Time);
			Rest = WorldXY - FVector2D(Offset.X, Offset.Y);
		}
		return GetDisplacement(Packed, Rest, Time).Z;
	}
}
