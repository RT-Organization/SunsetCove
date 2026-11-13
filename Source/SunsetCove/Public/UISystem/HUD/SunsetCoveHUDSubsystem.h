// SunsetCoveHUDSubsystem.h
// SunsetCove - UI System / HUD. The message hub between the game systems and the HUD: notifications and crosshair requests.
//
// WHAT THIS CLASS IS
//   A Game Instance Subsystem: it exists once per game session, is reachable from any actor, component or Blueprint
//   (Blueprint node "Get SunsetCoveHUDSubsystem") and never knows a widget. Game systems REQUEST things here; the HUD elements
//   LISTEN here. So a gameplay system never includes a widget header, and a widget never includes a gameplay system to receive
//   messages.
//
//     game system ──ShowNotificationById("Tool.Refused.HandsBusy")──► this subsystem ──OnNotificationRequested──► WBP_NotificationFeed
//     police event ──SetCrosshairHiddenBy("PoliceEvent", true)─────► this subsystem ──IsCrosshairHiddenByRequest──► WBP_Crosshair
//
// NOTIFICATIONS (the top-left suggestion feed)
//   ShowNotificationById(Id)   looks the id up in the notification table (Project Settings > Game > Sunset Cove UI) and sends
//                              its text. A row flagged "show once per session" is sent only the first time. A missing row
//                              logs one warning and shows nothing (the game never breaks on a missing message).
//   ShowNotification(Text)     sends runtime text directly (names, numbers).
//   A message asked for before the HUD exists (the first frame of a level, the HUD loads asynchronously) is kept in a small
//   queue and handed to the feed when it binds (TakePendingNotifications), so nothing is lost at start-up.
//
// CROSSHAIR REQUESTS
//   Several systems may want the crosshair hidden at the same time (a cutscene, an event, a full-screen tool). Each one calls
//   SetCrosshairHiddenBy(Reason, true) with its own reason name and (Reason, false) when done. The crosshair is hidden while
//   at least one reason is active. The crosshair widget also hides by itself while the tool menu is open or a menu shows the
//   mouse cursor (its own options), so callers do not need to handle those cases.
//
// NETWORK
//   Single player game: local only.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UISystem/HUD/HUDNotificationTypes.h"
#include "SunsetCoveHUDSubsystem.generated.h"

class UDataTable;

UCLASS()
class SUNSETCOVE_API USunsetCoveHUDSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the game instance of WorldContextObject's world. Null outside a game world (for example in the editor). */
	static USunsetCoveHUDSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem
	virtual void Deinitialize() override;
	//~ End USubsystem

	// ------------------------------------------------------------------
	// Notifications
	// ------------------------------------------------------------------

	/**
	 * Shows the message of a row of the notification table. Returns false if the row does not exist or is a one-time tip that
	 * was already shown. The row name is the id (for example "Tool.Refused.HandsBusy").
	 */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|HUD")
	bool ShowNotificationById(FName NotificationId);

	/** Shows runtime text. DurationSeconds 0 = the feed's default. Empty text is ignored. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|HUD")
	void ShowNotification(const FText& Message, float DurationSeconds = 0.0f);

	/** Sends a fully described message (what the two functions above end up calling). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|HUD")
	void PushNotification(const FHUDNotification& Notification);

	/** Forgets which one-time tips were shown, so they show again (a new game, a tutorial restart). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|HUD")
	void ResetShownOnceNotifications();

	/** Hands over (and clears) the messages asked for while no feed was listening. Called by the feed when it binds. */
	TArray<FHUDNotification> TakePendingNotifications();

	/** A message was asked for. The suggestion feed binds here. */
	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|HUD")
	FHUDNotificationRequestedSignature OnNotificationRequested;

	// ------------------------------------------------------------------
	// Crosshair
	// ------------------------------------------------------------------

	/** Adds (bHidden true) or removes (false) a reason to hide the crosshair. Reason is any name unique to the caller. */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|HUD")
	void SetCrosshairHiddenBy(FName Reason, bool bHidden);

	/** True while at least one system asked to hide the crosshair. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|HUD")
	bool IsCrosshairHiddenByRequest() const { return CrosshairHideReasons.Num() > 0; }

	/** The answer of IsCrosshairHiddenByRequest changed. */
	UPROPERTY(BlueprintAssignable, Category = "SunsetCove|HUD")
	FHUDCrosshairHideRequestChangedSignature OnCrosshairHideRequestChanged;

	// ------------------------------------------------------------------
	// Interaction prompts (what the player can do + the key, WBP_InteractionPrompt)
	// ------------------------------------------------------------------

	/** Same rules as SetCrosshairHiddenBy, for the interaction prompt (a cutscene, a dialogue that owns the screen). */
	UFUNCTION(BlueprintCallable, Category = "SunsetCove|HUD")
	void SetInteractionPromptsHiddenBy(FName Reason, bool bHidden);

	/** True while at least one system asked to hide the interaction prompt. */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|HUD")
	bool AreInteractionPromptsHiddenByRequest() const { return PromptHideReasons.Num() > 0; }

private:
	/** The table of Project Settings, loaded on first use (a small table of texts). Null if none is set. */
	const UDataTable* ResolveNotificationTable();

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> NotificationTable;

	/** Messages asked for while nobody listened, oldest first. Capped, the oldest are dropped. */
	TArray<FHUDNotification> PendingNotifications;

	/** Ids of the one-time tips already shown in this session. */
	TSet<FName> ShownOnceIds;

	/** Ids already reported as missing, so the log warns once per id. */
	TSet<FName> WarnedMissingIds;

	TSet<FName> CrosshairHideReasons;

	TSet<FName> PromptHideReasons;

	bool bTableResolved = false;
};
