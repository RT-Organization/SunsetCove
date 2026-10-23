// SunsetCoveGameInstance.h
// SunsetCove - Core area. The GameInstance of the game.
//
// WHAT A GAMEINSTANCE IS
//   The one object that lives for the whole run of the game, on every machine (server and each client, each with
//   its own copy, never replicated). It survives every map change, so it is the place for data that must outlive
//   a map. GameModes, controllers and pawns are all destroyed when the map changes. This object is not.
//
// HIERARCHY
//   UGameInstance -> USunsetCoveGameInstance (this class) -> BP_GameInstance (Blueprint)
//   A project has exactly ONE GameInstance class. That is why CoreFramework has no GameInstance base class: it
//   would block every other framework. A framework that needs session-long state ships a UGameInstanceSubsystem,
//   which can be combined with this class without any change here.
//
// SELECTING IT
//   Project Settings > Maps & Modes > Game Instance > Game Instance Class = BP_GameInstance.
//
// WHAT BELONGS HERE (SUNSETCOVE GAME LOGIC)
//   Loaded profile or settings, online session handles, a pending travel request.
//
// WHAT DOES NOT BELONG HERE
//   - The UI. It lives in UUIFrameSubsystem, a subsystem of the local player (one per split-screen player).
//   - Match rules. They belong in ASunsetCoveGameplayGameMode.
//   - Data that other players must see. That is GameState or PlayerState data.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SunsetCoveGameInstance.generated.h"

UCLASS(Blueprintable)
class SUNSETCOVE_API USunsetCoveGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//~ Begin UGameInstance Interface
	virtual void Init() override;
	virtual void OnStart() override;
	virtual void Shutdown() override;
	//~ End UGameInstance Interface

	// SUNSETCOVE GAME LOGIC: add the session-long data and functions of SunsetCove below.
};
