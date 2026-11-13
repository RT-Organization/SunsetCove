// InteractionComponent.h
// SunsetCove - Interaction System. "Look at something and press E".
//
// WHAT THIS CLASS IS
//   The one place that answers "what is the player aiming at, and what happens when the Interact action is pressed?".
//   It is generic: it knows nothing about props, inventories, the barrier or the bed. Things plug into it in two ways:
//
//     1. An actor implements IInteractableInterface (Public/Interfaces/). Interact is called on it. Use this for the
//        barrier button, the laptop, the bed.
//     2. A system registers a HANDLER (RegisterInteractionHandler). The handler says which actors it can deal with
//        (CanHandle) and deals with them (Handle). The Prop Inventory uses this to grab props, so a prop does not have
//        to implement the interactable interface and this class does not have to know what a prop is.
//
//   The interface is tried first, then the handlers in registration order. The first one that handles it wins.
//
// FOCUS
//   While the owner is locally controlled, a trace from the camera runs every FocusUpdateInterval seconds. The first
//   thing the trace hits becomes the focus if it is interactable (blocked by a wall means no focus). OnFocusChanged
//   tells listeners; GetFocusedActor and GetFocusedPrompt answer on demand.
//
// PROMPTS ("E  Pick up Sunbed", "[RT] Place (Hold)")
//   After every focus update the component builds the list of what the player can do now (FInteractionPrompt, see
//   InteractionTypes.h) and fires OnPromptsChanged when it differs from the last one. Two sources:
//     * the FOCUS: the interface's GetInteractionPrompt, or the Prompt delegate of the handler that accepts the focus
//       ("Pick up" + the item name). They use the Interact action;
//     * CONTEXT providers (RegisterPromptProvider): what the hands can do whatever is aimed at (place / throw / drop a prop,
//       use a tool), each with its own Input Action.
//   A prompt names an Input Action, never a key: the HUD (UHUDInteractionPromptWidget) shows the key of that action on the
//   device in use, remaps included, and switches when the player changes device. This class includes no UI.
//
// HIERARCHY AND OWNER
//   UActorComponent -> UInteractionComponent, created by ASunsetCoveCharacter (the composition root of the project),
//   which also calls SetupInput. The owner must be a pawn: the view point and the "locally controlled" test come from it.
//
// INPUT
//   The component owns the Interact action (InteractAction, default key E in the project mapping context) and binds it
//   in SetupInput. Nothing in the CoreFramework plugin is involved: the framework simply has no interact action, so the
//   project binds its own, as the framework documents.
//
// NETWORK
//   Single player game: everything runs locally. No replication, no RPC.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractionSystem/InteractionTypes.h"
#include "InteractionComponent.generated.h"

class UEnhancedInputComponent;
class UInputAction;

