// SunsetCoveMainMenuGameMode.cpp
// SunsetCove - Core area. See Public/Core/SunsetCoveMainMenuGameMode.h for the role of this class.

#include "Core/SunsetCoveMainMenuGameMode.h"

ASunsetCoveMainMenuGameMode::ASunsetCoveMainMenuGameMode()
{
	// The engine default spawns a free-flying DefaultPawn. A menu map must not spawn any pawn.
	DefaultPawnClass = nullptr;

	// The HUD Class (BP_Startup_MainMenu) is chosen in BP_GameMode_MainMenu: it is a Blueprint that lists the screens.
}
