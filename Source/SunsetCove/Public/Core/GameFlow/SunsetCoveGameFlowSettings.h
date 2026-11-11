// SunsetCoveGameFlowSettings.h
// SunsetCove - Core area / Game Flow. Project Settings > Game > Sunset Cove Game Flow.
//
// WHAT THIS CLASS IS
//   The data of the "start or continue the world" flow, so no code holds a slot name or a map path. Saved in
//   Config/DefaultGame.ini under [/Script/SunsetCove.SunsetCoveGameFlowSettings]. Read by USunsetCoveGameFlowSubsystem.
//
// THE WORLD SAVE
//   Sunset Cove has ONE world (the cove): one World slot of the CoreFramework save system (USaveSubsystem). Continue loads
//   it, New Game deletes it and writes a fresh one. The slot name is WorldSlotName.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SunsetCoveGameFlowSettings.generated.h"

class UWorld;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sunset Cove Game Flow"))
class SUNSETCOVE_API USunsetCoveGameFlowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USunsetCoveGameFlowSettings();

	/** Name of the World save slot of the cove (CoreFramework save system). Letters, digits, '_' and '-'. */
	UPROPERTY(Config, EditAnywhere, Category = "World Save")
	FString WorldSlotName = TEXT("World");

	/** Name stored in the slot header (what a slot list would show). */
	UPROPERTY(Config, EditAnywhere, Category = "World Save")
	FString WorldDisplayName = TEXT("Sunset Cove");

	/**
	 * True: when the gameplay map has loaded after New Game, the world is saved once more, so the save records the
	 * gameplay map and the state of the objects that registered at start (the first save happens in the menu).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "World Save")
	bool bSaveWhenNewWorldMapLoaded = true;

	/** The map of the cove opened by New Game, and by Continue when the save does not name a usable map. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> GameplayMap;

	/** The main-menu map. A save that recorded this map (made from the menu) opens GameplayMap instead. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps")
	TSoftObjectPtr<UWorld> MainMenuMap;
};
