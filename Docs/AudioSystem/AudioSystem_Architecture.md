# Audio System — Architecture

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Audience:** programmers
**Editor setup (what to create and click):** [`AudioSystem_Setup.md`](AudioSystem_Setup.md)
**Framework reference (read-only submodule):** `Plugins/CoreFramework/Docs/AudioSystem/Audio_Architecture_Overview.md`

---

## 1. Purpose and the one rule

Sunset Cove does **not** have its own audio engine. All playback, volume, persistence, music crossfades and UI sounds are done by the **CoreFramework audio system** (`Plugins/CoreFramework/Source/CoreFramework/.../AudioSystem/`). The project adds only what a framework cannot know:

| The project adds | Why the framework cannot |
|---|---|
| The sound **data**: Sound Classes, `DA_AudioConfig`, cue tags, the sound assets | they are the game's content |
| **Which music each map plays** (`FSunsetCoveMapAudio` on the GameModes) | maps and GameModes belong to the game |
| The **volume settings UI** (`USunsetCoveVolumeRowWidget`, `USunsetCoveAudioTabWidget`) | the framework ships no settings tab ("not included" in its doc) |

Rule: game code and data name a **cue tag** (`Audio.Cue.UI.Click`), never a sound asset or a Sound Class. Which asset plays, how loud, in which category, 2D or 3D, looping or not — all of that is data.

`Plugins/CoreFramework` is never edited from this project.

---

## 2. The two layers

```
 SUNSETCOVE (Source/SunsetCove/.../AudioSystem/, Content/Audio/, Config/)
 ┌──────────────────────────────────────────────────────────────────────────────────────┐
 │  BP_GameMode_MainMenu / BP_GameMode_Gameplay        WBP_Settings (UBaseMenuWidget)     │
 │     MapAudio : FSunsetCoveMapAudio                     └─ WBP_Tab_Audio                 │
 │        │ BeginPlay -> Apply()                             (USunsetCoveAudioTabWidget)   │
 │        │                                                  └─ WBP_VolumeRow x5           │
 │        │                                                     (USunsetCoveVolumeRowWidget)│
 │        │  PlayMusic / PlayAmbient / StopMusic               │ Set/GetCategoryVolume,      │
 │        │                                                    │ Set/IsCategoryMuted,        │
 │        │                                                    │ SaveAudioSettings,          │
 │        │                                                    │ OnCategoryVolumeChanged     │
 │  Content/Audio/DA_AudioConfig  ◄── Config/DefaultGame.ini   │                             │
 │     Categories[] -> SC_Master, SC_SFX, SC_Music, SC_Ambient, SC_UI (Content/Audio/Classes) │
 │     Cues[]       -> cue tag -> Sound Wave / Sound Cue / MetaSound                          │
 └────────┼────────────────────────────────────────────────────┼─────────────────────────────┘
          ▼                                                    ▼
 COREFRAMEWORK (read-only)
 ┌──────────────────────────────────────────────────────────────────────────────────────┐
 │  UAudioFrameSettings (Project Settings > CoreFramework > CoreFramework Audio)          │
 │  UAudioFrameSubsystem (UGameInstanceSubsystem, one per game instance)                  │
 │     cue library · category volumes · Sound Mix · music / ambient layers · handles      │
 │  UBaseButton / UBaseActivatableWidget / UBaseUserWidget  (UI sounds from the config)   │
 │  UAnimNotify_PlayAudioCue · UAudioEmitterComponent · UAudioFunctionLibrary             │
 └──────────────────────────────────────────────────────────────────────────────────────┘
          ▼
 ENGINE: Audio Components, Sound Classes, Sound Mix (SetBaseSoundMix / SetSoundMixClassOverride), MetaSounds
```

---

## 3. Classes and assets

### 3.1 Project C++ (`Source/SunsetCove/{Public,Private}/AudioSystem/`)

