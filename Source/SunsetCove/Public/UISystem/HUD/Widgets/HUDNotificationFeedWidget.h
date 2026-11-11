// HUDNotificationFeedWidget.h
// SunsetCove - UI System / HUD. The suggestion feed: short tips and "why not" messages in the top-left corner.
//
// WHAT THIS CLASS IS
//   A HUD element (see SunsetCoveHUDElementWidget.h) that listens to USunsetCoveHUDSubsystem::OnNotificationRequested and shows
//   each message as one line of plain gray text: no background, no border, no box (GDD mockup: "hypothetical text that can
//   appear on the screen for some suggestions"). Each line fades in, stays, fades out and is removed. With no message the
//   container is collapsed, so the feed costs nothing on screen.
//
// THE WIDGET BLUEPRINT (WBP_NotificationFeed)
//   Its tree is a Canvas Panel with ONE panel named exactly MessageContainer (a Vertical Box) anchored top-left: that panel's
//   position and padding decide where the feed is. Lines are created at runtime inside it; nothing else is in the tree.
//   The look and the timing are Class Defaults (categories "Notifications|Style" and "Notifications|Timing").
//
// WHO SENDS MESSAGES
//   Any system, through the subsystem (by table id or by text). The feed never knows who asked.
//   Repeats: the same message asked for again while it is on screen restarts its timer instead of stacking a copy
//   (bMergeDuplicates), so pressing a refused action many times shows one calm line.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "UISystem/HUD/HUDNotificationTypes.h"
#include "UISystem/HUD/Widgets/SunsetCoveHUDElementWidget.h"
#include "HUDNotificationFeedWidget.generated.h"

class UPanelWidget;
class UTextBlock;

UCLASS(Blueprintable)
class SUNSETCOVE_API UHUDNotificationFeedWidget : public USunsetCoveHUDElementWidget
{
	GENERATED_BODY()

public:
	UHUDNotificationFeedWidget(const FObjectInitializer& ObjectInitializer);

	/** Shows a message now (what the subsystem event does). Also usable directly, for example to test a style. */
	UFUNCTION(BlueprintCallable, Category = "Notifications")
	void AddNotification(const FHUDNotification& Notification);

	/** Removes every line at once. */
	UFUNCTION(BlueprintCallable, Category = "Notifications")
	void ClearNotifications();

	// ------------------------------------------------------------------
	// Style (Class Defaults of WBP_NotificationFeed)
	// ------------------------------------------------------------------

	/** Font of a line. Bold matches the GDD mockup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style")
	FSlateFontInfo MessageFont;

	/** Text color. A mid gray with some transparency: readable on sand and at night, never shouting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style")
	FLinearColor MessageColor = FLinearColor(0.30f, 0.30f, 0.30f, 0.9f);

	/** A very soft shadow, only to keep the text readable on a bright beach. Alpha 0 = no shadow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style")
	FVector2D ShadowOffset = FVector2D(1.0, 1.0);

	/** UPPERCASE like the mockup. Off = the text as written in the table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style")
	bool bUppercase = true;

	/** Lines longer than this (Slate units) wrap. 0 = never wrap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style", meta = (ClampMin = "0.0"))
	float WrapTextAt = 560.0f;

	/** Space around each line inside MessageContainer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Style")
	FMargin LinePadding = FMargin(0.0f, 0.0f, 0.0f, 4.0f);

	// ------------------------------------------------------------------
	// Timing (Class Defaults of WBP_NotificationFeed)
	// ------------------------------------------------------------------

	/** Seconds a line stays fully visible when the message does not say otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float DefaultDurationSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutSeconds = 0.8f;

	/** Most lines on screen at once; a new line pushes the oldest out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Timing", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxVisibleMessages = 3;

	/** True: newest line at the top. False: newest at the bottom (reads like a log). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Timing")
	bool bNewestOnTop = false;

	/** True: a message already on screen is refreshed instead of shown twice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notifications|Timing")
	bool bMergeDuplicates = true;

	// ------------------------------------------------------------------
	// Designer preview
	// ------------------------------------------------------------------

	/** Shown in the UMG designer only, to judge font and position. Never in game. */
	UPROPERTY(EditAnywhere, Category = "Notifications|Preview")
	FText DesignPreviewText;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	/** The panel the lines are added to. A Vertical Box anchored top-left in WBP_NotificationFeed. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> MessageContainer;

private:
	UFUNCTION()
	void HandleNotificationRequested(const FHUDNotification& Notification);

	/** One line on screen. */
	struct FActiveLine
	{
		TWeakObjectPtr<UTextBlock> Text;
		/** Table id, or the text itself for runtime messages: what "the same message" means. */
		FString Key;
		float Age = 0.0f;
		float HoldSeconds = 0.0f;
	};

	UTextBlock* CreateLine(const FText& Message);
	void AddLineToContainer(UTextBlock* Text);
	/** bNewestOnTop: re-adds the lines newest first. */
	void RefreshLineOrder();
	void ApplyLineStyle(UTextBlock* Text) const;
	void RemoveLineAt(int32 Index);
	void UpdateContainerVisibility();
	float ComputeLineOpacity(const FActiveLine& Line) const;

	TArray<FActiveLine> ActiveLines;

	bool bBoundToSubsystem = false;
};
