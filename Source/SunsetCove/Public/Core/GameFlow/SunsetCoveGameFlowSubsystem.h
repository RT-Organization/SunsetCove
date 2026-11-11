// SunsetCoveGameFlowSubsystem.h
// SunsetCove - Core area / Game Flow. Start a new world, continue the saved one, know whether a save exists.
//
// WHAT THIS CLASS IS
//   A Game Instance Subsystem (it survives map changes) that turns the CoreFramework save system (USaveSubsystem) into the
//   three questions of the main menu. It owns no save logic of its own: every file operation goes through USaveSubsystem.
//
//     RefreshWorldSaveState   ListSlots, then "does WorldSlotName exist?"  -> OnWorldSaveStateChanged(bHasWorldSave)
//     StartNewWorld           ListSlots -> (DeleteSlot if it exists) -> StartNewGame -> SaveToSlot -> open GameplayMap
//                             -> once the map is loaded: save again (bSaveWhenNewWorldMapLoaded)
//     ContinueWorld           LoadSlot -> open the saved map (or GameplayMap if the save was made in the menu)
//
//   The first New Game of a fresh install therefore writes the world file at once: the next time the menu opens it
//   shows Continue Game.
//
// WHERE THE DATA IS
//   Slot name, display name, gameplay map, menu map: Project Settings > Game > Sunset Cove Game Flow
//   (USunsetCoveGameFlowSettings). The UI never names a slot or a map.
//
// PROGRESS
//   What is restored by Continue is what the save system captured: every saveable object (USaveableComponent /
//   ISaveableInterface) registered in the world at the last save. SaveWorld() is the call for game systems that decide
//   when the cove is saved (going to sleep in the cave, end of a workday).
//
// NETWORK
//   Single player: the menu is standalone, so World operations are always allowed by the save system.

#pragma once

#include "CoreMinimal.h"
#include "SaveSystem/Core/SaveTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SunsetCoveGameFlowSubsystem.generated.h"

class USaveSubsystem;
class UWorld;

/** Whether the world save exists. Fires after every refresh, new world and deletion. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSunsetCoveWorldSaveStateChangedSignature, bool, bHasWorldSave);

/** A flow step failed (file error, save too new...). Reason is a sentence for the player. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSunsetCoveGameFlowFailedSignature, const FText&, Reason);

/** True while a new world or a continue is running (buttons should be disabled), false when it ended or failed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSunsetCoveGameFlowBusyChangedSignature, bool, bBusy);

UCLASS()
class SUNSETCOVE_API USunsetCoveGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the game instance of WorldContextObject. Null outside a game (editor designer). */
	static USunsetCoveGameFlowSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// ------------------------------------------------------------------
	// World save state
	// ------------------------------------------------------------------

	/** Asks the save system for its slot list; OnWorldSaveStateChanged answers. Cheap (the index is cached by the save system). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|GameFlow")
	void RefreshWorldSaveState();

	/** False until the first refresh answered. Until then the menu treats the save as missing (no Continue). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|GameFlow")
	bool IsWorldSaveStateKnown() const { return bWorldSaveStateKnown; }

	/** True if the world slot exists on disk (as of the last refresh / save / delete). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|GameFlow")
	bool HasWorldSave() const { return bHasWorldSave; }

	// ------------------------------------------------------------------
	// Flow
	// ------------------------------------------------------------------

	/** Deletes the existing world (if any), creates and saves a fresh one, opens the gameplay map. Ignored while busy. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|GameFlow")
	void StartNewWorld();

	/** Loads the world save and opens its map. Fails (OnFlowFailed) if there is no save. Ignored while busy. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|GameFlow")
	void ContinueWorld();

	/** Saves the running world into the world slot (for game systems: sleeping in the cave, end of a day). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|GameFlow")
	void SaveWorld();

	/** True while StartNewWorld / ContinueWorld runs, until the map change starts or the step fails. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|GameFlow")
	bool IsBusy() const { return bBusy; }

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|GameFlow")
	FSunsetCoveWorldSaveStateChangedSignature OnWorldSaveStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|GameFlow")
	FSunsetCoveGameFlowFailedSignature OnFlowFailed;

	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|GameFlow")
	FSunsetCoveGameFlowBusyChangedSignature OnBusyChanged;

private:
	USaveSubsystem* GetSaveSubsystem() const;
	FString GetWorldSlotName() const;

	/** Updates the cached state from the save system's slot cache and broadcasts it. */
	void UpdateWorldSaveStateFromCache(USaveSubsystem* Save);

	/** The part of StartNewWorld after the old slot is gone. */
	void CreateFreshWorld();

	/** Opens the map recorded in the save, or GameplayMap when the save names no usable map (empty, or the menu map). */
	void OpenWorldMap(const FString& SavedMapName);

	void HandlePostLoadMap(UWorld* LoadedWorld);

	void SetBusy(bool bNewBusy);
	void Fail(const FText& Reason);

	FDelegateHandle PostLoadMapHandle;

	bool bWorldSaveStateKnown = false;
	bool bHasWorldSave = false;
	bool bBusy = false;

	/** Set by StartNewWorld: save once more when the new world's map has loaded. */
	bool bSavePendingAfterMapLoad = false;
};