| Class / struct | File | Role |
|---|---|---|
| `FSunsetCoveMapAudio` | `AudioSystem/SunsetCoveMapAudio.h` | `MusicCue`, `AmbientCue`, `FadeTime`. `Apply()` starts, crossfades or stops the two framework layers. |
| `USunsetCoveVolumeRowWidget` | `AudioSystem/Widgets/SunsetCoveVolumeRowWidget.h` | One slider line for **one** category, built on the framework's slider row (same look, keys and focus as every settings row). Reads/writes the subsystem; holds no audio state; plays its `PreviewCue` when the slider is released. Parent of `WBP_VolumeRow`. |
| `USunsetCoveAudioTabWidget` | `AudioSystem/Widgets/SunsetCoveAudioTabWidget.h` | Parent of `WBP_Tab_Audio`. Finds every row in its layout and implements Apply / Revert of the framework tab contract. |
| `ASunsetCoveMainMenuGameMode`, `ASunsetCoveGameplayGameMode` | `Core/` | Each has `MapAudio` (Class Defaults > Audio) and calls `MapAudio.Apply(this)` in `BeginPlay`. |
| `ASunsetCoveWindAmbience` (`BP_WindAmbience`) | `AudioSystem/Ambience/SunsetCoveWindAmbience.h` | The wind of a gameplay level: 2D loop (`Audio.Cue.Ambient.Wind`), volume = shore proximity (asked to the sea) × shelter (8 horizontal rays: rocks close around the listener quiet it). Placed once per level. |
| `ASunsetCoveSea` (`BP_Sea`) | `Environment/Water/SunsetCoveSea.h` | Owns the **waves on the beach** sound: `Audio.Cue.Ambient.ShoreWaves` attached to an emitter that slides along the shoreline spline to the point closest to the listener; built-in attenuation when the cue has none. See `Docs/Environment/Sea_Architecture.md`. |
| `USunsetCoveFootstepComponent` (on `BP_Character`), `USunsetCoveFootstepSet` (`DA_Footsteps`) | `AudioSystem/Footsteps/` | Step and landing sounds chosen by the ground under the feet (physical material of landscape layer / mesh, or the sea). See `Footsteps.md`. |

