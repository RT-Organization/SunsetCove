// SunsetCovePlayerController.cpp
// SunsetCove - Player area. See Public/Player/SunsetCovePlayerController.h for the role of this class.

#include "Player/SunsetCovePlayerController.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "UISystem/Manager/UIFrameSubsystem.h"

void ASunsetCovePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Only the machine that owns this player has an input component worth configuring.
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (PlayerMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(PlayerMappingContext, MappingPriority);
		}
	}
	else
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: Player Mapping Context is not set (Class Defaults > SunsetCove|Input). The pause key has no mapping."), *GetNameSafe(this));
	}

	if (PauseAction)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &ASunsetCovePlayerController::HandlePauseAction);
		}
		else
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("%s: the input component is not an Enhanced Input component. Check Project Settings > Input > Default Input Component Class."), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: Pause Action is not set (Class Defaults > SunsetCove|Input). Use BP_PlayerController, not this C++ class directly."), *GetNameSafe(this));
	}

	// SUNSETCOVE GAME LOGIC: bind the other controller-level actions of SunsetCove here.
}

void ASunsetCovePlayerController::HandlePauseAction(const FInputActionValue& /*Value*/)
{
	TogglePauseScreen();
}

void ASunsetCovePlayerController::TogglePauseScreen()
{
	if (!PauseScreenTag.IsValid())
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: Pause Screen Tag is not set (Class Defaults > SunsetCove|UI)."), *GetNameSafe(this));
		return;
	}

	// The UI subsystem belongs to the local player. It does not exist on a dedicated server, so this is null there.
	UUIFrameSubsystem* UI = UUIFrameSubsystem::GetFromController(this);
	if (!UI)
	{
		UE_LOG(LogSunsetCove, Warning, TEXT("%s: no UI subsystem for this controller (no local player)."), *GetNameSafe(this));
		return;
	}

	if (UI->IsScreenOpen(PauseScreenTag))
	{
		UI->PopScreen(PauseScreenTag);
	}
	else
	{
		UI->PushScreen(PauseScreenTag);
	}
}
