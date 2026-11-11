// ToolRadialMenuWidget.cpp
// SunsetCove - UI System / HUD. See Public/UISystem/HUD/Widgets/ToolRadialMenuWidget.h.

#include "UISystem/HUD/Widgets/ToolRadialMenuWidget.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Tools/SunsetCoveTool.h"
#include "InventorySystem/ToolInventorySystem/ToolInventoryComponent.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "SunsetCoveHUD"

namespace
{
	/** Unit vector of an angle measured clockwise from straight up, in Slate space (X right, Y down). */
	FVector2f DirectionFromAngle(float AngleDegrees)
	{
		const float Radians = FMath::DegreesToRadians(AngleDegrees);
		return FVector2f(FMath::Sin(Radians), -FMath::Cos(Radians));
	}

	FLinearColor WithAlphaScaled(FLinearColor Color, float AlphaScale)
	{
		Color.A *= AlphaScale;
		return Color;
	}

	FString FirstLetter(const FText& Name)
	{
		const FString Text = Name.ToString().TrimStartAndEnd();
		return Text.IsEmpty() ? FString(TEXT("?")) : Text.Left(1).ToUpper();
	}
}

UToolRadialMenuWidget::UToolRadialMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningDedicatedServer())
	{
		// Plain white texture: the vertex colors give the slices their color. A designer can swap it for a texture or UI material.
		static ConstructorHelpers::FObjectFinder<UTexture2D> WhiteTexture(TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		if (WhiteTexture.Succeeded())
		{
			SliceBrush.SetResourceObject(WhiteTexture.Object);
		}

		static ConstructorHelpers::FObjectFinder<UObject> DefaultFont(*UWidget::GetDefaultFontName());
		NameFont = FSlateFontInfo(DefaultFont.Object, 18, FName(TEXT("Bold")));
		InitialFont = FSlateFontInfo(DefaultFont.Object, 26, FName(TEXT("Bold")));
	}
	SliceBrush.DrawAs = ESlateBrushDrawType::Image;
	SliceBrush.ImageSize = FVector2D(32.0, 32.0);

	SolidBrush.DrawAs = ESlateBrushDrawType::Image;
	SolidBrush.TintColor = FSlateColor(FLinearColor::White);

	DesignPreviewToolNames = {
		LOCTEXT("PreviewBucket", "Bucket"),
		LOCTEXT("PreviewSpray", "Deodorant Spray"),
		LOCTEXT("PreviewToy", "Toy"),
		LOCTEXT("PreviewGarbageBag", "Garbage Bag")
	};
}

// ----------------------------------------------------------------------
// UUserWidget
// ----------------------------------------------------------------------

void UToolRadialMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime())
	{
		BuildDesignPreview();
	}
}

void UToolRadialMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsDesignTime())
	{
		// Closed until the inventory says otherwise (BindInventory opens it if the menu is already held).
		OpenAlpha = 0.0f;
		if (!bMenuOpen)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UToolRadialMenuWidget::NativeDestruct()
{
	UnbindInventory();

	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	Super::NativeDestruct();
}

void UToolRadialMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (IsDesignTime())
	{
		return;
	}

	// The inventory is the source of truth: re-read the target every frame while open (events can be missed on a re-bind).
	if (const UToolInventoryComponent* InventoryComponent = Inventory.Get())
	{
		if (bMenuOpen)
		{
			TargetedSlot = InventoryComponent->GetHoveredSlotIndex();
		}
	}

	const float TargetOpen = bMenuOpen ? 1.0f : 0.0f;
	OpenAlpha = (OpenSpeed <= 0.0f) ? TargetOpen : FMath::FInterpTo(OpenAlpha, TargetOpen, InDeltaTime, OpenSpeed);

	if (!bMenuOpen && OpenAlpha <= 0.01f)
	{
		OpenAlpha = 0.0f;
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	for (int32 Index = 0; Index < Slices.Num(); ++Index)
	{
		FSliceVisual& Slice = Slices[Index];
		const float Target = (Index == TargetedSlot && Slice.bOccupied && bMenuOpen) ? 1.0f : 0.0f;
		Slice.PopAlpha = (AnimationSpeed <= 0.0f) ? Target : FMath::FInterpTo(Slice.PopAlpha, Target, InDeltaTime, AnimationSpeed);
	}
}

// ----------------------------------------------------------------------
// Binding to the Tool Inventory of the local pawn
// ----------------------------------------------------------------------

void UToolRadialMenuWidget::HandlePawnBound(APawn* Pawn)
{
	BindInventory(Pawn ? Pawn->FindComponentByClass<UToolInventoryComponent>() : nullptr);
}

void UToolRadialMenuWidget::HandlePawnUnbound(APawn* /*Pawn*/)
{
	UnbindInventory();
	SetMenuOpen(false);
}

void UToolRadialMenuWidget::BindInventory(UToolInventoryComponent* NewInventory)
{
	UnbindInventory();
	Inventory = NewInventory;
	if (!NewInventory)
	{
		return;
	}

	NewInventory->OnStateChanged.AddUniqueDynamic(this, &UToolRadialMenuWidget::HandleInventoryStateChanged);
	NewInventory->OnRadialHoverChanged.AddUniqueDynamic(this, &UToolRadialMenuWidget::HandleRadialHoverChanged);
	NewInventory->OnSlotsChanged.AddUniqueDynamic(this, &UToolRadialMenuWidget::HandleSlotsChanged);
	NewInventory->OnEquippedToolChanged.AddUniqueDynamic(this, &UToolRadialMenuWidget::HandleEquippedToolChanged);

	EquippedSlot = NewInventory->GetEquippedSlotIndex();
	TargetedSlot = NewInventory->GetHoveredSlotIndex();
	RebuildSlices(true);
	SetMenuOpen(NewInventory->IsRadialMenuOpen());
}

void UToolRadialMenuWidget::UnbindInventory()
{
	if (UToolInventoryComponent* OldInventory = Inventory.Get())
	{
		OldInventory->OnStateChanged.RemoveDynamic(this, &UToolRadialMenuWidget::HandleInventoryStateChanged);
		OldInventory->OnRadialHoverChanged.RemoveDynamic(this, &UToolRadialMenuWidget::HandleRadialHoverChanged);
		OldInventory->OnSlotsChanged.RemoveDynamic(this, &UToolRadialMenuWidget::HandleSlotsChanged);
		OldInventory->OnEquippedToolChanged.RemoveDynamic(this, &UToolRadialMenuWidget::HandleEquippedToolChanged);
	}
	Inventory.Reset();
}

void UToolRadialMenuWidget::HandleInventoryStateChanged(EToolInventoryState NewState, EToolInventoryState /*OldState*/)
{
	const bool bOpen = (NewState == EToolInventoryState::SelectingTool);
	if (bOpen)
	{
		// Fresh data on every open: names, icons and the slot count can have changed since the last time.
		if (const UToolInventoryComponent* InventoryComponent = Inventory.Get())
		{
			EquippedSlot = InventoryComponent->GetEquippedSlotIndex();
			TargetedSlot = InventoryComponent->GetHoveredSlotIndex();
		}
		RebuildSlices(true);
		for (FSliceVisual& Slice : Slices)
		{
			Slice.PopAlpha = 0.0f;
		}
	}
	SetMenuOpen(bOpen);
}

void UToolRadialMenuWidget::HandleRadialHoverChanged(int32 SlotIndex)
{
	const bool bChanged = (SlotIndex != TargetedSlot);
	TargetedSlot = SlotIndex;
	if (bChanged && SlotIndex != INDEX_NONE && HoverSoundCue.IsValid())
	{
		PlayUISound(HoverSoundCue);
	}
}

void UToolRadialMenuWidget::HandleSlotsChanged()
{
	RebuildSlices(true);
}

void UToolRadialMenuWidget::HandleEquippedToolChanged(ASunsetCoveTool* /*NewTool*/, ASunsetCoveTool* /*OldTool*/, int32 SlotIndex)
{
	EquippedSlot = SlotIndex;
}

void UToolRadialMenuWidget::SetMenuOpen(bool bOpen)
{
	bMenuOpen = bOpen;
	if (bOpen)
	{
		// Visible (so it ticks and paints) but never hit-testable: no cursor, no clicks, gameplay keeps the input.
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	// Closing: NativeTick fades the wheel out and collapses it.
}

// ----------------------------------------------------------------------
// Slices
// ----------------------------------------------------------------------

void UToolRadialMenuWidget::RefreshSlices()
{
	if (IsDesignTime())
	{
		BuildDesignPreview();
	}
	else
	{
		RebuildSlices(true);
	}
}

void UToolRadialMenuWidget::RebuildSlices(bool bAllowIconLoad)
{
	const UToolInventoryComponent* InventoryComponent = Inventory.Get();
	const int32 NumSlots = InventoryComponent ? InventoryComponent->GetNumSlots() : 0;

	// Keep the animation state when the slot count did not change (a pickup while the wheel is open).
	const bool bSameCount = (Slices.Num() == NumSlots);
	TArray<float> PreviousPops;
	if (bSameCount)
	{
		for (const FSliceVisual& Slice : Slices)
		{
			PreviousPops.Add(Slice.PopAlpha);
		}
	}

	Slices.SetNum(NumSlots);
	SliceIconTextures.Reset();
	SliceIconTextures.SetNum(NumSlots);

	TArray<FSoftObjectPath> IconsToLoad;
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		FSliceVisual& Slice = Slices[Index];
		Slice = FSliceVisual();
		Slice.PopAlpha = bSameCount ? PreviousPops[Index] : 0.0f;
		Slice.CenterAngle = InventoryComponent->GetRadialSlotAngle(Index);

		const ASunsetCoveTool* Tool = InventoryComponent->GetSlotTool(Index);
		Slice.bOccupied = (Tool != nullptr);
		if (!Tool)
		{
			continue;
		}

		Slice.Name = Tool->DisplayName.IsEmpty() ? FText::FromName(Tool->ToolId) : Tool->DisplayName;
		Slice.Initial = FirstLetter(Slice.Name);

		UTexture2D* IconTexture = Tool->Icon.Get();
		if (!IconTexture && !Tool->Icon.IsNull())
		{
			IconsToLoad.AddUnique(Tool->Icon.ToSoftObjectPath());
		}
		SetSliceIcon(Index, IconTexture ? IconTexture : PlaceholderIcon.Get());
	}

	if (bAllowIconLoad && IconsToLoad.Num() > 0)
	{
		if (IconLoadHandle.IsValid())
		{
			IconLoadHandle->CancelHandle();
		}
		IconLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			IconsToLoad, FStreamableDelegate::CreateUObject(this, &UToolRadialMenuWidget::HandleIconsLoaded));
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UToolRadialMenuWidget::HandleIconsLoaded()
{
	// The textures are in memory now; rebuild without asking again (a missing texture stays on the placeholder).
	RebuildSlices(false);
}

void UToolRadialMenuWidget::SetSliceIcon(int32 SliceIndex, UTexture2D* Texture)
{
	if (!Slices.IsValidIndex(SliceIndex))
	{
		return;
	}

	FSliceVisual& Slice = Slices[SliceIndex];
	Slice.bHasIcon = (Texture != nullptr);
	Slice.IconBrush = FSlateBrush();
	Slice.IconBrush.DrawAs = ESlateBrushDrawType::Image;
	Slice.IconBrush.ImageSize = FVector2D(IconSize, IconSize);
	Slice.IconBrush.SetResourceObject(Texture);

	if (SliceIconTextures.IsValidIndex(SliceIndex))
	{
		SliceIconTextures[SliceIndex] = Texture;
	}
}

void UToolRadialMenuWidget::BuildDesignPreview()
{
	const int32 NumSlots = FMath::Clamp(DesignPreviewSlotCount, 1, 12);
	const float SliceWidth = 360.0f / static_cast<float>(NumSlots);

	Slices.SetNum(NumSlots);
	SliceIconTextures.Reset();
	SliceIconTextures.SetNum(NumSlots);

	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		FSliceVisual& Slice = Slices[Index];
		Slice = FSliceVisual();
		Slice.CenterAngle = SliceWidth * static_cast<float>(Index);
		Slice.bOccupied = DesignPreviewToolNames.IsValidIndex(Index);
		Slice.PopAlpha = (Index == DesignPreviewTargetedSlot && Slice.bOccupied) ? 1.0f : 0.0f;
		if (Slice.bOccupied)
		{
			Slice.Name = DesignPreviewToolNames[Index];
			Slice.Initial = FirstLetter(Slice.Name);
			UTexture2D* PreviewIcon = DesignPreviewIcons.IsValidIndex(Index) ? DesignPreviewIcons[Index].Get() : nullptr;
			SetSliceIcon(Index, PreviewIcon ? PreviewIcon : PlaceholderIcon.Get());
		}
	}

	TargetedSlot = DesignPreviewTargetedSlot;
	EquippedSlot = DesignPreviewEquippedSlot;
	OpenAlpha = 1.0f;
	bMenuOpen = true;
}

// ----------------------------------------------------------------------
// Painting
// ----------------------------------------------------------------------

const FSlateResourceHandle& UToolRadialMenuWidget::GetSliceResourceHandle() const
{
	UObject* ResourceObject = SliceBrush.GetResourceObject();
	if (!SliceResourceHandle.IsValid() || SliceResourceObject.Get() != ResourceObject)
	{
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			SliceResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(SliceBrush);
		}
		SliceResourceObject = ResourceObject;
	}
	return SliceResourceHandle;
}

