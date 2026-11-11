// SunsetCoveHUDElementWidget.h
// SunsetCove - UI System / HUD. The parent of every Sunset Cove HUD element (crosshair, suggestion feed, tool radial menu).
//
// HIERARCHY
//   UCommonUserWidget -> UBaseUserWidget (CoreFramework) -> USunsetCoveHUDElementWidget (this class)
//     -> UHUDCrosshairWidget        -> WBP_Crosshair
//     -> UHUDNotificationFeedWidget -> WBP_NotificationFeed
//     -> UToolRadialMenuWidget      -> WBP_ToolRadialMenu
//
// HOW AN ELEMENT GETS ON SCREEN (nothing is hard-wired in WBP_HUD)
//   DA_UIConfig > HUD Elements lists the elements (id, slot tag, widget class). The CoreFramework HUD container (WBP_HUD, parent
//   UBaseHUDWidget) loads each class asynchronously and adds it to the panel registered for the slot tag; the default slot is the
//   full-screen Overlay named DefaultElementSlot. Adding, removing or swapping an element is a data change in DA_UIConfig.
//
// WHAT THIS CLASS ADDS TO UBaseUserWidget
//   * Fill: an element stretches over the whole panel slot it is added to (Overlay: Fill / Fill, Canvas: full anchors), so it can
//     place its content anywhere on screen (the crosshair in the center, the feed in the top-left) without the HUD knowing.
//   * Binding: UBaseUserWidget already registers with the CoreFramework binding hub, which tells the widget about the local
//     player's pawn (OnUIPawnBound / OnUIPawnUnbound, also on respawn and late join). Children override HandlePawnBound /
//     HandlePawnUnbound instead of the interface functions, and find the components they need on the pawn by class, so an
//     element never depends on the character class.
//   * Access to the HUD message hub (USunsetCoveHUDSubsystem).

#pragma once

#include "CoreMinimal.h"
#include "UISystem/Foundation/BaseUserWidget.h"
#include "SunsetCoveHUDElementWidget.generated.h"

class APawn;
class USunsetCoveHUDSubsystem;

UCLASS(Abstract, Blueprintable)
class SUNSETCOVE_API USunsetCoveHUDElementWidget : public UBaseUserWidget
{
	GENERATED_BODY()

public:
	USunsetCoveHUDElementWidget(const FObjectInitializer& ObjectInitializer);

	/** The HUD message hub of the current game instance (null in the editor designer). */
	UFUNCTION(BlueprintPure, Category = "SunsetCove|HUD")
	USunsetCoveHUDSubsystem* GetHUDSubsystem() const;

	//~ Begin IBaseUIBinderInterface
	virtual void OnUIPawnBound_Implementation(APawn* Pawn) override;
	virtual void OnUIPawnUnbound_Implementation(APawn* Pawn) override;
	//~ End IBaseUIBinderInterface

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeConstruct() override;
	//~ End UUserWidget Interface

	/** The local player's pawn is now Pawn (possess, respawn, or the element was created after the pawn existed). */
	virtual void HandlePawnBound(APawn* Pawn) {}

	/** The pawn is gone or replaced. Pawn can be null (already destroyed): drop subscriptions through cached weak pointers. */
	virtual void HandlePawnUnbound(APawn* Pawn) {}

	/**
	 * Stretches this widget over the whole panel slot it was added to. Called on construct and again on the next frame when
	 * bFillParentSlot is on (the panel builds the live slot before the widget exists; see the .cpp).
	 */
	void FillParentSlot();

	/** False while the element has no real size yet. Elements that paint relative to their size skip painting then. */
	static bool HasUsableGeometry(const FGeometry& Geometry);

	/**
	 * True: the element covers the whole slot it is placed in and positions its own content. Turn it off only for an element
	 * that should take its desired size inside a custom slot (a small panel in a corner box of the HUD).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD Element")
	bool bFillParentSlot = true;
};
