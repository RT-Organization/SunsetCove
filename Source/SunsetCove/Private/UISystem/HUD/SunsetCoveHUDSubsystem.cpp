// SunsetCoveHUDSubsystem.cpp
// SunsetCove - UI System / HUD. See Public/UISystem/HUD/SunsetCoveHUDSubsystem.h.

#include "UISystem/HUD/SunsetCoveHUDSubsystem.h"

#include "Core/Logging/SunsetCoveLog.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UISystem/HUD/SunsetCoveUISettings.h"

namespace
{
	/** Messages kept while no feed listens. Enough for the start-up burst, small enough to never flood the screen later. */
	constexpr int32 MaxPendingNotifications = 4;
}

USunsetCoveHUDSubsystem* USunsetCoveHUDSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USunsetCoveHUDSubsystem>() : nullptr;
}

void USunsetCoveHUDSubsystem::Deinitialize()
{
	OnNotificationRequested.Clear();
	OnCrosshairHideRequestChanged.Clear();
	PendingNotifications.Reset();
	CrosshairHideReasons.Reset();
	NotificationTable = nullptr;

	Super::Deinitialize();
}

// ----------------------------------------------------------------------
// Notifications
// ----------------------------------------------------------------------

bool USunsetCoveHUDSubsystem::ShowNotificationById(FName NotificationId)
{
	if (NotificationId.IsNone())
	{
		return false;
	}

	const UDataTable* Table = ResolveNotificationTable();
	const FHUDNotificationRow* Row = Table ? Table->FindRow<FHUDNotificationRow>(NotificationId, TEXT("HUD notification"), false) : nullptr;
	if (!Row)
	{
		if (!WarnedMissingIds.Contains(NotificationId))
		{
			WarnedMissingIds.Add(NotificationId);
			UE_LOG(LogSunsetCove, Warning, TEXT("HUD: no notification row '%s' (table: %s). Add the row to show a message."),
				*NotificationId.ToString(), Table ? *Table->GetPathName() : TEXT("none set in Project Settings > Game > Sunset Cove UI"));
		}
		return false;
	}

	if (Row->bShowOncePerSession)
	{
		if (ShownOnceIds.Contains(NotificationId))
		{
			return false;
		}
		ShownOnceIds.Add(NotificationId);
	}

	FHUDNotification Notification;
	Notification.Message = Row->Message;
	Notification.DurationSeconds = Row->DurationSeconds;
	Notification.NotificationId = NotificationId;
	PushNotification(Notification);
	return true;
}

void USunsetCoveHUDSubsystem::ShowNotification(const FText& Message, float DurationSeconds)
{
	FHUDNotification Notification;
	Notification.Message = Message;
	Notification.DurationSeconds = DurationSeconds;
	PushNotification(Notification);
}

void USunsetCoveHUDSubsystem::PushNotification(const FHUDNotification& Notification)
{
	if (Notification.Message.IsEmpty())
	{
		return;
	}

	if (OnNotificationRequested.IsBound())
	{
		OnNotificationRequested.Broadcast(Notification);
		return;
	}

	// No feed yet (the HUD streams in asynchronously): keep it for the feed.
	if (PendingNotifications.Num() >= MaxPendingNotifications)
	{
		PendingNotifications.RemoveAt(0);
	}
	PendingNotifications.Add(Notification);
}

void USunsetCoveHUDSubsystem::ResetShownOnceNotifications()
{
	ShownOnceIds.Reset();
}

TArray<FHUDNotification> USunsetCoveHUDSubsystem::TakePendingNotifications()
{
	return MoveTemp(PendingNotifications);
}

const UDataTable* USunsetCoveHUDSubsystem::ResolveNotificationTable()
{
	if (!bTableResolved)
	{
		bTableResolved = true;

		const USunsetCoveUISettings* Settings = GetDefault<USunsetCoveUISettings>();
		if (Settings && !Settings->NotificationTable.IsNull())
		{
			NotificationTable = Settings->NotificationTable.LoadSynchronous();
		}

		if (!NotificationTable)
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("HUD: no notification table. Set it in Project Settings > Game > Sunset Cove UI > Notification Table."));
		}
		else if (NotificationTable->GetRowStruct() != FHUDNotificationRow::StaticStruct())
		{
			UE_LOG(LogSunsetCove, Warning, TEXT("HUD: %s does not use the row structure HUDNotificationRow; messages by id are disabled."),
				*NotificationTable->GetPathName());
			NotificationTable = nullptr;
		}
	}

	return NotificationTable;
}

// ----------------------------------------------------------------------
// Crosshair
// ----------------------------------------------------------------------

void USunsetCoveHUDSubsystem::SetCrosshairHiddenBy(FName Reason, bool bHidden)
{
	if (Reason.IsNone())
	{
		return;
	}

	const bool bWasHidden = IsCrosshairHiddenByRequest();
	if (bHidden)
	{
		CrosshairHideReasons.Add(Reason);
	}
	else
	{
		CrosshairHideReasons.Remove(Reason);
	}

	const bool bIsHidden = IsCrosshairHiddenByRequest();
	if (bIsHidden != bWasHidden)
	{
		OnCrosshairHideRequestChanged.Broadcast(bIsHidden);
	}
}