**Two kinds of ambience.** A *map* ambience is a 2D layer of the GameMode (`MapAudio.AmbientCue`, crossfaded between maps: the
main menu waves). A *world* ambience belongs to places in a level and is played by actors there (the sea, the wind; later
seagulls, a bar's music). The gameplay GameMode has no map ambience: the cove is heard through its world ambience.

No new module dependency: `SunsetCove.Build.cs` already lists `CoreFramework`, `UMG`, `CommonUI`, `GameplayTags`.

> **Include note.** Since CoreFramework v2.2, `AudioFrameSubsystem.h` includes `Components/AudioComponent.h` itself (it used `EAudioFaderCurve` without it). The project's workaround includes were removed: include the subsystem header directly.

### 3.2 Assets (`Content/Audio/`)

| Asset | Type | Points at |
|---|---|---|
| `Classes/SC_Master` | Sound Class (root) | — |
| `Classes/SC_SFX`, `SC_Music`, `SC_Ambient`, `SC_UI` | Sound Class, **Parent = SC_Master** | — |
| `DA_AudioConfig` | `UAudioConfigDataAsset` | `Categories[]` → the five Sound Classes; `Cues[]` → sound assets; UI default cues |
| `UI/Beach/S_UI_Hover_Breeze`, `S_UI_Click_Shell`, `S_UI_Open_Breeze`, `S_UI_Close_Breeze` | Sound Wave (Sound Class `SC_UI`) | soft, beach-like UI sounds (air puff, shell tap, rising / falling breeze), quiet under the ambience; the source `.wav` files sit next to them. See `Docs/UISystem/UI_Settings_Architecture.md` § 12 |

Widgets (`Content/UI/`):

| Asset | Type | Contains |
|---|---|---|
| `Menus/Settings/Tabs/WBP_Tab_Audio` | Widget Blueprint, parent `USunsetCoveAudioTabWidget` | five `WBP_VolumeRow` instances |
| `Components/Settings/WBP_VolumeRow` | Widget Blueprint, parent `USunsetCoveVolumeRowWidget` (a CoreFramework `USliderSettingRowWidget`) | the slider row look of every settings page: `FocusHighlight`, `LabelText`, `ArrowLeft`, `ValueSlider` (step 0.05, Requires Controller Lock off), `ArrowRight`, `ValueText` (Percent) |

The framework doc suggests shared Sound Classes and a template config inside `CoreFramework Content/Audio`. That folder is not in this submodule, and the submodule is read-only, so Sunset Cove owns its Sound Classes in `Content/Audio/Classes`. Same hierarchy, same behavior.

### 3.3 Config

| File | Section | Content |
|---|---|---|
| `Config/DefaultGame.ini` | `[/Script/CoreFramework.AudioFrameSettings]` | `DefaultAudioConfig=/Game/Audio/DA_AudioConfig.DA_AudioConfig` |
| `Config/DefaultGameplayTags.ini` | GameplayTagsSettings | `Audio.Cue.*` tags (UI.Hover/Click/PopupOpen/PopupClose, Music.MainMenu/Gameplay, Ambient.MainMenu/ShoreWaves/Wind) + a redirect Ambient.Beach → Ambient.MainMenu (old name) |
| `Saved/Config/Windows/GameUserSettings.ini` (per player, written at runtime) | `[CoreFramework.Audio]` | `Volume.<CategoryTag>=0.8`, `Mute.<CategoryTag>=False` |

The five category tags `CoreFramework.Audio.Category.{Master,SFX,Music,Ambient,UI}` are **native** (C++, `FAudioTags`); they are not in the `.ini`.

---

## 4. Volume routing — what happens when a slider moves

```
Player drags the Music slider (WBP_VolumeRow, Category = CoreFramework.Audio.Category.Music)
 │
 ├─ USliderSettingRowWidget (framework) updates ValueText "60%" and broadcasts On Value Changed
 │  USunsetCoveVolumeRowWidget::HandleRowValueChanged(0.6)
 │     UAudioFrameSubsystem::SetCategoryVolume(Music, 0.6)
 │        ├─ CategoryVolumes[Music] = 0.6
 │        ├─ GConfig->SetFloat("CoreFramework.Audio", "Volume.CoreFramework.Audio.Category.Music", 0.6, GameUserSettings)
 │        │      (in memory; written to disk by SaveAudioSettings or at game-instance shutdown)
 │        ├─ EnsureMixApplied(): first time in this world -> SetBaseSoundMix(World, internal mix) + every category
 │        ├─ ApplyCategoryVolume(Music, VolumeFadeTime 0.05 s)
 │        │      SetSoundMixClassOverride(Mix, SC_Music, 0.6, pitch 1, fade, bApplyToChildren = true)
 │        └─ OnCategoryVolumeChanged.Broadcast(Music, 0.6)
 │                └─ every row whose Category == Music -> RefreshFromAudio() (guarded, no write-back)
 │
 └─ Engine: final volume of a music voice = SC_Master volume × SC_Music volume × cue volume × sound volume
```

- **Master scales everything**: each category's mix adjuster is applied with `bApplyToChildren = true`, so Master reaches SFX, Music, Ambient and UI. (Until 2026-11-13 the framework used `false`, believing the Sound Class tree would propagate it; the engine applies mix adjusters *after* propagating the tree, so Master did nothing. Fixed in the framework.)
- **Mute** (framework API, no widget in the Settings page any more) sets the class override to 0 without losing the slider value. The audio tab still snapshots and restores it, so a game feature that mutes a category is respected by Revert.
- **Every sound played through the framework is forced into its category's Sound Class** (`SoundClassOverride` on the Audio Component), so a sound asset with no Sound Class still obeys the slider when played through a cue. A sound played *outside* the framework (a placed *Ambient Sound* actor, a Niagara sound, a Blueprint *Play Sound 2D*) obeys the sliders only if its asset has a Sound Class set. The setup guide therefore asks to set the Sound Class on every imported asset.
- **Pause**: the UI category always plays while paused; Music too because its row in `DA_AudioConfig` has *Play When Paused* ticked.

### 4.1 The Audio tab (Apply / Revert)

`USunsetCoveAudioTabWidget` keeps a snapshot `{volume, muted}` of every row's category, taken when the tab is constructed and after every Apply / Revert (and when the tab is selected again with nothing pending).

| Framework call (`UBaseMenuWidget`) | Audio tab does |
|---|---|
| `HasPendingChanges` | any category differs from the snapshot |
| `ApplyChanges` | `SaveAudioSettings()` (flush `GameUserSettings.ini`) and new snapshot |
| `RevertChanges` | writes the snapshot back through `SetCategoryVolume` / `SetCategoryMuted`; rows refresh by event |

The three functions are `BlueprintNativeEvent`s of the framework. **A Blueprint override in `WBP_Tab_Audio` — even an empty one — replaces the C++ implementation.** The original placeholder tab had a *Has Pending Changes* override returning `false` and empty *Apply / Revert* events; they were deleted when the tab was reparented. Keep the Blueprint free of them.

The rows are found by walking the tab's `WidgetTree`, so the tab has no `BindWidget` and does not care how many rows there are or how they are laid out. `WBP_Settings` has no Apply button: **Back applies** every page (`USunsetCoveSettingsMenuWidget`, see `Docs/UISystem/UI_Settings_Architecture.md` § 7), so the volumes are saved when the player leaves Settings.

---

## 5. Sound routing — from a tag to the speakers

```
Game code / data names a cue tag             e.g. UBaseButton click -> DA_AudioConfig.DefaultButtonClickCue = Audio.Cue.UI.Click
 │
UAudioFrameSubsystem::PlayCue2D / AtLocation / Attached / PlayUICue / PlayMusic / PlayAmbient
 │  CueMap[Audio.Cue.UI.Click]  (built from DA_AudioConfig.Cues at game-instance start)
 │  sound loaded? no -> async load, plays when loaded (dropped if later than MaxLateCueSeconds)
 │  category = cue.Category, else caller's fallback (UI for UI calls, Music / Ambient for layers), else SFX
 │  volume / pitch = request × cue multiplier × random(1 ± range)
 ▼
UAudioComponent (SoundClassOverride = category Sound Class, attenuation / concurrency overrides, bIsUISound if play-when-paused)
```

Who triggers what in Sunset Cove today:

| Trigger | Where it is set | Framework call |
|---|---|---|
| Menu music / ambience | `BP_GameMode_MainMenu` > Audio > Map Audio | `PlayMusic` / `PlayAmbient` (or `Stop*` when empty) |
| Gameplay music / ambience | `BP_GameMode_Gameplay` > Audio > Map Audio | same |
| Button hover / click | `DA_AudioConfig` > UI Defaults (all `UBaseButton`, e.g. `WBP_Button_Menu`) | `PlayUIEvent` |
| Screen open / close | `DA_AudioConfig` > UI Defaults + *Use Default Screen Cues* on the screen | `PlayUIEvent` |
| Settings slider preview | `WBP_VolumeRow` instance > Preview Cue | `PlayUICue` (cue's own category wins) |

### 5.1 Music across maps

```
L_MainMenu begins play -> MapAudio(Music.MainMenu, Ambient.MainMenu) -> PlayMusic(MainMenu), PlayAmbient(MainMenu waves)
New Game / Continue -> OpenLevel(L_Gameplay)        (music components ignore level flushing: they keep playing)
L_Gameplay begins play -> MapAudio(Music.Gameplay, no ambient)
      PlayMusic(Gameplay)   -> MainMenu fades out, Gameplay fades in (DefaultMusicFadeTime, equal power)
      StopAmbient()         -> the menu waves fade out
      BP_Sea (BeginPlay)    -> PlayCueAttached(ShoreWaves) on the emitter that slides along the shoreline
      BP_WindAmbience       -> PlayCue2D(Wind), volume updated every 0.15 s (shore distance x shelter)
```

An empty `MusicCue` means *this map is silent*, so the framework stops the music. To keep one track across two map types, give both the same cue.

---

## 6. Extending (no C++ for any of these)

| Goal | Steps |
|---|---|
| A new sound | tag `Audio.Cue.<Area>.<Name>` → row in `DA_AudioConfig > Cues` → call it (Blueprint *Play Cue 2D / At Location / Attached*, *Play Audio Cue* anim notify, a button / screen field). |
| A new category (Voice, Footsteps) | gameplay tag `CoreFramework.Audio.Category.<Name>` (the `Categories` pickers filter on that root) → Sound Class child of `SC_Master` → row in `DA_AudioConfig > Categories` → one more `WBP_VolumeRow` in `WBP_Tab_Audio`. |
| Music of a new kind of map | its GameMode Blueprint child (or one of the existing two) → Class Defaults > Audio > Map Audio. |
| Drive a MetaSound | keep the `FAudioHandle` (or *Get Layer Handle (Music)*) → *Set Handle Float Parameter*. |
| Sounds in arms / prop animations | *Play Audio Cue* notify, Audience **Owner Only** for first-person arms. |

C++ callers include `AudioSystem/Manager/AudioFrameSubsystem.h` (or `AudioSystem/Library/AudioFunctionLibrary.h`) and keep cue tags in `UPROPERTY` fields filtered with `meta = (Categories = "Audio.Cue")`, never `RequestGameplayTag` strings.

---

## 7. Graceful degradation and limits

| Situation | Behavior |
|---|---|
| `DA_AudioConfig` missing | `LogCoreAudio: No audio config assigned` once; menus work, no cue sounds, sliders do nothing audible. |
| A row with no Category | `LogSunsetCove` warning naming the row; the row shows 100% and edits nothing. |
| A Map Audio cue with no row in the config | `LogCoreAudio` warning, silence. |
| A cue whose sound does not loop played as music | plays once and ends (the framework never forces looping). |
| Editor preview (UMG designer) | rows show their label only; no subsystem calls. |

Limits: cues stay in memory after first use (turn **Preload** off for rare/large sounds, use streaming for long music); the framework's own doc states its playback paths were not exercised before Sunset Cove adopted it — report anything odd so a fix can be prepared under `FrameworkPatches/`.
