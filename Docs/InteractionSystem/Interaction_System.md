# Interaction System

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Module:** `SunsetCove`
**Source of truth:** GDD, *Controlli* ("Interagire / raccogliere": `E`, "interazione contestuale con NPC, oggetti e sbarra").
**Audience:** programmers (sections 1-4) and designers who add things to interact with (sections 5-6).
**Revision 2** (2026-11-12: interaction prompts, section 9).

---

## 1. Purpose and boundaries

"Look at something and press **E**." The Interaction System answers *what is the player aiming at, and what happens when Interact is pressed?*. It is generic: it knows nothing about props, inventories, the barrier or the bed.

| It does | It does NOT do |
|---|---|
| trace from the camera, keep a **focus** (the interactable actor under the crosshair) | know what a prop, a hand or a sunbed is |
| call `Interact` on the focused actor, or ask a registered **handler** to deal with it | draw the prompt (the HUD element `WBP_InteractionPrompt` does) |
| own the Interact Input Action | decide *what* each interaction does |
| build the **prompt list**: what the player can do now and with which Input Action (section 9) | know any key or device (the HUD asks the CoreFramework input system) |

---

## 2. Files

```
Source/SunsetCove/
├── Public/Interfaces/InteractableInterface.h        IInteractableInterface
├── Public/InteractionSystem/
│   ├── InteractionComponent.h                       UInteractionComponent
│   └── InteractionTypes.h                           FInteractionCanHandleDelegate, FInteractionHandleDelegate, FInteractionHandler
├── Private/InteractionSystem/InteractionComponent.cpp
└── Public/Core/Utils/SunsetCoveViewUtils.h          shared camera view point helper
```

`ASunsetCoveCharacter` creates the component (`Interaction`), calls `Interaction->SetupInput()` from `SetupSunsetCoveInput`, and wires the Prop Inventory and the Tool Inventory to it. Nothing in `Plugins/CoreFramework` is involved.

---

## 3. How it works

### 3.1 Two ways to be interactable

1. **The actor implements `IInteractableInterface`** (Blueprint: Class Settings > Interfaces > *Interactable Interface*). Use this for the barrier button, the laptop, the bed.
2. **A system registers a handler** (`RegisterInteractionHandler(Id, CanHandle, Handle)`). `CanHandle(Target)` says whether the handler can deal with an actor (it runs while the player aims, so keep it cheap); `Handle(Target)` does it and returns `true` if it did. The **Prop Inventory** uses this to grab props and the **Tool Inventory** uses it to pick up tools (`ToolInventory.Pickup`), so neither a prop nor a tool needs the interactable interface and this system does not need to know what a prop or a tool is.

The interface is tried first, then the handlers in registration order. The first that handles it wins.

### 3.2 Focus

While the owner is a locally controlled pawn, every `FocusUpdateInterval` seconds (default 0.05, `0` = every frame) a sphere sweep (radius `TraceRadius`, or a line if 0) runs from the camera along the view for `InteractionRange` cm on `TraceChannel` (Visibility). The **first** thing it hits is the candidate. It becomes the **focus** only if it is interactable: implements the interface and `IsInteractionBlocked` is false, or some handler's `CanHandle` accepts it. A wall in front of an interactable therefore means *no focus*.

`OnFocusChanged(NewFocus, OldFocus)` fires when the focus changes. `GetFocusedActor()` and `GetFocusedPrompt()` answer on demand (the prompt text comes from `GetInteractionPrompt` of the interface). Right after each focus update the prompt list is rebuilt (section 9).

### 3.3 Interact

`Interact()` (what the input calls; also `BlueprintCallable`): runs a **fresh** trace (the press must act on what the player aims at *now*, not on a focus up to 50 ms old), then:

1. if the target implements the interface and is not blocked: `Interact(Interactor)` is called on it;
2. otherwise the handlers are tried in order on a copy of the list;
3. `OnInteracted(Target, bHandled)` fires, and the focus is refreshed (a grabbed prop is gone from the world).

The interactor passed to the interface is the player's pawn.

### 3.4 Input

`InteractAction` (property of the component) is bound to **Started** in `SetupInput`. Mapping: `E` in the project mapping context (`SunsetCoveMappingContext`). If the action is missing, one warning is logged and nothing is bound.

---

## 4. Parameters (`BP_Character` > Components > Interaction)

| Category | Property | Default | Meaning |
|---|---|---|---|
| `Interaction\|Input` | `InteractAction` | none | the Interact Input Action |
| `Interaction\|Trace` | `InteractionRange` | 250 cm | reach |
| | `TraceRadius` | 12 cm | sphere radius; 0 = thin line. A little radius makes small things easier to aim at |
| | `TraceChannel` | Visibility | interactable actors must **block** it |
| | `FocusUpdateInterval` | 0.05 s | focus refresh period; 0 = every frame |
| `Interaction\|Debug` | `bDrawDebug` | false | green line = focused, orange = hit but not interactable, red = nothing |

