// SunsetCoveCharacter.cpp
// SunsetCove - Character area. See Public/Character/SunsetCoveCharacter.h for the role of this class.

#include "Character/SunsetCoveCharacter.h"

#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Movement/BaseCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ASunsetCoveCharacter::ASunsetCoveCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The framework constructor has already created the rig: CameraManager, FirstPersonMesh, StateMachine and the
	// framework movement component. Nothing of that is repeated here.

	// SUNSETCOVE GAME LOGIC: create the components of your game systems here (CreateDefaultSubobject), and set
	// the default values of the project (capsule size, replication settings of your own properties).
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

void ASunsetCoveCharacter::SetupSunsetCoveInput(UEnhancedInputComponent* /*EnhancedInput*/)
{
	// SUNSETCOVE GAME LOGIC: bind the project's own actions here, for example
	//   EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ASunsetCoveCharacter::Input_InteractStarted);
	// The action assets live in your own properties, and their mappings in SunsetCoveMappingContext.
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
	Super::Input_SprintStarted(Value);
	// SUNSETCOVE GAME LOGIC (after Super): react to the sprint press (start a resource drain, change the weapon pose).
}

void ASunsetCoveCharacter::Input_SprintEnded(const FInputActionValue& Value)
{
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
