# UI / Settings Architecture

**Project:** Sunset Cove | **Engine:** Unreal Engine 5.8
**Code:** `Source/SunsetCove/*/UISystem/Menus/` (settings screen, controls page, input mapping screen), `Source/SunsetCove/*/UISystem/Input/` (controller icons), `Source/SunsetCove/*/AudioSystem/Widgets/` (audio page)
**Built on:** CoreFramework v2.2 UI system (`UBaseMenuWidget`, `UBaseTabWidget`, setting rows, `UGraphicsSettingsTabWidget`, `UControlsSettingsTabWidget`, `UKeyBindingsScreenWidget`, `UInputPromptWidget`) and input system (`UInputFrameSubsystem`). Nothing in `Plugins/CoreFramework` is edited by the project.
**Related:** `Docs/UISystem/UI_MainMenu_Architecture.md`, `Docs/UISystem/UI_HUD_Architecture.md`, `Docs/AudioSystem/AudioSystem_Architecture.md`, `Plugins/CoreFramework/Docs/UISystem/UI_Settings_Widgets.md`, `Plugins/CoreFramework/Docs/InputSystem/`.

---

## 1. Overview

```
Main Menu "Options"  ─┐
Pause Menu "Settings" ┴─> UUIFrameSubsystem::PushScreen(UI.Screen.Settings)       (DA_UIConfig > Screens, Menu layer)
                            WBP_Settings  (USunsetCoveSettingsMenuWidget -> UBaseMenuWidget)
                              tabs from DA_Settings_Tabs: Graphics | Audio | Controls       (Q / E, LB / RB, mouse)
                              ├─ WBP_Tab_Graphics  (UGraphicsSettingsTabWidget)
                              ├─ WBP_Tab_Audio     (USunsetCoveAudioTabWidget)
                              └─ WBP_Tab_Controls  (USunsetCoveControlsTabWidget)
                                   "Input Mapping" -> PushScreen(UI.Screen.KeyBindings)
                                                        WBP_KeyBindings (USunsetCoveKeyBindingsScreenWidget)
Back (Escape / B / the Back prompt) -> every page with pending changes applies -> screen closes
```

The look follows the reference: a minimal, left-aligned list in the lower middle of the screen. The tab names sit on top, with the active one in orange. The highlighted row has a white bar and a bold white label. Values use `<  value  >`, sliders are a thin line with a small handle and the value on the right, and toggles read On / Off. A thin separator and a description line for the highlighted row close the list. "Back" sits bottom right with the device's button glyph.

**Background**

| Opened from | Background |
|---|---|
| Main Menu | Live 3D view of the menu camera, dimmed by a light black `Shade` (no blur) |
| Pause Menu | `PausedBackground` blur (shown only while the game is paused) + `Shade` |

The rule is the framework's `PausedBackground`: a widget with that name is visible only while `UGameplayStatics::IsGamePaused`. `WBP_Settings` and `WBP_PauseMenu` get it through `UBaseMenuWidget`, and `WBP_KeyBindings` through `USunsetCoveKeyBindingsScreenWidget`. Settings opened on top of the Pause Menu keeps the game paused, because the framework treats a covered screen as covered, not closed (v2.2 covered-screen fix).

---

## 2. Pieces

