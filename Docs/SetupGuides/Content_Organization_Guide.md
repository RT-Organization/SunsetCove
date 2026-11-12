# Content Organization Guide — where every asset goes in the Unreal project

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8
**Purpose:** one place that says **where to create or import each kind of asset** and **how to name it**, so everybody (programmers, animator, artists) puts things in the same place.
**Companion:** `Unreal_Setup_Guide.md` (how to create and wire props and tools). That guide links here for folders.
**Rule that never changes:** `Plugins/CoreFramework` (and its `Content`) is a read-only submodule. Never put or edit anything there.

> **The one rule to remember.** Group by **what the thing is** (the item or the feature), not by file type. Everything that belongs to the *Sunbed* mesh lives under the Sunbed; everything that belongs to the *player's arms* lives under Animation/ArmsItems; the *Blueprints that run the game* live under Gameplay.

---

## 1. The map (what exists today, and the places for what is coming)

Marks: **exists** = already in the project, **new** = create it the first time you need it.

```
Content/
├── Animation/                     animations of the PLAYER'S ARMS (first person), not of props
│   ├── Blueprint/                 ABP_FPArms_Base, ABP_FPArms_Items                     exists
│   ├── FirstPerson/               base unarmed arms: AS_Idle, AS_Walk, AS_Run, AS_Jump*  exists (item animations NEVER here)
│   └── ArmsItems/                                                                       exists
│       ├── DA_ArmsAnimations      the data asset of all arms animations                 exists
│       └── Items/<Item>/          AS_Arms_<Item>_<Action>_<Hand> from the animator      new per item
│           └── ToolMenu/          radial menu arm clips                                 new
│
├── Mesh/                          every 3D model + ITS OWN animations
│   ├── Props/<Item>/              Sunbed, Umbrella...                                   exists (Sunbed)
│   ├── Tools/<Item>/              Bucket, Spray, Toy, GarbageBag                        exists, empty
│   ├── Characters/<Name>/         NPC models when they exist                            new
│   └── Environment/<Name>/        rocks, trees, cave, barrier...                        new
│
├── Character/                     the player and the visitors
│   ├── Blueprints/                BP_Character                                          exists
│   ├── Mesh/                      SM_Character (arms), SK_Character, PA_Character       exists
│   └── NPC/                       BP_Visitor                                            exists
│
├── Gameplay/                      Blueprints of game objects and rules
│   ├── Props/                     BP_Sunbed, BP_Umbrella...                             new (BP_Sunbed is in Blueprints/ today)
│   ├── Tools/                     BP_Bucket, BP_Spray...                                new
│   ├── Visitors/                  BP_VisitorSpawner                                     exists
│   ├── Adversities/               crab, seagull, wind...                                new
│   ├── Events/                    police, entrepreneur...                               new
│   ├── World/                     barrier button, bed, rest areas...                    new
│   └── Blueprints/                BP_Sunbed (legacy; move to Props/ when convenient)    exists
│
├── Core/                          GameInstance and GameModes (BP_GameInstance, BP_GameMode_*)   exists
├── Player/                        BP_PlayerController                                   exists
├── Input/                         Actions/ (IA_*), Context/ (IMC_SunsetCove, IMC_Movement, IMC_Player), Data/ (DA_InputConfig_SunsetCove)   exists
├── Environment/                   day/night and world systems                           exists
│   └── DayNightCycle/             BP_DayNightManager, DA_DayNightLightingProfile_Default, Curves/   exists
├── Materials/                     shared materials and instances (MI_Sand, M_Hologram...)  exists (Test/)
├── UI/                            all widgets: Components/, HUD/, Menus/, Popups/, Config/, Startup/   exists
│   ├── Components/Settings/       settings row presets, prompts (WBP_SettingRow_*, WBP_InputPrompt...)   exists
│   ├── Components/Text/           every text style (BP_TextStyle_*)                     exists
│   ├── Fonts/                     Font_UI_Main and the logo font                         exists
│   ├── Input/Data/                controller data: glyphs per device (BP_InputData_*)   exists
│   ├── Input/Glyphs/              button glyph textures: KeyboardMouse/, Xbox/, PlayStation5/, PlayStation4/, Switch/   exists
│   └── Icons/                     textures for icons (tools, requests, satisfaction emoji)    new
├── Audio/                         DA_AudioConfig, Classes/ (SC_*), UI/Beach/, SFX/, Ambient/, Music/   exists (Docs/AudioSystem/AudioSystem_Setup.md)
├── VFX/                           Niagara systems and their materials                   new
├── Levels/                        _Main/ (shipping maps), Ro/, Ni/, Fra/ (personal test areas)   exists
├── Collections/, Developers/      editor-side, personal; never reference them from the game   exists
└── Fab/                           marketplace imports, kept as they arrive; do not reorganize   exists
```

