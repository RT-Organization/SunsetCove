// ToolRadialMenuWidget.h
// SunsetCove - UI System / HUD. The tool wheel: hold the Tool Menu action (Tab / R1), point, release. GTA weapon wheel style.
//
// WHAT THIS CLASS IS
//   The drawing of the Tool Inventory's radial menu. The SELECTION is not done here: UToolInventoryComponent already turns the
//   look input (mouse delta or right stick) into a sector while its menu action is held, refuses to open while props are in the
//   hands, equips on release and blocks the camera meanwhile (see ToolInventoryComponent.h). This widget only listens and draws,
//   so the wheel behaves the same with or without a HUD, and no mouse cursor is ever shown (the HUD screen stays Game Only).
//
// PROCEDURAL, DATA-DRIVEN
//   Nothing in the widget is laid out by hand. On every open, and whenever a slot changes, the wheel is rebuilt from the
//   inventory: one equal slice per slot (GetNumSlots = MaxTools on BP_Character > Tool Inventory), centered where the inventory
//   says (GetRadialSlotAngle, which follows RadialStartAngleDegrees), filled with the tool of the slot (DisplayName, Icon). 4 slots
//   give 4 slices of 90 degrees, 10 slots give 10 slices of 36 degrees: no widget or code change.
//   The slices are triangle meshes built in NativePaint (an annulus segment per slot, FSlateDrawElement::MakeCustomVerts). Each
//   vertex carries UVs (U = across the slice 0..1, V = inner 0 .. outer 1), so a texture or a UI material set in SliceBrush maps
//   onto every slice whatever their count. The default brush is plain white, tinted by the slice colors.
//
// WHAT THE PLAYER SEES
//   * The wheel fades and scales in while the menu action is held, and out on release.
//   * The targeted slice pops out: it moves outward (HoverOffset), grows (HoverOuterRadiusScale), turns HoveredSliceColor and its
//     icon grows (HoverIconScale). It animates smoothly (AnimationSpeed).
//   * The slot of the tool in the hand has a thin band on its outer edge (EquippedMarker*).
//   * Empty slots are drawn darker (EmptySliceColor) and never highlight (the inventory does not select them).
//   * The name of the targeted tool is written in the hole of the wheel.
//
// ICONS
//   ASunsetCoveTool::Icon (a soft texture) is loaded asynchronously the first time it is needed. A tool without an icon shows
//   PlaceholderIcon; without that either, the first letter of its name. Icons are drawn square at IconSize.
//
// THE WIDGET BLUEPRINT (WBP_ToolRadialMenu)
//   An empty widget tree: everything is drawn by this class. All the look is in Class Defaults (categories "Radial|...").
//   The UMG designer shows a preview wheel (category "Radial|Preview") so the look can be tuned without playing.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "GameplayTagContainer.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryTypes.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateResourceHandle.h"
#include "Styling/SlateBrush.h"
#include "UISystem/HUD/Widgets/SunsetCoveHUDElementWidget.h"
#include "ToolRadialMenuWidget.generated.h"

class ASunsetCoveTool;
class UTexture2D;
class UToolInventoryComponent;
struct FStreamableHandle;

UCLASS(Blueprintable)
class SUNSETCOVE_API UToolRadialMenuWidget : public USunsetCoveHUDElementWidget
{
	GENERATED_BODY()

public:
	UToolRadialMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** True while the wheel is open (the Tool Menu action is held). */
	UFUNCTION(BlueprintPure, Category = "Radial")
	bool IsMenuOpen() const { return bMenuOpen; }

	/** Re-reads the slots of the inventory (slices, names, icons). Called automatically; public for a Blueprint that changes tools. */
	UFUNCTION(BlueprintCallable, Category = "Radial")
	void RefreshSlices();

	// ------------------------------------------------------------------
	// Layout (Slate units: pixels at 1080p, scaled with the DPI curve)
	// ------------------------------------------------------------------

