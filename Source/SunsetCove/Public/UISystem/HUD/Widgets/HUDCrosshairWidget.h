// HUDCrosshairWidget.h
// SunsetCove - UI System / HUD. The crosshair: a small clean dot in the exact center of the screen.
//
// WHAT THIS CLASS IS
//   A HUD element (see SunsetCoveHUDElementWidget.h) that draws itself in NativePaint: no image asset, no layout. The dot is a
//   rounded box drawn by Slate, so it stays a perfect circle at any size and any resolution. The Widget Blueprint WBP_Crosshair
//   only holds the look (Class Defaults > Crosshair); its widget tree is empty on purpose.
//
// THE LOOK (Class Defaults of WBP_Crosshair, category "Crosshair")
//   DotSize, DotColor (alpha = opacity), OutlineThickness / OutlineColor (a faint dark ring so the white dot reads on white sand),
//   bUseCustomBrush + CustomBrush (draw a texture instead of the dot, for example a future art-made crosshair).
//
// WHEN IT HIDES (it fades, FadeSpeed)
//   * SetCrosshairVisible(false) on the widget (local switch, from Blueprint or C++).
//   * Any system asked USunsetCoveHUDSubsystem::SetCrosshairHiddenBy(Reason, true) (events, cutscenes).
//   * bHideWhileToolMenuOpen: the tool radial menu is open (the Tool Inventory of the pawn, found by class).
//   * bHideWhileCursorVisible: the player controller shows the mouse cursor, which only happens while a menu is open (gameplay
//     never shows a cursor). This covers every menu of the CoreFramework layers without the crosshair knowing any menu.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UISystem/HUD/Widgets/SunsetCoveHUDElementWidget.h"
#include "HUDCrosshairWidget.generated.h"

class UToolInventoryComponent;

UCLASS(Blueprintable)
class SUNSETCOVE_API UHUDCrosshairWidget : public USunsetCoveHUDElementWidget
{
	GENERATED_BODY()

public:
	UHUDCrosshairWidget(const FObjectInitializer& ObjectInitializer);

	// ------------------------------------------------------------------
	// Runtime control
	// ------------------------------------------------------------------

	/** Local switch. False fades the crosshair out whatever the other rules say; true lets the other rules decide. */
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairVisible(bool bVisible);

	/** Changes size and color at runtime (for example a bigger dot while aiming a tool). */
	UFUNCTION(BlueprintCallable, Category = "Crosshair")
	void SetCrosshairStyle(float NewDotSize, FLinearColor NewDotColor);

	/** True while the crosshair is (or is fading towards) visible. */
	UFUNCTION(BlueprintPure, Category = "Crosshair")
	bool IsCrosshairShown() const { return bTargetVisible; }

	// ------------------------------------------------------------------
	// Look (Class Defaults of WBP_Crosshair)
	// ------------------------------------------------------------------

	/** Diameter of the dot (Slate units: pixels at 1080p, scaled with the DPI curve). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Look", meta = (ClampMin = "1.0", UIMax = "32.0"))
	float DotSize = 6.0f;

	/** Color of the dot. Alpha is its opacity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Look")
	FLinearColor DotColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	/** Width of a faint ring around the dot so it reads on bright sand. 0 = no ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Look", meta = (ClampMin = "0.0", UIMax = "4.0"))
	float OutlineThickness = 1.0f;

	/** Color of the ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Look")
	FLinearColor OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);

	/** True: CustomBrush is drawn (centered, at its Image Size) instead of the dot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Look")
	bool bUseCustomBrush = false;

	/** A texture or material crosshair. Tinted by DotColor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Look", meta = (EditCondition = "bUseCustomBrush"))
	FSlateBrush CustomBrush;

	// ------------------------------------------------------------------
	// Behavior
	// ------------------------------------------------------------------

	/** Hide while the tool radial menu is open (the wheel takes the center of the screen). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Behavior")
	bool bHideWhileToolMenuOpen = true;

	/** Hide while the mouse cursor is shown (a menu is open). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Behavior")
	bool bHideWhileCursorVisible = true;

	/** How fast the crosshair fades in and out. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crosshair|Behavior", meta = (ClampMin = "0.0"))
	float FadeSpeed = 14.0f;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//~ End UUserWidget Interface

	//~ Begin USunsetCoveHUDElementWidget
	virtual void HandlePawnBound(APawn* Pawn) override;
	virtual void HandlePawnUnbound(APawn* Pawn) override;
	//~ End USunsetCoveHUDElementWidget

private:
	/** All the hide rules together. */
	bool ShouldBeVisible() const;

	/** The rounded-box brush of the dot (no texture: Slate draws the circle). */
	FSlateBrush DotBrush;

	TWeakObjectPtr<UToolInventoryComponent> ToolInventory;

	/** Current opacity of the whole crosshair (0..1), follows bTargetVisible at FadeSpeed. */
	float VisibilityAlpha = 1.0f;

	bool bLocallyVisible = true;
	bool bTargetVisible = true;
};
