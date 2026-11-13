// SunsetCovePlayerPreferences.h
// SunsetCove - Core / Settings. Small gameplay preferences of the player that the framework does not have.
//
// WHAT
//   Sprint Mode: Hold (sprint while the key is held, default) or Toggle (one press starts running, the next press or
//   stopping stops it). Read by ASunsetCoveCharacter on every sprint press, edited by Settings > Controls.
//   Hints: On (default) or Off. Off hides every on-screen help: the suggestion feed (USunsetCoveHUDSubsystem drops the
//   messages, one-time tips are kept for when hints come back) and the interaction prompt (UHUDInteractionPromptWidget).
//   Edited by Settings > Controls > Hints; takes effect at once, also during a game.
//
// WHERE IT IS SAVED
//   GameUserSettings.ini, section [SunsetCove.Input] (same section as Controller Icons), per player and per machine.
//   Hints are cached in memory (read every frame by the HUD); the cache is filled from the ini on first use.
//   Set...(bSave = false) changes the value in memory only (the settings page previews); bSave = true writes the file.
//
// ADDING A PREFERENCE
//   One enum or bool, a Get / Set pair here with its own key, a row in the settings page that owns it.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SunsetCovePlayerPreferences.generated.h"

UENUM(BlueprintType)
enum class ESunsetCoveSprintMode : uint8
{
	/** Sprint while the sprint key is held. */
	Hold	UMETA(DisplayName = "Hold"),
	/** One press starts sprinting; the next press, or stopping, ends it. */
	Toggle	UMETA(DisplayName = "Toggle"),
};

UCLASS()
class SUNSETCOVE_API USunsetCovePlayerPreferences : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Preferences")
	static ESunsetCoveSprintMode GetSprintMode();

	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Preferences")
	static void SetSprintMode(ESunsetCoveSprintMode NewMode, bool bSave);

	/** True while hints are shown (suggestion feed and interaction prompt). Default true. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|Preferences")
	static bool AreHintsEnabled();

	UFUNCTION(BlueprintCallable, Category = "SunsetCove|Preferences")
	static void SetHintsEnabled(bool bEnabled, bool bSave);
};
