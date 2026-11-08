# Engine Setup Guide — Arms Animation System (props and tools)

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8
**Audience:** whoever sets this up in the editor and later drops in the animator's files. How it works: see `Architecture_Overview.md`.
**Goal:** do the setup once. After that, every animation the animator delivers is **import, press one button, test**.

> The code was written without access to the editor. Every graph and asset step below is a description of what to build, not something that was run. The steps marked **(check)** are the ones to confirm first; section 7 lists them.

---

## 0. What you are setting up

| # | What | Where |
|---|---|---|
| 1 | Build the C++ | Visual Studio / `Build.bat` |
| 2 | Two animation slots on the arms skeleton | `SK_Character` (done by a button) |
| 3 | The post-process Animation Blueprint | `Content/Animation/Blueprint/ABP_FPArms_Items` |
| 4 | The data asset with all the animations | `Content/Animation/ArmsItems/DA_ArmsAnimations` |
| 5 | Two properties on `BP_Character` | Components > Arms Animation |
| 6 | Item ids on the prop and tool Blueprints | `PropId` / `ToolId` |
| 7 | The animator's files | `Content/Animation/ArmsItems/Items/<Item>/` |

### Folder structure and names

`Content/Animation/FirstPerson/` keeps **only the base unarmed animations** (`AS_Idle`, `AS_Walk`, `AS_Run`, `AS_Jump*`). Do not put item animations there. Everything new goes in a separate folder, `Content/Animation/ArmsItems/`:

```
Content/Animation/
├── Blueprint/
│   ├── ABP_FPArms_Base                  base arms ABP (your renamed ABP_SunsetCoveCharacter), untouched
│   └── ABP_FPArms_Items                 NEW, post-process ABP
├── FirstPerson/                         base unarmed animations, untouched
│   └── AS_Idle, AS_Walk, AS_Run, AS_JumpStart, AS_JumpLoop, AS_JumpEnd
└── ArmsItems/                           NEW, everything of this system
    ├── DA_ArmsAnimations                the data asset
    └── Items/                           the animator's animations, one folder per item
        ├── Sunbed/      AS_Arms_Sunbed_Grab_R, AS_Arms_Sunbed_Grab_L, AS_Arms_Sunbed_Hold_R ...
        ├── Umbrella/    AS_Arms_Umbrella_Grab_R ...
        ├── Bucket/      AS_Arms_Bucket_Equip_R, AS_Arms_Bucket_Use_R ...
        ├── Spray/       AS_Arms_Spray_Use_R ...
        ├── Toy/         AS_Arms_Toy_Equip_R ...
        └── ToolMenu/    AS_Arms_ToolMenu_Open_R, AS_Arms_ToolMenu_Close_R   (optional)
```

| What | Folder | Name |
|---|---|---|
| Base unarmed animations | `Content/Animation/FirstPerson/` | `AS_Idle`, `AS_Walk`... (as they are) |
| Base arms ABP | `Content/Animation/Blueprint/` | `ABP_FPArms_Base` |
| Items ABP | `Content/Animation/Blueprint/` | `ABP_FPArms_Items` |
| Data asset | `Content/Animation/ArmsItems/` | `DA_ArmsAnimations` |
| Arm animation of an item | `Content/Animation/ArmsItems/Items/<Item>/` | `AS_Arms_<Item>_<Action>_<Hand>` (the file name of the animator, unchanged) |
| Radial menu animation | `Content/Animation/ArmsItems/Items/ToolMenu/` | `AS_Arms_ToolMenu_Open_R`, `AS_Arms_ToolMenu_Close_R` |
| Animator's source FBX (repository, not Content) | `Export (blender)/Character/AnimazioniInventario/<Item>/` | same names |

`<Item>` = `Sunbed`, `Umbrella`, `Bucket`, `Spray`, `Toy`; `<Action>` = `Hold`, `PlaceAim`, `ThrowCharge`, `Grab`, `Place`, `Throw`, `Drop`, `Equip`, `Holster`, `Use`, `Pickup`, `Open`, `Close`; `<Hand>` = `R` or `L`. The item's own animations (`AS_Sunbed_Open`, `AS_Sunbed_Close`...) are a different thing and stay with the prop, not here.