Events: `OnFocusChanged(NewFocus, OldFocus)`, `OnInteracted(Target, bHandled)`.

---

## 5. Setup in the editor

1. Create `IA_Interact`: **Input Action**, Value Type **Digital (bool)**, **no triggers**.
2. Add it to the project mapping context (`IMC_SunsetCove`, the one assigned as **Sunset Cove Mapping Context** on `BP_Character`): `E`, and on controller *Gamepad Face Button Bottom* (A / Cross, GDD proposal).
3. `BP_Character` > Components > **Interaction** > **Interact Action** = `IA_Interact`.

The full walk-through, including the mapping context and key conflicts, is in `../InventorySystem/PropInventorySystem/Engine_Setup_Guide.md`, sections 3-5 (and, for the tools, `../InventorySystem/ToolInventorySystem/Engine_Setup_Guide.md`).

---

## 6. Making an actor interactable (Blueprint example: a button next to the barrier)

1. Actor Blueprint with a mesh that **blocks the Visibility channel** (a normal static mesh with the default collision preset does).
2. Class Settings > Interfaces > Add > **Interactable Interface**. Compile.
3. In *My Blueprint* > Interfaces, implement:
   - **Interact (Interactor)**: your logic (for example tell the WorkdayManager to start the day).
   - **Is Interaction Blocked (Interactor)**: return `true` while it must not be used (return value left unimplemented = *not blocked*).
   - **Get Interaction Prompt**: the action text of the HUD prompt ("Open the barrier"). It is shown with the key of the Interact action on the device in use. Empty = no prompt.

C++: `class AMyActor : public AActor, public IInteractableInterface` and override `Interact_Implementation(AActor*)`, `IsInteractionBlocked_Implementation(AActor*) const`, `GetInteractionPrompt_Implementation() const`. Always call interface events through `IInteractableInterface::Execute_*`.

> **Why "blocked" and not "can interact"?** A Blueprint that adds an interface but does not implement a function with a return value gets the default of the type, `false`. With "blocked" semantics the default is permissive.

---

## 7. Adding a handler from C++ (how the Prop Inventory does it)

```cpp
Interaction->RegisterInteractionHandler(
    TEXT("MySystem.Thing"),
    FInteractionCanHandleDelegate::CreateUObject(this, &UMySystem::CanHandle),   // bool CanHandle(AActor* Target)
    FInteractionHandleDelegate::CreateUObject(this, &UMySystem::Handle));        // bool Handle(AActor* Target)
// and in EndPlay:
Interaction->UnregisterInteractionHandler(TEXT("MySystem.Thing"));
```

The component that registers must be wired to the Interaction component by the character (the composition root), so the two systems never include each other's headers in the wrong direction.

---

## 8. Assumptions and limits

