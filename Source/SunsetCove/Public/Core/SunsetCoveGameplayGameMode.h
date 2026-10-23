// SunsetCoveGameplayGameMode.h
// SunsetCove - Core area. The GameMode of every gameplay map.
//
// WHAT A GAMEMODE IS
//   The rulebook of one kind of map. It decides which classes are used (pawn, controller, HUD) and holds the rules
//   of the match. It exists on the SERVER ONLY (also in single player and for the listen-server host), never on a
//   client. UI and client code must therefore never read it: data that every player must see goes into the
//   GameState or the PlayerState.
//
// HIERARCHY
//   AGameModeBase -> ASunsetCoveGameplayGameMode (this class) -> BP_GameMode_Gameplay (Blueprint)
//   CoreFramework has no GameMode base class on purpose: match rules belong to the game.
//   Need match states (warm-up, in progress, ended)? Change the parent to AGameMode and the GameState to AGameState.
//
// HOW IT CONNECTS TO THE FRAMEWORKS (set in BP_GameMode_Gameplay, Class Defaults > Classes)
//   Default Pawn Class       = BP_Character (child of ASunsetCoveCharacter, which is a child of ABaseCharacter).
//   HUD Class                = BP_Startup_Gameplay (child of ABaseHUD). This is what starts the UI: the HUD actor
//                              opens the "Startup Screens" of this map type through UUIFrameSubsystem.
//   Player Controller Class  = BP_PlayerController (child of ASunsetCovePlayerController).
//   The constructor below already points the pawn and the controller at the C++ classes. That is a safety net:
//   the Blueprint values win, and a forgotten assignment still gives a SunsetCove class, not an engine default.
//
// SELECTING IT
//   Each gameplay map: Window > World Settings > Game Mode > GameMode Override = BP_GameMode_Gameplay.
//
// MAP TRAVEL
//   A new GameMode exists on every map. Whatever must survive a map change lives in USunsetCoveGameInstance.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SunsetCoveGameplayGameMode.generated.h"

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveGameplayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASunsetCoveGameplayGameMode();

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	//~ End AGameModeBase Interface

	// SUNSETCOVE GAME LOGIC: add the match rules of SunsetCove below (spawn rules, win and lose conditions, round flow).
	// Keep them here and never in CoreFramework: rules differ from game to game.
};
