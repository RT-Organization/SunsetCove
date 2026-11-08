// ArmsAnimationSet.h
// SunsetCove - Animation System / Arms Animation. The data asset that says which arm animation belongs to which item.
//
// WHAT THIS IS
//   The mapping  item id  ->  action  ->  hand  ->  animation.  One asset (DA_ArmsAnimations) for the whole game.
//   Every item has ITS OWN animations (the umbrella is held differently from the sunbed): there is no generic fallback.
//   An item, an action or a hand that is not in the asset simply plays nothing.
//   ADDING A NEW ITEM IS A DATA ENTRY: add its id to Items and fill the clips. No code.
//
// THE ID
//   The key is the item id: the PropId of a prop Blueprint, the ToolId of a tool Blueprint (Sunbed, Umbrella, Bucket, Spray, Toy),
//   read through IItemIdentityInterface. The radial menu clips use the generic id "ToolMenu".
//
// THE NAMES OF THE ANIMATIONS (the animator's contract, Docs/InventorySystem/Guida_Animatore.md)
//   AS_Arms_<Item>_<Action>_<Hand>      for example AS_Arms_Umbrella_Drop_L, AS_Arms_Bucket_Use_R.
//   The editor button "Populate From Naming Convention" reads these names and fills the map for you.
//
// THE SLOTS
//   The names of the Animation Blueprint slots live here, in one place: the component reads them, the Animation Blueprint must use the
//   same names in its two Slot nodes, and "Ensure Skeleton Slots" creates them on the skeleton.
//
// DEBUG PLACEHOLDER
//   With bUseDebugPlaceholder on, every clip that is not filled in resolves to DebugPlaceholderSequence. It makes every event visible
//   on the correct arm before a single animation exists. Turn it off for the real game.

#pragma once

#include "CoreMinimal.h"
#include "AnimationSystem/ArmsAnimation/ArmsAnimationTypes.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ArmsAnimationSet.generated.h"

class USkeleton;
class UAnimSequence;

UCLASS(BlueprintType)
class SUNSETCOVE_API UArmsAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	// The mapping
	// ------------------------------------------------------------------

	/** Item id (Sunbed, Bucket...) -> its animations. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation")
	TMap<FName, FArmsItemAnimations> Items;

	// ------------------------------------------------------------------
	// Slots of the Animation Blueprint (the same names must be used in its Slot nodes)
	// ------------------------------------------------------------------

	/** Slot of the right arm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation|Slots")
	FName RightSlotName = TEXT("ArmItem_R");

	/** Slot of the left arm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation|Slots")
	FName LeftSlotName = TEXT("ArmItem_L");

	/** Slot group of the right arm. A group of its own, so a right-hand animation never interrupts a left-hand one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation|Slots")
	FName RightSlotGroupName = TEXT("ArmsRight");

	/** Slot group of the left arm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation|Slots")
	FName LeftSlotGroupName = TEXT("ArmsLeft");

	// ------------------------------------------------------------------
	// Debug
	// ------------------------------------------------------------------

	/** Any arms animation (AS_Run, for example). Used for every empty clip while bUseDebugPlaceholder is on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation|Debug")
	TObjectPtr<UAnimSequence> DebugPlaceholderSequence;

	/** True: every clip that is not filled in plays DebugPlaceholderSequence. For testing only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arms Animation|Debug")
	bool bUseDebugPlaceholder = false;

#if WITH_EDITORONLY_DATA
	// ------------------------------------------------------------------
	// Editor helpers
	// ------------------------------------------------------------------

	/** The arms skeleton (SK_Character). Used by Ensure Skeleton Slots and by the validation. */
	UPROPERTY(EditAnywhere, Category = "Editor Helpers")
	TSoftObjectPtr<USkeleton> TargetSkeleton;

	/** Folder searched (with its sub folders) by Populate From Naming Convention. */
	UPROPERTY(EditAnywhere, Category = "Editor Helpers", meta = (ContentDir))
	FDirectoryPath SearchFolder;
#endif

	// ------------------------------------------------------------------
	// Query
	// ------------------------------------------------------------------

	/**
	 * Finds the clip of an item, action and arm. False when there is none (or no sequence in it): the caller plays nothing.
	 * With bUseDebugPlaceholder on, an empty clip is answered with DebugPlaceholderSequence.
	 */
	bool FindClip(FName ItemId, EArmsAnimAction Action, EArmSide Side, FArmsClip& OutClip) const;

	FName GetSlotName(EArmSide Side) const { return Side == EArmSide::Right ? RightSlotName : LeftSlotName; }

#if WITH_EDITOR
	// ------------------------------------------------------------------
	// Editor helpers (buttons in the Details panel of the asset)
	// ------------------------------------------------------------------

	/**
	 * Reads every AS_Arms_<Item>_<Action>_<Hand> animation in Search Folder and puts it in the map (Sequence only: the blend and commit
	 * settings you already set are kept). Names that do not follow the convention are listed in the Output Log.
	 */
	UFUNCTION(CallInEditor, Category = "Editor Helpers")
	void PopulateFromNamingConvention();

	/**
	 * Creates the two slot groups and the two slots on the Target Skeleton, with the names above. Save the skeleton afterwards.
	 */
	UFUNCTION(CallInEditor, Category = "Editor Helpers")
	void EnsureSkeletonSlots();

	//~ Begin UObject Interface
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	//~ End UObject Interface
#endif
};
