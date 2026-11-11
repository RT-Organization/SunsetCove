// HUDNotificationTypes.h
// SunsetCove - UI System / HUD. The data of the top-left suggestion feed: one message, one table row, the delegate signatures.
//
// WHAT THIS FILE IS
//   Kept apart from the subsystem and the widgets so that game code (inventories, events, the workday) can describe a message
//   without including any widget header.
//
// HOW A MESSAGE IS DESCRIBED
//   * By id (preferred): game code asks for a row name of the notification table (DT_HUDNotifications, a table of
//     FHUDNotificationRow). The text, how long it stays and whether it is a one-time tip live in the table, so a designer
//     rewrites or translates a message without touching code. The table is chosen in Project Settings > Game > Sunset Cove UI.
//   * By text: for messages built at runtime (a name, a number). FHUDNotification carries the text directly.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "HUDNotificationTypes.generated.h"

/** One message for the suggestion feed (top-left of the HUD). */
USTRUCT(BlueprintType)
struct SUNSETCOVE_API FHUDNotification
{
	GENERATED_BODY()

	/** The text shown to the player. Short: one line, a tip or the reason something was refused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Notification")
	FText Message;

	/** Seconds the message stays fully visible before it fades out. 0 or less = the feed's DefaultDurationSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Notification", meta = (Units = "s"))
	float DurationSeconds = 0.0f;

	/**
	 * Row name of the table when the message came from it, None for runtime text. The feed uses it to merge repeats: the same
	 * message asked for again while it is on screen restarts its timer instead of stacking a copy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Notification")
	FName NotificationId = NAME_None;
};

/**
 * One row of the notification table. The ROW NAME is the id that game code asks for (for example "Tool.Refused.HandsBusy").
 * Create the table with this row structure (Miscellaneous > Data Table > HUDNotificationRow).
 */
USTRUCT(BlueprintType)
struct SUNSETCOVE_API FHUDNotificationRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The text shown to the player. Name actions, never physical keys: every key can be remapped (GDD, Controlli). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Notification", meta = (MultiLine = "true"))
	FText Message;

	/** Seconds fully visible before fading. 0 = the feed's default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Notification", meta = (ClampMin = "0.0", Units = "s"))
	float DurationSeconds = 0.0f;

	/** True: shown only the first time it is asked for in a play session (tutorial tips). Refusal messages leave it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Notification")
	bool bShowOncePerSession = false;
};

// ----------------------------------------------------------------------
// Delegate signatures (BlueprintAssignable on USunsetCoveHUDSubsystem)
// ----------------------------------------------------------------------

/** A message was asked for. The suggestion feed listens to it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUDNotificationRequestedSignature, const FHUDNotification&, Notification);

/** The set of systems that want the crosshair hidden became empty (bHiddenByRequest false) or non-empty (true). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHUDCrosshairHideRequestChangedSignature, bool, bHiddenByRequest);