Not needed: any `AM_` montage, any change to the base arms ABP (`ABP_FPArms_Base`, today's `ABP_SunsetCoveCharacter` after your rename), any change to `Plugins/`.

---

## 1. Build the C++

Close the editor (Live Coding blocks the command line build), then:

```
Build.bat SunsetCoveEditor Win64 Development -Project="D:\GitHub\Unreal\SunsetCove\SunsetCove.uproject" -WaitMutex
```

(`Build.bat` is in `Engine/Build/BatchFiles` of the engine.) Or build from Visual Studio. New classes you will see in the editor: **Arms Animation Component**, **Arms Animation Set**, **Arms Anim Instance**, the notify **Arms Item Commit**.

---

## 2. Names that must match (the animator's part and yours)

The animator follows `Docs/InventorySystem/Guida_Animatore.md`. The three names that connect Blender, the item Blueprints and this system:

| What | Value |
|---|---|
| Item ids | `Sunbed`, `Umbrella`, `Bucket`, `Spray`, `Toy` |
| Animation files | `AS_Arms_<Item>_<Action>_<Hand>` (for example `AS_Arms_Umbrella_Drop_L`) |
| Radial menu clips (optional) | `AS_Arms_ToolMenu_Open_R`, `AS_Arms_ToolMenu_Close_R` |

Actions: `Hold`, `PlaceAim`, `ThrowCharge`, `Grab`, `Place`, `Throw`, `Drop`, `Equip`, `Holster`, `Use`, `Pickup`, `Open`, `Close`. Hands: `R`, `L`. Spelling and capitals count.

If the animator's file names are wrong, the validation (section 5.3) and *Populate From Naming Convention* tell you which ones.

---

## 3. The item ids on the Blueprints

The id of an item is what picks its animations.

- **Tools:** set `Tool Id` on every tool Blueprint (Class Defaults > Tool > Identity): `Bucket`, `Spray`, `Toy`.
- **Props:** `Prop Id` (Class Defaults > Prop > Identity). If it is empty, the id is the Blueprint name without `BP_`: `BP_Sunbed` is `Sunbed`, so **`BP_Sunbed` needs nothing**. A Blueprint named differently needs it set by hand: `BP_Sunbed_Proto` and `BP_Umbrella_Proto` (in `Levels/Ni/Prototype`) must get `Sunbed` and `Umbrella`.

---

## 4. The post-process Animation Blueprint `ABP_FPArms_Items`

### 4.1 Create it

1. In `Content/Animation/Blueprint/`: right-click > **Animation > Animation Blueprint**.
2. **Parent Class:** search **Arms Anim Instance**. **Target Skeleton:** `SK_Character`.
3. Name it `ABP_FPArms_Items`. Leave the Event Graph empty (the C++ writes the variables).
4. Toolbar **Class Settings:** *Use Multi Threaded Animation Update* on.
5. In **My Blueprint**, tick *Show Inherited Variables*: you will see `RightArmPose`, `RightHoldSequence`, `RightPlaceAimSequence`, `RightThrowChargeSequence` and the same four for `Left`.

### 4.2 The slots (once, on the skeleton)

The slot names live in the data asset (`ArmItem_R`, `ArmItem_L`, groups `ArmsRight`, `ArmsLeft`). The data asset button creates them for you (section 5.2). By hand instead: **Window > Anim Slot Manager** in the ABP editor, **Add Slot Group** `ArmsRight` and add slot `ArmItem_R` in it; the same for `ArmsLeft` / `ArmItem_L`. Each arm needs **its own group**, otherwise a montage of one arm interrupts the other.

### 4.3 The AnimGraph

Open **AnimGraph**. Build this (it is the same chain twice, one per arm):

```
Input Pose ─► Save cached pose "Base"

Layered blend per bone:  Base Pose = Use cached pose "Base"
    Blend Poses 0 = Slot "ArmItem_R" ◄─ Blend Poses by EArmHeldPose (RightArmPose)
    Blend Poses 1 = Slot "ArmItem_L" ◄─ Blend Poses by EArmHeldPose (LeftArmPose)
    Blend Weights 0 = 1.0,  Blend Weights 1 = 1.0
─► Output Pose
```

1. **Input Pose (check).** Right-click in the graph, search **Input Pose** and add it: it receives the pose of the base ABP. (In a post-process Animation Blueprint the graph has this node; if it is not listed, use *Linked Input Pose*.) Drag from it > **Save cached pose**, name `Base`.
2. **Layered blend per bone.** Add the node. Select it, in **Details**:
   - **Layer Setup**: it needs two layers. Use the node's **Add Blend Pin** (right-click the node, or the + in Details) until there are two *Blend Poses* pins.
   - **Layer Setup > 0 > Branch Filters**: add one entry, **Bone Name** = `Arm_R`, **Blend Depth** = `0`.
   - **Layer Setup > 1 > Branch Filters**: **Bone Name** = `Arm_L`, **Blend Depth** = `0`.
   - Leave *Mesh Space Rotation Blend* off.
   - *Base Pose* = **Use cached pose "Base"**. *Blend Weights 0* and *1* = `1.0` (type 1 in the pin).
3. **Right arm chain** (repeat for the left with the `Left` variables and `ArmItem_L`):
   1. Add a **Blend Poses by EArmHeldPose** node (search "Blend Poses by", pick the one for `EArmHeldPose`). Drag the variable **RightArmPose** into its *Active Enum Value* pin. In Details tick **Reset Child On Activation**.
   2. Pins and blend times:

      | Pin | Content | Blend Time |
      |---|---|---|
      | None | **Use cached pose "Base"** | 0.15 |
      | Hold | Sequence Player, **Loop on** | 0.15 |
      | PlaceAim | Sequence Player, **Loop on** | 0.15 |
      | ThrowCharge | Sequence Player, **Loop OFF** (it must stay on its last frame) | 0.1 |

   3. In each Sequence Player: select the node, in Details tick the pin icon next to **Sequence** (*Expose as Pin*) and connect the variable: `RightHoldSequence`, `RightPlaceAimSequence`, `RightThrowChargeSequence`. Set **Loop Animation** as in the table.
   4. Add a **Slot** node: *Slot Name* = `ArmItem_R` (a dropdown of the slots on the skeleton; if it is not listed, do section 4.2 first). Its source = the *Blend Poses by EArmHeldPose* output; its output = *Blend Poses 0* of the layered blend.
4. Connect the layered blend output to **Output Pose**. Compile. The log must be clean (the graph reads only variables of the class).

**Why the arm does not stiffen:** with no item the layer is the base pose itself. **Why the free arm keeps walking:** each layer only overrides the bones from `Arm_R` / `Arm_L` down.

### 4.4 If the animator also keys the IK or control bones (check)

`CB_Hand_*`, `Ik_Hand_*` and `Pole_*` are children of `Root`, not of the arms, so the filters above do not include them. The animator's guide asks for FK animation of the arm chain only. If an animation moves `Ik_Hand_R` or `Pole_R`, add one more **Branch Filter** to that layer for each of them (`CB_Hand_R` covers `Ik_Hand_R` below it). Ask the animator which bones are keyed in the arm animations.

---

## 5. The data asset `DA_ArmsAnimations`

### 5.1 Create it

1. In `Content/Animation/ArmsItems/` (create the folder): right-click > **Miscellaneous > Data Asset** > pick **Arms Animation Set**. Name it `DA_ArmsAnimations`.
2. Open it. **Editor Helpers:** set **Target Skeleton** = `SK_Character`, **Search Folder** = `/Game/Animation/ArmsItems/Items`.

### 5.2 Press the buttons

In the Details panel, under *Editor Helpers*:

- **Ensure Skeleton Slots**: creates the groups and slots of section 4.2 on `SK_Character`. Then **save the skeleton** (File > Save All). Do this before the ABP uses the slots.
- **Populate From Naming Convention** (after you imported animations, section 8): fills `Items` from the names. Names that start with `AS_Arms_` but do not parse are listed in the Output Log (`LogSunsetCove`). Your blend and commit settings are kept.

### 5.3 What the entries mean

`Items` is item id > action > hand > clip. A clip has: the **Sequence**; **Blend In / Blend Out / Play Rate** (one-shots); **Loop While Active** (only `Use`: the spray repeats while the button is held); and the **Commit** settings (section 9). You can also fill and edit it by hand. A new item = a new key, no code. Anything left empty plays nothing.

Right-click the asset > **Asset Actions > Validate** (or run Data Validation): it reports a sequence made for another skeleton and a clip put under the wrong item, action or hand.

### 5.4 Debug placeholder (to test before any animation exists)

In **Debug**: set **Debug Placeholder Sequence** to any arms animation (for example `AS_Run`) and turn **Use Debug Placeholder** on. Every empty clip now plays that animation, on the right arm, at the right moment. Turn it off for the real game.

---

## 6. `BP_Character`

Open `BP_Character` > Components > **Arms Animation** (new, created by the C++):

| Property | Value |
|---|---|
| Animation Set | `DA_ArmsAnimations` |
| Post Process Anim Class | `ABP_FPArms_Items` |
| Log Missing Clips (Debug) | off (on while testing, it lists each missing clip once) |

The component installs the post-process Animation Blueprint on the arms mesh by itself. Do not also set it on `SM_Character` (if you do, leave *Post Process Anim Class* empty).

Check at Play: the Output Log (`LogSunsetCove`) shows no `ArmsAnimation:` warning. The possible ones say what to do (no instance of Arms Anim Instance, a slot missing on the skeleton...).

---

## 7. First test, with the placeholder and no animation (do this before importing anything)

Section 5.4 done. Play:

| Test | Expected |
|---|---|
| Output Log at start | no `ArmsAnimation:` warning |
| Console: `SunsetCove.Arms.Play Sunbed Grab R` | the right arm plays the placeholder, the left arm keeps its idle. If nothing moves: **(check)** the post-process instance plays montages; see the troubleshooting table |
| Walk while it plays | the **left arm keeps its walk sway** while the right arm shows the placeholder |
| `SunsetCove.Arms.Play Sunbed Hold R`, then `SunsetCove.Arms.Clear` | the right arm holds the placeholder pose; Clear returns it to locomotion |
| `SunsetCove.Arms.Dump` | prints, for each arm, the item, the wanted pose and the shown pose |
| Grab a sunbed (E) | the right arm plays the placeholder; the arm holds the Hold pose; the sunbed is attached at once (commit Immediate) |
| Q | the prop drops; the arm returns to locomotion when the animation blends out |

If the right arm shows the placeholder and the left does not move, per-arm layering works.

---

## 8. Importing the animator's files

The animator puts the FBX in `Export (blender)/Character/AnimazioniInventario/<Item>/` (see the guide, section 8). For each delivery:

1. **Arms mesh first (once):** the new `SM_Character.fbx` with `Prop_R`, `Prop_L`, `Tool_R`, `Tool_L` is imported **over** the existing `SM_Character` (re-import, same skeleton), as before. Then check the Skeleton Tree: the four bones are there.
2. **Animations:** import into `Content/Animation/ArmsItems/Items/<Item>/` (for example `.../Items/Sunbed/`): Import as an **Animation**, **Skeleton = `SK_Character`**, keep the file name. Root motion off.
3. Open `DA_ArmsAnimations` and press **Populate From Naming Convention**. Save.
4. Test (section 10).

A re-export of the same name: re-import over the existing asset. **Notifies are stored on the sequence (check):** after a re-import, make sure the *Arms Item Commit* notify (section 9) is still there. If it is not, the clip commits at its timeout and the Output Log says so once.

---

## 9. When the item moves: commit timing and the notify

For each clip of `Grab`, `Place`, `Throw`, `Drop`, `Equip`, `Holster` and `Pickup` you choose **when the item really moves**:

| Commit Mode | The item moves... | Use it for |
|---|---|---|
| **Immediate** (default) | the instant the player acts | the first tests; actions whose animation does not matter |
| **After Delay** | *Commit Delay Seconds* later | a quick tune without touching the animation |
| **On Notify** | when the animation reaches the **Arms Item Commit** notify | the final result: it follows the animation if its speed changes |

Steps for **On Notify**, per clip:

1. Open the `AS_` sequence. Right-click the **Notifies** track > **Add Notify > Arms Item Commit**.
2. Drag it to the frame the animator wrote in the delivery notes (the guide asks for it): *Grab*: where the hand closes. *Place*: where the item reaches the ground. *Throw*: where the arm releases. *Drop*: where the hand opens. *Equip*: where the tool appears. *Holster*: where the tool disappears.
3. In `DA_ArmsAnimations` set the clip's **Commit Mode** = *On Notify*. **Commit Timeout Seconds** = 0 uses the clip length plus 0.1 s (the safety net, in case the notify is lost).

What the player sees: **the hand is occupied / free at the click** (tool unusable while an Equip waits), and the **item** attaches, detaches or appears at the notify. A new action, a cancel, the end of the game or an unpossess apply a pending item move at once.

---

## 10. Test checklist per animation

Use it every time animations arrive. For each, use the console command for a quick look (`SunsetCove.Arms.Play <Item> <Action> <R|L>`), then play it for real.

| Animation | Real test | Must see |
|---|---|---|
| `Grab` | E on the item, in each hand (grab two items) | the right (then the left) arm reaches; the other arm keeps swaying; the item attaches at the commit frame; no pop into `Hold` at the end |
| `Hold` | carry the item and walk, sprint, jump | loops; the free arm keeps its walk; the item does not shift in the hand (if it does, `Grip_*` has keys: Guide, section 12) |
| `PlaceAim` (optional) | hold the place button | the arm stays in the aim pose while the hologram shows |
| `Place` | release on a green hologram | the item reaches the ground at the commit frame; the arm blends back to locomotion; a release on a red spot plays nothing |
| `ThrowCharge` | hold the throw button | the arm goes back and **stays still** on the last frame while the bar rises and falls |
| `Throw` | release the throw button | starts from the charge pose **without a jump**; the item leaves at the commit frame, with the force of the bar |
| `Drop` | Q | the item leaves at the commit frame; the arm returns |
| `Equip` | pick a tool in the radial menu (Tab) | the tool appears at the commit frame; it cannot be used before; then `Hold` |
| `Holster` | select the equipped tool again | the tool disappears at the commit frame; a swap to another tool plays only the `Equip` of the new one |
| `Use` | left mouse with a tool | a single use plays once; a spray (*Loop While Active*) repeats while held and stops when released |
| `Pickup` (optional) | E on a tool on the ground | the tool is in the inventory at the click, the arm plays the pickup |
| `ToolMenu` `Open` / `Close` (optional) | hold and release Tab | the arm relaxes and comes back |
| Two items | grab a sunbed, then an umbrella | each hand has its own animation; actions act on the last grabbed |
| Interruption | press Q during a Grab, throw right after a Grab | no stuck item, no stuck arm; the pending move is applied at once |

---

## 11. Console commands (not in shipping builds)

| Command | What it does |
|---|---|
| `SunsetCove.Arms.Play <Item> <Action> <R\|L>` | plays one clip, or holds one pose, with no gameplay (for example `SunsetCove.Arms.Play Umbrella ThrowCharge L`) |
| `SunsetCove.Arms.Clear` | back to what the inventories say |
| `SunsetCove.Arms.Dump` | prints the state of both arms (item, wanted pose, shown pose, releasing) |

---

## 12. Troubleshooting (Output Log, filter `LogSunsetCove`)

| Symptom | Cause |
|---|---|
| `no Animation Instance of class Arms Anim Instance` | `Post Process Anim Class` is empty or the ABP's parent is not *Arms Anim Instance* |
| `the slot 'ArmItem_R' is not on the skeleton` | section 4.2 / *Ensure Skeleton Slots*, then save the skeleton |
| `SunsetCove.Arms.Play` says "no clip" | the data asset has no clip for that item / action / hand (turn on the debug placeholder) |
| A clip plays but nothing moves | the slot node of the ABP uses another name, or the Layered Blend has no *Branch Filter*, or the post-process instance does not run montages **(check)**: as a fallback copy the graph of 4.3 into the base ABP and change its parent class to *Arms Anim Instance*; nothing else changes |
| The arm shows the reference pose (T-pose) | a Sequence Player of the graph has no sequence variable connected |
| Both arms move for a one-arm clip | the animation has keys on both arms, or the branch filter bone is wrong |
| The hand goes wrong but fingers are fine | the animation keys `Ik_Hand` / `Pole`: section 4.4 |
| The item moves at the wrong time | the Commit Mode of the clip, or the notify is on the wrong frame |
| `commit On Notify but has no 'Arms Item Commit' notify` | add the notify (section 9) or choose another mode |
| An item has no animation | its id is empty or different from the one in the data asset (section 3); turn on *Log Missing Clips* |
| The tool cannot be used right after Equip | expected while an Equip waits for its notify |

---

## 13. What was and was not verified

- **Compilation:** the first build had not been run when this guide was written (Live Coding was active in the editor). Run section 1; if it reports errors, they are in the new files of `AnimationSystem/` or the inventory commit changes.
- **Read, not run:** the engine source was read for the dynamic montage API, the post-process anim instance update and the skeleton slot API.
- **Not verified (needs you in the editor):** the post-process graph and its bone filters; the slot creation button; dynamic montage playback in the post-process instance; the notify and its survival of a re-import; the feel of the commit timing; the interplay of two held props and a tool. The checks are marked **(check)** above.
