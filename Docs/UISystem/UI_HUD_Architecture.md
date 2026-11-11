# UI / HUD Architecture — Phase 1 (crosshair, suggestion feed, tool radial menu)

**Project:** Sunset Cove | **Engine:** Unreal Engine 5.8 | **Code:** `Source/SunsetCove/Public/UISystem/HUD/` (+ `Private/`)
**Built on:** the CoreFramework UI system (`Plugins/CoreFramework`, read only). Its own docs: `Plugins/CoreFramework/Docs/UISystem/`.

---

## 1. System overview

The HUD is **one container plus independent elements**. Nothing is placed by hand inside `WBP_HUD`:

| Piece | Asset | C++ parent | Job |
|---|---|---|---|
| HUD container | `Content/UI/HUD/WBP_HUD` | `UBaseHUDWidget` (CoreFramework) | full-screen Overlay `DefaultElementSlot` that receives the elements |
| Crosshair | `Content/UI/HUD/Elements/WBP_Crosshair` | `UHUDCrosshairWidget` | white dot, exact screen center, drawn in code |
| Suggestion feed | `Content/UI/HUD/Elements/WBP_NotificationFeed` | `UHUDNotificationFeedWidget` | gray text lines, top-left, fade in / out |
| Tool radial menu | `Content/UI/HUD/Elements/WBP_ToolRadialMenu` | `UToolRadialMenuWidget` | GTA-style wheel, one slice per tool slot, drawn in code |
| Message hub | (no asset) | `USunsetCoveHUDSubsystem` (Game Instance Subsystem) | game systems ask for messages / crosshair hiding here |
| Messages | `Content/UI/HUD/Data/DT_HUDNotifications` | row struct `FHUDNotificationRow` | text of every message, by id |
| Settings | Project Settings > Game > **Sunset Cove UI** | `USunsetCoveUISettings` | which notification table to use (`DefaultGame.ini`) |

All three elements derive from `USunsetCoveHUDElementWidget` (→ `UBaseUserWidget`), which stretches each element over the whole slot and gives it the local player's pawn.

### How the HUD starts

```
L_xxx map → GameMode HUD class BP_Startup_Gameplay (ABaseHUD, CoreFramework, Startup Screens = [UI.Screen.HUD])
  └ UUIFrameSubsystem loads DA_UIConfig, builds the root layout, pushes UI.Screen.HUD
      (DA_UIConfig > Screens: UI.Screen.HUD → WBP_HUD on CoreFramework.UI.Layer.Game, preloaded; input Game Only)
      └ WBP_HUD.NativeConstruct registers DefaultElementSlot as CoreFramework.UI.HUDSlot.Default
          └ for each entry of DA_UIConfig > HUD Elements: async load the class, CreateWidget, add to the slot
              Crosshair → NotificationFeed → ToolRadialMenu      (array order = draw order, last on top)
                └ each element registers with the CoreFramework binding hub → OnUIPawnBound(pawn)
                    the element finds what it needs on the pawn BY CLASS (UToolInventoryComponent) — never the character class
```

Respawn / possess / late HUD creation are handled by the binding hub (Unbound then Bound again).

**Screen registry (`DA_UIConfig` > Screens).** Every screen pushed by tag must be listed there, otherwise the log says `PushScreen: no screen registered for tag ...` and nothing appears. Registered today:

| Screen tag | Widget | Layer |
|---|---|---|
| `UI.Screen.HUD` | `WBP_HUD` (preload + keep resident) | `CoreFramework.UI.Layer.Game` |
| `UI.Screen.MainMenu` | `WBP_MainMenu` | `CoreFramework.UI.Layer.Menu` |
| `UI.Screen.PauseMenu` | `WBP_PauseMenu` | `CoreFramework.UI.Layer.Menu` |
| `UI.Screen.Settings` | `WBP_Settings` | `CoreFramework.UI.Layer.Menu` |
| `UI.Screen.ConfirmPopup` | `WBP_Popup_Confirm` | `CoreFramework.UI.Layer.Modal` |

