// SunsetCovePlayerController.h
// SunsetCove - Player area. The PlayerController of the gameplay maps.
//
// WHAT A PLAYERCONTROLLER IS, IN PLAIN WORDS
//   The "person at the keyboard". The pawn (the character) is the body; the controller is the person who drives it.
//   The body can die and be replaced; the controller stays. So the controller is the right owner of everything
//   that must keep working when there is no body: pause, menu keys, scoreboard, chat.
//   Input that is about the body (move, look, jump...) stays in the character (ABaseCharacter and its InputConfig).
//
// WHAT IT NEEDS (and nothing more)
//   1. A mapping context with its own actions (here: Pause).
//   2. A link to the UI, through UUIFrameSubsystem.
//
// WHERE IT EXISTS
//   Server (one per player) and the owning client. Input is only set up on the machine that owns the player.
//
// HIERARCHY
//   APlayerController -> ASunsetCovePlayerController (this class) -> BP_PlayerController (Blueprint)
//   CoreFramework has no PlayerController base class on purpose: a project has one controller class, so a framework
//   base would block other frameworks, and the part that every game repeats is only the pause toggle below.
//
// HOW THE PAUSE KEY WORKS
//   Escape -> IA_Pause (Started) -> HandlePauseAction -> TogglePauseScreen
//     -> UUIFrameSubsystem::GetFromController(this) -> PushScreen / PopScreen(PauseScreenTag)
//   The pause SCREEN decides what pausing means (Input Mode, Pause Game While Active, Is Back Handler in its Class
//   Defaults). This class never calls SetInputMode and never pauses the game itself.
//   While a UI Only screen is open (the pause menu), game input is blocked, so this action does not fire again; the
//   screen closes itself through its Back action. If a pause screen uses "Game And UI", tick "Trigger When Paused"
//   on IA_Pause so the toggle also works while the game is paused.
//
// SETUP (BP_PlayerController, Class Defaults)
//   Pause Action           = IA_Pause (Input Action, Digital bool)
//   Player Mapping Context = IMC_Player (maps IA_Pause to Escape and Gamepad Special Right)
//   Pause Screen Tag       = UI.Screen.PauseMenu
//   Then set BP_PlayerController as "Player Controller Class" in BP_GameMode_Gameplay.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "SunsetCovePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS(Blueprintable)
class SUNSETCOVE_API ASunsetCovePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Opens the pause screen, or closes it when it is already open. Callable from Blueprint (for example a UI button). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|UI")
	void TogglePauseScreen();

protected:
	//~ Begin APlayerController Interface
	virtual void SetupInputComponent() override;
	//~ End APlayerController Interface

	/** Bound to PauseAction. */
	void HandlePauseAction(const FInputActionValue& Value);

	/** Mapping context added for the local player. It holds the controller-level actions (pause). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	/** Priority of PlayerMappingContext. The character's own contexts have their priority in their own settings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Input")
	int32 MappingPriority = 0;

	/** Digital (bool) action that toggles the pause screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|Input")
	TObjectPtr<UInputAction> PauseAction;

	/** Gameplay tag of the pause screen, as registered in DA_UIConfig (Screens). Set in the Blueprint, never hardcoded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunsetCove|UI")
	FGameplayTag PauseScreenTag;

	// SUNSETCOVE GAME LOGIC: add the other controller-level actions of SunsetCove below (scoreboard, chat, map...).
	// A key that opens a screen follows the same pattern as the pause key: action -> handler -> PushScreen(Tag).
};