| Piece | Asset / class | Role |
|---|---|---|
| Settings screen | `Content/UI/Menus/Settings/WBP_Settings` → `USunsetCoveSettingsMenuWidget` | layout; focus, description line, Back applies |
| Pages | `Content/UI/Menus/Settings/Tabs/WBP_Tab_Graphics`, `_Audio`, `_Controls` | one Scroll Box + rows each |
| Tab list | `DA_Settings_Tabs` (UI Tab Set) | which pages exist, order, names |
| Input Mapping screen | `Content/UI/Menus/Settings/WBP_KeyBindings` → `USunsetCoveKeyBindingsScreenWidget` → `UKeyBindingsScreenWidget` | remapping list, Back prompt |
| Row presets | `Content/UI/Components/Settings/WBP_SettingRow_Slider`, `_Option`, `_Toggle`, `_Header`, `WBP_KeyBindingRow`, `WBP_VolumeRow` | one Blueprint per kind of row: restyle once, every page follows |
| Row parts | `WBP_Button_SettingValue` (value), `WBP_Button_SettingArrow` (`<` `>`), `WBP_Button_SettingLink` (a row that opens something), `WBP_Button_SettingTab` (tab name), `WBP_Button_KeyBinding` (binding box) | buttons used inside rows and screens |
| Prompts | `WBP_InputPrompt` (`UInputPromptWidget`), `WBP_Button_Prompt` ("Back" + prompt) | glyph on controller, text or nothing on keyboard |
| Text styles | `Content/UI/Components/Text/BP_TextStyle_Setting*` | `Setting`, `_Highlighted`, `_Disabled`, `_Title`, `_Tab`, `_TabHovered`, `_Description`, `_Value`, `_Prompt` |
| Button styles | `Content/UI/Components/Buttons/Styles/BP_ButtonStyle_Setting`, `_SettingTab`, `_KeyBinding`, `_Prompt` | text style per state, no backgrounds (key binding: thin rounded outline) |
| Font | `Content/UI/Fonts/Font_UI_Main` | the only font of every settings text style |
| Glyph data | `Content/UI/Input/Data/BP_InputData_KeyboardMouse`, `_Gamepad_Xbox`, `_Gamepad_PlayStation` | key → texture, from `Content/UI/Input/Glyphs` |
| Controller icons | `USunsetCoveControllerIconsSubsystem` (Local Player Subsystem) | Automatic / Xbox / PlayStation icon set |
| UI sounds | `Content/Audio/UI/Beach/S_UI_*` + `DA_AudioConfig` cues `Audio.Cue.UI.*` | hover, click, open, close |

**Text styles and fonts.** All styles are Common Text Styles that use `Font_UI_Main`, with a soft drop shadow (offset 1,1, 55% black) so white text stays readable over the sky and the sand. To change the font of the whole settings UI, change `Font_UI_Main`. To change a size or color, change the one style that owns it.

| Style | Font | Color | Used by |
|---|---|---|---|
| `BP_TextStyle_Setting` | Regular 19 | grey 0.58 | row labels, values |
| `BP_TextStyle_Setting_Highlighted` | Bold 19 | white | highlighted row label and value |
| `BP_TextStyle_Setting_Disabled` | Regular 19 | grey 0.2 | disabled row (Resolution in Borderless) |
| `BP_TextStyle_Setting_Title` | Bold 22 | orange | section headers, active tab, screen titles |
| `BP_TextStyle_Setting_Tab` / `_TabHovered` | Bold 22 | grey 0.45 / white | inactive / hovered tab |
| `BP_TextStyle_Setting_Description` | Regular 17 | grey 0.72 | description line, status line |
| `BP_TextStyle_Setting_Value` | Regular 17 | grey 0.45 | slider value, column headers |
| `BP_TextStyle_Setting_Prompt` | Regular 19 | grey 0.85 | key text of prompts |

---

## 3. Widget hierarchy

### `WBP_Settings` (parent `USunsetCoveSettingsMenuWidget`)