Source files that are **not** Content (Blender, FBX) stay in the repository folder `Export (blender)/` (see section 6).

---

## 2. Where does it go? (quick answer table)

| I have... | It goes in | Example |
|---|---|---|
| A **prop** skeletal mesh, skeleton, physics asset | `Content/Mesh/Props/<Item>/` | `SM_Sunbed`, `SK_Sunbed_Skeleton`, `PA_Sunbed` |
| **The prop's own animations** (Open, Close, poses) | `Content/Mesh/Props/<Item>/` (next to the mesh) | `AS_Sunbed_Open`, `AS_Sunbed_Close`, `AS_Sunbed_OpenPose`, `AS_Sunbed_ClosedPose` |
| **The arms animations** for that prop (Grab, Hold, Place...) | `Content/Animation/ArmsItems/Items/<Item>/` | `AS_Arms_Sunbed_Grab_R` |
| A **tool** skeletal mesh, skeleton, physics asset | `Content/Mesh/Tools/<Item>/` | `SM_Bucket`, `SK_Bucket_Skeleton` |
| **The tool's own animations** (Equip, Use) | `Content/Mesh/Tools/<Item>/` | `AS_Spray_Use`, `AS_Bucket_Equip` |
| **The arms animations** for that tool | `Content/Animation/ArmsItems/Items/<Item>/` | `AS_Arms_Bucket_Use_R` |
| **Prop Blueprint** | `Content/Gameplay/Props/` | `BP_Sunbed` |
| **Tool Blueprint** | `Content/Gameplay/Tools/` | `BP_Bucket` |
| **Material** of a prop or tool | next to its mesh, `Content/Mesh/<Props or Tools>/<Item>/` | `MI_Sunbed` |
| A **shared material** (sand, water, hologram) | `Content/Materials/` | `M_PropHologram` |
| **Texture** of a prop or tool | next to its mesh, in a `Textures/` sub folder | `T_Sunbed_BaseColor` |
| **Tool / request / emoji icon** (a Texture2D for the UI) | `Content/UI/Icons/<Group>/` | `T_Icon_Bucket` |
| A **visitor / NPC** model | `Content/Mesh/Characters/<Name>/` | `SM_VisitorA` |
| A **visitor / NPC** animation or ABP | `Content/Mesh/Characters/<Name>/Animations/` | `ABP_Visitor` |
| The **player's base arms animation** (idle, walk, run, jump) | `Content/Animation/FirstPerson/` | `AS_Walk` |
| The **arms Animation Blueprints** | `Content/Animation/Blueprint/` | `ABP_FPArms_Items` |
| An **adversity** actor (crab, seagull) and its assets | `Content/Gameplay/Adversities/<Name>/` | `BP_Crab` |
| An **event** actor (police, bodyguard) | `Content/Gameplay/Events/<Name>/` | `BP_PoliceOfficer` |
| A **widget** (HUD, menu, popup) | `Content/UI/...` (see its sub folders) | `WBP_HUD` |
| An **Input Action** / **mapping context** | `Content/Input/Actions/`, `Content/Input/Context/` | `IA_PropPlace` |
| A **button glyph** set / **controller data** | textures in `Content/UI/Input/Glyphs/<Family>/`, data in `Content/UI/Input/Data/` | `BP_InputData_Gamepad_Xbox` |
| A **settings row** or **prompt** preset | `Content/UI/Components/Settings/` | `WBP_SettingRow_Slider` |
| A **data asset** | next to the system that reads it | `DA_ArmsAnimations` (in `Animation/ArmsItems/`), `DA_UIConfig` (in `UI/Config/`) |
| A **curve** | in the folder of its system | `Environment/DayNightCycle/Curves/` |
| A **sound** | `Content/Audio/<SFX or Ambient or Music>/` | `S_Sunbed_Open` |
| A **Niagara** system | `Content/VFX/<Name>/` | `NS_SprayMist` |
| A **level** | `Content/Levels/_Main/` (shipping), or your own `Content/Levels/<YourName>/` for tests | `L_TestRo` |