	/** Outer radius of the wheel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Layout", meta = (ClampMin = "20.0"))
	float OuterRadius = 230.0f;

	/** Radius of the hole in the middle (where the name of the tool is written). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Layout", meta = (ClampMin = "0.0"))
	float InnerRadius = 118.0f;

	/** Empty angle between two slices (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Layout", meta = (ClampMin = "0.0", ClampMax = "20.0", Units = "deg"))
	float SliceGapDegrees = 1.5f;

	/** Smoothness of the arcs: one vertex every N degrees. Lower = rounder and more triangles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Layout", meta = (ClampMin = "0.5", ClampMax = "15.0", Units = "deg"))
	float ArcResolutionDegrees = 3.0f;

	// ------------------------------------------------------------------
	// Selection feedback
	// ------------------------------------------------------------------

	/** How far the targeted slice moves outward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Selection", meta = (ClampMin = "0.0"))
	float HoverOffset = 14.0f;

	/** Outer radius multiplier of the targeted slice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Selection", meta = (ClampMin = "1.0", ClampMax = "1.5"))
	float HoverOuterRadiusScale = 1.06f;

	/** Icon size multiplier of the targeted slice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Selection", meta = (ClampMin = "1.0", ClampMax = "2.0"))
	float HoverIconScale = 1.18f;

	/** Speed of the pop-out animation of a slice. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Selection", meta = (ClampMin = "0.0"))
	float AnimationSpeed = 18.0f;

	/** Optional UI sound cue (CoreFramework audio config) played when the targeted tool changes. Empty = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Selection")
	FGameplayTag HoverSoundCue;

	// ------------------------------------------------------------------
	// Open / close
	// ------------------------------------------------------------------

	/** Speed of the fade in / out of the whole wheel. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Open", meta = (ClampMin = "0.0"))
	float OpenSpeed = 16.0f;

	/** Scale of the wheel when it starts to open (grows to 1). 1 = no scale animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Open", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float OpenStartScale = 0.9f;

	/** Full-screen tint behind the wheel while it is open (the world stays visible). Alpha 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Open")
	FLinearColor BackgroundDimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.18f);

	// ------------------------------------------------------------------
	// Colors and slice texture
	// ------------------------------------------------------------------

	/** A slot that holds a tool. Minimal gray. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors")
	FLinearColor SliceColor = FLinearColor(0.10f, 0.10f, 0.10f, 0.62f);

	/** The targeted slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors")
	FLinearColor HoveredSliceColor = FLinearColor(0.70f, 0.70f, 0.70f, 0.88f);

	/** A slot without a tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors")
	FLinearColor EmptySliceColor = FLinearColor(0.03f, 0.03f, 0.03f, 0.40f);

	/** Thin line around every slice (anti-aliased, hides the stair-steps of the mesh edges). Alpha 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors")
	FLinearColor OutlineColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float OutlineThickness = 1.0f;

	/** Band on the outer edge of the slot of the tool in the hand. Alpha 0 = no marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors")
	FLinearColor EquippedMarkerColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors", meta = (ClampMin = "0.0"))
	float EquippedMarkerThickness = 4.0f;

	/**
	 * Texture or UI material of the slice mesh, tinted by the slice colors. UVs: U goes across the slice (0 = first edge
	 * clockwise, 1 = second edge), V from the inner radius (0) to the outer radius (1). Default: the engine's white square.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Colors")
	FSlateBrush SliceBrush;

	// ------------------------------------------------------------------
	// Icons
	// ------------------------------------------------------------------

	/** Size of a tool icon (square). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Icons", meta = (ClampMin = "8.0"))
	float IconSize = 60.0f;

	/** Where the icon sits across the ring: 0 = inner edge, 1 = outer edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Icons", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IconRadiusAlpha = 0.5f;

	/** Icon tint on a normal slice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Icons")
	FLinearColor IconColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	/** Icon tint on the targeted slice (dark on the light highlight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Icons")
	FLinearColor HoveredIconColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);

	/** Drawn for a tool with no Icon. Empty = the first letter of the tool's name instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Icons")
	TObjectPtr<UTexture2D> PlaceholderIcon;

	/** Font of the letter drawn when a tool has neither an Icon nor a PlaceholderIcon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Icons")
	FSlateFontInfo InitialFont;

	// ------------------------------------------------------------------
	// Name in the center
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Label")
	bool bShowTargetedName = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Label")
	FSlateFontInfo NameFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Label")
	FLinearColor NameColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.92f);

	/** Upper case like the rest of the HUD text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radial|Label")
	bool bUppercaseName = true;

	// ------------------------------------------------------------------
	// Designer preview (UMG designer only, never in game)
	// ------------------------------------------------------------------

	/** Slices shown in the designer. */
	UPROPERTY(EditAnywhere, Category = "Radial|Preview", meta = (ClampMin = "1", ClampMax = "12"))
	int32 DesignPreviewSlotCount = 4;

