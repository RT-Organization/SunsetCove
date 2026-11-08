// ItemIdentityInterface.h
// SunsetCove - Interfaces area. "This actor is an item with an id": the key that systems use to look up per-item data.
//
// WHAT THIS IS
//   Props (sunbed, umbrella) and tools (bucket, spray, toy) are different classes with different inventories. Systems that
//   only need to know WHICH item it is (today the arms animation layer, later UI or sheets) ask this interface instead of
//   casting to ASunsetCoveProp or ASunsetCoveTool.
//
// THE ID
//   One word, capital first letter, English: Sunbed, Umbrella, Bucket, Spray, Toy. It is the <Item> of the animator's file
//   names (AS_Arms_<Item>_<Action>_<Hand>) and the key of the data asset of the arms animations. ASunsetCoveTool returns its
//   ToolId, ASunsetCoveProp returns its PropId (see there).
//
// DEFAULT
//   NAME_None = "no id": systems that need an id simply do nothing for this actor (no animation, no error).
//
// CALLING IT FROM C++
//   if (Actor->Implements<UItemIdentityInterface>()) { const FName Id = IItemIdentityInterface::Execute_GetItemId(Actor); }

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemIdentityInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UItemIdentityInterface : public UInterface
{
	GENERATED_BODY()
};

class SUNSETCOVE_API IItemIdentityInterface
{
	GENERATED_BODY()

public:
	/** The id of this item (Sunbed, Bucket...). NAME_None when it has none. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Item")
	FName GetItemId() const;
	virtual FName GetItemId_Implementation() const { return NAME_None; }
};