> **Why the prop's own animations are next to its mesh, and the arms animations are in `Animation/ArmsItems`:** the prop's own animations use the **prop's skeleton** (`SK_Sunbed_Skeleton`) and move the sunbed's bones; they travel with the mesh (if you move or delete the sunbed, they go with it). The arms animations use the **player's skeleton** (`SK_Character`) and are all read by one data asset, `DA_ArmsAnimations`, which looks in `Animation/ArmsItems/Items` (its *Search Folder*). **Rule of thumb: the folder follows the skeleton the animation was made for.**

---

## 3. The full tree of one prop (the Sunbed, as the model for every new prop)

```
Content/
├── Mesh/Props/Sunbed/                          (the prop and ITS OWN animations)
│   ├── SM_Sunbed                               skeletal mesh
│   ├── SK_Sunbed_Skeleton                      skeleton
│   ├── PA_Sunbed                               physics asset
│   ├── MI_Sunbed                               material instance (or Materials/ sub folder)
│   ├── Textures/T_Sunbed_BaseColor ...
│   ├── AS_Sunbed_Close                         closing animation (ends closed)
│   ├── AS_Sunbed_Open                          opening animation (ends open)
│   ├── AS_Sunbed_ClosedPose                    optional, one frame
│   └── AS_Sunbed_OpenPose                      optional, one frame
├── Gameplay/Props/
│   └── BP_Sunbed                               parent: Sunset Cove Prop
└── Animation/ArmsItems/Items/Sunbed/           (the player's arms with the sunbed)
    ├── AS_Arms_Sunbed_Grab_R / _L
    ├── AS_Arms_Sunbed_Hold_R / _L
    ├── AS_Arms_Sunbed_Place_R / _L
    ├── AS_Arms_Sunbed_ThrowCharge_R / _L
    ├── AS_Arms_Sunbed_Throw_R / _L
    └── AS_Arms_Sunbed_Drop_R / _L              (+ optional PlaceAim)
```

A **tool** is the same with `Mesh/Tools/<Item>/`, `Gameplay/Tools/BP_<Item>` and arms animations only for `_R` (Equip, Holster, Hold, Use, optional Pickup and Drop).

---

## 4. Naming (prefixes and patterns)

Follow what is already in the project. The Unreal convention is `<Prefix>_<Name>`; keep the **item name in the file name** so a name is unique across the project.

