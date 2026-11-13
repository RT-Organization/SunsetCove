// SunsetCoveCharacter.cpp
// SunsetCove - Character area. See Public/Character/SunsetCoveCharacter.h for the role of this class.

#include "Character/SunsetCoveCharacter.h"

#include "AnimationSystem/ArmsAnimation/ArmsAnimationComponent.h"
#include "AudioSystem/Footsteps/SunsetCoveFootstepComponent.h"
#include "Core/Settings/SunsetCovePlayerPreferences.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/RestSpots/SunsetCoveRestSpotComponent.h"
#include "InputMappingContext.h"
#include "InteractionSystem/InteractionComponent.h"
#include "InventorySystem/PropInventorySystem/PropInventoryComponent.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryComponent.h"
#include "Movement/BaseCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UISystem/HUD/SunsetCoveHUDSubsystem.h"

ASunsetCoveCharacter::ASunsetCoveCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The framework constructor has already created the rig: CameraManager, FirstPersonMesh, StateMachine and the
	// framework movement component. Nothing of that is repeated here.

	// SUNSETCOVE GAME LOGIC: create the components of your game systems here (CreateDefaultSubobject), and set
	// the default values of the project (capsule size, replication settings of your own properties).
	Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
	PropInventory = CreateDefaultSubobject<UPropInventoryComponent>(TEXT("PropInventory"));
	ToolInventory = CreateDefaultSubobject<UToolInventoryComponent>(TEXT("ToolInventory"));
	ArmsAnimation = CreateDefaultSubobject<UArmsAnimationComponent>(TEXT("ArmsAnimation"));
	Footsteps = CreateDefaultSubobject<USunsetCoveFootstepComponent>(TEXT("Footsteps"));

	// Default HUD messages of the tool refusals (rows of DT_HUDNotifications). Editable in BP_Character > Class Defaults.
	ToolRefusalNotifications.Add(EToolRefusal::HandsBusy, TEXT("Tool.Refused.HandsBusy"));
	ToolRefusalNotifications.Add(EToolRefusal::InventoryFull, TEXT("Tool.Refused.InventoryFull"));
	ToolRefusalNotifications.Add(EToolRefusal::AlreadyOwned, TEXT("Tool.Refused.AlreadyOwned"));
	ToolRefusalNotifications.Add(EToolRefusal::ToolBlocked, TEXT("Tool.Refused.ToolBlocked"));
	ToolRefusalNotifications.Add(EToolRefusal::NoTools, TEXT("Tool.Refused.NoTools"));
}

// ----------------------------------------------------------------------
// AActor
// ----------------------------------------------------------------------

void ASunsetCoveCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Project listeners next to the ones of the framework. Both receive every event.
	OnPerspectiveChanged.AddUniqueDynamic(this, &ASunsetCoveCharacter::OnSunsetCovePerspectiveChanged);

	if (UBaseCharacterMovementComponent* Movement = GetBaseMovement())
	{
		Movement->OnActionStarted.AddUObject(this, &ASunsetCoveCharacter::OnSunsetCoveMovementActionStarted);
	}

	// SUNSETCOVE GAME LOGIC: wire your components to each other here (all components exist at this point).
	if (PropInventory)
	{
		// The hand sockets (Prop_R / Prop_L) live on the first-person arms; the body mesh is the alternative.
		PropInventory->SetHandMeshes(FirstPersonMesh, GetMesh());

		// Grabbing a prop is an interaction: the Interaction System aims, the Prop Inventory does the grab.
		if (Interaction)
		{
			PropInventory->RegisterWithInteraction(Interaction);
		}

		PropInventory->OnPropAction.AddUniqueDynamic(this, &ASunsetCoveCharacter::OnPropInventoryAction);
		PropInventory->OnSlotsChanged.AddUniqueDynamic(this, &ASunsetCoveCharacter::OnPropSlotsChanged);
	}

	if (ToolInventory)
	{
		// Same hand sockets as the props: the tool hand (Tool_R, or Prop_R as a fallback) is on the first-person arms too.
		ToolInventory->SetHandMeshes(FirstPersonMesh, GetMesh());

		// Picking up a tool is an interaction, like grabbing a prop.
		if (Interaction)
		{
			ToolInventory->RegisterWithInteraction(Interaction);
		}

		// The HUD tells the player why a tool action was refused, and how to open the tool menu after the first pickup.
		ToolInventory->OnActionRefused.AddUniqueDynamic(this, &ASunsetCoveCharacter::OnToolActionRefused);
		ToolInventory->OnToolAction.AddUniqueDynamic(this, &ASunsetCoveCharacter::OnToolInventoryAction);
	}

	if (ArmsAnimation)
	{
		// The animation layer listens to both inventories (they do not know it) and animates the first-person arms.
		ArmsAnimation->SetArmsMesh(FirstPersonMesh);
		ArmsAnimation->BindToPropInventory(PropInventory);
		ArmsAnimation->BindToToolInventory(ToolInventory);
	}
}

