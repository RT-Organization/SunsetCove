// SunsetCoveFootstepSet.cpp
// SunsetCove - Audio System / Footsteps. See Public/AudioSystem/Footsteps/SunsetCoveFootstepSet.h.

#include "AudioSystem/Footsteps/SunsetCoveFootstepSet.h"

const FSunsetCoveFootstepSurface& USunsetCoveFootstepSet::FindSurface(EPhysicalSurface Surface) const
{
	for (const FSunsetCoveFootstepSurface& Row : Surfaces)
	{
		if (Row.Surface == Surface && Row.Steps.Num() > 0)
		{
			return Row;
		}
	}
	return Default;
}