void UToolRadialMenuWidget::AppendArcBand(TArray<FSlateVertex>& Vertices, TArray<SlateIndex>& Indices, const FSlateRenderTransform& SlateTransform,
	const FVector2f& Center, float StartDeg, float EndDeg, float RadiusIn, float RadiusOut, const FColor& Color) const
{
	const float Span = EndDeg - StartDeg;
	if (Span <= 0.0f || RadiusOut <= RadiusIn)
	{
		return;
	}

	// A strip of quads along the arc: one inner and one outer vertex per step.
	const int32 Steps = FMath::Max(2, FMath::CeilToInt(Span / FMath::Max(0.5f, ArcResolutionDegrees)));
	const SlateIndex BaseIndex = static_cast<SlateIndex>(Vertices.Num());

	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float U = static_cast<float>(Step) / static_cast<float>(Steps);
		const FVector2f Direction = DirectionFromAngle(StartDeg + Span * U);
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(SlateTransform, Center + Direction * RadiusIn, FVector2f(U, 0.0f), Color));
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(SlateTransform, Center + Direction * RadiusOut, FVector2f(U, 1.0f), Color));
	}

	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const SlateIndex Inner = BaseIndex + static_cast<SlateIndex>(Step * 2);
		const SlateIndex Outer = Inner + 1;
		const SlateIndex NextInner = Inner + 2;
		const SlateIndex NextOuter = Inner + 3;
		Indices.Append({ Inner, Outer, NextOuter, Inner, NextOuter, NextInner });
	}
}

