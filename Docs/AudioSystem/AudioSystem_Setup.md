# Audio System — Editor Setup

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Audience:** whoever works in the Unreal Editor
**How it works inside:** [`AudioSystem_Architecture.md`](AudioSystem_Architecture.md)

Status (2026-11-12): **everything below is already created, configured and saved** (done through the editor's MCP connection) and was checked in Play-In-Editor (section 10). What is still missing is **real sounds**: music, ambience and gameplay effects (section 4). Use this guide to check the setup, to rebuild a piece if it gets lost, and to add new sounds.

---

## 0. Before you start

- [x] C++ compiled (`SunsetCoveEditor`, Development). The classes `SunsetCoveVolumeRowWidget`, `SunsetCoveAudioTabWidget` and the `Map Audio` field on the GameModes come from it.
- [x] `Plugins/CoreFramework` is never touched. Everything lives in `Content/Audio/`, `Content/UI/` and `Config/`.

## 1. Sound Classes — `Content/Audio/Classes/`

| Asset | Parent Class | Flags |
|---|---|---|
| `SC_Master` | None (root) | — |
| `SC_SFX` | `SC_Master` | — |
| `SC_Music` | `SC_Master` | Is Music |
| `SC_Ambient` | `SC_Master` | — |
| `SC_UI` | `SC_Master` | Is UI Sound |

Check / rebuild:
- [x] Open `SC_Master`: the Sound Class graph shows the four children under it.
- [x] **Volume = 1.0 on every class.** The sliders change volume through a Sound Mix at runtime; a class volume below 1 would silently lower that category forever.
- To create one by hand: *Add > Audio > Sound > Sound Class*, open it, set **Parent Class** in Details (or drag it under `SC_Master` in the graph).

## 2. The config — `Content/Audio/DA_AudioConfig`

Type **Audio Config Data Asset** (CoreFramework). It is the single place that says which sound plays for which tag.

**Categories** (one row each):

| Category | Sound Class | Default Volume | Play When Paused |
|---|---|---|---|
| `CoreFramework.Audio.Category.Master` | `SC_Master` | 1.0 | off |
| `CoreFramework.Audio.Category.SFX` | `SC_SFX` | 1.0 | off |
| `CoreFramework.Audio.Category.Music` | `SC_Music` | 0.7 | **on** |
| `CoreFramework.Audio.Category.Ambient` | `SC_Ambient` | 0.8 | off |
| `CoreFramework.Audio.Category.UI` | `SC_UI` | 1.0 | on (always on anyway) |

**Master Sound Mix**: empty (the framework uses its own internal mix). Fade times: framework defaults (volume 0.05 s, music 2 s, ambient 3 s, equal-power crossfade).

**Cues**:

| Cue Tag | Sound | Category | Volume | Pitch random | Preload |
|---|---|---|---|---|---|
| `Audio.Cue.UI.Hover` | `Audio/UI/Beach/S_UI_Hover_Breeze` | UI | 1.0 (±0.05) | 0.05 | on |
| `Audio.Cue.UI.Click` | `Audio/UI/Beach/S_UI_Click_Shell` | UI | 1.0 (±0.05) | 0.06 | on |
| `Audio.Cue.UI.PopupOpen` | `Audio/UI/Beach/S_UI_Open_Breeze` | UI | 1.0 (±0.05) | 0.03 | on |
| `Audio.Cue.UI.PopupClose` | `Audio/UI/Beach/S_UI_Close_Breeze` | UI | 1.0 (±0.05) | 0.03 | on |
| `Audio.Cue.Music.MainMenu` | **empty — add the track** (looping, `SC_Music`) | Music | 1.0 | 0 | off |
| `Audio.Cue.Music.Gameplay` | **empty — add the track** | Music | 1.0 | 0 | off |
| `Audio.Cue.Ambient.MainMenu` | `Audio/Ambient/S_AmbMainMenu_Waves_LoopA` (looping) | Ambient | 0.9 | 0 | on |
| `Audio.Cue.Ambient.ShoreWaves` | `Audio/Ambient/S_AmbGameplay_Waves_Loop` (looping; played by the sea along its shoreline, 3D) | Ambient | 1.0 | 0 | on |
| `Audio.Cue.Ambient.Wind` | `Audio/Ambient/S_Amb_Wind_Loop` (looping; 2D, volume driven by the wind ambience) | Ambient | 0.3 | 0 | on |

**UI Defaults**: Hover = `Audio.Cue.UI.Hover`, Click = `Audio.Cue.UI.Click`, Screen Open = `Audio.Cue.UI.PopupOpen`, Screen Close = `Audio.Cue.UI.PopupClose`.

> The four beach UI sounds are soft procedural sounds (a puff of air, a shell tap, a rising and a falling breeze), mastered quiet so they sit under the sea and the music. Their source `.wav` files are next to them in `Content/Audio/UI/Beach/`. To replace one, re-import over it (same name) or point the cue row at the new asset. Their level is set in the files, so the cue volume stays 1.0.

## 3. Project settings and tags (already in `Config/`)

- [x] **Edit > Project Settings > CoreFramework > CoreFramework Audio > Default Audio Config** = `DA_AudioConfig` (`Config/DefaultGame.ini`, section `[/Script/CoreFramework.AudioFrameSettings]`).
- [x] **Project Settings > Gameplay Tags**: the seven `Audio.Cue.*` tags above (`Config/DefaultGameplayTags.ini`). The five `CoreFramework.Audio.Category.*` tags are native C++ tags and are not in the list.

If the Output Log shows `LogCoreAudio: No audio config assigned`, the setting is empty or points at a moved asset.

## 4. Adding a sound (the everyday task)

Which sounds the game needs, their names and folders: [`AudioSystem_SoundList.md`](AudioSystem_SoundList.md).

1. **Import** the file into `Content/Audio/<SFX | Ambient | Music | UI>/`, name it `S_<Area>_<Name>` (`S_Sunbed_Open`).
2. **Open the sound asset** and set:
   - **Sound Class** = the matching `SC_*` (needed for sounds that are *not* played through a cue, e.g. a placed *Ambient Sound* actor; harmless otherwise).
   - **Looping** on for music, ambience and anything started as a loop. The framework never forces looping: a non-looping music file plays once and stops.
   - For long music: **Loading Behavior** = a streaming option (*Load on Demand*) so only a small header stays in memory.
   - For 3D sounds: **Attenuation Settings** (or set an *Attenuation Override* on the cue row instead).
3. **Tag**: Project Settings > Gameplay Tags > add `Audio.Cue.<Area>.<Name>` (skip if the tag exists, e.g. the music ones).
4. **Cue row**: `DA_AudioConfig > Cues > +`: Cue Tag, Sound, Category. Tick **Preload** for short, frequent sounds; untick it for music and big files. For repeated sounds (footsteps, splashes) set Volume/Pitch Random Range `0.05`–`0.1`.
5. **Play it** — pick one:

| Where | How |
|---|---|
| Music / ambience of a map | already wired: just fill the Sound of `Audio.Cue.Music.MainMenu` / `.Music.Gameplay`. World ambience (waves on the beach, wind) is played by actors in the level, see `Docs/Environment/Sea_Setup_Guide.md` §7-9 |
| Blueprint | *Play Cue 2D* / *Play Cue At Location* / *Play Cue Attached* (Audio function library), *Play Music*, *Play Ambient* |
| An animation (arms, props) | notify track > *Add Notify > Play Audio Cue*; Cue Tag; Audience **Owner Only** for first-person arms |
| A widget | *Play UI Sound* (Cue Tag) on any CoreFramework widget |
| C++ | `UAudioFunctionLibrary::PlayCueAtLocation(this, CueTag, Location)` with `CueTag` an `UPROPERTY(EditAnywhere, meta=(Categories="Audio.Cue"))` |

Nothing else to register. A tag that has no row, or a row without a Sound, logs one `LogCoreAudio` warning and stays silent.

## 5. Music per map — GameMode Blueprints

| Blueprint (`Content/Core/`) | Class Defaults > Audio > Map Audio |
|---|---|
| `BP_GameMode_MainMenu` | Music Cue `Audio.Cue.Music.MainMenu`, Ambient Cue `Audio.Cue.Ambient.MainMenu` (waves heard from the beach), Fade Time -1 |
| `BP_GameMode_Gameplay` | Music Cue `Audio.Cue.Music.Gameplay`, Ambient Cue **empty** (the cove ambience is in the world: `BP_Sea` + `BP_WindAmbience`; the empty cue fades the menu waves out), Fade Time -1 |
| `BP_GameMode_Test` | empty (test maps are silent; fill it if you want music while testing) |

- Fade Time **-1** = the config's defaults. **0** = hard cut.
- An **empty** cue **stops** that layer when the map starts. The **same** cue on two maps keeps playing without restarting (the menu → gameplay ambience does that).
- A map that needs other music: make a child of the GameMode Blueprint, change Map Audio, select it in that map's World Settings > GameMode Override.

## 6. UI sounds

- [x] Every `UBaseButton` (`WBP_Button_Menu` in the main menu, pause menu and popups, the settings buttons and tabs) plays the config's Hover / Click cues. Per button: **Base UI > Audio** > *Hover Cue*, *Click Cue*, or *Silent*.
- [x] **Button styles** (`Content/UI/Components/Buttons/Styles/BP_ButtonStyle_*`): the *Hovered / Pressed Slate Sound* slots are **empty** and must stay empty — they bypass the Sound Classes and would play a second sound.
- [x] **Use Default Screen Cues** (Class Defaults > Base UI > Audio) is **on** for `WBP_Settings`, `WBP_PauseMenu`, `WBP_Popup_Confirm`; **off** for `WBP_MainMenu` (it opens at boot) and the HUD.

## 7. The volume menu

### 7.1 `WBP_VolumeRow` — `Content/UI/Components/Settings/`

Parent class **Sunset Cove Volume Row Widget**, itself a CoreFramework **Slider Setting Row**. So the row has the same look, keys and focus as every other settings row (`WBP_SettingRow_Slider`). Widget tree (names are required by C++):

```
Root (Size Box 920 x 38)
 └─ Row (Horizontal Box)
     ├─ FocusHighlight (Image 5 x 24, white)           shown while the row is highlighted
     ├─ LabelText      (Common Text, fill 0.44)        style per state from the row's Class Defaults
     ├─ ArrowLeft      (Common Text "<")
     ├─ ValueSlider    (Slider, fill 0.56, 2 px bar, small handle, Requires Controller Lock OFF)
     ├─ ArrowRight     (Common Text ">")
     └─ ValueText      (Common Text, Value style, min width 64)   "80%"
```

`ValueSlider` is required, the rest is optional. **Requires Controller Lock must stay off**, otherwise keyboard / gamepad players have to press Accept before Left / Right moves the slider. Range 0..1, step 0.05, Percent format are set by the C++ class. The old mute check box is gone: the reference design has none, and the framework mute API is still there for game code.

### 7.2 `WBP_Tab_Audio` — `Content/UI/Menus/Settings/Tabs/`

- Parent class **Sunset Cove Audio Tab Widget** (was `BaseTabWidget`).
- Inside `ScrollBox > VerticalBox`: five `WBP_VolumeRow` instances. Select one and set, in Details > **Volume Row**:

| Row | Category | Label | Preview Cue |
|---|---|---|---|
| `MasterRow` | `CoreFramework.Audio.Category.Master` | Master | — |
| `MusicRow` | `...Category.Music` | Music | — |
| `AmbientRow` | `...Category.Ambient` | Ambient | — |
| `SFXRow` | `...Category.SFX` | Effects | — (set one when a short SFX cue exists) |
| `UIRow` | `...Category.UI` | Interface | `Audio.Cue.UI.Click` |

- **Do not add** *Has Pending Changes*, *Apply Changes* or *Revert Changes* overrides in this Blueprint. The C++ parent implements them; a Blueprint override (even an empty one) replaces it and Apply / Revert stop working. The old placeholder overrides were removed for this reason.
- New category in the menu: drag one more `WBP_VolumeRow` in and pick its Category. No code.

### 7.3 `WBP_Settings`

There are no Apply / Revert buttons: **Back applies** every page (`USunsetCoveSettingsMenuWidget`), so the volumes are written to disk when the player leaves Settings. Volumes are live while the slider moves. See `Docs/UISystem/UI_Settings_Architecture.md`.

## 8. Is anything else global touched?

| Where | Touched? |
|---|---|
| Project Settings > Audio (sample rate, channels, default Sound Class / Mix) | **No.** Leave *Default Sound Class* and *Default Base Sound Mix* empty: the framework sets its own mix |
| World Settings of each level | **No** (do not set a Base Sound Mix there; it would replace the volume mix) |
| `Plugins/CoreFramework` | **Never** |

## 9. Player volumes on disk

`Saved/Config/<Platform>/GameUserSettings.ini` (in the editor: `Saved/Config/WindowsEditor/`):

```ini
[CoreFramework.Audio]
Volume.CoreFramework.Audio.Category.Music=0.7
Mute.CoreFramework.Audio.Category.Master=False
```

Delete the section to return to the config's Default Volumes.

## 10. Test checklist (done 2026-11-12 in PIE on `L_MainMenu`)

| # | Check | Result |
|---|---|---|
| 1 | Start PIE: no `No audio config assigned` | OK |
| 2 | Map Audio runs (`LogSunsetCove Verbose: Map audio of BP_GameMode_MainMenu_C_0 ...`) | OK — music/ambient warn "no sound" until real tracks are added (expected) |
| 3 | Hover / click menu buttons, open Options: no `LogCoreAudio` warnings for UI cues | OK |
| 4 | Options > Audio shows five rows with config defaults (100 / 70 / 80 / 100 / 100 %) | OK |
| 5 | Move a slider: the % follows | OK |
| 6 | **Revert** puts the old value back (tested before Back-applies; the tab still supports it) | OK |
| 7 | **Back** writes `GameUserSettings.ini` (was the Apply button; re-tested with the Graphics page) | OK |
| 8 | Mute (framework API) is snapshotted and restored by the tab | OK (no mute widget any more) |
| 9 | Keyboard: Up/Down between rows, Left/Right change by 5 % | OK |
| 10 | Stop and restart PIE: saved values come back | OK |
| 11 | **By ear** (not checked by the automated pass): Master scales everything, Music slider changes only music, music keeps playing in the pause menu, menu → gameplay crossfades | to do once real sounds are in |

Turn on details with the console: `Log LogCoreAudio Verbose`.

## 11. Troubleshooting

| Symptom | Fix |
|---|---|
| `Cue X is not in the audio config or has no sound` | Add the row / fill its Sound in `DA_AudioConfig` |
| `Audio category X has no Sound Class` | Fill Sound Class in that Categories row |
| Slider moves but nothing changes | the sound is played outside the framework and its asset has no Sound Class (section 4 step 2) |
| Master too loud/quiet | a Sound Class is not a child of `SC_Master`, or its Volume is not 1.0 |
| Music plays once and stops | the sound asset is not set to Looping |
| A button plays two sounds | a Slate sound slot is set in its `BP_ButtonStyle_*` |
| HUD makes a "popup" sound | *Use Default Screen Cues* is on for the HUD screen |
| Apply / Revert do nothing on the Audio tab | a Blueprint override of Has Pending Changes / Apply / Revert was added to `WBP_Tab_Audio` (section 7.2) |
| Row shows 100% and does nothing | its Category is empty (warning `volume row without a Category` in `LogSunsetCove`) |
| Gamepad must press A before the slider moves | *Requires Controller Lock* got turned back on in `WBP_VolumeRow` |
