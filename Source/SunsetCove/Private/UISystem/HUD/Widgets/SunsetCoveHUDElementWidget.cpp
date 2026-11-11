// SunsetCoveHUDElementWidget.cpp
// SunsetCove - UI System / HUD. See Public/UISystem/HUD/Widgets/SunsetCoveHUDElementWidget.h.

#include "UISystem/HUD/Widgets/SunsetCoveHUDElementWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/OverlaySlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "UISystem/HUD/SunsetCoveHUDSubsystem.h"

USunsetCoveHUDElementWidget::USunsetCoveHUDElementWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// HUD elements never take mouse clicks: gameplay keeps all input (the HUD screen is Game Only).
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
	ShownVisibility = ESlateVisibility::SelfHitTestInvisible;
}

USunsetCoveHUDSubsystem* USunsetCoveHUDElementWidget::GetHUDSubsystem() const
{
	return USunsetCoveHUDSubsystem::Get(this);
}

void USunsetCoveHUDElementWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bFillParentSlot)
	{
		FillParentSlot();

		// When a panel adds this widget, it reads the slot alignment BEFORE it constructs the widget (UOverlaySlot::BuildSlot),
		// and NativeConstruct runs during that construction: the change above only reaches the UMG slot, while the live Slate
		// slot was already built with the default Left / Top (zero size, drawn at the top-left corner). One frame later the
		// Slate slot exists, so the same call updates it too.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				FillParentSlot();
			}));
		}
	}
}

bool USunsetCoveHUDElementWidget::HasUsableGeometry(const FGeometry& Geometry)
{
	// Zero size means the element is not stretched over its slot yet (first frame): drawing now would land in the corner.
	const FVector2f Size = Geometry.GetLocalSize();
	return Size.X > 1.0f && Size.Y > 1.0f;
}

void USunsetCoveHUDElementWidget::FillParentSlot()
{
	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
		CanvasSlot->SetAlignment(FVector2D::ZeroVector);
	}
}

void USunsetCoveHUDElementWidget::OnUIPawnBound_Implementation(APawn* Pawn)
{
	Super::OnUIPawnBound_Implementation(Pawn);
	HandlePawnBound(Pawn);
}

void USunsetCoveHUDElementWidget::OnUIPawnUnbound_Implementation(APawn* Pawn)
{
	HandlePawnUnbound(Pawn);
	Super::OnUIPawnUnbound_Implementation(Pawn);
}