| Asset | Prefix | Example |
|---|---|---|
| Blueprint (actor, character, gamemode) | `BP_` | `BP_Sunbed` |
| Widget Blueprint | `WBP_` | `WBP_HUD` |
| Animation Blueprint | `ABP_` | `ABP_FPArms_Items` |
| Skeletal mesh | `SM_` (this project's choice) | `SM_Sunbed` |
| Skeleton | `SK_` and `_Skeleton` | `SK_Sunbed_Skeleton` |
| Physics asset | `PA_` | `PA_Sunbed` |
| Animation sequence | `AS_` | `AS_Sunbed_Open` |
| Arms animation | `AS_Arms_<Item>_<Action>_<Hand>` | `AS_Arms_Umbrella_Drop_L` (exact, the code reads it) |
| Material / instance | `M_` / `MI_` | `MI_Sand` |
| Texture | `T_` | `T_Sunbed_BaseColor` |
| Data asset | `DA_` | `DA_ArmsAnimations` |
| Curve | `Curve_` | `Curve_SunIntensity` |
| Input Action / mapping context | `IA_` / `IMC_` | `IA_PropPlace` |
| Niagara system | `NS_` | `NS_SprayMist` |
| Sound | `S_` | `S_Sunbed_Open` |

- **Item ids** (`Sunbed`, `Bucket`...): one English word, capital first letter, the same everywhere (Blueprint, animator's files, `DA_ArmsAnimations`, data sheet). Details in `Unreal_Setup_Guide.md`, 1.1.
- **English only** for every asset and folder name; no spaces; `PascalCase` for folders.
- **Never** name a file only by its function (`AS_Open_Pose`): add the item (`AS_Sunbed_OpenPose`).

---

## 5. Habits that keep the project tidy

1. **Create and move assets only inside the Content Browser**, never in the file explorer: Unreal fixes references. After moving a folder or an asset, right-click the folder > **Fix Up Redirectors in Folder**.
2. **No loose assets in the `Content/` root.** Every asset lives in a folder above.
3. **No "Test", "New", "Copy", "Final" names** in shipping folders. Experiments go in your personal folder (`Content/Levels/<YourName>/`, `Content/Developers/<YourName>/`) and are not referenced by the game.
4. **One item, one folder.** If you delete an item, you delete its folder (and its arms animations folder) and nothing else breaks.
5. **Do not mix skeletons in a folder of animations.** An animation made for `SK_Sunbed_Skeleton` is never in `Animation/ArmsItems`, and an arms animation is never in `Mesh/Props`.
6. **Do not put anything in `Animation/FirstPerson/`** except the base unarmed animations.
7. **Do not move or rename** `DA_ArmsAnimations` without also changing the *Post Process / Animation Set* references on `BP_Character` (the editor updates them if you use Move).
8. **Check in what changed.** Unreal assets are binary: commit your assets in small, separate commits and avoid two people editing the same asset.

---

## 6. Source files outside Content (repository root)

| Folder | What | Rule |
|---|---|---|
| `Export (blender)/Assets/<Item>/` | the Blender export of a prop / tool: `SK_<Item>.fbx` and an `Anim<Item>/` sub folder for its own animations | the animator delivers here; the project owner imports into `Content/Mesh/...` |
| `Export (blender)/Character/` | the arms: meshes (`MeshCharacter`) and animations (`AnimazioniCharacter*`, and `AnimazioniInventario/<Item>/` for the arms animations of items) | same |
| `Blender/` | the `.blend` working files | not imported by Unreal |
| `Docs/` | documentation, one folder per system (`Docs/<System>/`), and `Docs/SetupGuides/` for how-to guides | English; the animator guide is the only Italian one |
| `Config/` | project settings files | edited through the editor's Project Settings when possible |
| `Source/` | C++ (see the system docs) | |
| `Plugins/CoreFramework` | read-only submodule | **never edit** |

### Import checklist (any FBX into Content)

1. Decide the family with the table in section 2 (**the folder follows the skeleton**).
2. Create the folder if the item is new (`Content/Mesh/Props/<Item>/`).
3. Drag the FBX in; for animations choose **Skeleton = the right one**, root motion off, **keep the file name**.
4. Rename if the name does not follow section 4.
5. Open it once (check the preview), save, and commit.

---

## 7. Open points (decide once, then write the answer here)

- Whether the `Gameplay/Blueprints/BP_Sunbed` moves to `Gameplay/Props/` (recommended; use the Content Browser **Move**, then Fix Up Redirectors).
- Where visitor (NPC) meshes go once they exist: `Mesh/Characters/<Name>/` is proposed above, to be confirmed.
- VFX folder structure is a proposal: the project has none yet. (Audio exists since 2026-11-12: `Content/Audio/`, see `Docs/AudioSystem/AudioSystem_Setup.md`.)
- Sub folders for textures and materials of an item (`Textures/`, `Materials/`) are optional while an item has only a few assets.
