# Unreal Setup Guide — how to create and wire new things in the editor

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8
**What this is:** the recipe book for adding new content to the game. Each chapter answers the same three questions: **what do I create, what do I set in it, and what else in the project do I have to touch.**
**What this is not:** an architecture description. How the systems work inside is in the system docs (`Docs/InventorySystem/`, `Docs/AnimationSystem/`, `Docs/InteractionSystem/`); this guide only links to them.
**Rule that never changes:** `Plugins/CoreFramework` is a read-only submodule. Nothing in this guide asks you to edit anything inside it.

> **Where does each asset go?** The full folder map, naming prefixes and the "where do I put this file" table are in [`Content_Organization_Guide.md`](Content_Organization_Guide.md) (same folder). Read it first if you are unsure where to import or create something.

## Chapters

| # | Chapter | Use it when |
|---|---|---|
| 1 | [Conventions shared by every item](#1-conventions-shared-by-every-item) | before creating anything: names, folders, ids |
| 2 | [Creating a new Prop](#2-creating-a-new-prop) | a new carriable object (sunbed, umbrella, towel...) |
| 3 | [Creating a new Tool](#3-creating-a-new-tool) | a new radial-menu tool (bucket, spray, toy...) |
| 4 | [Gameplay Tags: when and how to add one](#4-gameplay-tags-when-and-how-to-add-one) | a new kind of rest spot or visitor activity |
| 5 | [Is anything global touched? (project settings)](#5-is-anything-global-touched-project-settings) | quick answer: "do I need to change a setting?" |
| 6 | [Troubleshooting quick table](#6-troubleshooting-quick-table) | something does not work |
| 7 | [Template for new chapters](#7-template-for-new-chapters) | adding a new kind of thing to this guide |

> **How this guide was written.** It was derived by reading the C++ in `Source/SunsetCove/` and the existing system docs, and by looking at the folder structure of `Content/` (not the contents of the assets). Nothing here was run in the editor. Steps that depend on what is inside an asset (for example the exact bones of your Blender rig) are marked **(check)**.

---

## 1. Conventions shared by every item

### 1.1 The item id

Every prop and every tool has one **id**: a single English word, capital first letter (`Sunbed`, `Umbrella`, `Bucket`, `Spray`, `Toy`). It is the key that connects four things, so it must be written the same everywhere:

| Where the id appears | Example for a new "Towel" prop |
|---|---|
| the Blueprint (`Prop Id` / `Tool Id`) | `Towel` |
| the animator's file names | `AS_Arms_Towel_Grab_R` |
| the entry in `DA_ArmsAnimations` (`Items` map key) | `Towel` |
| the row name in the Google Sheets data (`Prop` / `Strumenti` tab of `SunsetCove_Dati_Oggetti`) | `Towel` |

- **Prop:** if `Prop Id` is empty, the id is the Blueprint name without `BP_`: `BP_Towel` → `Towel`. Set it by hand only if the Blueprint has another name (like `BP_Sunbed_Proto`).
- **Tool:** `Tool Id` is **always set by hand**, and must be **unique** (two tools with the same id count as the same tool, and the second one is refused as a duplicate).
- Spelling and capitals count. The old sheet ids like `TOOL-01` are superseded by these names.

### 1.2 Names of assets

Follow what the Sunbed already does:

| Asset | Name | Example |
|---|---|---|
| Blueprint | `BP_<Item>` | `BP_Towel`, `BP_Bucket` |
| Skeletal mesh | `SM_<Item>` | `SM_Sunbed` |
| Skeleton | `SK_<Item>_Skeleton` | `SK_Sunbed_Skeleton` |
| Physics asset | `PA_<Item>` | `PA_Sunbed` |
| Animation of the item itself | `AS_<Item>_<Action>` | `AS_Umbrella_Open`, `AS_Umbrella_Close`, `AS_Spray_Use` |
| Arms animation (made by the animator) | `AS_Arms_<Item>_<Action>_<Hand>` | `AS_Arms_Umbrella_Drop_L` — **keep the file name unchanged on import** |
| Material instance | `MI_<Item>` | `MI_Proto_Sunbed` |

### 1.3 Where things go in the Content Drawer

What exists today, and what is suggested for new items. The suggestions are consistent with the structure that is already there; nothing is forced by code (the code never looks at folders, except the one search folder of `DA_ArmsAnimations`).

| What | Folder | Status |
|---|---|---|
| Meshes, skeleton and physics asset of a **prop** | `Content/Mesh/Props/<Item>/` | exists (`Sunbed/`) |
| Meshes, skeleton and physics asset of a **tool** | `Content/Mesh/Tools/<Item>/` | folder exists, empty |
| Animations **of the item itself** (`AS_Umbrella_Open`...) | next to its mesh, in `Content/Mesh/Props/<Item>/` or `Content/Mesh/Tools/<Item>/` | suggested |
| **Prop Blueprints** (`BP_Sunbed`...) | `Content/Gameplay/Props/` | suggested. Today `BP_Sunbed` is in `Content/Gameplay/Blueprints/`: leave it, or move it in the Content Browser (Move + *Fix Up Redirectors*) |
| **Tool Blueprints** (`BP_Bucket`...) | `Content/Gameplay/Tools/` | suggested (new folder) |
| Tool icon (radial menu) | `Content/UI/HUD/Elements/` or a new `Content/UI/Icons/Tools/` | suggested |
| **Arms animations** from the animator | `Content/Animation/ArmsItems/Items/<Item>/` | exists as a convention (see the Animation guide) |
| The data asset of all arms animations | `Content/Animation/ArmsItems/DA_ArmsAnimations` | exists |
| Base unarmed arms animations (`AS_Idle`...) | `Content/Animation/FirstPerson/` | do **not** put item animations here |
| Input Actions / mapping context | `Content/Input/Actions/`, `Content/Input/Context/IMC_SunsetCove` | exists; a new prop or tool needs **no** new input |
| Blender source files | `Export (blender)/Assets/<Item>/` (+ `AnimSunbed`-style sub folder for animations) | repository only, not Content |
| Other Blueprints you will make later (visitors, adversities...) | `Content/Gameplay/Visitors/`, or a new `Content/Gameplay/<Feature>/` | exists / suggested |

### 1.4 The Blender → Unreal contract (names only; nothing is added by hand in Unreal)

The animator builds each item as a skeleton in Blender; sockets and bones arrive with the FBX. **You never create a socket by hand in the Unreal skeleton editor** (a re-import would override it).

| On the… | Name | Needed for |
|---|---|---|
| arms (`SM_Character`) | `Prop_R`, `Prop_L` | where props are held |
| arms (`SM_Character`) | `Tool_R` (`Tool_L` is reserved) | where tools are held (falls back to `Prop_R`) |
| every **prop** | `Grip_R` bone, plus `Grip_L` (create it always; if missing, `Grip_R` is used for the left hand) | the point the hand holds. Child of the root, **never animated**, axes aligned to the hand socket |
| every **tool** | `Grip_R` bone | same; tools are held with the right hand only |

Details and the animator's checklist: `Docs/InventorySystem/Guida_Animatore.md` (Italian, on purpose).

### 1.5 The no-code rule for new items

Adding a prop or a tool of the **same kind as the existing ones** needs **no C++ and no change to any other Blueprint**: it is one Blueprint, one mesh, some data. C++ is needed only if the new item has **behavior no current system has** (see 2.9 and 3.9).

---

## 2. Creating a new Prop

A prop is something the player carries in the hands, places with a hologram, throws or drops: sunbed, umbrella, towel.

### 2.1 Before you start (from the animator / Blender)

- [ ] `SK_<Item>.fbx` with the skeleton, the mesh, and the bones `Grip_R` and `Grip_L` (never keyed). **Rest pose = the prop open / placed.**
- [ ] Optional: the item's own animations `AS_<Item>_Close` (ends closed) and `AS_<Item>_Open` (ends open/rest pose), exported as separate FBX. Their length matters: the prop waits for Close before it goes into the hand, so keep them short (about 0.3-0.6 s) and about as long as the arms `Grab` clip.
- [ ] Optional: one-frame poses `AS_<Item>_ClosedPose` and `AS_<Item>_OpenPose` (section 2.6). The grip bone is **not keyed** in any of the four.
- [ ] Optional now, needed for polish: the arms animations `AS_Arms_<Item>_<Action>_<R|L>`. Per hand, **mandatory:** `Grab`, `Hold` (loop), `Place`, `ThrowCharge` (held pose, not a loop), `Throw`, `Drop`. **Optional:** `PlaceAim` (loop). So 12 files (+2) per prop.
- [ ] The id of the prop decided (section 1.1).

You can build and test the Blueprint **before** any of this is delivered: a missing grip bone puts the center of the prop in the hand (one warning), and the arms animation can run on the debug placeholder.

### 2.2 Import the meshes

1. Folder: `Content/Mesh/Props/<Item>/`.
2. Import the FBX as **Skeletal Mesh** with its own new skeleton (not `SK_Character`).
3. Check in the **Skeleton Tree** that `Grip_R` (and `Grip_L`) exist **(check)**.
4. Import `AS_<Item>_Open` / `AS_<Item>_Close` and the two pose files (if delivered) as **Animation** on this skeleton, in the same folder. Name the poses with the item (`AS_Sunbed_ClosedPose`, `AS_Sunbed_OpenPose`): files called just `AS_Closed_Pose` collide as soon as there is a second prop.

### 2.3 Create the Blueprint

1. Content Browser > `Content/Gameplay/Props/` > right-click > **Blueprint Class**.
2. Open **All Classes** and search **`Sunset Cove Prop`** (`ASunsetCoveProp`). Do **not** parent to `Actor`.
3. Name it `BP_<Item>` (for example `BP_Towel`).

> Parent class: **`Sunset Cove Prop`**. (It already implements `Prop Interface` and `Item Identity Interface`; do not add them again.)

### 2.4 Set the Blueprint

**Mandatory (the only two things that must be done):**

| Where | What |
|---|---|
| Components panel > **PropMesh** > Details > Mesh > **Skeletal Mesh Asset** | the prop's skeletal mesh |
| Class Defaults > **Prop \| Identity** > **Prop Id** | leave empty if the Blueprint is named `BP_<Item>`; otherwise the id |

Compile and save. The Blueprint already has by itself (done in C++): a collision box as root sized to the mesh, the physics, the grip alignment, grab / place / throw / drop, and the hologram.

**Do not** move `PropMesh` in the viewport (Auto Fit re-centers it), do not change the collision preset of `CollisionBody` (`PhysicsActor`), do not add the interfaces.

**Optional, per prop (Class Defaults):**

| Category | Property | Default | When to change it |
|---|---|---|---|
| `Prop \| Grip` | `Grip Socket Name` / `Left Grip Socket Name` | `Grip_R` / `Grip_L` | the Blender bones have other names |
| `Prop \| Grip` | `Hold Location Offset` / `Hold Rotation Offset` | 0 | a quick nudge in the hand without going back to Blender |
| `Prop \| Collision` | `Auto Fit Collision To Mesh`, `Collision Padding` | on, 2 cm | off = size the box yourself with `Collision Box Extent`; padding makes it easier to aim at |
| `Prop \| Physics` | `Mass Kg` | 10 | how it reacts when pushed. The throw speed does not depend on it |
| `Prop \| Physics` | `Start Simulating Physics` | off | on = it falls/can be pushed from the start. Leave off for a prop that is part of the scene |
| `Prop \| Drop` | `Drop Upright` | on | when you drop it (Q) the prop is turned upright (no pitch/roll, only your heading) before it falls, so a sunbed lands flat and an umbrella stands. Thrown props keep their tumble |
| `Prop \| Drop` | `Drop Rotation Offset` | 0 | extra rotation on top of upright, in the prop's own space, for a prop whose closed shape should rest on another side (for example Pitch 90 to lay an umbrella down) |
| `Prop \| Rules` | `Can Be Picked Up` | on | off = fixed prop (settable at runtime too) |
| `Prop \| Animation` | `Picked Up Animation` = **Close**, `Placed Animation` = **Open** | none | the simple way to open and close the prop (see 2.6). Close plays on the ground at the pickup and the prop waits for it before going into the hand |
| `Prop \| Animation` | `Open Pose Animation` / `Closed Pose Animation` | none | optional one-frame poses (see 2.6) |

### 2.5 Rest spots (only if visitors use the prop)

A sunbed has one, an umbrella has some, a towel has one. A prop that visitors do not rest on (a bucket-like prop) needs none.

1. In the Blueprint, Add Component > **Sunset Cove Rest Spot**. Make it a child of `PropMesh` (or of the root).
2. Move it **where the visitor's feet go** (on the mattress, at feet height) and rotate it so its **X axis points where the visitor faces**.
3. Set **Spot Type** (a `Cove.Spot.*` tag) and **Rest Activities** (`NPC.Activity.Rest.*` tags). One activity is picked at random for each visit.
   - Existing spot type: `Cove.Spot.Sunbed` (the only one for now; umbrellas and free sand are **not** rest spots, the umbrella will organize the stations and the NPC assignment).
   - Existing activities: `NPC.Activity.Rest.Lay` and `NPC.Activity.Rest.Sit` (only `Lay` is used by the sunbed today).
   - A new kind of spot or activity needs a **new tag**: chapter 4.
4. **Approach Offset** (default 100 cm to the side): the point on the navmesh where the visitor stands before sitting/lying. It must be reachable on the navmesh, because the prop's own box cuts its footprint out of it.
5. Nothing else: the spots of a prop are **disabled** from the start of the pickup (while it closes) and while it is in the hands, and **enabled** when you place it. A dropped or thrown prop stays disabled until placed.

Check in play with the console command `SunsetCove.Debug.RestSpots 1` (green = free, yellow = claimed, red = occupied, grey = off).

**(check) Big props such as an umbrella:** the collision box wraps the whole mesh, canopy included, and it is what cuts the navmesh. If visitors cannot walk under or around it as you want, turn off `Auto Fit Collision To Mesh` and size the box yourself.

### 2.6 Animations of the prop itself (when it is placed / picked up)

#### 2.6.1 Where the files go (two different animation families: do not mix them)

| Family | What it is | Folder | Skeleton to import on |
|---|---|---|---|
| **The prop's own animations** (this section) | `AS_Sunbed_Open`, `AS_Sunbed_Close`, `AS_Sunbed_OpenPose`, `AS_Sunbed_ClosedPose`: they move the **sunbed's** bones | **`Content/Mesh/Props/<Item>/`**, next to the mesh | the **prop's** skeleton (`SK_Sunbed_Skeleton`) |
| **The arms animations** (section 2.7) | `AS_Arms_Sunbed_Grab_R`, `..._Hold_L`...: they move the **player's arms** | `Content/Animation/ArmsItems/Items/<Item>/` | `SK_Character` |

```
Content/Mesh/Props/Sunbed/
├── SM_Sunbed                  skeletal mesh
├── SK_Sunbed_Skeleton         skeleton
├── PA_Sunbed                  physics asset
├── AS_Sunbed_Close            NEW  Close  (ends closed)
├── AS_Sunbed_Open             NEW  Open   (ends open)
├── AS_Sunbed_ClosedPose       NEW  optional, one frame (rename of the animator's AS_Closed_Pose)
└── AS_Sunbed_OpenPose         NEW  optional, one frame (rename of the animator's AS_Open_Pose)
```

(The Blender source FBX stay in `Export (blender)/Assets/<Item>/`; they are not in `Content`.) If two prop files have the same name in different folders the Content Browser accepts it, but the Blueprint dropdowns become confusing: always keep the item name in the file name.

#### 2.6.2 Step by step (about 5 minutes per prop)

1. **Import.** Drag the 4 FBX into `Content/Mesh/Props/<Item>/`. In the import dialog choose **Import as Animation** (not as Skeletal Mesh), **Skeleton = the prop's skeleton** (`SK_Sunbed_Skeleton`), root motion off. Keep or set the names above. Open `AS_Sunbed_Close` and check the preview: the sunbed folds from open to closed and the grip bone does not move **(check)**.
2. **Open the prop Blueprint** (`BP_Sunbed`, and `BP_Sunbed_Proto` if you use it) > **Class Defaults** > category **Prop \| Animation**.
3. Fill the four fields:

   | Field | Asset | Required? |
   |---|---|---|
   | **Picked Up Animation** | `AS_Sunbed_Close` | yes, for the Close on the ground |
   | **Placed Animation** | `AS_Sunbed_Open` | yes, for the Open after placing |
   | **Open Pose Animation** | `AS_Sunbed_OpenPose` | optional (hologram shows the open prop, safe starting pose) |
   | **Closed Pose Animation** | `AS_Sunbed_ClosedPose` | optional (safe snap if the pickup is interrupted) |

4. **Check the PropMesh.** Components panel > **PropMesh** > Details > **Animation** > **Animation Mode** = *Use Animation Asset* (the default), with **no Animation Blueprint** and no Anim To Play. The code plays the assets itself.
5. **Compile and save.** You do not add any Blueprint logic for this (no events, no rest-spot switching): it is all automatic.
6. **Arms side (once per prop):** if the arms `Grab` clip is much shorter or longer than `AS_Sunbed_Close`, the prop waits for the longer one. Aim for the same length (about 0.3-0.6 s).
7. **Test** with the checklist of section 2.8 (E on the open prop, Q during the Close, the hologram, drop then pick up again).

Nothing else changes: no new Input Action, no change to `BP_Character`, no project setting, no Gameplay Tag, no change to `DA_ArmsAnimations` for these four files (they are the prop's, not the arms').

**The travel (new): nothing to set up.** The prop now **flies** from where it lies to the hand (while Close plays) and from the hand to the placement spot (while Open plays); it is automatic and needs no asset or Blueprint change. It is tuned in one place, **`BP_Character` > Components > Prop Inventory > `PropInventory | Travel`** (editable while playing):

| Property | Default | What it does |
|---|---|---|
| `Travel To Hand` / `Travel To Placement` | on | off = the prop teleports like before |
| `Grab Travel Seconds` | 0.35 | minimum flight time to the hand. The flight also lasts as long as `AS_<Item>_Close` |
| `Place Travel Seconds` | 0.4 | minimum flight time to the spot. The flight also lasts as long as `AS_<Item>_Open` |
| `Travel Ease Exponent` | 2 | 1 = constant speed, 2 = soft start and arrival, higher = sharper |

So the **length of Close and Open is the flight time** (when longer than the minimum): keep them about 0.3-0.5 s. The rest spots turn on when the prop **lands**.

#### 2.6.3 How it behaves, and the Animation Blueprint alternative

Choose **one** way:

- **Simple (the Open / Close flow):** set `Picked Up Animation` = `AS_<Item>_Close` and `Placed Animation` = `AS_<Item>_Open` (Class Defaults > Prop \| Animation). Leave `PropMesh` on *Use Animation Asset*. What happens:

  | Moment | What the player sees |
  |---|---|
  | **E** on the open prop | the prop **plays Close while it flies to the hand** (it follows the hand if you move). The hand counts as occupied at the click and the prop's rest spots turn off at once (visitors leave it) |
  | Flight, Close and the arms `Grab` clip reached its commit point all finished (the **latest** of the three) | the prop **snaps into the hand**, closed, exactly where the flight ended |
  | **Q**, Place or another grab during Close | the prop attaches immediately, already closed (never half-folded) |
  | carried | closed |
  | **Q** (drop) or throw | it lands **closed** and stays closed until placed; picking it up again plays **no** Close |
  | Place released on a green hologram | the prop leaves the hand and **flies to the hologram spot while it plays Open**, lands exactly on it, and only then its rest spots turn on. The hologram itself shows the prop **open** |

  **The two pose files are optional.** Without them the reference pose of the mesh must be the open pose (the animator's rule) and everything above still works. With them: `Open Pose Animation` = the starting pose of the prop in the level and the pose of the hologram; `Closed Pose Animation` = the pose the prop is snapped to when the pickup is interrupted (otherwise the last frame of Close is used). They are single-frame animation assets, applied and held (no playing).
- **Animation Blueprint:** give `PropMesh` an ABP and drive it from the Blueprint events **On Grab Started / On Grabbed / On Dropped / On Placed / On Thrown**. Leave the animation assets empty. A prop driven this way cannot tell the system how long its Close lasts, so it is attached at the arms animation's commit point (no wait for the prop).

### 2.7 Arms animations (the hand animations)

This is **data, not code.** When the animator's `AS_Arms_<Item>_*` files arrive:

1. Import them into `Content/Animation/ArmsItems/Items/<Item>/` as **Animation** on **`SK_Character`**, keeping the file names, root motion off.
2. Open `Content/Animation/ArmsItems/DA_ArmsAnimations` > press **Populate From Naming Convention** (Search Folder `/Game/Animation/ArmsItems/Items`). It adds the new item key by itself. Save.
3. Per clip, choose **when the item really moves** (Commit Mode: Immediate / After Delay / On Notify). For *On Notify*, add the **Arms Item Commit** notify to the sequence at the right frame.
4. Test with the console: `SunsetCove.Arms.Play <Item> Grab R`.

Not needed: any new ABP, any change to `ABP_FPArms_Items`, any change to `BP_Character`. To test before any animation exists, turn on *Use Debug Placeholder* in `DA_ArmsAnimations`. Full detail: `Docs/AnimationSystem/Engine_Setup_Guide.md`.

### 2.8 Put it in the level and test

1. Drag the Blueprint into the level (a prop in the level is solid and stays put until grabbed).
2. Play, and run the table:

| Do | Expect |
|---|---|
| look at it (< 2.5 m), **E** | it plays Close where it lies, then goes into the right hand, `Grip_R` on `Prop_R`; second prop goes to the left hand |
| **E**, then **Q** before Close ends | it attaches at once, closed, then drops closed |
| hold **left mouse**, wheel | hologram shows the **open** prop, follows, green on valid ground, wheel rotates it |
| release over green | the prop flies to the spot opening and lands upright; its rest spots turn on at the landing |
| pick up a prop you dropped | no Close (it is already closed), but it still flies to the hand |
| hold **right mouse**, release | charge bar rises/falls; thrown as one solid object |
| **Q** | falls gently forward |
| Output Log (`LogSunsetCove`) | no warning about the prop |

More checks and the tuning cheat sheet: `Docs/InventorySystem/PropInventorySystem/Engine_Setup_Guide.md`, sections 8-9.

### 2.9 What else to touch after adding a prop (the checklist)

| # | Where | Needed? | What to do |
|---|---|---|---|
| 1 | **Project Settings** (any page) | **No** | nothing. The collision uses the engine's `PhysicsActor` preset; there are no custom channels or object types in `Config/DefaultEngine.ini` |
| 2 | **Input** (Input Actions, `IMC_SunsetCove`) | **No** | Place / Throw / Drop / Rotate / Interact already work on every `Sunset Cove Prop` |
| 3 | **`BP_Character`** | **No** | the inventories and the arms animation read the prop through interfaces |
| 4 | **Hologram material** | **No** | it is created from the prop's own mesh. The material needs *Used with Skeletal Meshes* only once, and it is shared by every prop |
| 5 | **Gameplay Tags** (`Config/DefaultGameplayTags.ini`) | **Only if** the prop introduces a new `Cove.Spot.*` or `NPC.Activity.*` | chapter 4 |
| 6 | **`DA_ArmsAnimations`** | **Yes, for hand animations** | section 2.7 (a new key, filled by *Populate From Naming Convention*). Make the arms `Grab` clip about as long as the prop's Close: the prop goes into the hand at the later of the two |
| 7 | **Visitors:** `Spot Type Weights` on `BP_Visitor` (Components panel > **Sunset Cove Visitor** > Details) | **Only if** you made a **new** `Cove.Spot.*` type | add the tag with a weight (how much this visitor wants that kind of spot; one kind is rolled per visit by these weights). A type with no weight is never *wanted*; a visitor only ends up on it as a fallback when its wanted kind is full and the fallback option of the component is on. Existing types are already weighted |
| 8 | **NPC Animation Blueprint** (the visitor's ABP) | **Only if** you made a **new** `NPC.Activity.*` | add a state entered with `Has Activity(<new tag>)`. Without it the visitor uses the spot with no matching animation |
| 9 | **Rest areas** (`SunsetCoveRestArea` in the level) | **Not used today** | it generates free-sand rest spots, and the `Cove.Spot.Sand` tag was removed (clients rest only on sunbeds). Do not place new ones; delete the existing one from the level |
| 10 | **NavMesh** (`NavMeshBoundsVolume`) | **Check once** | the prop's box carves the navmesh; make sure visitors can still reach the **Approach Offset** of each rest spot (Build > Build Paths, or it rebuilds dynamically: `RuntimeGeneration=Dynamic`) |
| 11 | **Level** | Yes | place instances; or the spawner/system that creates them (none creates props yet: the water-source idea of the GDD is not built) |
| 12 | **Data in Google Sheets** (`SunsetCove_Dati_Oggetti`, tab `Prop`; `Prezzi` if it changes prices) | **Yes, as design data** | add a row with the same id. Not read by the game yet (the link to Unreal is a pending task) |
| 13 | **Cooking / packaging** | **No** | no Asset Manager rules or "Additional Asset Directories to Cook" in `DefaultGame.ini` today; assets referenced from the level cook by themselves. This changes if you start **spawning props by soft class reference** (then add the folder) |
| 14 | **GDD / docs** | optional | if the prop adds a rule, note it in the GDD |

**You need C++ (ask for it) only if** the new prop has behavior that is not "carry, place, throw, drop, rest on it", for example: an umbrella whose shade changes the visitors' satisfaction, a prop that breaks, a prop with a capacity, a prop that blocks the visitors' main path (this is what `Is Placement Blocked` of the Prop Interface is for), or a prop that is not a skeletal mesh. For small reactions, the Blueprint events (`On Placed`...) are enough.

### 2.10 A prop that is not skeletal

`ASunsetCoveProp` is the easy way, not the only one. A static-mesh prop is possible (parent `Actor`, add *Prop Interface*, a Movable root with simple collision), but it loses the grip alignment and the animation hooks. Read `Docs/InventorySystem/PropInventorySystem/Architecture_Overview.md` (4.2) before doing it.

---

## 3. Creating a new Tool

A tool lives in the radial menu (hold **Tab**), is used with the left mouse and is never consumed: bucket, spray, toy, garbage bag. Up to 4 are carried. Tools are picked up from the ground with **E**; in the finished game they will appear in the visitors' spots.

### 3.1 Before you start

- [ ] `SK_<Item>.fbx`: skeleton, mesh, bone `Grip_R` (never keyed; no `Grip_L`: tools are right hand only). **Rest pose = the tool as held, ready to use.**
- [ ] Optional animations of the tool itself: `AS_<Item>_Equip`, `AS_<Item>_Use`.
- [ ] Arms animations, right hand only. **Mandatory (4):** `AS_Arms_<Item>_Equip_R`, `_Holster_R`, `_Hold_R` (loop), `_Use_R` (a loop for something held like the spray). **Optional (2):** `_Pickup_R`, `_Drop_R`.
- [ ] Decide: the **Tool Id** (section 1.1), what it **does**, and **who it works on** (which target).

### 3.2 Import the meshes

Same as 2.2, into `Content/Mesh/Tools/<Item>/`.

### 3.3 Create the Blueprint

1. Content Browser > `Content/Gameplay/Tools/` > right-click > **Blueprint Class**.
2. **All Classes** > **`Sunset Cove Tool`** (`ASunsetCoveTool`).
3. Name it `BP_<Item>`.

> Parent class: **`Sunset Cove Tool`**. (It already implements `Item Identity Interface`.)

### 3.4 Set the Blueprint

**Mandatory:**

| Where | What |
|---|---|
| Components panel > **ToolMesh** > Skeletal Mesh Asset | the tool's skeletal mesh |
| Class Defaults > `Tool \| Identity` > **Tool Id** | the unique id (`Bucket`). **Not** automatic for tools |
| Class Defaults > `Tool \| Identity` > **Display Name** | the name shown to the player |

**Recommended:** `Tool \| Identity` > **Icon** (a `Texture2D` for the radial menu; the menu widget is not built yet, so it can wait).

**How the tool behaves (Class Defaults > `Tool \| Use`):**

| Property | Default | Typical values |
|---|---|---|
| `Use Mode` | Press | **Press** one use per click (bucket, toy, garbage bag); **Hold** repeats while held (spray) |
| `Use Cooldown Seconds` | 0.4 | minimum time between two uses |
| `Hold Use Interval` | 0.2 | Hold mode: time between repeats |
| `Use Range` | 250 cm | how far it reaches from the camera |
| `Use Trace Radius` | 15 cm | a bit of radius makes small targets (crabs) easier to hit |
| `Use Trace Channel` | Visibility | targets must **block** this channel |
| `Requires Target` | on | on = a click without a valid target does nothing. Off = it is a use anyway (a spray works on empty air) |

Also available: `Tool \| Grip` (offsets), `Tool \| Collision`, `Tool \| Physics` (2 kg), `Tool \| Rules` (`Can Be Picked Up`), `Tool \| Animation` (`Equip Animation`, `Use Animation`: optional assets, they replace an ABP on the mesh).

Compile and save. As for props, root box, grip, pickup, storing, showing in the hand, use and drop are built by C++.

### 3.5 What the tool does when used

Pick one:

1. **Nothing special: tell the target (default).** The actor in front of the player (within `Use Range`) that implements the **Tool Target Interface** gets `Receive Tool Use`. The effect is written **on the target**, not on the tool (see 3.6).
2. **The tool has its own effect** (a spray particle, a sound): in the tool Blueprint, override **Perform Use** (Functions > Override) or react to the events **On Use Started / On Used / On Use Ended**.
3. **The tool can refuse to be used** (empty, broken, not now): override **Is Use Blocked** and return `true`.

### 3.6 Making something a target of the tool (crab, child, smelly visitor, litter...)

Open the **target's** Blueprint > Class Settings > Interfaces > **Add** > *Tool Target Interface*, then implement:

- **Is Tool Use Blocked** (Tool, User, Hit): return **true** to refuse. Typical: return true unless `Tool > Tool Id` equals the id that works on this target (`Bucket` for a crab). Not implemented = any tool is accepted.
- **Receive Tool Use** (Tool, User, Hit): do the effect (the crab goes into the bucket, the child calms down). Return true if something happened.

The target must **block the tool's `Use Trace Channel`** (Visibility by default; characters and static meshes do).

### 3.7 Arms animations

Identical to 2.7, but only the right hand and the actions of 3.1. For a spray, tick **Loop While Active** on the `Use` clip in `DA_ArmsAnimations`.

### 3.8 Put it in the level and test

Place the Blueprint in the level (it is solid and stays until picked up).

| Do | Expect |
|---|---|
| **E** on it | stored; the first tool is taken out into the hand |
| hold **Tab**, move the mouse to a sector, release | that tool is equipped; hold Tab + **Q** drops it |
| **left mouse** at a valid target | `On Used` fires; the target reacts |
| second tool with the **same Tool Id** | refused (duplicate) |
| a fifth tool | refused (full) |
| tool in hand, grab a sunbed | the tool is put away and stays away until chosen again |

Complete checklist and tuning: `Docs/InventorySystem/ToolInventorySystem/Engine_Setup_Guide.md`, sections 7-9.

### 3.9 What else to touch after adding a tool (the checklist)

| # | Where | Needed? | What to do |
|---|---|---|---|
| 1 | **Project Settings** | **No** | nothing (same as props: standard collision, no custom channels) |
| 2 | **Input** | **No** | Tab (`IA_ToolMenu`), left mouse (`IA_PropPlace`), Q (`IA_PropDrop`) and E are already mapped and shared with the props |
| 3 | **`BP_Character` > Tool Inventory** | **Only if** you will carry **more than 4 tools** | raise `ToolInventory \| Slots > Max Tools` (1-8). The radial menu has one sector per slot. GDD says "about 4", still to confirm |
| 4 | **`DA_ArmsAnimations`** | **Yes, for hand animations** | section 3.7 |
| 5 | **Targets** (crab, child, visitor, litter actors) | **Yes, for the tool to do anything** | section 3.6. Without a target implementing the interface, the tool is used on nothing. This is where the effect lives |
| 6 | **Radial menu widget (HUD)** | **When it is built** | the widget reads `Tool Id`, `Display Name` and `Icon` from the tool; the system fires the events (`On State Changed`, `On Radial Hover Changed`) but draws nothing yet. Set the **Icon** so it is ready |
| 7 | **Spawning in the visitors' spots** (GDD: random, `probabilita_spawn`) | **Not implemented yet** | no code in the project spawns tools; for now place them by hand. When that system exists, the new tool must be added to its list/table |
| 8 | **Gameplay Tags** | **No** | tools do not use tags |
| 9 | **Collision / trace channel** | **Only if** your target does not block Visibility | change `Use Trace Channel` on the tool, or make the target block Visibility |
| 10 | **Data in Google Sheets** (`SunsetCove_Dati_Oggetti`, tab `Strumenti`, `probabilita_spawn`; `SunsetCove_Dati_Avversita` for the adversity it solves) | **Yes, as design data** | row name = `Tool Id`. Not read by the game yet |
| 11 | **Cooking / packaging** | **No** | same note as 2.9 row 13 |
| 12 | **GDD tool list** | optional | the GDD lists bucket, spray, toy, garbage bag as known tools; add the new one |

**You need C++ (ask for it) only if** the tool needs something the base class cannot do from Blueprint: a use that is not "aim from the view and hit one target" (an area effect, a throw, a beam), a resource (charges, water), or a new kind of aim.

---

## 4. Gameplay Tags: when and how to add one

### 4.1 When

Only for these cases. A plain new prop or tool needs **no** tag.

| You add... | New tag needed | Tag family |
|---|---|---|
| a new **kind of rest spot** (for example a deckchair that is not a `Sunbed`) | yes | `Cove.Spot.<Name>` |
| a new **thing visitors do** on a spot (for example `Read`) | yes | `NPC.Activity.Rest.<Name>` |
| a new UI screen / HUD slot | yes (not covered here) | `UI.Screen.*`, `UI.HUDSlot.*` |

Tags today (`Config/DefaultGameplayTags.ini`): `Cove.Spot.Sunbed`, `NPC.Activity.Rest.Lay`, `NPC.Activity.Rest.Sit`, `UI.HUDSlot.BottomLeft`, `UI.Screen.ConfirmPopup`, `.HUD`, `.MainMenu`, `.PauseMenu`, `.Settings`. (`Cove.Spot.Sand`, `Cove.Spot.Umbrella` and `NPC.Activity.Rest.Stand` were removed on purpose.)

### 4.2 How (in the editor)

1. **Edit > Project Settings > Project > Gameplay Tags**.
2. **Manage Gameplay Tags...** > **+** > enter the full name (`Cove.Spot.Deckchair`), add a dev comment, choose the **Source = `DefaultGameplayTags.ini`** > Add.
3. The editor writes the line in `Config/DefaultGameplayTags.ini`. Commit that file.

You can also add a line by hand: `+GameplayTagList=(Tag="Cove.Spot.Deckchair",DevComment="...")`. The project has `WarnOnInvalidTags=True`, so a typo in a tag shows as a warning.

### 4.3 What a new tag must be connected to

| New tag | Connect it to |
|---|---|
| `Cove.Spot.<New>` | the prop's **Rest Spot > Spot Type**, and the **Spot Type Weights** of `BP_Visitor` (Sunset Cove Visitor component), otherwise no visitor ever *wants* it (it is used only as a fallback when the wanted kind is full) |
| `NPC.Activity.Rest.<New>` | the prop's **Rest Spot > Rest Activities**, and a state in the **NPC Animation Blueprint** entered with `Has Activity` on that tag |

---

## 5. Is anything global touched? (project settings)

Quick answer to "do I have to change a setting?" when adding a **prop or a tool**:

| Place | Change needed? |
|---|---|
| Project Settings > Collision (channels, presets) | **No** |
| Project Settings > Input / `DefaultInput.ini` / mapping context | **No** |
| Project Settings > Gameplay Tags | only for a new spot type or activity (chapter 4) |
| Project Settings > Maps & Modes / Game Instance | **No** |
| Project Settings > Navigation (agent, `RuntimeGeneration=Dynamic`) | **No**; only check the navmesh covers the area |
| Project Settings > Asset Manager / Packaging (cook) | **No** today (see 2.9 row 13) |
| `SunsetCove.Build.cs` (modules) | **No**: only when new C++ needs a new engine module |
| `Plugins/CoreFramework` | **Never** (read-only) |

---

## 6. Troubleshooting quick table

Filter the Output Log with **`LogSunsetCove`**.

| Symptom | Likely cause |
|---|---|
| **E** does nothing on the new prop / tool | the Blueprint is not a child of `Sunset Cove Prop` / `Sunset Cove Tool`; or out of `Interaction Range`; or the box does not block Visibility (the root preset was changed) |
| Warning "no Skeletal Mesh Asset" | `PropMesh` / `ToolMesh` has no mesh set |
| Warning "grip bone or socket 'Grip_R' does not exist" | the Blender skeleton has no such bone: fix in Blender, or set `Grip Socket Name` to an existing bone |
| In the hand but turned or offset | grip axes differ from the hand socket: fix in Blender, or nudge with `Hold Location / Rotation Offset` |
| The prop jumps when grabbed after being placed | the grip bone is animated in `AS_<Item>_Open` / `Close` |
| The prop does not close before it goes into the hand | `Picked Up Animation` is empty, or it has no length; or the prop was already closed (dropped / thrown earlier: by design) |
| The prop waits too long before it jumps into the hand | the Close animation is long, or the arms `Grab` clip commits late (the prop waits for the later of the two). Shorten one of them |
| The hologram is folded | no `Open Pose Animation` and no `Placed Animation` on the prop (the hologram then copies the current, closed pose) |
| The prop is half-folded in the hand | the pickup was interrupted and neither `Closed Pose Animation` is set nor Close ends in the closed pose: set the pose asset |
| Hologram missing or a gray grid | the shared hologram material is missing, or lacks *Used with Skeletal Meshes* (set once for all props) |
| The new item has no hand animation | the id differs from the key in `DA_ArmsAnimations`, or the sequences are not filled: run *Populate From Naming Convention*; turn on *Log Missing Clips* on the Arms Animation component |
| A second copy of a tool cannot be picked up | same `Tool Id` as one already carried (by design). Give each tool a unique id |
| The tool is used but nothing happens | `Requires Target` is on and there is no valid target; or the target lacks *Tool Target Interface*, does not block the trace channel, is out of range; or `Is Tool Use Blocked` returns true |
| Visitors ignore the new sunbed-like prop | rest spot disabled (the prop is in the hands or was dropped, not placed); no `Spot Type`; no weight for the new type; the approach point is off the navmesh (`SunsetCove.Debug.RestSpots 1`) |
| Visitors reach the spot but do not sit/lie | no state in the NPC ABP for the new `NPC.Activity.*` tag |

---

## 7. Template for new chapters

Copy this when you add another kind of thing (an adversity, an event, a new visitor type, a new level area...). Keep the same questions.

```
## N. Creating a new <Thing>

<One sentence: what it is, how it is used in the game.>

### N.1 Before you start
- what must exist first (art, names, ids, tags, data rows)

### N.2 Create the asset
- exact class to parent to ("All Classes" name) and why
- folder in the Content Drawer, naming

### N.3 Set the asset
- mandatory settings (table)
- optional settings (table)

### N.4 Put it in the level and test
- a table: Do | Expect

### N.5 What else to touch after adding it (the checklist)
| # | Where | Needed? | What to do |
(Project Settings, Input, Gameplay Tags, other Blueprints, data assets, data sheets, level, cook)

C++ is needed only if: <what the current systems cannot do>
```

Add the new chapter to the table at the top of this file, and link the system doc that explains how it works inside.
