// HUDNotificationFeedWidget.cpp
// SunsetCove - UI System / HUD. See Public/UISystem/HUD/Widgets/HUDNotificationFeedWidget.h.

#include "UISystem/HUD/Widgets/HUDNotificationFeedWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/Settings/SunsetCovePlayerPreferences.h"
#include "Engine/Font.h"
#include "UISystem/HUD/SunsetCoveHUDSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "SunsetCoveHUD"

UHUDNotificationFeedWidget::UHUDNotificationFeedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The engine's default UMG font (Roboto), bold like the mockup. Replace it per project in the Class Defaults.
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UObject> DefaultFont(*UWidget::GetDefaultFontName());
		MessageFont = FSlateFontInfo(DefaultFont.Object, 15, FName(TEXT("Bold")));
	}

	DesignPreviewText = LOCTEXT("FeedPreview", "Suggestion: hypothetical text that can appear on the screen");
}

void UHUDNotificationFeedWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// The designer shows one sample line with the current style; the game starts empty.
	if (IsDesignTime() && MessageContainer)
	{
		MessageContainer->ClearChildren();
		if (!DesignPreviewText.IsEmpty())
		{
			CreateLine(DesignPreviewText);
		}
	}
}

void UHUDNotificationFeedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MessageContainer)
	{
		MessageContainer->ClearChildren();
	}
	ActiveLines.Reset();

	if (USunsetCoveHUDSubsystem* HUDSubsystem = GetHUDSubsystem())
	{
		HUDSubsystem->OnNotificationRequested.AddUniqueDynamic(this, &UHUDNotificationFeedWidget::HandleNotificationRequested);
		bBoundToSubsystem = true;

		// Messages asked for before the HUD finished loading (first frames of a level).
		for (const FHUDNotification& Pending : HUDSubsystem->TakePendingNotifications())
		{
			AddNotification(Pending);
		}
	}

	UpdateContainerVisibility();
}

void UHUDNotificationFeedWidget::NativeDestruct()
{
	if (bBoundToSubsystem)
	{
		if (USunsetCoveHUDSubsystem* HUDSubsystem = GetHUDSubsystem())
		{
			HUDSubsystem->OnNotificationRequested.RemoveDynamic(this, &UHUDNotificationFeedWidget::HandleNotificationRequested);
		}
		bBoundToSubsystem = false;
	}

	ClearNotifications();
	Super::NativeDestruct();
}

void UHUDNotificationFeedWidget::HandleNotificationRequested(const FHUDNotification& Notification)
{
	AddNotification(Notification);
}

void UHUDNotificationFeedWidget::AddNotification(const FHUDNotification& Notification)
{
	if (!MessageContainer || Notification.Message.IsEmpty())
	{
		return;
	}

	const FString Key = Notification.NotificationId.IsNone() ? Notification.Message.ToString() : Notification.NotificationId.ToString();
	const float HoldSeconds = Notification.DurationSeconds > 0.0f ? Notification.DurationSeconds : DefaultDurationSeconds;

	// Same message already on screen: restart its timer (keep it fully visible, no second fade-in).
	if (bMergeDuplicates)
	{
		for (FActiveLine& Line : ActiveLines)
		{
			if (Line.Key == Key && Line.Text.IsValid())
			{
				Line.Age = FMath::Min(Line.Age, FadeInSeconds);
				Line.HoldSeconds = HoldSeconds;
				return;
			}
		}
	}

	// Make room: the oldest line leaves at once.
	while (ActiveLines.Num() >= FMath::Max(1, MaxVisibleMessages))
	{
		RemoveLineAt(0);
	}

	UTextBlock* Text = CreateLine(Notification.Message);
	if (!Text)
	{
		return;
	}

	FActiveLine& Line = ActiveLines.AddDefaulted_GetRef();
	Line.Text = Text;
	Line.Key = Key;
	Line.Age = 0.0f;
	Line.HoldSeconds = HoldSeconds;

	Text->SetRenderOpacity(ComputeLineOpacity(Line));
	RefreshLineOrder();
	UpdateContainerVisibility();
}

void UHUDNotificationFeedWidget::ClearNotifications()
{
	while (ActiveLines.Num() > 0)
	{
		RemoveLineAt(ActiveLines.Num() - 1);
	}
	UpdateContainerVisibility();
}

