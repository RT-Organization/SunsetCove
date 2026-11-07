// ToolTargetInterface.h
// SunsetCove - Interfaces area. Marks an actor as something a tool can be used ON: a crab, a smelly visitor, a screaming
// child, a piece of litter, a vandal.
//
// WHAT THIS IS
//   The counterpart of the tools. A tool (ASunsetCoveTool, Public/Gameplay/Tools/) aims from the player's view when it is
//   used. If the actor it hits implements this interface, the tool asks it whether the use is accepted (IsToolUseBlocked)
//   and then tells it that the tool was used on it (ReceiveToolUse). The target decides what that means: the crab asks
//   "is this the bucket?" and goes into it, the child asks "is this the toy?" and calms down, and so on.
//
//   The tool never knows what the target is, and the target never knows how the tool was selected or carried. Nothing of
//   the Tool Inventory appears here (no slots, no hands): the dependency is one way, like the Prop Interface.
//
// HOW AN ACTOR IMPLEMENTS IT
//   Blueprint:  Class Settings > Interfaces > Add > "Tool Target Interface". Every function is optional.
//   C++:        class AMyTarget : public AActor, public IToolTargetInterface { ... } and override the _Implementation functions.
//
// WHY "BLOCKED" AND NOT "CAN"
//   A Blueprint that adds an interface but implements no function returns the default of the type (false). The question is
//   phrased so that false means "no objection": an actor that implements nothing accepts every tool, and ReceiveToolUse
//   then simply does nothing.
//
// CALLING IT FROM C++
//   if (Actor->Implements<UToolTargetInterface>()) { IToolTargetInterface::Execute_ReceiveToolUse(Actor, Tool, User, Hit); }
//
// WHICH TOOL IS IT
//   Read Tool->ToolId (FName, the row name of the tools table). Compare it, for example: Tool->ToolId == "Bucket".

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "ToolTargetInterface.generated.h"

class ASunsetCoveTool;

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UToolTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class SUNSETCOVE_API IToolTargetInterface
{
	GENERATED_BODY()

public:
	/**
	 * Return true to refuse this use: wrong tool for this target, the target is not in a state to be affected (the crab
	 * is already gone, the child is already calm). A refused use does not reach ReceiveToolUse and the tool treats it as
	 * "no target". Default: false.
	 * @param Tool  The tool that is being used (read its ToolId).
	 * @param User  The actor that holds the tool (the player's pawn).
	 * @param Hit   What the use trace hit on this target.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Tool")
	bool IsToolUseBlocked(ASunsetCoveTool* Tool, AActor* User, const FHitResult& Hit) const;
	virtual bool IsToolUseBlocked_Implementation(ASunsetCoveTool* Tool, AActor* User, const FHitResult& Hit) const { return false; }

	/**
	 * The tool was used on this actor. Do the effect here (collect the crab, calm the child, clean the litter).
	 * Return true if something happened. Default: does nothing and returns false.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Tool")
	bool ReceiveToolUse(ASunsetCoveTool* Tool, AActor* User, const FHitResult& Hit);
	virtual bool ReceiveToolUse_Implementation(ASunsetCoveTool* Tool, AActor* User, const FHitResult& Hit) { return false; }
};