- Single player: no replication, no RPC. The trace and focus run only for a locally controlled pawn.
- The first thing under the crosshair decides. There is no "interact through" and no priority between overlapping interactables.
- Camera: the view point comes from `AController::GetPlayerViewPoint`, which returns the camera that is actually rendered (the framework's managed camera included). In third person the trace starts at the (behind-the-character) camera.
- Verified in PIE on 2026-11-12 (focus, prompts, device switch keyboard / gamepad). Use `Draw Debug` to tune the trace.

---

## 9. Interaction prompts ("[E] Pick up")

### 9.1 What the player sees

A small list at the bottom center of the screen of **what can be done right now, and with which input**:

| Situation | Rows (keyboard / mouse, default keys) | Same rows with a gamepad |
|---|---|---|
| looking at a sunbed, hands free | `[E]  Pick up` | `[X]  Pick up` |
| carrying a prop | `[LMB]  Place (Hold)` · `[RMB]  Throw (Hold)` · `[Q]  Drop` | `[RT] Place (Hold)` · `[LT] Throw (Hold)` · `[B] Drop` |
| holding Place (hologram) | `[LMB]  Place (Release)` (dimmed on an invalid spot) · `[Wheel]  Rotate` | `[RT]` · `[RB]` |
| a tool equipped, aiming at something that tool works on (a seagull, a crab for the bucket) | `[LMB]  Use` (or the tool's own verb) | `[RT]  Use` (never shown while aiming at nothing usable) |
| tool wheel open, a tool selected | `[Q]  Drop` | `[B]  Drop` |
| an actor with the interactable interface | `[E]  <its Get Interaction Prompt text>` | `[X]  ...` |

`[...]` = a key icon: the dark keyboard glyph set on keyboard / mouse (text only for a key without a glyph), the controller
family's buttons on gamepad. No background, no object name (minimal by design, 2026-11-12). The keys are never written anywhere: each row names an **Input Action** and the HUD asks the CoreFramework input system for the key
of that action **on the device the player is using**, remaps included. Touching the gamepad switches every row to button icons at
once (and back on keyboard / mouse), and a remap in Options > Controls shows up immediately.

### 9.2 Data flow

```
 aimed actor ──► UInteractionComponent ◄── prompt providers (PropInventory.Carry, ToolInventory.Tool)
                     │  BuildPrompts() after every focus update (FocusUpdateInterval)
                     │  OnPromptsChanged(TArray<FInteractionPrompt>)  — only when the list really changed
                     ▼
            UHUDInteractionPromptWidget (WBP_InteractionPrompt, a HUD element of DA_UIConfig)
                     │  one row per prompt: UInputPromptWidget (CoreFramework) = WBP_InteractionPromptRow
                     ▼
            UInputFrameSubsystem (CoreFramework): key of the Input Action on the current device + glyph,
            OnInputDeviceChanged / OnInputBindingsChanged refresh every row by themselves
```

`FInteractionPrompt` (`InteractionTypes.h`): `ActionText` ("Pick up"; no object name on purpose: the player is looking at the object), `InputAction` (null =
the Interact action), `Trigger` (Press / Hold / Release, worded by the widget), `bAvailable` (false = dimmed), `SortOrder`
(0 = the aimed object, 100+ = the hands, 200+ = tools).

Where the rows come from:

1. **The focus.** The interface's `GetInteractionPrompt`, otherwise the **third delegate** of the handler that accepts the focus:
   `RegisterInteractionHandler(Id, CanHandle, Handle, Prompt)`. The Prop / Tool Inventory give "Pick up".
2. **Context providers**: `RegisterPromptProvider(Id, Delegate)`, called at every rebuild whatever is aimed at. The Prop Inventory
   lists Place / Throw / Drop / Rotate by state; the Tool Inventory lists Drop on the tool selected in the wheel, and **Use only
   while the equipped tool aims at a valid target** (`ASunsetCoveTool::FindUseTarget` finds an actor with the Tool Target
   Interface that does not refuse this tool, and the tool is not `IsUseBlocked`; never while the hands hold props).

### 9.3 Adding prompts for a new system (C++)

```cpp
// A new interaction: give the prompt with the handler (InputAction left null = the Interact key).
Interaction->RegisterInteractionHandler(TEXT("MySystem.Thing"), CanHandle, Handle,
    FInteractionTargetPromptDelegate::CreateUObject(this, &UMySystem::GetPrompts));   // void GetPrompts(AActor*, TArray<FInteractionPrompt>&)

// Something the player can do whatever is aimed at (an action of what is in the hand):
Interaction->RegisterPromptProvider(TEXT("MySystem.Hand"),
    FInteractionContextPromptDelegate::CreateUObject(this, &UMySystem::GetHandPrompts)); // void GetHandPrompts(TArray<FInteractionPrompt>&)
// EndPlay: UnregisterInteractionHandler / UnregisterPromptProvider.
```

Rules: providers are polled a few times per second, so keep them cheap and free of side effects; set the `InputAction` of every
context prompt (one with no action is dropped); put the verbs in editable `FText` properties (the inventories have a
`Prompts` category), never in code paths. A Blueprint interactable only implements `Get Interaction Prompt` (section 6).

### 9.4 The HUD side (where to change the look)

| What | Where |
|---|---|
| position of the list | `WBP_InteractionPrompt` > `PromptList` (Vertical Box) slot: bottom center, 190 px above the edge |
| look of one row | `WBP_InteractionPromptRow` (parent CoreFramework `InputPromptWidget`, no background): `GlyphImage` (26 px, keyboard keys too: Keyboard Display = Glyph, dark set from `BP_InputData_KeyboardMouse`), `KeyText` (fallback when a key has no glyph) and `LabelText` = `BP_TextStyle_HUD_Prompt` (18, white, shadow) |
| wording | `WBP_InteractionPrompt` Class Defaults: `Hold / Release Label Format`, `Max Rows`, `Unavailable Opacity`, `Hide Rows Without Key` |
| verbs | `BP_Character` > Prop Inventory / Tool Inventory > **Prompts**; per tool `Use Prompt Text` |
| keyboard keys as text or icons | Project Settings > CoreFramework Input > `Keyboard Prompt Display` (SunsetCove: `KeyText`) |
| hide for a cutscene | `USunsetCoveHUDSubsystem::SetInteractionPromptsHiddenBy(Reason, true/false)` |

The prompt also hides by itself while a menu shows the mouse cursor.
