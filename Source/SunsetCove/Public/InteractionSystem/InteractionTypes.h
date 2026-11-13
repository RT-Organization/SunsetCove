// InteractionTypes.h
// SunsetCove - Interaction System. The delegate types a system uses to plug into the interaction component, and the prompt data.
//
// Plain C++ delegates (not Blueprint delegates): handlers and prompt providers are registered by C++ systems, for example the
// Prop Inventory and the Tool Inventory.
//
// PROMPTS IN ONE PARAGRAPH
//   A prompt is "what the player can do now, and with which input": an action text ("Pick up", "Place") and the Input Action
//   that does it. No object name (the player is already looking at the object). It names the ACTION, never a key: the HUD asks
//   the CoreFramework input system for the key of that action on the device the player is using (keyboard / mouse or gamepad,
//   remaps included), so the same prompt shows "[E] Pick up" or "[X] Pick up" and switches by itself. Gameplay code never knows
//   keys or widgets.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "InteractionTypes.generated.h"

class AActor;
class UInputAction;

/** How the input of a prompt is used. The HUD words it (Class Defaults of the prompt widget). */
UENUM(BlueprintType)
enum class EInteractionPromptTrigger : uint8
{
	/** Press once ("Pick up"). */
	Press	UMETA(DisplayName = "Press"),
	/** Hold the input; the action happens on release or while held ("Place", "Throw"). */
	Hold	UMETA(DisplayName = "Hold"),
	/** The input is held now; letting go does the action ("Place" while the hologram shows). */
	Release	UMETA(DisplayName = "Release")
};

/** One line of the interaction prompt: an action and the input that does it. */
USTRUCT(BlueprintType)
struct SUNSETCOVE_API FInteractionPrompt
{
	GENERATED_BODY()

	/** The verb: "Pick up", "Open", "Place". Localized text set by the system that gives the prompt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt")
	FText ActionText;

	/** The Enhanced Input action whose key is shown. Null: the Interact action of the interaction component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt")
	TObjectPtr<UInputAction> InputAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt")
	EInteractionPromptTrigger Trigger = EInteractionPromptTrigger::Press;

	/** False: the action exists but cannot be done right now (the hologram is on an invalid spot). Shown dimmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt")
	bool bAvailable = true;

	/** Lower first. Prompts about the aimed object use 0, actions on the hand use 100+. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction Prompt")
	int32 SortOrder = 0;

	FInteractionPrompt() = default;
	FInteractionPrompt(const FText& InAction, UInputAction* InInputAction, EInteractionPromptTrigger InTrigger = EInteractionPromptTrigger::Press, int32 InSortOrder = 0, bool bInAvailable = true)
		: ActionText(InAction), InputAction(InInputAction), Trigger(InTrigger), bAvailable(bInAvailable), SortOrder(InSortOrder)
	{
	}

	/** Same content (texts compared by value). Used to broadcast only real changes. */
	bool IsSameAs(const FInteractionPrompt& Other) const
	{
		return InputAction == Other.InputAction && Trigger == Other.Trigger && bAvailable == Other.bAvailable && SortOrder == Other.SortOrder
			&& ActionText.EqualTo(Other.ActionText);
	}
};

/** Can this handler interact with Target? Called while the player aims, so keep it cheap and free of side effects. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractionCanHandleDelegate, AActor* /*Target*/);

/** Interact with Target. Return true if the interaction was done (false lets the next handler try). */
DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractionHandleDelegate, AActor* /*Target*/);

/**
 * Optional third delegate of a handler: the prompt(s) for Target, which the handler accepted (CanHandle was true). Append to
 * OutPrompts; InputAction may be left null (the Interact action). Called while the player aims: cheap, no side effects.
 */
DECLARE_DELEGATE_TwoParams(FInteractionTargetPromptDelegate, AActor* /*Target*/, TArray<FInteractionPrompt>& /*OutPrompts*/);

/**
 * A context prompt provider: what the player can do now WHATEVER is aimed at (place / throw / drop the prop in the hand, use the
 * tool). Append to OutPrompts. Called a few times per second: cheap, no side effects.
 */
DECLARE_DELEGATE_OneParam(FInteractionContextPromptDelegate, TArray<FInteractionPrompt>& /*OutPrompts*/);

/** The delegates of one registered handler. */
struct FInteractionHandler
{
	FInteractionCanHandleDelegate CanHandle;
	FInteractionHandleDelegate Handle;
	FInteractionTargetPromptDelegate Prompt;
};