void ASunsetCoveCharacter::BeginPlay()
{
	Super::BeginPlay();

	// SUNSETCOVE GAME LOGIC: start-up of the game systems (starting inventory, first weapon, initial values).
	// Server-only start-up belongs behind HasAuthority().
}

void ASunsetCoveCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// SUNSETCOVE GAME LOGIC: shut the game systems down here, before the framework tears the character down.

	Super::EndPlay(EndPlayReason);
}

void ASunsetCoveCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// SUNSETCOVE GAME LOGIC: per-frame work that cannot be event driven (for example an interaction trace).
	// Run it only where it is needed: IsLocallyControlled() for client-side traces, HasAuthority() for server rules.

	// Toggle sprint ends when the player lets go of the movement input (or switched the setting back to Hold).
	// A press while standing still waits for the first step instead of cancelling at once.
	if (bSprintToggledOn)
	{
		const UCharacterMovementComponent* Movement = GetCharacterMovement();
		const bool bMoving = Movement && !Movement->GetCurrentAcceleration().IsNearlyZero();
		bToggledSprintHasMoved |= bMoving;
		if ((bToggledSprintHasMoved && !bMoving) || USunsetCovePlayerPreferences::GetSprintMode() != ESunsetCoveSprintMode::Toggle)
		{
			StopToggledSprint();
		}
	}
}

void ASunsetCoveCharacter::StopToggledSprint()
{
	bSprintToggledOn = false;
	bToggledSprintHasMoved = false;
	Super::Input_SprintEnded(FInputActionValue());
}

void ASunsetCoveCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// SUNSETCOVE GAME LOGIC: register the replicated properties of this class, for example
	//   DOREPLIFETIME(ASunsetCoveCharacter, MyReplicatedProperty);
	// The perspective and the state machine tags are already replicated by the framework.
}

// ----------------------------------------------------------------------
// APawn
// ----------------------------------------------------------------------

void ASunsetCoveCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Runs on the owning client every time the pawn is possessed. The framework context is already added.
	if (!SunsetCoveMappingContext)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		// Remove first so a respawn never stacks the same context twice (same pattern as the framework).
		InputSubsystem->RemoveMappingContext(SunsetCoveMappingContext);
		InputSubsystem->AddMappingContext(SunsetCoveMappingContext, SunsetCoveMappingPriority);
	}
}

void ASunsetCoveCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server (and listen-server host) only. A controller took this character.
	// SUNSETCOVE GAME LOGIC: per-player setup that depends on the controller or the player state.
}

void ASunsetCoveCharacter::UnPossessed()
{
	// Server only. The controller is about to leave this character.
	// SUNSETCOVE GAME LOGIC: per-player cleanup, before the engine clears the controller.
	if (PropInventory)
	{
		// A placement or a throw charge cannot outlive the player's control (the release input would never arrive).
		PropInventory->CancelCurrentAction();
	}
	if (ToolInventory)
	{
		// Same for the open radial menu and a tool in use.
		ToolInventory->CancelCurrentAction();
	}

	Super::UnPossessed();
}

void ASunsetCoveCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// The framework binds every action of its InputConfig here.
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		SetupSunsetCoveInput(EnhancedInput);
	}
}

// ----------------------------------------------------------------------
// Project hooks
// ----------------------------------------------------------------------