---

## 2. Connections and data flow

### 2.1 Who talks to whom

```
                         ┌─────────────────────────── ASunsetCoveCharacter (composition root) ──────────────────────────┐
                         │ ToolInventory->OnActionRefused ──► OnToolActionRefused ──┐                                    │
                         │ ToolInventory->OnToolAction(Pickup) ─► OnToolInventoryAction ─┐                               │
                         │ Input_Look ──(menu open)──► ToolInventory->AddRadialInput     │                               │
                         └───────────────────────────────────────────────────────────────┼───────────────────────────────┘
                                                                                         ▼ ShowNotificationById(Id)
 any system (events, workday, adversities, Blueprints) ──────────────────────► USunsetCoveHUDSubsystem (Game Instance)
                                                                                 │ looks Id up in DT_HUDNotifications
                                                                                 │ OnNotificationRequested ─────────► WBP_NotificationFeed
                                                                                 │ SetCrosshairHiddenBy / IsCrosshairHiddenByRequest ─► WBP_Crosshair
 UToolInventoryComponent (on the pawn) ── OnStateChanged / OnRadialHoverChanged / OnSlotsChanged / OnEquippedToolChanged ─► WBP_ToolRadialMenu
                                       ── IsRadialMenuOpen ───────────────────────────────────────────────────────────────► WBP_Crosshair
```

### 2.2 Exact bindings

| From | Delegate / call | To | When |
|---|---|---|---|
| CoreFramework binding hub | `IBaseUIBinderInterface::OnUIPawnBound / OnUIPawnUnbound` | all three elements (`HandlePawnBound`) | pawn possessed, respawned, element created late |
| `UToolInventoryComponent` | `OnStateChanged(New, Old)` | `UToolRadialMenuWidget::HandleInventoryStateChanged` | `SelectingTool` = open (rebuild slices, fade in); `Idle` = fade out |
| `UToolInventoryComponent` | `OnRadialHoverChanged(Slot)` | `UToolRadialMenuWidget::HandleRadialHoverChanged` | targeted slice changed (plus optional `HoverSoundCue`) |
| `UToolInventoryComponent` | `OnSlotsChanged()` | `UToolRadialMenuWidget::HandleSlotsChanged` | pickup / drop: slices rebuilt |
| `UToolInventoryComponent` | `OnEquippedToolChanged(New, Old, Slot)` | `UToolRadialMenuWidget::HandleEquippedToolChanged` | equipped marker moves |
| `UToolInventoryComponent` | `OnActionRefused(Reason, Tool)` | `ASunsetCoveCharacter::OnToolActionRefused` → `ShowNotificationById(ToolRefusalNotifications[Reason])` | hands busy, no tools, full, duplicate, blocked |
| `UToolInventoryComponent` | `OnToolAction(Pickup, ...)` | `ASunsetCoveCharacter::OnToolInventoryAction` → `ShowNotificationById(ToolPickupTipNotification)` | first pickup (row is "show once") |
| `USunsetCoveHUDSubsystem` | `OnNotificationRequested(FHUDNotification)` | `UHUDNotificationFeedWidget::HandleNotificationRequested` | any message |
| `USunsetCoveHUDSubsystem` | `TakePendingNotifications()` | feed, on construct | messages sent before the HUD finished loading |
| `APlayerController` | `ShouldShowMouseCursor()` (polled) | crosshair | a menu is open → crosshair hidden |

### 2.3 Input (the radial menu)

The wheel draws; the **Tool Inventory selects** (see `Docs/InventorySystem/ToolInventorySystem/Architecture_Overview.md` §5.2):

