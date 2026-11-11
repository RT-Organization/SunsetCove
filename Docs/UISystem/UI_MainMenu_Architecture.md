# UI / Main Menu Architecture

**Project:** Sunset Cove | **Engine:** Unreal Engine 5.8
**Code:** `Source/SunsetCove/*/UISystem/Menus/` (menu) and `Source/SunsetCove/*/Core/GameFlow/` (new game / continue)
**Built on:** CoreFramework UI system (`UBaseMenuWidget`, `UBaseButton`, `UBaseConfirmPopupWidget`, `UUIFrameSubsystem`) and CoreFramework save system (`USaveSubsystem`). Nothing in `Plugins/CoreFramework` is edited by the project.
**Related:** `Docs/UISystem/UI_HUD_Architecture.md` (the gameplay HUD), `Plugins/CoreFramework/Docs/UISystem/`, `Plugins/CoreFramework/Docs/SaveSystem/`.

---

## 1. Overview

```
Game starts → GameDefaultMap = L_MainMenu
  World Settings: GameMode Override = BP_GameMode_MainMenu (no pawn, HUD = BP_Startup_MainMenu)
  L_MainMenu contains: MenuCamera (Auto Activate for Player 0) looking at the cove at sunset
                       a Level Instance of L_MapBlockout (the cove)  +  PP_MenuSunset (warm grade)
  BP_Startup_MainMenu (ABaseHUD) → UUIFrameSubsystem pushes UI.Screen.MainMenu
      DA_UIConfig > Screens: UI.Screen.MainMenu → WBP_MainMenu (Menu layer, UI Only input)
          WBP_MainMenu activates → asks USunsetCoveGameFlowSubsystem: "does the world save exist?"
              → Continue Game shown or not, focus on the first button
```

The widget has **no background**: it is drawn over the 3D view of the menu camera.

**The menu level (`L_MainMenu`)**

| Actor | Setting |
|---|---|
| World Settings | GameMode Override = `BP_GameMode_MainMenu` (without it the map would use the global gameplay GameMode and spawn the character) |
| `Cove_LevelInstance` | Level Instance of `L_MapBlockout`: the cove, its sky and its light, without copying any art |
| `MenuCamera` | Camera Actor, **Auto Activate for Player = Player 0**, at (−2000, −1500, 3800), pitch −12, yaw 2, FOV 80, *Constrain Aspect Ratio* off (no black bars on any screen ratio). Frames the beach, the sea and the horizon; the dark rock on the right sits behind the logo |
| `PP_MenuSunset` | Post Process Volume, **Unbound**, priority 10: white balance 4300 K, warm gain, slight vignette, −0.4 exposure. A sunset mood for the menu only |

`Project Settings > Maps & Modes > Game Default Map` = `L_MainMenu` (a packaged game starts on the menu; the editor startup map is unchanged).

*Limitation:* the blockout's sun is a fixed midday light. A level instance cannot change it without editing `L_MapBlockout`, so the sunset look is the post process. When the final cove map exists with the Day/Night manager, replace the level instance and set its time to sunset (19:00, paused) for the menu.

| Piece | Asset / class | Role |
|---|---|---|
| Menu screen | `Content/UI/Menus/MainMenu/WBP_MainMenu` → `USunsetCoveMainMenuWidget` → `UBaseMenuWidget` | layout + button logic |
| Menu button preset | `Content/UI/Components/Buttons/WBP_Button_Menu` → `UBaseButton` | text-only button with a white selection bar |
| Button style | `Content/UI/Components/Buttons/Styles/BP_ButtonStyle_Menu` (CommonButtonStyle) | no background; text style per state |
| Text styles | `Content/UI/Components/Text/BP_TextStyle_*` (CommonTextStyle) | every font, size and color of the menu |
| Logo font | `Content/UI/Fonts/Scriptina-6BM_Font` (Font) + `Scriptina-6BM` (Font Face) | used only through `BP_TextStyle_Logo` |
| Confirmation popup | `Content/UI/Popups/WBP_Popup_Confirm` → `UBaseConfirmPopupWidget` (CoreFramework) | "delete the world?" |
| Flow | `USunsetCoveGameFlowSubsystem` (Game Instance Subsystem) | new world / continue / save state, on top of `USaveSubsystem` |
| Flow data | Project Settings > Game > **Sunset Cove Game Flow** (`USunsetCoveGameFlowSettings`, `DefaultGame.ini`) | slot name, gameplay map, menu map |

---

## 2. Widget hierarchy

### `WBP_MainMenu` (parent `USunsetCoveMainMenuWidget`)

