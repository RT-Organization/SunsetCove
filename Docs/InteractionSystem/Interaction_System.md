# Interaction System

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Module:** `SunsetCove`
**Source of truth:** GDD, *Controlli* ("Interagire / raccogliere": `E`, "interazione contestuale con NPC, oggetti e sbarra").
**Audience:** programmers (sections 1-4) and designers who add things to interact with (sections 5-6).
**Revision 1.**

---

## 1. Purpose and boundaries

"Look at something and press **E**." The Interaction System answers *what is the player aiming at, and what happens when Interact is pressed?*. It is generic: it knows nothing about props, inventories, the barrier or the bed.

| It does | It does NOT do |
|---|---|
| trace from the camera, keep a **focus** (the interactable actor under the crosshair) | know what a prop, a hand or a sunbed is |
| call `Interact` on the focused actor, or ask a registered **handler** to deal with it | draw a prompt (it only provides the focus event and the prompt text) |
| own the Interact Input Action | decide *what* each interaction does |

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

`OnFocusChanged(NewFocus, OldFocus)` fires when the focus changes (the hook for the future prompt widget). `GetFocusedActor()` and `GetFocusedPrompt()` answer on demand (the prompt text comes from `GetInteractionPrompt` of the interface; handlers give none).

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
   - **Get Interaction Prompt**: the text for the future prompt widget.

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
- **Not run in the editor** by the author: check the focus with `Draw Debug` on first use.