| Action | Asset | Keys (`IMC_SunsetCove`) | Bound by |
|---|---|---|---|
| Tool Menu (hold) | `Content/Input/Actions/IA_ToolMenu` (bool, no triggers) | `Tab`, `Gamepad_RightShoulder` | `BP_Character` > Tool Inventory > **Open Tool Menu Action** |
| Use Tool | `IA_PropPlace` (shared with props) | Left mouse, RT | **Use Tool Action** |
| Drop (menu open) | `IA_PropDrop` (shared with props) | Q, Y | **Drop Action** |

- Hold → `OpenToolMenu` (refused with a HUD message when props are in the hands or there are no tools).
- While held, `ASunsetCoveCharacter::Input_Look` sends the mouse delta / right stick to `AddRadialInput` instead of the camera: **no cursor is ever shown**, the HUD screen stays *Game Only*.
- Release → the targeted tool is equipped (or holstered). The wheel needs no click and no confirm of its own.

### 2.4 How the wheel builds itself (no hard-coded layout)

On every open and on `OnSlotsChanged`:

1. `N = Inventory->GetNumSlots()` (= **Max Tools** on `BP_Character` > Tool Inventory, 1–12).
2. For slot *i*: center angle `GetRadialSlotAngle(i)` (follows **Radial Start Angle Degrees**), width `360 / N`.
3. Tool of the slot (`GetSlotTool(i)`): `DisplayName` (center label) and `Icon` (soft texture, loaded async). No icon → `PlaceholderIcon`; none either → first letter of the name. Empty slot → darker slice, never targeted.
4. In `NativePaint` each slice is an annulus segment mesh (`FSlateDrawElement::MakeCustomVerts`, one draw call for all slices) with UVs **U = across the slice, V = inner → outer**, then anti-aliased outlines, icons and the label.
5. The targeted slice animates out (`HoverOffset`, `HoverOuterRadiusScale`, `HoveredSliceColor`, `HoverIconScale`).

---

## 3. Maintenance and scaling guide

### 3.1 Folders

| What | Folder |
|---|---|
| HUD element widgets | `Content/UI/HUD/Elements/` (`WBP_Crosshair`, `WBP_NotificationFeed`, `WBP_ToolRadialMenu`) |
| Message table | `Content/UI/HUD/Data/DT_HUDNotifications` |
| Tool icons | `Content/UI/HUD/Textures/ToolIcons/` (`T_ToolIcon_<ToolId>`, `T_ToolIcon_Placeholder`) |
| Which elements the HUD shows | `Content/UI/Config/DA_UIConfig` > **HUD Elements** |
| Tool Blueprints | `Content/Gameplay/Tools/` (`BP_Bucket`, `BP_Spray`, `BP_Toy`, `BP_GarbageBag`) |
| C++ | `Source/SunsetCove/Public/UISystem/HUD/` and `Private/UISystem/HUD/` |

### 3.2 Swap or add a tool icon

1. Import a square PNG (white glyph on transparent works best: the wheel tints it) into `Content/UI/HUD/Textures/ToolIcons/`, name `T_ToolIcon_<ToolId>`.
2. Texture settings: **Compression** `UserInterface2D` (or `EditorIcon`), **Texture Group** `UI`, **Mip Gen** `NoMipmaps`.
3. Open the tool Blueprint (`Content/Gameplay/Tools/BP_<Item>`) > Class Defaults > `Tool | Identity` > **Icon** = the texture. Done: no widget change.
4. Generic fallback for every tool without icon: `WBP_ToolRadialMenu` > Class Defaults > `Radial | Icons` > **Placeholder Icon**.

### 3.3 Change the number of slices / tools

- **Slices** = `BP_Character` > Components > Tool Inventory > `ToolInventory | Slots` > **Max Tools** (1–12). 4 → four 90° slices, 10 → ten 36° slices. Nothing else to change.
- **Which tools exist**: create tool Blueprints (setup guide chapter 3). **Starting tools** (testing, or the GDD "initial tools"): **Starting Tools** list on the same component; the first entry lands in slot 0 (top). Today it holds the four placeholder tools — **empty the list** once tools come from the visitors' spots.
- Rotate the layout: `ToolInventory | Radial` > **Radial Start Angle Degrees** (0 = slot 0 at the top, clockwise). Sensitivity: **Radial Radius**, **Radial Dead Zone**.