/** The actor the player aims at (and can interact with) changed. Either actor can be null. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteractionFocusChangedSignature, AActor*, NewFocus, AActor*, OldFocus);

/** The player pressed Interact on Target. bHandled is false if nothing could deal with it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteractionPerformedSignature, AActor*, Target, bool, bHandled);

/** The prompt list changed (what the player can do now). Prompts is sorted and may be empty (hide the prompt). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionPromptsChangedSignature, const TArray<FInteractionPrompt>&, Prompts);

UCLASS(ClassGroup = (SunsetCove), meta = (BlueprintSpawnableComponent))
class SUNSETCOVE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	//~ Begin UActorComponent Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// ------------------------------------------------------------------
	// Configuration (BP_Character > Components > Interaction)
	// ------------------------------------------------------------------

	/** The Interact input action (Digital / bool, no triggers). Mapped to E in the SunsetCove mapping context. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Input")
	TObjectPtr<UInputAction> InteractAction;

	/** How far from the camera the player can interact (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float InteractionRange = 250.0f;

	/** Radius of the trace sphere (cm). 0 = a thin line. A little radius makes small things easier to aim at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 12.0f;

	/** Trace channel of the interaction trace. Interactable actors must BLOCK it (static meshes do by default on Visibility). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	/** Seconds between two focus traces. 0 = every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Trace", meta = (ClampMin = "0.0", Units = "s"))
	float FocusUpdateInterval = 0.05f;

	/** Draws the trace and the focus in the world. For tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Debug")
	bool bDrawDebug = false;

	// ------------------------------------------------------------------
	// Wiring (called by the character, C++ only)
	// ------------------------------------------------------------------

	/** Binds the Interact action. Call it from the character's SetupSunsetCoveInput. */
	void SetupInput(UEnhancedInputComponent* EnhancedInput);

	/**
	 * Plugs a system into the interaction. HandlerId names it (registering the same id again replaces it). CanHandle runs while
	 * the player aims, Handle runs when Interact is pressed. See the notes at the top of this file.
	 */
	void RegisterInteractionHandler(FName HandlerId, const FInteractionCanHandleDelegate& CanHandle, const FInteractionHandleDelegate& Handle,
		const FInteractionTargetPromptDelegate& Prompt = FInteractionTargetPromptDelegate());

	/** Removes a handler. Unknown ids are ignored. */
	void UnregisterInteractionHandler(FName HandlerId);

	/**
	 * Adds a context prompt provider: what the player can do now whatever is aimed at (see PROMPTS at the top). ProviderId names
	 * it (the same id again replaces it, keeping its place).
	 */
	void RegisterPromptProvider(FName ProviderId, const FInteractionContextPromptDelegate& Provider);

	/** Removes a prompt provider. Unknown ids are ignored. */
	void UnregisterPromptProvider(FName ProviderId);

	// ------------------------------------------------------------------
	// Commands and queries
	// ------------------------------------------------------------------

	/** Does what the Interact action does: interacts with whatever the player aims at. Returns true if it was handled. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool Interact();

	/** The actor the player can interact with right now, or null. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** The prompt of the focused actor if it implements IInteractableInterface, otherwise empty. For the future prompt widget. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetFocusedPrompt() const;

	/** What the player can do now (focus + context), sorted by SortOrder. Updated with the focus. Every InputAction is set. */
	UFUNCTION(BlueprintPure, Category = "Interaction|Prompts")
	const TArray<FInteractionPrompt>& GetCurrentPrompts() const { return CurrentPrompts; }

	/** Builds the prompt list now (instead of at the next focus update) and fires OnPromptsChanged if it changed. */
	UFUNCTION(BlueprintCallable, Category = "Interaction|Prompts")
	void RefreshPrompts();

	/** The Interact action (what a focus prompt without its own action uses). */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	UInputAction* GetInteractAction() const { return InteractAction; }

	// ------------------------------------------------------------------
	// Events
	// ------------------------------------------------------------------

	/** Fires when the focus changes. Bind the prompt widget here (show it when NewFocus is valid). */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractionFocusChangedSignature OnFocusChanged;

	/** Fires after every Interact press that had a focus. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FInteractionPerformedSignature OnInteracted;

	/** Fires when the prompt list changes. The HUD prompt binds here. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction|Prompts")
	FInteractionPromptsChangedSignature OnPromptsChanged;

private:
	/** Input callback of InteractAction. */
	void HandleInteractInput();

	/** Traces from the camera and returns the interactable actor that is hit first, or null. */
	AActor* FindInteractionTarget() const;

	/** True if Target implements the interactable interface (and is not blocked) or some handler accepts it. */
	bool IsTargetInteractable(AActor* Target) const;

	void SetFocusedActor(AActor* NewFocus);

	/** True when the owner is a pawn controlled by a player on this machine. */
	bool IsOwnerLocallyControlled() const;

	/** Registered handlers in registration order. Not a UPROPERTY: holds native delegates only. */
	TArray<TPair<FName, FInteractionHandler>> Handlers;

	/** Weak on purpose: the focus must never keep an actor alive. */
	TWeakObjectPtr<AActor> FocusedActor;

	/** Builds the prompts of the focus and of the providers, sorted. */
	void BuildPrompts(TArray<FInteractionPrompt>& OutPrompts) const;

	/** Stores Prompts and broadcasts when they differ from the current ones. */
	void SetCurrentPrompts(TArray<FInteractionPrompt>&& Prompts);

	/** Context prompt providers in registration order. Native delegates only. */
	TArray<TPair<FName, FInteractionContextPromptDelegate>> PromptProviders;

	/** Last built prompt list (what GetCurrentPrompts returns). UPROPERTY: it holds Input Action pointers. */
	UPROPERTY(Transient)
	TArray<FInteractionPrompt> CurrentPrompts;

	float TimeSinceFocusUpdate = 0.0f;
	bool bWarnedMissingAction = false;
};
