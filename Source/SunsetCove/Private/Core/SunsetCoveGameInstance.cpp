// SunsetCoveGameInstance.cpp
// SunsetCove - Core area. See Public/Core/SunsetCoveGameInstance.h for the role of this class.

#include "Core/SunsetCoveGameInstance.h"

#include "Core/Logging/SunsetCoveLog.h"

void USunsetCoveGameInstance::Init()
{
	Super::Init();

	// Runs once when the game process starts, before the first map loads.
	// CoreFramework needs nothing from you here: UUIFrameSubsystem is created by the engine together with the
	// local player, and the UI config (Default UI Config in the project settings) is read when the first HUD starts.
	UE_LOG(LogSunsetCove, Verbose, TEXT("GameInstance Init."));

	// SUNSETCOVE GAME LOGIC: register delegates, load the profile or settings that every map needs.
}

void USunsetCoveGameInstance::OnStart()
{
	Super::OnStart();

	// Runs when the game instance starts playing, right before the first map is entered.
	// SUNSETCOVE GAME LOGIC: start-up work that needs a world.
}

void USunsetCoveGameInstance::Shutdown()
{
	// SUNSETCOVE GAME LOGIC: undo what Init registered (delegates, handles).
	UE_LOG(LogSunsetCove, Verbose, TEXT("GameInstance Shutdown."));

	Super::Shutdown();
}