```
CanvasPanel
├─ PausedBackground   Background Blur, strength 8, full screen        (shown only while paused)
├─ Shade              Image, black 32%, full screen
├─ Content            Vertical Box, anchor (0, 0.25), left 170, auto size
│  ├─ TabRow          Horizontal Box
│  │  ├─ PrevTabPrompt   WBP_InputPrompt   Q / LB      keycap glyph on keyboard (Keyboard Display = Glyph), LB glyph on controller, 44 px
│  │  ├─ PrevTabArrow    Common Text "‹"   (Tab style)  shows the direction
│  │  ├─ TabList         Common Tab List   (buttons = WBP_Button_SettingTab, built from DA_Settings_Tabs)
│  │  ├─ NextTabArrow    Common Text "›"
│  │  └─ NextTabPrompt   WBP_InputPrompt   E / RB
│  ├─ PageBox         Size Box 940 x 456
│  │  └─ TabSwitcher  Common Animated Switcher (Fade Only, Cubic In Out, 0.35 s)
│  ├─ Separator       Image 900 x 2, white 22%
│  └─ DescriptionText Common Text, Description style, wraps at 900
└─ BackButton         WBP_Button_Prompt "Back" + Back glyph, anchor bottom right (-170, -110)
```

### A page (`WBP_Tab_Graphics`; Controls and Audio are the same)

```
Scroll            Scroll Box: thin scroll bar on the right (3 px, shown only when the page is longer than the box),
                  no edge shadows, Scroll When Focus Changes = No Scroll (see 5)
└─ List           Vertical Box
   ├─ HeaderDisplay        WBP_SettingRow_Header   "Display"
   ├─ WindowModeRow        WBP_SettingRow_Option   (name = the framework's binding)
   ├─ ResolutionRow        WBP_SettingRow_Option
   ├─ VSyncRow             WBP_SettingRow_Toggle
   ├─ FrameRateLimitRow    WBP_SettingRow_Option
   ├─ ResolutionScaleRow   WBP_SettingRow_Slider   (Integer, 50..100, step 5)
   ├─ HeaderQuality        WBP_SettingRow_Header   "Quality"
   ├─ QualityPresetRow     WBP_SettingRow_Option
   └─ ViewDistanceRow … ShadingRow                 (ten quality groups)
```

Controls: `KeyBindingsButton` (link "Input Mapping"), `ControllerIconsRow`, `MouseSensitivityRow`, `GamepadSensitivityRow` (Multiplier, 0.10x..3.00x), `SprintModeRow` (Hold / Toggle), `InvertXRow`, `InvertYRow`, `HintsRow` (On / Off). (The framework also supports `AimAssistRow` and `VibrationRow`; SunsetCove has neither feature, so they are not placed.)
Audio: five `WBP_VolumeRow` (Master, Music, Ambient, Effects, Interface), see `Docs/AudioSystem/AudioSystem_Architecture.md`.

Every row has a **Label** and a **Description** typed on the placed row (Details panel). The page classes find their rows **by name**, so a row can be moved, restyled or removed (a missing row is simply skipped) without code.

### A row (`WBP_SettingRow_Option`; the others differ only on the right side)

```
Root  Size Box 920 x 38
└─ Row  Horizontal Box
   ├─ FocusHighlight   Image 5 x 24, white          (visible while highlighted)
   ├─ LabelText        Common Text, fill 0.44        (style per state: Setting / Highlighted / Disabled)
   ├─ PreviousButton   WBP_Button_SettingArrow "<"   (mouse only, never focusable)
   ├─ SelectorButton   WBP_Button_SettingValue, fill 0.56   ← the focusable control
   ├─ NextButton       WBP_Button_SettingArrow ">"
   └─ ValueColumn      Spacer 64                      (lines up with the slider value column)
```

Slider row: `ArrowLeft` `<`, `ValueSlider` (2 px bar, 7 x 18 handle, steps with Left / Right without "press to lock"), `ArrowRight` `>`, `ValueText`. Header row: indent + title. Key binding row: `FocusHighlight`, `LabelText`, `KeyboardBinding`, `GamepadBinding`.

The label style per state comes from the row's Class Defaults (`Label Style`, `Highlighted Label Style`, `Disabled Label Style`, framework v2.2.1), so no row Blueprint has a graph.

### `WBP_KeyBindings` (parent `USunsetCoveKeyBindingsScreenWidget`)