```
RootCanvas (Canvas Panel)
├─ LogoText            Common Text   style BP_TextStyle_Logo   "Sunset Cove"     anchor top-right, no background
└─ MenuButtons         Vertical Box                                              anchor bottom-left
   ├─ ContinueButton   WBP_Button_Menu  "Continue Game"   (collapsed when there is no save)
   ├─ NewGameButton    WBP_Button_Menu  "New Game"
   ├─ OptionsButton    WBP_Button_Menu  "Options"
   ├─ QuitButton       WBP_Button_Menu  "Quit"
   └─ StatusText       Common Text   style BP_TextStyle_MenuStatus  (collapsed when empty)
```

The four button names and `StatusText` are bound by name (`BindWidget` / `BindWidgetOptional`); the rest is free design. **Class Defaults** (category *Main Menu*): `Options Screen Tag` = `UI.Screen.Settings`, `Confirm Popup Screen Tag` = `UI.Screen.ConfirmPopup`, `New Game Confirm Request` (title, message, Confirm / Cancel labels), `Hide Continue Without Save`.

### `WBP_Button_Menu` (parent `UBaseButton`) — the menu button preset

```
HorizontalBox
├─ SelectionIndicator   Image (white bar)   visible only while highlighted (hover OR keyboard / gamepad focus)
└─ LabelText            Common Text         text = Button Label of each placed button
```

Class Defaults: **Style** = `BP_ButtonStyle_Menu`. `UBaseButton` applies the style's text style of the current state to `LabelText` and shows `SelectionIndicator` while highlighted (CoreFramework v2.1).

### `WBP_Popup_Confirm` (parent `UBaseConfirmPopupWidget`)

```
Root (Overlay)
├─ Backdrop            Border, black 55 %, fills the screen
└─ Panel               Border, dark, centered
   └─ VerticalBox
      ├─ TitleText     Common Text  BP_TextStyle_PopupTitle
      ├─ MessageText   Common Text  BP_TextStyle_PopupBody (wraps)
      └─ Buttons       Horizontal Box, right aligned
         ├─ ConfirmButton   WBP_Button_Menu
         └─ CancelButton    WBP_Button_Menu
```

Modal, Back = Cancel, focus on **Cancel** (destructive question). Texts come from the request; no graph.

---

## 3. Styles and fonts (change one asset → everything follows)

| Asset | Font | Used by |
|---|---|---|
| `BP_TextStyle_MenuButton` | Roboto Bold, gray | label of every `WBP_Button_Menu`, idle |
| `BP_TextStyle_MenuButton_Highlighted` | Roboto Bold, white | label while hovered / focused |
| `BP_TextStyle_MenuButton_Disabled` | Roboto Bold, dark gray, faded | label while disabled (busy, popup open) |
| `BP_TextStyle_Logo` | **Scriptina** (`/Game/UI/Fonts/Scriptina-6BM_Font`) | the logo |
| `BP_TextStyle_PopupTitle` / `BP_TextStyle_PopupBody` | Roboto Bold / Regular | popup texts |
| `BP_TextStyle_MenuStatus` | Roboto Italic, soft red | error line of the menu |

`BP_ButtonStyle_Menu` references the three button text styles (Normal / Normal Hovered / Disabled Text Style) and draws no background. **Swapping the logo font:** import the new `.ttf` into `Content/UI/Fonts/` (it creates a Font Face and a Font), then set *Font > Font Family* of `BP_TextStyle_Logo` to the new Font. Nothing else references a font directly.

**Scriptina line height (important if you re-import the font).** Scriptina declares an ascender of 1971 and a descender of −947 on a 1000-unit em: one line box is ~2.9 em tall and the glyphs sit far below the top of the box, so a top-anchored logo appeared in the middle of the screen. The Font Face `Scriptina-6BM` therefore overrides its metrics: **Ascend Overridden** = on, value **17**; **Descend Overridden** = on, value **−6**. These values are in 1/64 of the font's design units (UE applies its 26.6 fixed-point conversion before the font scale: 1100 / 64 ≈ 17, −380 / 64 ≈ −6). Raise Ascend if the tops of the capitals get clipped by another widget; the swashes may overflow the box, which is harmless here. A re-import keeps these settings; a new Font Face needs them set again.

---

## 4. Save system integration

One world = one slot of the CoreFramework save system, named by `World Slot Name` (default `World`).

| Action | `USunsetCoveGameFlowSubsystem` | `USaveSubsystem` calls |
|---|---|---|
| Menu opens | `RefreshWorldSaveState` → `OnWorldSaveStateChanged(bHasSave)` | `ListSlots` → `DoesSlotExist` |
| **Continue Game** | `ContinueWorld` | `LoadSlot` → open `Info.MapName` (or `Gameplay Map` if the save names the menu map / nothing) |
| **New Game**, no save | `StartNewWorld` | `ListSlots` → `StartNewGame` → `SaveToSlot` → open `Gameplay Map` → `SaveToSlot` again next frame after the map loaded |
| **New Game**, save exists | popup → Confirm → `StartNewWorld` | `DeleteSlot` first, then as above |
| Game systems | `SaveWorld` | `SaveToSlot` (e.g. call it when the player sleeps in the cave) |

