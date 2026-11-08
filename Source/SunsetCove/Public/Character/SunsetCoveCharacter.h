// SunsetCoveCharacter.h
// SunsetCove - Character area. The playable character of the game.
//
// HIERARCHY
//   ACharacter -> ABaseCharacter (CoreFramework) -> ASunsetCoveCharacter (this class) -> BP_SunsetCoveCharacter (Blueprint)
//
// THE FRAMEWORK ALREADY DOES THIS, SO NOTHING HERE REPEATS IT
//   Enhanced Input routing (InputConfig data asset), movement domains and predicted actions, the runtime camera
//   rig (CameraManager), perspective (first / third person) and the StateMachine tag driver.
//   See Docs/Character/Character_Architecture_Overview.md and Docs/Character/Character_Blueprint_Setup.md.
//
// WHAT THIS CLASS IS FOR
//   Game logic only. Every place meant for it is marked "SUNSETCOVE GAME LOGIC".
//   Keep this class thin. Game systems (health, inventory, weapons, interaction traces) are built as
//   components in their own area folders (for example Public/Gameplay/Health/) and this class only creates them
//   and forwards events to them.
//
// HOW THE INPUT OVERRIDES WORK
//   Each Input_* function below overrides the framework handler. Code placed BEFORE the Super call can veto the
//   input (return without calling Super). Code placed AFTER the Super call reacts to what the framework did.
//   Always call Super for the "Ended" handlers (JumpEnded, CrouchEnded, SprintEnded): a released key must never
//   be lost, or the movement component keeps a button logically held.
//
// NETWORK ROLES
//   Input handlers run on the owning client only. UpdateStateMachine runs on the server and the owning client.
//   The perspective and movement hooks run on every machine that learns about the event.

#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "InventorySystem/PropInventorySystem/PropInventoryTypes.h"
#include "SunsetCoveCharacter.generated.h"

class UEnhancedInputComponent;
class UInputMappingContext;
class UInteractionComponent;
class UArmsAnimationComponent;
class UPropInventoryComponent;
class UToolInventoryComponent;
enum class EMovementAction : uint8;
enum class EMovementDomain : uint8;

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCoveCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	/** Passes the initializer to the framework, which replaces the movement component class. Do not drop the parameter. */
	ASunsetCoveCharacter(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	//~ End APawn Interface

	// ------------------------------------------------------------------
	// Configuration (set in BP_SunsetCoveCharacter, Class Defaults)
	// ------------------------------------------------------------------

	/**
	 * Optional mapping context with the project's own actions (interact, fire, ...).
	 * It is added after the framework context, on the owning client, every time the pawn is possessed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Input")
	TObjectPtr<UInputMappingContext> SunsetCoveMappingContext;

	/** Priority of SunsetCoveMappingContext. Higher than the framework context (0) so a project mapping wins a key conflict. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Input")
	int32 SunsetCoveMappingPriority = 1;

	// SUNSETCOVE GAME LOGIC: add the configuration of your game here (default values, class references).
	// Add the components of your game systems here as well, created in the constructor, for example:
	//   Health       -> USunsetCoveHealthComponent       (Public/Gameplay/Health/)
	//   Inventory    -> USunsetCoveInventoryComponent    (Public/Gameplay/Inventory/)
	//   Weapons      -> USunsetCoveWeaponComponent       (Public/Gameplay/Weapons/)
	//   Interaction  -> USunsetCoveInteractionComponent  (Public/Gameplay/Interaction/)

	// ------------------------------------------------------------------
	// Game systems (components of this character; tune them in BP_Character > Components)
	// ------------------------------------------------------------------

	/**
	 * Aims from the camera and handles the Interact action (Public/InteractionSystem/). Grabbing props goes through it:
	 * the Prop Inventory registers itself as an interaction handler (see PostInitializeComponents).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Components")
	TObjectPtr<UInteractionComponent> Interaction;

	/** The two hands of the player: carry, place, throw and drop props (Public/InventorySystem/PropInventorySystem/). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Components")
	TObjectPtr<UPropInventoryComponent> PropInventory;

	/**
	 * The tools of the player: pick up, choose from the radial menu, use, drop (Public/InventorySystem/ToolInventorySystem/).
	 * It never talks to the Prop Inventory: this class tells it when the hands hold props (see OnPropSlotsChanged).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Components")
	TObjectPtr<UToolInventoryComponent> ToolInventory;

	/**
	 * The animation of the arms with props and tools (Public/AnimationSystem/ArmsAnimation/). It listens to both inventories, which do not
	 * know it exists, and plays the animation of the item in the hand through the Arms Animation Set data asset.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunsetCove|Components")
	TObjectPtr<UArmsAnimationComponent> ArmsAnimation;

protected:
	// ------------------------------------------------------------------
	// Project hooks (new virtual functions of this class)
	// ------------------------------------------------------------------

	/**
	 * Binds the project's own actions. Called from SetupPlayerInputComponent after the framework has bound its
	 * actions, only when the input component is an Enhanced Input component.
	 */
	virtual void SetupSunsetCoveInput(UEnhancedInputComponent* EnhancedInput);

	/** The perspective changed (first / third person). Runs on every machine that learns about the change. */
	UFUNCTION()
	virtual void OnSunsetCovePerspectiveChanged(ECharacterPerspective NewPerspective, ECharacterPerspective OldPerspective);

	/** Dash or Dodge actually started. Runs on every machine that runs the movement, not while saved moves replay. */
	virtual void OnSunsetCoveMovementActionStarted(EMovementAction Action, EMovementDomain Domain, float Duration);

	/**
	 * An action of the Prop Inventory took effect (bound in PostInitializeComponents).
	 * Rest spots: a prop in the hands is not a place to rest, so Grab disables the rest spots of the prop (a visitor that
	 * had claimed one goes elsewhere) and Place enables them again. A dropped or thrown prop stays disabled until it is placed.
	 */
	UFUNCTION()
	virtual void OnPropInventoryAction(EPropAction Action, EPropHand Hand, AActor* Prop);

	/**
	 * A hand of the Prop Inventory was filled or emptied (bound in PostInitializeComponents). GDD: tools cannot be used while
	 * props are in hand, so the Tool Inventory is told whether the hands are busy.
	 */
	UFUNCTION()
	virtual void OnPropSlotsChanged();

	// ------------------------------------------------------------------
	// Framework input handlers (overrides, see the note at the top of this file)
	// ------------------------------------------------------------------

	virtual void Input_Move(const FInputActionValue& Value) override;
	virtual void Input_Look(const FInputActionValue& Value) override;
	virtual void Input_JumpStarted(const FInputActionValue& Value) override;
	virtual void Input_JumpHeld(const FInputActionValue& Value) override;
	virtual void Input_JumpEnded(const FInputActionValue& Value) override;
	virtual void Input_CrouchStarted(const FInputActionValue& Value) override;
	virtual void Input_CrouchHeld(const FInputActionValue& Value) override;
	virtual void Input_CrouchEnded(const FInputActionValue& Value) override;
	virtual void Input_SprintStarted(const FInputActionValue& Value) override;
	virtual void Input_SprintEnded(const FInputActionValue& Value) override;
	virtual void Input_Fly(const FInputActionValue& Value) override;
	virtual void Input_Dash(const FInputActionValue& Value) override;
	virtual void Input_Dodge(const FInputActionValue& Value) override;
	virtual void Input_PerspectiveToggle(const FInputActionValue& Value) override;

	/** Pushes domain and locomotion state into the state machine. Runs on the server and the owning client. */
	virtual void UpdateStateMachine() override;
};