```
Canvas
├─ PausedBackground   Background Blur (only while paused)
├─ Shade
├─ Content            same anchor and left edge as Settings
│  ├─ TitleText       "Input Mapping" (Title style)
│  ├─ ColumnHeaders   Action | Keyboard & Mouse | Controller
│  ├─ PageBox 940 x 460 > Scroll > RowsContainer     (rows created at runtime)
│  ├─ ResetAllButton  WBP_Button_SettingLink "Reset to Defaults"
│  ├─ Separator
│  └─ StatusText      feedback line ("…the two keys were swapped.")
└─ BackButton         WBP_Button_Prompt
```

Binding boxes have fixed widths so long key names ("Mouse Wheel Down") do not make one box wider than the others: `WBP_KeyBindingRow` wraps `KeyboardBinding` in `KeyboardBox` (Size Box 270, gap 16) and `GamepadBinding` in `GamepadBox` (200); the column headers in `WBP_KeyBindings` use the same widths and are centered over them.

Class Defaults: `Row Class` = `WBP_KeyBindingRow`, `Category Header Class` = `WBP_SettingRow_Header`.

---

## 4. Tab fade

`TabSwitcher` is a **Common Animated Switcher**, Transition Type **Fade Only**, curve **Cubic In Out**, **0.35 s**. When a tab is selected the framework (`UBaseMenuWidget::ActivateTabContent`) calls `SetActiveWidget`. The switcher fades the old page out, swaps pages halfway, and fades the new one in. Changing the feel means changing those three values on `TabSwitcher`. No code is involved.

