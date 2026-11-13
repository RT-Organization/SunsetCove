// InteractableInterface.h
// SunsetCove - Interfaces area. Marks an actor the player can use with the Interact action (default key E).
//
// WHAT THIS IS
//   The contract between the Interaction System (Public/InteractionSystem/) and the things it can interact with: the
//   barrier button, the laptop in the cave, the bed. An actor that implements this interface shows up as the focus of
//   the interaction trace, and Interact is called when the player presses the action.
//
//   Props (sunbeds, umbrellas...) do NOT need this interface to be grabbed: the Prop Inventory plugs into the
//   Interaction System as a handler (see UInteractionComponent::RegisterInteractionHandler). Use this interface for
//   everything that is not a carriable prop.
//
//   Like IPropInterface, it lives outside the systems that use it and knows nothing about them.
//
// HOW AN ACTOR IMPLEMENTS IT
//   Blueprint:  Class Settings > Interfaces > Add > "Interactable Interface", then implement "Interact" (event).
//   C++:        class AMyActor : public AActor, public IInteractableInterface { ... } and override _Implementation.
//
// WHY "BLOCKED" AND NOT "CAN"
//   A Blueprint that adds an interface but does not implement a function that returns a value returns false. The
//   check is phrased so that the default (false) means "allowed": an actor that only implements Interact works.
//
// CALLING IT FROM C++ (always through Execute_, it works for Blueprint and C++ implementers)
//     if (Actor->Implements<UInteractableInterface>()) { IInteractableInterface::Execute_Interact(Actor, Interactor); }

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class SUNSETCOVE_API IInteractableInterface
{
	GENERATED_BODY()

public:
	/** Return true to refuse the interaction right now (locked, already used, wrong time of day). A blocked actor is not focused. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Interaction")
	bool IsInteractionBlocked(AActor* Interactor) const;
	virtual bool IsInteractionBlocked_Implementation(AActor* Interactor) const { return false; }

	/** The player pressed the Interact action while this actor was focused. Interactor is the player's pawn. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor) {}

	/** Action text of the HUD interaction prompt ("Open the barrier", "Sleep"), shown with the key of the Interact action. Empty = no prompt. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SunsetCove|Interaction")
	FText GetInteractionPrompt() const;
	virtual FText GetInteractionPrompt_Implementation() const { return FText::GetEmpty(); }
};
