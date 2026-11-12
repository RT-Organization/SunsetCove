// SunsetCoveMapAudio.cpp
// SunsetCove - Audio System. See Public/AudioSystem/SunsetCoveMapAudio.h.

#include "AudioSystem/SunsetCoveMapAudio.h"

#include "AudioSystem/Manager/AudioFrameSubsystem.h"
#include "Core/Logging/SunsetCoveLog.h"

void FSunsetCoveMapAudio::Apply(const UObject* WorldContextObject) const
{
	UAudioFrameSubsystem* Audio = UAudioFrameSubsystem::Get(WorldContextObject);
	if (!Audio)
	{
		// Editor preview or no game instance: nothing to play.
		return;
	}

	UE_LOG(LogSunsetCove, Verbose, TEXT("Map audio of %s: music '%s', ambient '%s'."),
		*GetNameSafe(WorldContextObject), *MusicCue.ToString(), *AmbientCue.ToString());

	if (MusicCue.IsValid())
	{
		Audio->PlayMusic(MusicCue, FadeTime, /*bRestartIfSame=*/false);
	}
	else
	{
		Audio->StopMusic(FadeTime);
	}

	if (AmbientCue.IsValid())
	{
		Audio->PlayAmbient(AmbientCue, FadeTime, /*bRestartIfSame=*/false);
	}
	else
	{
		Audio->StopAmbient(FadeTime);
	}
}