Tabs change with **Q / E** on keyboard and **LB / RB** on controller (the tab list's `NextTab` / `PreviousTab` rows of `DT_UI_Actions`), or with a click. The prompts beside the tab names show the right key for the device as a key glyph (dark keycap for Q / E, shoulder button for LB / RB), with a `‹` / `›` arrow and a gap before the first and after the last tab, so they read as "previous / next", not as tabs.

---

## 5. Focus and navigation (`USunsetCoveSettingsMenuWidget`)

| Situation | Where the focus goes |
|---|---|
| Screen opens | first usable control of the active page (`ResolveDefaultFocusWidget` → `FindFirstFocusTarget`) |
| Page changes with Q / E / LB / RB | first usable control of the new page, **after** the fade (the new page is not focusable before the switcher swaps it in) |
| Page changes from the tab bar (click, or Enter on a focused tab) | stays on the tab bar |
| Mouse moves onto a row | that row takes the focus (`bHoverMovesFocus`); **never while a controller is in use** (CommonUI moves the hidden cursor onto each focused control, and the row it just left still reports a hover for a frame, which used to pull the focus back up) |
| Returning from Input Mapping | the control that had it (`bAutoRestoreFocus`) |

**CommonUI's focus request.** CommonUI asks the top screen for its focus target every time it becomes the top one (opened, back from a screen that covered it, device switch). If the screen answers nothing, CommonUI focuses the game viewport and a controller selects nothing. The SunsetCove screens (`USunsetCoveMainMenuWidget`, `USunsetCovePauseMenuWidget`, `USunsetCoveSettingsMenuWidget`, `USunsetCoveKeyBindingsScreenWidget`) override `NativeGetDesiredFocusTarget` with their default focus widget and turn on `bAutoRestoreFocus`; the generic version is in the framework since v2.2.2 (`UBaseActivatableWidget::NativeGetDesiredFocusTarget`, FrameWorks commit `08ea898`), so every framework screen answers it by default. The SunsetCove overrides are now redundant but harmless: they can be removed (keep `bAutoRestoreFocus = true`, which the framework does not set).

**Pause Menu** (`WBP_PauseMenu` → `USunsetCovePauseMenuWidget`): Resume is selected on open, the Settings button again on return from Settings. The pause key also closes it: the game input is blocked while the menu is open, so the menu binds a CommonUI action itself, **Close Pause Action** = `Content/UI/Input/DT_SunsetCove_UIActions` › `ClosePause` (gamepad Start, keyboard Insert for testing in the editor). Escape / B are the Back action.

Why this is needed: every focusable control (value selector, slider) sits in the right-hand column. Slate's Up / Down search from a tab button looks straight down and finds nothing there. So the focus is kept in the list, and the tab bar is driven by its own keys.

"First usable" means rows in visual order, skipping headers, disabled rows and hidden widgets. A row gives its control (`GetFocusTarget`), and a link button placed directly on the page (Input Mapping) is taken as is.

The mouse-follow rule only reacts to **real mouse movement**. A list that scrolls under a still cursor does not steal the focus from the keyboard. The result is that there is only ever one highlighted row, and the description always describes it.

**Scrolling.** The pages never scroll by themselves when the focus changes (`Scroll When Focus Changes` = No Scroll). The mouse scrolls with the wheel or the scroll bar on the right, which appears only on pages longer than the box (Graphics today). Keyboard and controller navigation call `ScrollRowIntoView`: the least scrolling that shows the newly focused row, and nothing at all while the row is already fully visible. A row reached by the mouse never scrolls the list, so hovering the last visible row no longer moves the page. The Input Mapping list uses the same scroll bar, with Animated Scroll and no padding (it scrolls only to show a hidden row).

Keys inside the list: Up / Down (D-pad, left stick) move between rows, Left / Right change the value (slider by one step, option by one entry; the first / last entry greys its arrow), Enter / A on an option goes to the next value.

`Project Settings > User Interface > Render Focus Rule` = **Never** (`DefaultEngine.ini`). The rows draw their own focus (white bar and bold text), so Slate's blue focus rectangle is turned off for the whole game.

---

## 6. Description line

Every frame while the screen is active (a page has about 20 rows), `RefreshDescription` looks at the active page's rows. It takes the focused row first, otherwise the row under the mouse, and shows that row's **Description** in `DescriptionText`. An empty description hides the line (Hidden, so the layout does not jump). The text in the Blueprint designer is only a sample: the screen clears it on activation.

The Input Mapping screen uses the same place for its `StatusText` (rebind feedback).

---

## 7. Back applies

There is no Apply button. Back applies:

- Escape (keyboard), B (controller): `NativeOnHandleBackAction` → `CommitPages` → screen closes.
- The Back prompt (mouse): `CloseSettings` → `CommitPages` → `CloseScreen`.

`CommitPages` calls `ApplyAllTabs` (each page with `HasPendingChanges` applies). The pages already preview their values live: graphics rows apply at once, volumes and sensitivities change while you move them. So Apply only **saves**: `GameUserSettings.ini` for graphics, `[CoreFramework.Input]` for input preferences, `[SunsetCove.Input]` for controller icons, and `[CoreFramework.Audio]` for volumes. `bApplyChangesOnClose = false` on `WBP_Settings` gives the classic behavior (Back reverts what was not applied).

Input Mapping saves each binding when it changes (Enhanced Input user settings, its own save file), so its Back only closes.

---

## 8. Graphics preset auto-switch

Implemented by the framework's `UGraphicsSettingsTabWidget`:

- **Picking a preset** (Low, Medium, High, Ultra) sets every quality group to that level (`SetOverallScalabilityLevel`), and the ten group rows refresh.
- **Changing one group** makes the groups differ, so the preset row shows **Custom**. Custom is only a display state: it is never offered while the groups match, and navigating onto it snaps back to the real state.
- **Matching again** (every group back on the same level) shows that level's name again.

Tested in PIE: Shadows Ultra → High turned the preset into Custom, and back to Ultra turned it into Ultra.

---

## 9. Input Mapping listener

```
Player clicks / presses Accept on a binding box (UKeyBindingButton)
  → UKeyBindingRowWidget::OnBindingClicked(Row, bGamepad)
  → UKeyBindingsScreenWidget::BeginRebind → box shows "Press a key" / "Press a button"
  → UInputFrameSubsystem starts the key capture (a Slate input preprocessor: it sees every key, mouse button
    and gamepad button before any widget, and eats them while listening; nothing reaches the menu)
       next key of the right device  → FinishRebind(Key)
       Escape / gamepad View        → cancelled, nothing changes                  (Cancel Capture Keys)
       8 s without input            → cancelled                                    (Key Capture Timeout)
       reserved key (Escape, Start) → rejected with a message                      (Reserved Keys: they open / close menus)
  → conflict check inside the same device: Conflict Policy = Swap
       the other action gets the old key; StatusText: "<key> was used by <action>: the two keys were swapped."
  → Enhanced Input user settings: MapPlayerKey + SaveSettings (per player, own save file)
  → every row refreshes, and so does every prompt showing that action (OnInputBindingsChanged)
Reset to Defaults → UnMapPlayerKey on every row → "All controls are back to their defaults."
```

Settings: `Project Settings > CoreFramework > CoreFramework Input` (`DefaultGame.ini [/Script/CoreFramework.InputFrameSettings]`): `Remappable Contexts` = `IMC_Movement`, `IMC_SunsetCove`; `Keyboard Prompt Display` = Key Text; `Conflict Policy` = Swap. `DefaultInput.ini`: `bEnableUserSettings=True` (Enhanced Input user settings must be on).

**Which actions are listed.** Every mapping of the Remappable Contexts that has Player Mappable Key Settings. The Name groups a keyboard mapping and a gamepad mapping into one row, the Display Name is the row label, and the Display Category is the header.

| Category | Rows (keyboard / controller) |
|---|---|
| Movement | Move Forward W, Move Backward S, Move Left A, Move Right D (controller: left stick, not remappable, shown as "–"), Jump Space / A, Crouch Left Ctrl / R3, Sprint Left Shift / L3 |
| Actions | Interact E / X, Place / Use Tool, Throw, Drop Q / B, Rotate Right (wheel up / RB), Rotate Left (wheel down / LB), Tool Wheel (from `IMC_SunsetCove`) |

Not remappable on purpose: look (mouse / right stick), move on the left stick, and pause (`IMC_Player`: Escape, Start, and Insert for testing in the editor).

A cell without a binding for that device is disabled ("–"), so a gamepad button cannot be put on "Move Forward".

**Movement context: why `IMC_Movement` exists next to the framework's `IMC_Character`.** `IMC_Character` is the framework's generic character context. SunsetCove cannot use it as it is, and cannot edit it (the plugin is read-only):

- remapping needs Player Mappable Key Settings **on each mapping** (W / S / A / D are four directions of one Move action, so each needs its own name), and `IMC_Character` has none, so movement would not appear in Input Mapping;
- it has no controller move / look / sprint / jump / crouch;
- it maps Fly, Dash, Dodge and Perspective on keys SunsetCove uses for Interact (E) and Drop (Q).

So the project owns a copy, `Content/Input/Context/IMC_Movement`, set as Default Mapping Context in `Content/Input/Data/DA_InputConfig_SunsetCove`, which `BP_Character` uses (Class Defaults > Input Config). `IMC_Character` stays in the plugin, unused by SunsetCove. If the framework one day ships remappable settings in `IMC_Character`, the copy can go.

**Display.** Keyboard keys are plain text in the binding boxes ("Space", "Left Ctrl"): the pack's keyboard glyphs are square, and wide keys become unreadable at row size. Controller buttons are glyphs (`WBP_Button_KeyBinding`: `Glyph For Keyboard Keys` off, `Glyph For Gamepad Keys` on).

---

## 10. Input detection and glyphs

```
Any input → CommonInput (UCommonInputSubsystem) decides the current input type: MouseAndKeyboard / Gamepad
                         and, for gamepads, the gamepad name (XSX, PS5) from the controller data
          → UInputFrameSubsystem (CoreFramework, Local Player Subsystem) re-broadcasts it:
               OnInputDeviceChanged(InputType, GamepadName)
          → every UInputPromptWidget refreshes itself:
               gamepad  → glyph from the controller data of the current gamepad name
               keyboard → Keyboard Prompt Display: Key Text ("E"), Glyph, or None
```

Glyphs come from `Content/UI/Input/Glyphs/` (the input prompts pack, next to the controller data that uses it; **dark** set). Sub-folders: `KeyboardMouse`, `Xbox`, `PlayStation5`, `PlayStation4`, `Switch`, each with its style variants (Default, Dark, Light, Alt...).

| Controller data | Gamepad name | Textures |
|---|---|---|
| `BP_InputData_KeyboardMouse` | – | `Glyphs/KeyboardMouse/Dark` |
| `BP_InputData_Gamepad_Xbox` | `XSX` | `Glyphs/Xbox/Default` (face buttons in their colors) |
| `BP_InputData_Gamepad_PlayStation` | `PS5` | `Glyphs/PlayStation5/Default` (face buttons in their colors) |

They are registered for Windows in `Config/Windows/WindowsGame.ini` (`[CommonInputPlatformSettings_Windows CommonInputPlatformSettings]`): default gamepad `XSX`, `bCanChangeGamepadType=True`, and the three `+ControllerData` lines. Adding a pad family means one more controller data asset and one more line there.

### Xbox or PlayStation icons

Windows reports XInput pads without telling their brand, so a DualSense through Steam Input or DS4Windows looks like an Xbox pad. The controller data also maps the GameInput hardware ids (`XboxOne`, `Xbox360` → Xbox; `DualSense`, `DualShock4` → PlayStation) for when the engine reports them. For the rest, the player chooses:

**Settings > Controls > Controller Icons**: Automatic / Xbox / PlayStation (`USunsetCoveControllerIconsSubsystem`).

- **Automatic**: CommonInput's detection (Xbox unless a PlayStation pad is reported).
- **Xbox / PlayStation**: `UCommonInputSubsystem::SetGamepadInputType` with the names in `Project Settings > Game > Sunset Cove UI > Input | Controller Icons` (`XSX`, `PS5`). It is applied again after any hardware detection.
- Saved in `GameUserSettings.ini [SunsetCove.Input] ControllerIcons`. It previews at once (every prompt switches), saves on Back, and the page's Revert restores it.

### Reusing prompts (world prompts, suggestion box, HUD)

Place a `WBP_InputPrompt` (or a widget with a `UInputPromptWidget` parent) and set one source:

| Property | Use |
|---|---|
| `Input Action` | a gameplay Enhanced Input action: the prompt shows **the player's current binding** (remaps included). Example: `IA_Interact` + Label "Pick up" → `[A] Pick up` / `E Pick up` |
| `UI Action` | a `DT_UI_Actions` row (Back, Confirm, NextTab, PreviousTab) |
| `Keyboard Key` / `Gamepad Key` | fixed keys (the tab prompts) |

`Keyboard Display` overrides the project rule for one prompt (the Back prompt uses None: plain "Back" on keyboard, "Back (B)" on controller). `Collapse When No Key` hides the prompt when the device has no key for it. No Blueprint graph is needed: the prompt listens to the input system itself.

---

## 11. Controls page values

| Row | Meaning in SunsetCove | Stored |
|---|---|---|
| Mouse Sensitivity / Controller Sensitivity | multiplier on look input (`ABaseCharacter::Input_Look` through `ApplyLookPreferences`) | `[CoreFramework.Input]` |
| Invert X / Invert Y | flips look axes | same |
| Sprint | **Hold** (default): run while the sprint key is held. **Toggle**: a press starts running, the next press or stopping ends it (`ASunsetCoveCharacter` Input_SprintStarted / Ended + Tick). `USunsetCovePlayerPreferences` | `[SunsetCove.Input] SprintMode` (0 Hold, 1 Toggle) |
| Hints | **On** (default) / **Off**. Off hides every on-screen help at once, also mid-game: the suggestion feed (`USunsetCoveHUDSubsystem` drops new messages, one-time tips are not used up; lines on screen are cleared) and the interaction prompt (`UHUDInteractionPromptWidget::ShouldBeVisible`). `USunsetCovePlayerPreferences::AreHintsEnabled` (cached) | `[SunsetCove.Input] ShowHints` (True / False) |
| Controller Icons | see 10 | `[SunsetCove.Input]` |

---

## 12. UI sounds

Soft, quiet sounds that sit under the sea and the music. They are made procedurally (filtered noise for air, a short warm tone for the tap), so there is no harsh transient.

| Cue | Sound | Character |
|---|---|---|
| `Audio.Cue.UI.Hover` | `S_UI_Hover_Breeze` | a tiny puff of air |
| `Audio.Cue.UI.Click` | `S_UI_Click_Shell` | a soft shell / pebble tap with a low tone |
| `Audio.Cue.UI.PopupOpen` | `S_UI_Open_Breeze` | a breeze rising and settling |
| `Audio.Cue.UI.PopupClose` | `S_UI_Close_Breeze` | a shorter breeze falling away |

All four use Sound Class `SC_UI` (Interface volume slider), with ±5% volume and ±3–6% pitch variation per play (`DA_AudioConfig`), so repeated hovers do not sound mechanical. Buttons and screens play them through the framework's default cues: there is no sound setup per widget. The arrow buttons are silent, so a value change gives only the click.

---

## 13. Extending

**A new setting row on an existing page.** Drop a row preset into the page's `List`, type Label and Description, and bind it:

- framework rows: use the exact row name the page class expects (see `Plugins/CoreFramework/Docs/UISystem/UI_Settings_Widgets.md`);
- a new game setting: add a `BindWidgetOptional` row to the page's C++ class (as `ControllerIconsRow` in `USunsetCoveControlsTabWidget`), read it in `NotifyTabSelected`, write it in `ApplyChanges`, restore it in `RevertChanges`, and include it in `HasPendingChanges`.

**A new page.** Create a Widget Blueprint whose parent is a `UBaseTabWidget` child, give it a `Scroll > List` like the others, and add a row to `DA_Settings_Tabs`.

**A new remappable action.** On the Input Action: Player Mappable Key Settings (Name, Display Name, Display Category), or per mapping in the context (Setting Behavior = Override Settings, with the same Name on the keyboard and the gamepad mapping). The context must be in `Remappable Contexts`. The Input Mapping screen lists it with no other step.

**A new prompt anywhere.** See 10, "Reusing prompts".

---

## 14. Verified, and not verified

Played in the editor (PIE, main menu), with keyboard, mouse clicks and controller input sent through Slate:

- Opening Settings: focus on the first row, description shown.
- Keyboard and D-pad navigation; the disabled Resolution row is skipped; option and slider changes.
- Q / E and LB / RB page switches with the fade.
- Preset → Custom → Ultra.
- Back by prompt click and by controller B both apply and save (`bUseVSync` changed in `GameUserSettings.ini`).
- Glyph swap keyboard ↔ Xbox, and the PlayStation icon option (L1 / R1, circle).
- Input Mapping: listen, rebind, conflict swap with message, Reset to Defaults, return to Controls with focus restored.

Not verified by automation:

- **The pause flow** (Escape in game → Pause Menu → Settings → Back → Resume, game kept paused, blurred background). Synthetic input does not reach gameplay input, and Escape stops PIE. Check it by hand.
- **Mouse hover moving the focus** (it needs the real cursor). Check: move the mouse over rows, and the white bar and the description should follow the mouse.
- Escape as Back inside a packaged / standalone game (in PIE, Escape stops the session; in a standalone game it is the Back key).