UTextBlock* UHUDNotificationFeedWidget::CreateLine(const FText& Message)
{
	if (!MessageContainer || !WidgetTree)
	{
		return nullptr;
	}

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Message);
	ApplyLineStyle(Text);
	AddLineToContainer(Text);
	return Text;
}

void UHUDNotificationFeedWidget::AddLineToContainer(UTextBlock* Text)
{
	MessageContainer->AddChild(Text);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Text->Slot))
	{
		BoxSlot->SetPadding(LinePadding);
		BoxSlot->SetHorizontalAlignment(HAlign_Left);
	}
}

void UHUDNotificationFeedWidget::RefreshLineOrder()
{
	// Panels only append at runtime (InsertChildAt does not update the live Slate panel), so the order is rebuilt:
	// ActiveLines is oldest -> newest, the container shows newest first.
	if (!MessageContainer || !bNewestOnTop)
	{
		return;
	}

	TArray<UTextBlock*> Texts;
	for (const FActiveLine& Line : ActiveLines)
	{
		if (UTextBlock* Text = Line.Text.Get())
		{
			Texts.Add(Text);
		}
	}

	MessageContainer->ClearChildren();
	for (int32 Index = Texts.Num() - 1; Index >= 0; --Index)
	{
		AddLineToContainer(Texts[Index]);
	}
}

void UHUDNotificationFeedWidget::ApplyLineStyle(UTextBlock* Text) const
{
	Text->SetFont(MessageFont);
	Text->SetColorAndOpacity(FSlateColor(MessageColor));
	Text->SetShadowColorAndOpacity(ShadowColor);
	Text->SetShadowOffset(ShadowOffset);
	Text->SetTextTransformPolicy(bUppercase ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	Text->SetJustification(ETextJustify::Left);
	Text->SetAutoWrapText(false);
	Text->SetWrapTextAt(WrapTextAt);
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UHUDNotificationFeedWidget::RemoveLineAt(int32 Index)
{
	if (!ActiveLines.IsValidIndex(Index))
	{
		return;
	}

	if (UTextBlock* Text = ActiveLines[Index].Text.Get())
	{
		Text->RemoveFromParent();
	}
	ActiveLines.RemoveAt(Index);
}

void UHUDNotificationFeedWidget::UpdateContainerVisibility()
{
	if (MessageContainer)
	{
		const bool bAnyLine = ActiveLines.Num() > 0 || IsDesignTime();
		MessageContainer->SetVisibility(bAnyLine ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

float UHUDNotificationFeedWidget::ComputeLineOpacity(const FActiveLine& Line) const
{
	if (FadeInSeconds > 0.0f && Line.Age < FadeInSeconds)
	{
		return Line.Age / FadeInSeconds;
	}

	const float FadeOutStart = FadeInSeconds + Line.HoldSeconds;
	if (Line.Age <= FadeOutStart)
	{
		return 1.0f;
	}

	return FadeOutSeconds > 0.0f ? FMath::Clamp(1.0f - (Line.Age - FadeOutStart) / FadeOutSeconds, 0.0f, 1.0f) : 0.0f;
}

void UHUDNotificationFeedWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ActiveLines.Num() == 0)
	{
		return;
	}

	// Hints switched off (Settings > Controls) while messages were on screen: they go at once. New ones are never sent.
	if (!USunsetCovePlayerPreferences::AreHintsEnabled())
	{
		ClearNotifications();
		return;
	}

	for (int32 Index = ActiveLines.Num() - 1; Index >= 0; --Index)
	{
		FActiveLine& Line = ActiveLines[Index];
		UTextBlock* Text = Line.Text.Get();
		if (!Text)
		{
			ActiveLines.RemoveAt(Index);
			continue;
		}

		Line.Age += InDeltaTime;
		const float Opacity = ComputeLineOpacity(Line);
		if (Line.Age > FadeInSeconds + Line.HoldSeconds && Opacity <= 0.0f)
		{
			RemoveLineAt(Index);
			continue;
		}
		Text->SetRenderOpacity(Opacity);
	}

	if (ActiveLines.Num() == 0)
	{
		UpdateContainerVisibility();
	}
}

#undef LOCTEXT_NAMESPACE
