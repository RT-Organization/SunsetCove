// SunsetCoveMainMenuGameMode.h
// SunsetCove - Core area. The GameMode of the main-menu map.
//
// WHY A SECOND GAMEMODE
//   A menu map has no character and no match rules. A separate class keeps the gameplay GameMode free of menu
//   special cases, and it guarantees that the menu map never spawns a pawn.
//
// HIERARCHY
//   AGameModeBase -> ASunsetCoveMainMenuGameMode (this class) -> BP_GameMode_MainMenu (Blueprint)
//   The two SunsetCove GameModes share nothing, so they do not share a parent class either.
//
// HOW IT CONNECTS TO THE FRAMEWORKS (set in BP_GameMode_MainMenu, Class Defaults > Classes)
//   HUD Class = BP_Startup_MainMenu (child of ABaseHUD). Its "Startup Screens" list opens the main-menu screen
//               through UUIFrameSubsystem. That is the only thing this map needs.
//   Default Pawn Class stays None (set in the constructor).
//   Player Controller Class stays the engine default: the menu has no pause key. CommonUI handles the cursor and
//   the input mode of the menu screen.
//
// SELECTING IT
//   The main-menu map: Window > World Settings > Game Mode > GameMode Override = BP_GameMode_MainMenu.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SunsetCoveMainMenuGameMode.generated.h"

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASunsetCoveMainMenuGameMode();

	// SUNSETCOVE GAME LOGIC: the menu map normally needs no server logic. Add a hook here only if it does
	// (for example a lobby step before the gameplay map).
};