- **First play on a fresh install:** New Game writes the world file immediately, so every later menu shows *Continue Game* + *New Game*.
- **Progress:** Continue restores what the save system captured: all saveables (`USaveableComponent`, `ISaveableInterface`) registered at the last `SaveWorld`. Objects restore themselves when they register in the loaded map ("registering = restoring").
- **Busy:** while a step runs every button is disabled (`OnBusyChanged`); a failure is written in `StatusText` (`OnFlowFailed`, text from `USaveFunctionLibrary::ResultToText`).

---

## 5. Popup wiring

```cpp
UBaseConfirmPopupWidget::OpenConfirmation(GetUIFrameSubsystem(), ConfirmPopupScreenTag, NewGameConfirmRequest, Callback);
```

`UI.Screen.ConfirmPopup` → `WBP_Popup_Confirm` on `CoreFramework.UI.Layer.Modal` (`DA_UIConfig` > Screens). The popup answers exactly once: **Confirm** → `StartNewWorld`; **Cancel** / Back (Escape, gamepad B) → nothing changes, focus returns to New Game. While it is open the menu buttons are disabled and the modal layer blocks input below.

---

## 6. Input and navigation

| Device | Behaviour |
|---|---|
| Mouse | hover highlights (white text + bar), click activates. Menu screen input mode *UI Only*: cursor visible. |
| Keyboard | Up / Down move between buttons (one vertical box), Enter activates, Escape cancels the popup |
| Gamepad | D-pad / stick move, A activates, B cancels the popup |

Default focus: Continue Game if visible, else New Game; it moves to Continue when the save answer arrives. Focus highlights exactly like hover (`Highlight On Focus`).

**One highlighted button at a time ("last input wins").** `WBP_Button_Menu` is a `USunsetCoveMenuButton` (child of `UBaseButton`):

| Event | Effect |
|---|---|
| Mouse enters a button | that button **takes the focus**: the previously focused one loses its highlight at once |
| Keyboard / gamepad moves the focus while the mouse rests on a button | the resting mouse stops counting as hover (`IsHovered` returns false), so only the focused button is highlighted |
| The mouse really moves again over a button (or leaves and re-enters) | the mouse takes over again |
| Disabled button | never takes focus from the mouse |

Slate sends a synthetic mouse-move (cursor unmoved, zero delta) after every layout or focus change; the button ignores those, otherwise it would take the focus back from the keyboard on every key press. `Focus On Hover` (Class Defaults > *Menu Button*) turns the hover focus off for one button. Tab buttons (`WBP_Button_Tab`) are plain `UBaseButton` and keep the old behavior. If this is wanted for all buttons later, the logic belongs in `UBaseButton` (framework patch).

---

## 7. Maintenance

| Change | Where |
|---|---|
| Button texts | `WBP_MainMenu` > select a button > **Button Label** |
| Button look (all menu and popup buttons) | `BP_ButtonStyle_Menu`, `BP_TextStyle_MenuButton*`, or `WBP_Button_Menu` (bar size / color) |
| Logo text / position | `WBP_MainMenu` > `LogoText` (text, Canvas slot anchors) |
| Logo font / size / color | `BP_TextStyle_Logo` |
| Popup question | `WBP_MainMenu` > Class Defaults > **New Game Confirm Request** |
| Popup look | `WBP_Popup_Confirm`, `BP_TextStyle_Popup*` |
| Options screen | `WBP_MainMenu` > Class Defaults > **Options Screen Tag** (+ `DA_UIConfig` > Screens) |
| Which map New Game / Continue opens, slot name | Project Settings > Game > **Sunset Cove Game Flow** |
| Menu background view | `L_MainMenu` > `MenuCamera` (move it), `PP_MenuSunset` (warm grade); replace the `L_MapBlockout` level instance with the final cove map when it exists |
| Add a menu entry | add a `WBP_Button_Menu` to `MenuButtons`; for logic add a `BindWidget` button + handler in `USunsetCoveMainMenuWidget` |
| Reset the save while testing | delete `Saved/SaveGames/CF_Slot_World.sav` and `CF_Index.sav`, or press New Game and confirm |

## 8. Verified in Play-In-Editor (2026-11-11)

| Case | Result |
|---|---|
| Fresh install (no save) | Continue hidden, focus on New Game → Enter: world saved (`CF_Slot_World.sav`), `L_Gameplay` opened with the gameplay GameMode and HUD |
| Save exists | Continue Game visible and focused |
| New Game → popup | title + question shown, focus on Cancel, menu disabled behind |
| Popup → Escape | closed, focus back on New Game, save untouched |
| Popup → Confirm | old world deleted, new one saved, `L_Gameplay` opened |
| Continue Game | save loaded (it recorded `L_Gameplay`), `L_Gameplay` opened |
| Options | `WBP_Settings` opened |
