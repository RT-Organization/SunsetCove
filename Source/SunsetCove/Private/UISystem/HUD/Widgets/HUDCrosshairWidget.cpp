// HUDCrosshairWidget.cpp
// SunsetCove - UI System / HUD. See Public/UISystem/HUD/Widgets/HUDCrosshairWidget.h.

#include "UISystem/HUD/Widgets/HUDCrosshairWidget.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryComponent.h"
#include "Rendering/DrawElements.h"
#include "UISystem/HUD/SunsetCoveHUDSubsystem.h"

UHUDCrosshairWidget::UHUDCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A rounded box with "half height" rounding is a circle at any size, drawn by Slate (no texture).
	DotBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	DotBrush.TintColor = FSlateColor(FLinearColor::White);
	DotBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	DotBrush.OutlineSettings.Width = 0.0f;

	CustomBrush.ImageSize = FVector2D(16.0, 16.0);
}

void UHUDCrosshairWidget::SetCrosshairVisible(bool bVisible)
{
	bLocallyVisible = bVisible;
}

void UHUDCrosshairWidget::SetCrosshairStyle(float NewDotSize, FLinearColor NewDotColor)
{
	DotSize = FMath::Max(1.0f, NewDotSize);
	DotColor = NewDotColor;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UHUDCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// No fade-in at start: the crosshair is simply there.
	bTargetVisible = ShouldBeVisible();
	VisibilityAlpha = bTargetVisible ? 1.0f : 0.0f;
}

void UHUDCrosshairWidget::HandlePawnBound(APawn* Pawn)
{
	ToolInventory = Pawn ? Pawn->FindComponentByClass<UToolInventoryComponent>() : nullptr;
}

void UHUDCrosshairWidget::HandlePawnUnbound(APawn* /*Pawn*/)
{
	ToolInventory.Reset();
}

bool UHUDCrosshairWidget::ShouldBeVisible() const
{
	if (!bLocallyVisible)
	{
		return false;
	}

	if (const USunsetCoveHUDSubsystem* HUDSubsystem = GetHUDSubsystem())
	{
		if (HUDSubsystem->IsCrosshairHiddenByRequest())
		{
			return false;
		}
	}

	if (bHideWhileToolMenuOpen)
	{
		const UToolInventoryComponent* Inventory = ToolInventory.Get();
		if (Inventory && Inventory->IsRadialMenuOpen())
		{
			return false;
		}
	}

	if (bHideWhileCursorVisible)
	{
		const APlayerController* PlayerController = GetOwningPlayer();
		if (PlayerController && PlayerController->ShouldShowMouseCursor())
		{
			return false;
		}
	}

	return true;
}

void UHUDCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Polling a few booleans is cheaper and safer than tracking every source of "hide" with events.
	bTargetVisible = ShouldBeVisible();

	const float Target = bTargetVisible ? 1.0f : 0.0f;
	VisibilityAlpha = (FadeSpeed <= 0.0f) ? Target : FMath::FInterpTo(VisibilityAlpha, Target, InDeltaTime, FadeSpeed);
}

int32 UHUDCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// The designer preview always shows the crosshair.
	const float Alpha = (IsDesignTime() ? 1.0f : VisibilityAlpha) * InWidgetStyle.GetColorAndOpacityTint().A;
	if (Alpha <= KINDA_SMALL_NUMBER || !HasUsableGeometry(AllottedGeometry))
	{
		return MaxLayer;
	}

	const FVector2f Center = FVector2f(AllottedGeometry.GetLocalSize()) * 0.5f;
	const int32 DrawLayer = MaxLayer + 1;

	auto DrawCentered = [&](const FSlateBrush* Brush, const FVector2f& Size, FLinearColor Tint, int32 Layer)
	{
		Tint.A *= Alpha;
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer,
			AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Center - Size * 0.5f)),
			Brush,
			ESlateDrawEffect::None,
			Tint);
	};

	if (bUseCustomBrush && CustomBrush.DrawAs != ESlateBrushDrawType::NoDrawType)
	{
		const FVector2f BrushSize(static_cast<float>(CustomBrush.ImageSize.X), static_cast<float>(CustomBrush.ImageSize.Y));
		DrawCentered(&CustomBrush, BrushSize, DotColor, DrawLayer);
		return DrawLayer;
	}

	// Ring first (a slightly bigger dark circle behind), then the dot one layer above it.
	if (OutlineThickness > 0.0f && OutlineColor.A > 0.0f)
	{
		const float RingSize = DotSize + OutlineThickness * 2.0f;
		DrawCentered(&DotBrush, FVector2f(RingSize, RingSize), OutlineColor, DrawLayer);
	}
	DrawCentered(&DotBrush, FVector2f(DotSize, DotSize), DotColor, DrawLayer + 1);

	return DrawLayer + 1;
}