void ASunsetCoveCharacter::SetupSunsetCoveInput(UEnhancedInputComponent* EnhancedInput)
{
	// SUNSETCOVE GAME LOGIC: bind the project's own actions here, for example
	//   EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ASunsetCoveCharacter::Input_InteractStarted);
	// The action assets live in your own properties, and their mappings in SunsetCoveMappingContext.
	//
	// The CoreFramework has no handler for these actions, so the project binds them itself, through its own components.
	// Each component owns its Input Action properties; the mappings (E, left / right mouse, wheel, Q) are in the
	// SunsetCoveMappingContext.
	if (Interaction)
	{
		Interaction->SetupInput(EnhancedInput);         // Interact (E)
	}
	if (PropInventory)
	{
		PropInventory->SetupInput(EnhancedInput);       // Place (hold), Throw (hold), Rotate (wheel), Drop (Q)
	}
	if (ToolInventory)
	{
		ToolInventory->SetupInput(EnhancedInput);       // Tool menu (hold Tab), Use tool (left mouse), Drop (Q, shared with props)
	}
}

void ASunsetCoveCharacter::OnSunsetCovePerspectiveChanged(ECharacterPerspective /*NewPerspective*/, ECharacterPerspective /*OldPerspective*/)
{
	// SUNSETCOVE GAME LOGIC: react to the view change (hide first-person-only items, switch the weapon view, audio).
}

void ASunsetCoveCharacter::OnSunsetCoveMovementActionStarted(EMovementAction /*Action*/, EMovementDomain /*Domain*/, float /*Duration*/)
{
	// SUNSETCOVE GAME LOGIC: react to Dash and Dodge (cancel an interaction, spend a resource, play a game effect).
	// The state machine already received the action from the framework.
}

void ASunsetCoveCharacter::OnPropInventoryAction(EPropAction Action, EPropHand /*Hand*/, AActor* Prop)
{
	// Only these two change what a visitor may use. A dropped or thrown prop keeps its spots disabled until it is placed.
	if (!Prop || (Action != EPropAction::Grab && Action != EPropAction::Place))
	{
		return;
	}

	const bool bEnableSpots = (Action == EPropAction::Place);

	TInlineComponentArray<USunsetCoveRestSpotComponent*> RestSpots(Prop);
	for (USunsetCoveRestSpotComponent* RestSpot : RestSpots)
	{
		if (RestSpot)
		{
			RestSpot->SetSpotEnabled(bEnableSpots);
		}
	}
}

void ASunsetCoveCharacter::OnPropSlotsChanged()
{
	// The two inventories do not know each other: this class is the only place that connects them.
	if (ToolInventory && PropInventory)
	{
		ToolInventory->SetHandsBusy(PropInventory->IsHoldingAnyProp());
	}
}

void ASunsetCoveCharacter::OnToolActionRefused(EToolRefusal Reason, ASunsetCoveTool* /*Tool*/)
{
	// The HUD only exists for the local player; the inventories never know the HUD (the subsystem is the message hub).
	if (!IsLocallyControlled())
	{
		return;
	}

	// While a prop is being placed, LB / RB rotate it and RB is also the tool wheel key: the wheel is refused (hands busy)
	// on every rotation step, which is expected, so no message then.
	if (Reason == EToolRefusal::HandsBusy && PropInventory && PropInventory->GetState() == EPropInventoryState::Placing)
	{
		return;
	}

	const FName* NotificationId = ToolRefusalNotifications.Find(Reason);
	USunsetCoveHUDSubsystem* HUD = USunsetCoveHUDSubsystem::Get(this);
	if (NotificationId && HUD)
	{
		HUD->ShowNotificationById(*NotificationId);
	}
}

void ASunsetCoveCharacter::OnToolInventoryAction(EToolAction Action, int32 /*SlotIndex*/, ASunsetCoveTool* /*Tool*/)
{
	if (Action != EToolAction::Pickup || ToolPickupTipNotification.IsNone() || !IsLocallyControlled())
	{
		return;
	}

	if (USunsetCoveHUDSubsystem* HUD = USunsetCoveHUDSubsystem::Get(this))
	{
		HUD->ShowNotificationById(ToolPickupTipNotification);
	}
}

// ----------------------------------------------------------------------
// Framework input handlers
// ----------------------------------------------------------------------