	/** Names of the preview tools, one per slot; slots past the list are drawn empty. */
	UPROPERTY(EditAnywhere, Category = "Radial|Preview")
	TArray<FText> DesignPreviewToolNames;

	/** Icons of the preview tools, same order as the names (optional). */
	UPROPERTY(EditAnywhere, Category = "Radial|Preview")
	TArray<TObjectPtr<UTexture2D>> DesignPreviewIcons;

	/** The targeted slice in the preview. -1 = none. */
	UPROPERTY(EditAnywhere, Category = "Radial|Preview", meta = (ClampMin = "-1", ClampMax = "11"))
	int32 DesignPreviewTargetedSlot = 1;

	/** The equipped slot in the preview. -1 = none. */
	UPROPERTY(EditAnywhere, Category = "Radial|Preview", meta = (ClampMin = "-1", ClampMax = "11"))
	int32 DesignPreviewEquippedSlot = 0;

protected:
	//~ Begin UUserWidget Interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	//~ End UUserWidget Interface

	//~ Begin USunsetCoveHUDElementWidget
	virtual void HandlePawnBound(APawn* Pawn) override;
	virtual void HandlePawnUnbound(APawn* Pawn) override;
	//~ End USunsetCoveHUDElementWidget

private:
	// ---- Inventory events ----
	UFUNCTION()
	void HandleInventoryStateChanged(EToolInventoryState NewState, EToolInventoryState OldState);

	UFUNCTION()
	void HandleRadialHoverChanged(int32 SlotIndex);

	UFUNCTION()
	void HandleSlotsChanged();

	UFUNCTION()
	void HandleEquippedToolChanged(ASunsetCoveTool* NewTool, ASunsetCoveTool* OldTool, int32 SlotIndex);

	void BindInventory(UToolInventoryComponent* NewInventory);
	void UnbindInventory();

	// ---- Slices ----
	/** One slice as drawn: what the slot holds and its animation state. */
	struct FSliceVisual
	{
		FText Name;
		FString Initial;
		FSlateBrush IconBrush;
		/** Angle of the slice center, clockwise from straight up (degrees). */
		float CenterAngle = 0.0f;
		/** 0 = resting, 1 = fully popped out (targeted). */
		float PopAlpha = 0.0f;
		bool bOccupied = false;
		bool bHasIcon = false;
	};

	/** Rebuilds Slices from the inventory. bAllowIconLoad starts an async load of the icons not in memory yet. */
	void RebuildSlices(bool bAllowIconLoad);
	void BuildDesignPreview();
	void SetSliceIcon(int32 SliceIndex, UTexture2D* Texture);
	void HandleIconsLoaded();
	void SetMenuOpen(bool bOpen);

	// ---- Painting ----
	/** Appends an annulus segment (StartDeg..EndDeg, RadiusIn..RadiusOut around Center) to the mesh. */
	void AppendArcBand(TArray<FSlateVertex>& Vertices, TArray<SlateIndex>& Indices, const FSlateRenderTransform& SlateTransform,
		const FVector2f& Center, float StartDeg, float EndDeg, float RadiusIn, float RadiusOut, const FColor& Color) const;
	/** Draws a text centered on Position. */
	void DrawCenteredText(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& AllottedGeometry, const FString& Text,
		const FSlateFontInfo& Font, const FVector2f& Position, const FLinearColor& Color) const;
	/** The render resource of SliceBrush, refreshed when the brush's texture or material changes. */
	const FSlateResourceHandle& GetSliceResourceHandle() const;

	TArray<FSliceVisual> Slices;

	/** Keeps the icon textures of the slices alive (the brushes only point at them). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> SliceIconTextures;

	TSharedPtr<FStreamableHandle> IconLoadHandle;

	TWeakObjectPtr<UToolInventoryComponent> Inventory;

	mutable FSlateResourceHandle SliceResourceHandle;
	mutable TWeakObjectPtr<UObject> SliceResourceObject;

	/** Solid brush of the background tint. */
	FSlateBrush SolidBrush;

	int32 TargetedSlot = INDEX_NONE;
	int32 EquippedSlot = INDEX_NONE;

	/** 0 = closed, 1 = fully open. Drives the fade and the scale-in. */
	float OpenAlpha = 0.0f;

	bool bMenuOpen = false;
};