### 3.4 Restyle the wheel (`WBP_ToolRadialMenu` > Class Defaults)

| Category | Properties |
|---|---|
| `Radial|Layout` | Outer Radius, Inner Radius, Slice Gap Degrees, Arc Resolution Degrees |
| `Radial|Selection` | Hover Offset, Hover Outer Radius Scale, Hover Icon Scale, Animation Speed, Hover Sound Cue |
| `Radial|Open` | Open Speed, Open Start Scale, Background Dim Color |
| `Radial|Colors` | Slice / Hovered / Empty colors, Outline, Equipped Marker, **Slice Brush** (texture or UI material; UV U = across, V = inner→outer) |
| `Radial|Icons` | Icon Size, Icon Radius Alpha, Icon / Hovered Icon color, Placeholder Icon, Initial Font |
| `Radial|Label` | Show Targeted Name, Name Font, Name Color, Uppercase |
| `Radial|Preview` | slot count, names, icons, targeted and equipped slot shown **in the UMG designer only** |

The designer viewport shows a live preview: tune it without pressing Play.

### 3.5 Restyle the crosshair (`WBP_Crosshair` > Class Defaults)

`Crosshair|Look`: **Dot Size**, **Dot Color** (alpha = opacity), Outline Thickness / Color, **Use Custom Brush** + Custom Brush (an art crosshair). `Crosshair|Behavior`: hide while the tool menu is open, hide while a cursor is visible (menus), Fade Speed.
From code / Blueprint: `SetCrosshairVisible(bool)`, `SetCrosshairStyle(Size, Color)` on the widget, or — from any system, without the widget — `Get SunsetCoveHUDSubsystem` > **Set Crosshair Hidden By** (`Reason` name, `true`/`false`).

### 3.6 Messages of the suggestion feed

- **Write / change a message**: open `DT_HUDNotifications`, edit **Message** (action names, never physical keys), **Duration Seconds** (0 = default), **Show Once Per Session** (tips).
- **Send a message** from any system: `Get SunsetCoveHUDSubsystem` > **Show Notification By Id** (row name) or **Show Notification** (runtime text). C++: `USunsetCoveHUDSubsystem::Get(this)->ShowNotificationById(TEXT("..."))`.
- **Which row a tool refusal uses**: `BP_Character` > Class Defaults > `SunsetCove|HUD` > **Tool Refusal Notifications** (map reason → row), **Tool Pickup Tip Notification**.
- **Look and timing**: `WBP_NotificationFeed` > Class Defaults > `Notifications|Style` (font, color, shadow, uppercase, wrap) and `Notifications|Timing` (default duration, fades, max lines, newest on top, merge duplicates). **Position**: move the Vertical Box `MessageContainer` in the designer (keep the name).
- A missing row logs one warning (`LogSunsetCove`) and shows nothing.

### 3.7 Add another HUD element (minimap, clock, money...)

1. C++ class deriving from `USunsetCoveHUDElementWidget` (override `HandlePawnBound` to find components), or a Blueprint with that parent.
2. Widget Blueprint in `Content/UI/HUD/Elements/`.
3. `DA_UIConfig` > **HUD Elements** > add `{Element Id, Slot Tag = CoreFramework.UI.HUDSlot.Default, Widget Class}`. Array order = draw order.

### 3.8 Rules

- `Plugins/CoreFramework` is never edited; everything here extends it from the project.
- Widgets never include gameplay headers to receive messages (they listen to the subsystem); gameplay never includes widget headers (it calls the subsystem). The only widget → gameplay link is the radial / crosshair reading the **Tool Inventory component found by class** on the pawn.
- Single player: nothing replicates.
