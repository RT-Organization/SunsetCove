// SunsetCoveGameplayGameMode.cpp
// SunsetCove - Core area. See Public/Core/SunsetCoveGameplayGameMode.h for the role of this class.

#include "Core/SunsetCoveGameplayGameMode.h"

#include "Character/SunsetCoveCharacter.h"
#include "Core/Logging/SunsetCoveLog.h"
#include "GameFramework/PlayerController.h"
#include "Player/SunsetCovePlayerController.h"

ASunsetCoveGameplayGameMode::ASunsetCoveGameplayGameMode()
{
	// Safety net: use the SunsetCove C++ classes unless the Blueprint (BP_GameMode_Gameplay) names Blueprint ones.
	DefaultPawnClass = ASunsetCoveCharacter::StaticClass();
	PlayerControllerClass = ASunsetCovePlayerController::StaticClass();

	// The HUD Class is NOT set here: it is a Blueprint (BP_Startup_Gameplay, child of ABaseHUD) that lists the
	// screens to open, so it is chosen in BP_GameMode_Gameplay.

	// SUNSETCOVE GAME LOGIC: seamless travel keeps player controllers alive between maps. It needs a transition map
	// in Project Settings > Maps & Modes. Leave it off until the game needs it.
	// bUseSeamlessTravel = true;
}

void ASunsetCoveGameplayGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Server only. Runs before any actor of the map begins play.
	UE_LOG(LogSunsetCove, Verbose, TEXT("Gameplay InitGame: map '%s', options '%s'."), *MapName, *Options);

	// SUNSETCOVE GAME LOGIC: read game options from the travel URL here (for example UGameplayStatics::ParseOption).
}

void ASunsetCoveGameplayGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Server only. A player controller has joined this map.
	// The UI is NOT started here: the HUD actor of that player (the HUD Class of this GameMode) starts it on the
	// machine that owns the player, through UUIFrameSubsystem::InitializeForPlayerController.
	UE_LOG(LogSunsetCove, Verbose, TEXT("Gameplay PostLogin: %s."), *GetNameSafe(NewPlayer));

	// SUNSETCOVE GAME LOGIC: per-player setup that only the server may do (team assignment, spawn point choice).
}

void ASunsetCoveGameplayGameMode::Logout(AController* Exiting)
{
	// SUNSETCOVE GAME LOGIC: server-side cleanup for a leaving player, before the engine removes the controller.
	UE_LOG(LogSunsetCove, Verbose, TEXT("Gameplay Logout: %s."), *GetNameSafe(Exiting));

	Super::Logout(Exiting);
}