void ASunsetCoveCharacter::Input_Move(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto movement (stunned, in a cutscene) by returning here.
	Super::Input_Move(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to movement input.
}

void ASunsetCoveCharacter::Input_Look(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto or scale look input (for example while aiming).
	// While the tool radial menu is open the look input picks a sector of the wheel instead of turning the camera.
	if (ToolInventory && ToolInventory->IsRadialMenuOpen())
	{
		// Controller: the wheel follows the raw right stick direction (the look value is scaled per frame and per axis,
		// which makes a cursor slow and skews diagonals). Mouse: the look delta moves a cursor on the wheel.
		const APlayerController* PlayerController = Cast<APlayerController>(GetController());
		const FVector2D Stick = PlayerController
			? FVector2D(PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightX), PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightY))
			: FVector2D::ZeroVector;
		if (!Stick.IsNearlyZero(0.05))
		{
			ToolInventory->SetRadialStickDirection(Stick);
		}
		else
		{
			ToolInventory->AddRadialInput(Value.Get<FVector2D>());
		}
		return;
	}

	Super::Input_Look(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to look input.
}

void ASunsetCoveCharacter::Input_JumpStarted(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto the jump, or cancel an interaction that the jump interrupts.
	Super::Input_JumpStarted(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the jump press.
}

void ASunsetCoveCharacter::Input_JumpHeld(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto the held jump (it drives ascent while flying or swimming).
	Super::Input_JumpHeld(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the held jump.
}

void ASunsetCoveCharacter::Input_JumpEnded(const FInputActionValue& Value)
{
	// Always call Super for a release (see the note in the header).
	Super::Input_JumpEnded(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the jump release.
}

void ASunsetCoveCharacter::Input_CrouchStarted(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto crouching (for example under a low ceiling rule of your game).
	Super::Input_CrouchStarted(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the crouch press.
}

void ASunsetCoveCharacter::Input_CrouchHeld(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto the held crouch (it drives descent while flying or swimming).
	Super::Input_CrouchHeld(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the held crouch.
}

void ASunsetCoveCharacter::Input_CrouchEnded(const FInputActionValue& Value)
{
	// Always call Super for a release (see the note in the header).
	Super::Input_CrouchEnded(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the crouch release.
}

void ASunsetCoveCharacter::Input_SprintStarted(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto sprinting (exhausted, carrying something heavy, aiming).

	// Toggle mode: each press flips sprinting on / off; the release is ignored (see Input_SprintEnded).
	if (USunsetCovePlayerPreferences::GetSprintMode() == ESunsetCoveSprintMode::Toggle)
	{
		if (bSprintToggledOn)
		{
			StopToggledSprint();
			return;
		}
		bSprintToggledOn = true;
	}

	Super::Input_SprintStarted(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the sprint press (start a resource drain, change the weapon pose).
}

void ASunsetCoveCharacter::Input_SprintEnded(const FInputActionValue& Value)
{
	// Toggle mode: releasing the key does not stop a toggled sprint (the next press or stopping does).
	if (bSprintToggledOn)
	{
		return;
	}

	// Always call Super for a release (see the note in the header).
	Super::Input_SprintEnded(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the sprint release (stop the resource drain).
}

void ASunsetCoveCharacter::Input_Fly(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto toggling flight.
	Super::Input_Fly(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the flight toggle.
}

void ASunsetCoveCharacter::Input_Dash(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto the dash (cooldown, resource cost check).
	Super::Input_Dash(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the dash request.
}

void ASunsetCoveCharacter::Input_Dodge(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto the dodge (cooldown, resource cost check).
	Super::Input_Dodge(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the dodge request.
}

void ASunsetCoveCharacter::Input_PerspectiveToggle(const FInputActionValue& Value)
{
	// SUNSETCOVE GAME LOGIC (before Super): veto the view change (for example while a weapon is drawn).
	Super::Input_PerspectiveToggle(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the toggle request.
}

// ----------------------------------------------------------------------
// State machine
// ----------------------------------------------------------------------

void ASunsetCoveCharacter::UpdateStateMachine()
{
	Super::UpdateStateMachine();

	// SUNSETCOVE GAME LOGIC (after Super): push project state into GetStateMachine().
	// The framework returns early on simulated proxies because they mirror the server through replication.
	// Do the same here: skip work when GetLocalRole() == ROLE_SimulatedProxy.
}