void UToolRadialMenuWidget::DrawCenteredText(FSlateWindowElementList& OutDrawElements, int32 Layer, const FGeometry& AllottedGeometry,
	const FString& Text, const FSlateFontInfo& Font, const FVector2f& Position, const FLinearColor& Color) const
{
	if (Text.IsEmpty() || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return;
	}

	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2f TextSize = FontMeasure->Measure(Text, Font);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		Layer,
		AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position - TextSize * 0.5f)),
		Text,
		Font,
		ESlateDrawEffect::None,
		Color);
}

int32 UToolRadialMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const int32 NumSlices = Slices.Num();
	const float Alpha = FMath::Clamp(OpenAlpha, 0.0f, 1.0f) * InWidgetStyle.GetColorAndOpacityTint().A;
	if (NumSlices == 0 || Alpha <= KINDA_SMALL_NUMBER || !HasUsableGeometry(AllottedGeometry))
	{
		return MaxLayer;
	}

	const FVector2f Center = FVector2f(AllottedGeometry.GetLocalSize()) * 0.5f;
	const FSlateRenderTransform& SlateTransform = AllottedGeometry.GetAccumulatedRenderTransform();

	// The whole wheel grows from OpenStartScale to 1 while it opens.
	const float Scale = FMath::Lerp(OpenStartScale, 1.0f, Alpha);
	const float BaseInner = InnerRadius * Scale;
	const float BaseOuter = FMath::Max(OuterRadius, InnerRadius + 1.0f) * Scale;

	const float SliceWidth = 360.0f / static_cast<float>(NumSlices);
	const float HalfGap = FMath::Min(SliceGapDegrees * 0.5f, SliceWidth * 0.25f);

	// Per-slice geometry, computed once and used by the mesh, the outlines and the icons.
	struct FSliceGeometry
	{
		FVector2f Center;
		float StartDeg;
		float EndDeg;
		float RadiusIn;
		float RadiusOut;
	};
	TArray<FSliceGeometry, TInlineAllocator<12>> Geometry;
	Geometry.SetNum(NumSlices);
	for (int32 Index = 0; Index < NumSlices; ++Index)
	{
		const FSliceVisual& Slice = Slices[Index];
		FSliceGeometry& SliceGeometry = Geometry[Index];
		SliceGeometry.Center = Center + DirectionFromAngle(Slice.CenterAngle) * (HoverOffset * Scale * Slice.PopAlpha);
		SliceGeometry.StartDeg = Slice.CenterAngle - SliceWidth * 0.5f + HalfGap;
		SliceGeometry.EndDeg = Slice.CenterAngle + SliceWidth * 0.5f - HalfGap;
		SliceGeometry.RadiusIn = BaseInner;
		SliceGeometry.RadiusOut = BaseOuter * FMath::Lerp(1.0f, HoverOuterRadiusScale, Slice.PopAlpha);
	}

	int32 Layer = MaxLayer + 1;

	// 1. Background tint.
	if (BackgroundDimColor.A > 0.0f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), &SolidBrush, ESlateDrawEffect::None,
			WithAlphaScaled(BackgroundDimColor, Alpha));
	}
	++Layer;

	// 2. Slices (one mesh, one draw call) and the equipped marker on top of its slice.
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		const int32 StepsPerSlice = FMath::CeilToInt(SliceWidth / FMath::Max(0.5f, ArcResolutionDegrees)) + 2;
		Vertices.Reserve(NumSlices * StepsPerSlice * 2 + StepsPerSlice * 2);
		Indices.Reserve(NumSlices * StepsPerSlice * 6 + StepsPerSlice * 6);

		for (int32 Index = 0; Index < NumSlices; ++Index)
		{
			const FSliceVisual& Slice = Slices[Index];
			const FSliceGeometry& SliceGeometry = Geometry[Index];
			const FLinearColor BaseColor = Slice.bOccupied
				? FMath::Lerp(SliceColor, HoveredSliceColor, Slice.PopAlpha)
				: EmptySliceColor;
			AppendArcBand(Vertices, Indices, SlateTransform, SliceGeometry.Center, SliceGeometry.StartDeg, SliceGeometry.EndDeg,
				SliceGeometry.RadiusIn, SliceGeometry.RadiusOut, WithAlphaScaled(BaseColor, Alpha).ToFColor(true));
		}

		if (Geometry.IsValidIndex(EquippedSlot) && Slices[EquippedSlot].bOccupied && EquippedMarkerThickness > 0.0f && EquippedMarkerColor.A > 0.0f)
		{
			const FSliceGeometry& SliceGeometry = Geometry[EquippedSlot];
			const float MarkerOuter = SliceGeometry.RadiusOut;
			const float MarkerInner = FMath::Max(SliceGeometry.RadiusIn, MarkerOuter - EquippedMarkerThickness * Scale);
			AppendArcBand(Vertices, Indices, SlateTransform, SliceGeometry.Center, SliceGeometry.StartDeg, SliceGeometry.EndDeg,
				MarkerInner, MarkerOuter, WithAlphaScaled(EquippedMarkerColor, Alpha).ToFColor(true));
		}

		if (Vertices.Num() > 0)
		{
			FSlateDrawElement::MakeCustomVerts(OutDrawElements, Layer, GetSliceResourceHandle(), Vertices, Indices, nullptr, 0, 0);
		}
	}
	++Layer;

	// 3. Anti-aliased outlines (they also smooth the edges of the mesh).
	if (OutlineThickness > 0.0f && OutlineColor.A > 0.0f)
	{
		const FLinearColor LineColor = WithAlphaScaled(OutlineColor, Alpha);
		for (const FSliceGeometry& SliceGeometry : Geometry)
		{
			const float Span = SliceGeometry.EndDeg - SliceGeometry.StartDeg;
			const int32 Steps = FMath::Max(2, FMath::CeilToInt(Span / FMath::Max(0.5f, ArcResolutionDegrees)));
			TArray<FVector2f> Points;
			Points.Reserve(Steps * 2 + 3);
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float Angle = SliceGeometry.StartDeg + Span * static_cast<float>(Step) / static_cast<float>(Steps);
				Points.Add(SliceGeometry.Center + DirectionFromAngle(Angle) * SliceGeometry.RadiusOut);
			}
			for (int32 Step = Steps; Step >= 0; --Step)
			{
				const float Angle = SliceGeometry.StartDeg + Span * static_cast<float>(Step) / static_cast<float>(Steps);
				Points.Add(SliceGeometry.Center + DirectionFromAngle(Angle) * SliceGeometry.RadiusIn);
			}
			// Close the loop. Copy first: adding an element of the array to the same array is not allowed (it may reallocate).
			const FVector2f FirstPoint = Points[0];
			Points.Add(FirstPoint);
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
				ESlateDrawEffect::None, LineColor, true, OutlineThickness);
		}
	}
	++Layer;

	// 4. Icons (or initials) and the name of the targeted tool.
	for (int32 Index = 0; Index < NumSlices; ++Index)
	{
		const FSliceVisual& Slice = Slices[Index];
		if (!Slice.bOccupied)
		{
			continue;
		}

		const FSliceGeometry& SliceGeometry = Geometry[Index];
		const FVector2f IconCenter = SliceGeometry.Center
			+ DirectionFromAngle(Slice.CenterAngle) * FMath::Lerp(SliceGeometry.RadiusIn, SliceGeometry.RadiusOut, IconRadiusAlpha);
		const FLinearColor Tint = WithAlphaScaled(FMath::Lerp(IconColor, HoveredIconColor, Slice.PopAlpha), Alpha);

		if (Slice.bHasIcon)
		{
			const float Size = IconSize * Scale * FMath::Lerp(1.0f, HoverIconScale, Slice.PopAlpha);
			const FVector2f IconExtent(Size, Size);
			FSlateDrawElement::MakeBox(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(IconExtent, FSlateLayoutTransform(IconCenter - IconExtent * 0.5f)),
				&Slice.IconBrush, ESlateDrawEffect::None, Tint);
		}
		else
		{
			DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, Slice.Initial, InitialFont, IconCenter, Tint);
		}
	}

	if (bShowTargetedName && Slices.IsValidIndex(TargetedSlot) && Slices[TargetedSlot].bOccupied)
	{
		const FString Name = Slices[TargetedSlot].Name.ToString();
		DrawCenteredText(OutDrawElements, Layer, AllottedGeometry, bUppercaseName ? Name.ToUpper() : Name, NameFont, Center,
			WithAlphaScaled(NameColor, Alpha));
	}

	return Layer;
}

#undef LOCTEXT_NAMESPACE
