// SunsetCoveMapAudio.h
// SunsetCove - Audio System. "What does this map play": the music and the ambient bed of one kind of map.
//
// WHAT THIS IS
//   A small data struct that lives on each SunsetCove GameMode (property MapAudio). When the map begins play the GameMode
//   calls Apply(), which hands the two cue tags to the CoreFramework audio subsystem (UAudioFrameSubsystem):
//
//     MusicCue   set   -> PlayMusic(MusicCue)      the running track crossfades into the new one;
//                                                  the same track already playing is left alone (no restart)
//     MusicCue   empty -> StopMusic()              so the menu music never leaks into a map that has none
//     AmbientCue set   -> PlayAmbient(AmbientCue)  same rules, on the ambient layer
//     AmbientCue empty -> StopAmbient()
//
//   The cue tags are rows of DA_AudioConfig > Cues (CoreFramework). Which sound asset plays, its category (Music / Ambient)
//   and whether it loops are data of that row and of the sound asset, never of this struct.
//
// WHERE IT IS SET
//   BP_GameMode_MainMenu and BP_GameMode_Gameplay > Class Defaults > Audio > Map Audio. Every map that uses one of these
//   GameModes plays the same music; a map that needs another track gets its own GameMode Blueprint child.
//
// NETWORK
//   Single player. The GameMode exists on the local machine, and music is local presentation.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SunsetCoveMapAudio.generated.h"

USTRUCT(BlueprintType)
struct SUNSETCOVE_API FSunsetCoveMapAudio
{
	GENERATED_BODY()

	/** Music of this map (a cue of DA_AudioConfig, category Music, looping sound). Empty = this map is silent: music stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (Categories = "Audio.Cue"))
	FGameplayTag MusicCue;

	/** Ambient bed of this map (waves, wind, seagulls; category Ambient, looping sound). Empty = the ambient layer stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (Categories = "Audio.Cue"))
	FGameplayTag AmbientCue;

	/** Crossfade in seconds. Negative uses the defaults of DA_AudioConfig (Default Music / Ambient Fade Time). 0 cuts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "-1.0", Units = "s"))
	float FadeTime = -1.0f;

	/** Starts, crossfades or stops the music and ambient layers. Safe without a game instance (does nothing). */
	void Apply(const UObject* WorldContextObject) const;
};
