// HUDInteractionPromptWidget.h
// SunsetCove - UI System / HUD. The interaction prompt: "what can I do now, and with which key".
//
// WHAT IT SHOWS
//   One row per action the player can do right now, for example while looking at a sunbed:   [E]  Pick up
//   and while carrying it:                                                                   LMB  Place (Hold)
//                                                                                            RMB  Throw (Hold)
//                                                                                              Q  Drop
//   With a gamepad the same rows show the button icons ([X] Pick up). The rows switch the moment the player touches the
//   other device, and follow the player's remaps, with no code here: each row is a CoreFramework UInputPromptWidget
//   (Entry Class, WBP_InputPrompt), which asks the framework input system for the key of the row's Input Action on the device
//   in use and listens to its device / bindings events.
//
// WHERE THE ROWS COME FROM (this widget knows no gameplay system)
//   The Interaction component of the pawn (found by class) builds the list (FInteractionPrompt: action text, target text,
//   Input Action, trigger, available) from the aimed object and from the systems that registered a prompt provider (Prop
//   Inventory, Tool Inventory), and fires OnPromptsChanged. This widget only formats and draws that list. A new interactable
//   or a new system adds prompts without touching the HUD.
//
// HIERARCHY AND SETUP
//   USunsetCoveHUDElementWidget -> UHUDInteractionPromptWidget (this class) -> WBP_InteractionPrompt (Content/UI/HUD/Elements),
//   registered in DA_UIConfig > HUD Elements like every HUD element. The Widget Blueprint holds the layout: a panel named
//   PromptList (a Vertical Box) where the rows go, placed anywhere on the full-screen element. Without PromptList a default
//   layout (bottom center) is built at runtime.
//
// WHEN IT HIDES (it fades, FadeSpeed)
//   No prompts; a system asked USunsetCoveHUDSubsystem::SetInteractionPromptsHiddenBy; or (bHideWhileCursorVisible) the mouse
//   cursor is shown, which only happens while a menu is open.

#pragma once

#include "CoreMinimal.h"
#include "InteractionSystem/InteractionTypes.h"
#include "UISystem/HUD/Widgets/SunsetCoveHUDElementWidget.h"
#include "HUDInteractionPromptWidget.generated.h"

class UInputPromptWidget;
class UInteractionComponent;
class UPanelWidget;

UCLASS(Blueprintable)
class SUNSETCOVE_API UHUDInteractionPromptWidget : public USunsetCoveHUDElementWidget
{
	GENERATED_BODY()

public:
	UHUDInteractionPromptWidget(const FObjectInitializer& ObjectInitializer);

	// ------------------------------------------------------------------
	// Look (Class Defaults of WBP_InteractionPrompt)
	// ------------------------------------------------------------------

	/** The widget of one row: a CoreFramework Input Prompt (WBP_InputPrompt: GlyphImage, KeyText, LabelText). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt")
	TSubclassOf<UInputPromptWidget> EntryClass;

	/** Label of a "hold the input" row. {Label} = the action label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt")
	FText HoldLabelFormat;

	/** Label of a "let go of the held input" row. {Label} = the action label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt")
	FText ReleaseLabelFormat;

	/** Opacity of a row whose action is not possible right now (placing on an invalid spot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnavailableOpacity = 0.4f;

	/** At most this many rows are shown (the first ones by sort order). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt", meta = (ClampMin = "1", ClampMax = "12"))
	int32 MaxRows = 5;

	/** Rows whose action has no key on the device in use are collapsed (instead of showing a label without a key). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt")
	bool bHideRowsWithoutKey = true;

	/** Hide while the mouse cursor is shown (a menu is open). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt|Behavior")
	bool bHideWhileCursorVisible = true;

	/** How fast the prompt fades in and out. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction Prompt|Behavior", meta = (ClampMin = "0.0"))
	float FadeSpeed = 12.0f;

	/** The prompts shown now (empty while hidden for lack of prompts). */
	UFUNCTION(BlueprintPure, Category = "Interaction Prompt")
	const TArray<FInteractionPrompt>& GetShownPrompts() const { return ShownPrompts; }

protected:
	//~ Begin UUserWidget Interface
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface

	//~ Begin USunsetCoveHUDElementWidget
	virtual void HandlePawnBound(APawn* Pawn) override;
	virtual void HandlePawnUnbound(APawn* Pawn) override;
	//~ End USunsetCoveHUDElementWidget

	/** After the rows were rebuilt (Prompts can be empty). For custom visuals or sounds in the Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction Prompt", meta = (DisplayName = "On Prompts Changed"))
	void BP_OnPromptsChanged(const TArray<FInteractionPrompt>& Prompts);

	/** Where the rows go (a Vertical Box in WBP_InteractionPrompt). Optional: a default layout is built when missing. */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PromptList;

private:
	UFUNCTION()
	void HandlePromptsChanged(const TArray<FInteractionPrompt>& Prompts);

	/** Fills the rows from Prompts (keeps the old rows while fading out to nothing). */
	void ApplyPrompts(const TArray<FInteractionPrompt>& Prompts);

	/** Row Index of the pool, created when needed. Null without Entry Class. */
	UInputPromptWidget* GetOrCreateRow(int32 Index);

	FText FormatLabel(const FInteractionPrompt& Prompt) const;

	/** All the hide rules together. */
	bool ShouldBeVisible() const;

	/** Builds an Overlay + bottom-center Vertical Box when the Blueprint has no PromptList. */
	void BuildDefaultLayout();

	/** Shows two sample rows in the UMG designer. */
	void BuildDesignerPreview();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputPromptWidget>> Rows;

	UPROPERTY(Transient)
	TArray<FInteractionPrompt> ShownPrompts;

	TWeakObjectPtr<UInteractionComponent> Interaction;

	float VisibilityAlpha = 0.0f;
	bool bWarnedMissingEntryClass = false;
};
