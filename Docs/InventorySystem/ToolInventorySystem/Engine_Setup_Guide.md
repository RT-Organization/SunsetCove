# Engine Setup Guide — Tool Inventory System

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Systems:** `UToolInventoryComponent`, `ASunsetCoveTool`, `UInteractionComponent`
**Audience:** whoever sets the system up in the editor and tests it (no C++ needed), and the animator (section 2).
**Companion documents:** `Architecture_Overview.md` (how it works inside), `../PropInventorySystem/Engine_Setup_Guide.md` (the props: **do that guide first**, this one builds on it), `../../InteractionSystem/Interaction_System.md`.

---

## 0. What you are setting up

When the steps below are done, in first person you can:

| Do this | What happens |
|---|---|
| Aim at a tool lying in the world and press **E** | the tool is stored in the inventory (up to 4). The first one is taken out into the hand |
| **Hold Tab**, move the mouse toward a tool's sector, **release Tab** | that tool is equipped (GTA weapon wheel: no click, the direction of the mouse decides) |
| **Tap Tab** (nothing selected), or select the tool already in the hand | the tool is put away, the hand is free |
| **Left mouse** | uses the equipped tool on what you aim at |
| **Hold Tab**, point at a tool, press **Q** | that tool falls to the ground, pushed gently forward. It can be picked up again |
| Grab a sunbed or an umbrella while a tool is in the hand | the tool is put away and **stays** away until you choose it again |
| Hands full of props, press **Tab** or **E** on a tool | refused |

There is **no radial menu drawing** and **no animation** yet (the system only fires the events for them). While the menu is open you see nothing; you will see the tool change in the hand when you release Tab. Use the Output Log to follow it (section 9).

What is left to do **in the editor** (Input Actions and the mapping context are assets, so they cannot be made by code):

1. Build the C++ (section 1)
2. The animator's names in Blender (section 2) — optional to start
3. Create **1 new Input Action** and add **3 mapping entries** (sections 3, 4)
4. Assign them on **BP_Character** (section 5)
5. Make a **tool**: one Blueprint, one mesh (section 6), then test (section 8)

You never touch `Plugins/CoreFramework`.

---

## 1. Build the C++

1. Close the editor.
2. Right-click `SunsetCove.uproject` > **Generate Visual Studio project files** (new source files were added).
3. Build the **SunsetCoveEditor** target (Development Editor), then open the editor.

After the build, `BP_Character` shows a new component, **Tool Inventory**, next to **Interaction** and **Prop Inventory**. (If you do not see it, check that `BP_Character` derives from `SunsetCoveCharacter`.)

---

## 2. Names that must match between Blender and Unreal (the animator's part)

Same rule as for props: everything that connects an object to the hand is made in Blender and arrives with the FBX. The code only needs the **names**.

### 2.1 The arms

| In Blender | Name | Notes |
|---|---|---|
| a **bone** or a **socket** on the right hand | `Tool_R` | **optional.** Where a tool is held. If the arms have no `Tool_R`, tools are held on **`Prop_R`** (the prop socket) |

- You do **not** need anything new to start: with only `Prop_R` on the arms, tools are held there (one log line says so).
- Add `Tool_R` when a tool should be held differently from a prop (a different palm position). Same rules as `Prop_R` (see the prop guide, 2.1): a bone is the safest choice; a Blender socket is an Empty named `SOCKET_Tool_R`.
- To use other names: `BP_Character` > **Tool Inventory** > `ToolInventory|Hand` > **Tool Socket Name** / **Fallback Tool Socket Name**.

### 2.2 Every tool

| In Blender | Name | Notes |
|---|---|---|
| a **bone** (or a socket) of the tool's skeleton | `Grip` | the point and orientation by which the hand holds the tool |

- **Align it like the hand socket**: the system places the tool so its `Grip` has the same position **and orientation** as `Tool_R` / `Prop_R`. If both are built with the same convention, the tool sits right with no offset.
- **Keep the grip still in the tool's animations** (a bone child of the root that is never keyed). It is read at the moment the tool is picked up.
- Other bones are free (the lid of the spray, the handle of the bucket).
- If the bone is not called `Grip`: Blueprint > Class Defaults > **Tool|Grip > Grip Socket Name**.

### 2.3 Check it quickly

Open the imported skeleton > **Skeleton Tree**: `Grip` on the tool, `Tool_R` or `Prop_R` on the arms. If something is missing the Output Log says so at Play (filter `LogSunsetCove`):

- arms: *ToolInventory: socket or bone 'Tool_R' not found on 'FirstPersonMesh', so tools are held on 'Prop_R'...* (fine) or *neither 'Tool_R' nor 'Prop_R' was found...* (fix it)
- tool: *Tool 'BP_X': grip bone or socket 'Grip' does not exist on skeleton of 'SK_X'...*

### While the Blender work is not ready yet

You can test now. Without hand sockets the tool hangs at a **fallback offset** from the arms mesh (**Fallback Location / Rotation** on the Tool Inventory component, one warning); without a grip bone the **center of the tool** sits on the hand (one warning). For a single tool, **Tool|Grip > Hold Location / Rotation Offset** nudges it without going back to Blender.

First person is required, exactly as for props (prop guide, section 2).

---

## 3. The Input Actions (only ONE is new)

| Asset | Value Type | Triggers | Notes |
|---|---|---|---|
| `IA_ToolMenu` | Digital (bool) | **none** | **new.** Hold = menu open, release = confirm |
| `IA_PropPlace` | — | — | **already exists** (prop guide). Used for **Place** and also for **Use Tool** |
| `IA_PropDrop` | — | — | **already exists**. Used for **Drop prop** and also for **Drop tool** |
| `IA_Interact` | — | — | **already exists**. Picks the tool up |

Create `IA_ToolMenu` in `Content/InventorySystem/ToolInventorySystem/Input/` (right-click > **Input > Input Action**). **No triggers** (no Hold, no Pressed): the system needs *Started* on press and *Completed* on release.

**Why the same asset for two components?** The left mouse button is "place a prop" and "use the tool"; Q is "drop a prop" and "drop a tool". Each component acts only in its own situation (props in the hands / a tool in the hand, menu open), so they never both act. Using **one asset per key** also avoids a classic Enhanced Input trap: two different actions mapped to the same key, where the first one swallows the key and the second never fires.

---

## 4. The mapping context (keys)

Open the `IMC_SunsetCove` you made for the props and add **one** mapping:

| Action | Keyboard & mouse | Controller (GDD proposal) | Modifiers / triggers on the mapping |
|---|---|---|---|
| `IA_ToolMenu` | `Tab` | Gamepad Right Shoulder (R1 / RB) | none |

`IA_PropPlace` (left mouse, R2 / RT) and `IA_PropDrop` (Q, Y / Triangle) are already mapped: **nothing to add** for using and dropping tools.

### Key conflicts with the framework

As for props: the framework context (`IMC_Character`) is read only and ours has priority 1, so our mappings win. Check that `Tab` is not used there (if it is, change *our* key here, never the plugin asset). The Tab of the Unreal editor itself does not matter in game.

---

## 5. Assign everything on BP_Character

Open `BP_Character` (`Content/Character/Blueprints/`).

1. **Components** panel > select **Tool Inventory**. In Details > **ToolInventory|Input**:
   - **Open Tool Menu Action** = `IA_ToolMenu`
   - **Use Tool Action** = `IA_PropPlace` (**the same asset as the Place Action of the Prop Inventory**)
   - **Drop Action** = `IA_PropDrop` (**the same asset as the Drop Action of the Prop Inventory**)
2. Compile and save.

If an action is left empty, the Output Log shows one warning at the start of Play (*'...' has no Use Tool Action...*) and only that action is disabled.

---

## 6. Making a tool (one Blueprint, one mesh)

1. Content Browser > right-click > **Blueprint Class** > in *All Classes* search **Sunset Cove Tool** > create, for example `BP_Bucket` in `Content/Gameplay/Tools/`.
2. Open it. In the **Components** panel select **ToolMesh** and, in Details > **Mesh** > **Skeletal Mesh Asset**, pick the tool's skeletal mesh (the one with the `Grip` bone).
3. **Class Defaults** > **Tool|Identity**: set **Tool Id** (the row name of the tool in the "Strumenti" table, for example `Bucket`; it must be **unique per tool**), **Display Name** (for the future menu) and **Icon** (optional for now).
4. Compile and save. **That is all.** The Blueprint already has:

| What | Done by C++ |
|---|---|
| a **collision box as the root**, Movable, `PhysicsActor` preset (the interaction trace hits it; it can fall when dropped) | yes |
| the box **sized to the mesh** and the mesh centered in it | yes (`Auto Fit Collision To Mesh`) |
| the grip: the `Grip` bone aligned to the hand socket | yes |
| pickup, storing, showing in the hand, use, drop | yes |

Drop it in the level (**do not move ToolMesh in the viewport**). A tool in the level is solid and stays put until picked up. In the game, other systems will spawn tools at the visitors' spots; for testing, place a few by hand.

### 6.1 Properties of the Blueprint (Class Defaults)

| Category | Property | Default | What for |
|---|---|---|---|
| `Tool\|Identity` | `Tool Id` | none | unique id; two tools with the same id are the same tool (a second one is refused) |
| | `Display Name`, `Icon` | empty | what the radial menu will show |
| `Tool\|Grip` | `Grip Socket Name` | `Grip` | bone / socket held by the hand |
| | `Hold Location / Rotation Offset` | 0 | a nudge relative to the hand socket |
| `Tool\|Use` | `Use Mode` | Press | **Press**: one use per click. **Hold**: repeats while the button is held (spray) |
| | `Use Cooldown Seconds` | 0.4 | minimum time between two uses |
| | `Hold Use Interval` | 0.2 | Hold mode: time between repeats |
| | `Use Range` | 250 cm | how far the tool reaches from the camera |
| | `Use Trace Radius` | 15 cm | a little radius makes small targets (crabs) easier to hit |
| | `Use Trace Channel` | Visibility | targets must **block** it |
| | `Requires Target` | on | on: a click without a target does nothing. Off: it is a use anyway (a spray works on empty air) |
| `Tool\|Rules` | `Can Be Picked Up` | on | off: the player cannot take it (refused as *Tool Blocked*) |
| `Tool\|Collision` | `Auto Fit Collision To Mesh`, `Collision Padding`, `Collision Box Extent` | on, 2 cm | box size; padding makes it easier to aim at |
| `Tool\|Physics` | `Mass Kg`, `Start Simulating Physics` | 2 kg, off | when lying loose / dropped |
| `Tool\|Animation` | `Equip Animation`, `Use Animation` | none | optional assets played once on ToolMesh (replace an Animation Blueprint on the mesh) |

### 6.2 What a tool does when used

By default a use **tells the target**: the actor in front of you (within `Use Range`) that has the **Tool Target Interface** gets `Receive Tool Use`. A tool with its own effect (a spray particle, a sound) overrides **Perform Use** in its Blueprint (Functions > Override), or reacts in the Blueprint events **On Use Started / On Used / On Use Ended**.

### 6.3 Making something a target of tools (for the adversities)

Open the Blueprint of the crab / child / visitor, **Class Settings** > Interfaces > **Add** > *Tool Target Interface*. Then, in the Functions list:

- **Is Tool Use Blocked** (Tool, User, Hit): return **true** to refuse. Typical: return true unless `Tool > Tool Id` equals the one that works on you. (Left unimplemented it returns false: any tool is accepted.)
- **Receive Tool Use** (Tool, User, Hit): do the effect (the crab goes into the bucket, the child calms down). Return true if something happened.

The target must **block the Use Trace Channel** (Visibility by default) to be hit: characters and static meshes do by default.

---

## 7. Parameter cheat-sheet (the Tool Inventory component)

Select **Tool Inventory** on `BP_Character`.

| Category | Property | Default | Change it when |
|---|---|---|---|
| `ToolInventory\|Slots` | `Max Tools` | 4 | the GDD limit changes (1-8). The wheel has one sector per slot |
| | `Allow Duplicate Tools` | off | you want to carry two of the same `Tool Id` |
| | `Auto Equip First Tool` | on | you do not want the first tool taken out on pickup |
| `ToolInventory\|Hand` | `Use First Person Arms` | on | tools on the third-person body instead |
| | `Tool Socket Name` / `Fallback Tool Socket Name` | `Tool_R` / `Prop_R` | the names from Blender are different |
| | `Fallback Location / Rotation` | 45, 25, -30 cm | no sockets yet and the tool hangs badly |
| `ToolInventory\|Radial` | `Radial Radius` | 60 | the wheel is too sensitive (raise) or too sluggish (lower). It is how much mouse movement pushes the cursor to the edge |
| | `Radial Dead Zone` | 0.35 | accidental selections near the center (raise) or you have to move too far (lower) |
| | `Radial Start Angle Degrees` | 0 | rotate the layout. 0 = slot 0 at the top, the rest clockwise |
| | `Invert Radial Y` | off | moving the mouse **up** selects the **bottom** sector: turn it on |
| `ToolInventory\|Drop` | `Drop Forward Speed` / `Drop Up Speed` | 250 / 100 cm/s | the tool flies too far / too little |
| | `Inherit Owner Velocity Scale` | 1 | 0 = ignore the player's speed |
| | `Drop Pawn Ignore Seconds` | 0.35 | the tool bumps into you |
| | `Drop Wall Margin`, `Wall Trace Channel` | 20 cm, Visibility | the tool ends up inside a wall |

---

## 8. Test checklist (in the editor, first person)

Put in the level: two different tools (for example `BP_Bucket` with `Tool Id = Bucket`, `BP_Spray` with `Tool Id = Spray`), a duplicate of the bucket, a fifth tool, and a prop (a sunbed).

1. **Pickup.** Aim at the bucket, press **E**: it disappears from the world and appears in your hand (auto equip). Grip on the hand: the bucket sits in the hand as the animator built it.
2. **Second tool.** Pick up the spray: it goes into storage; the bucket stays in the hand.
3. **Duplicate.** Press E on the duplicate bucket: nothing happens, it stays (refused: already owned).
4. **Select.** Hold **Tab**, move the mouse toward the *other* sector, release: the spray is in the hand and the bucket is hidden. (Which sector is which: slot 0 is at the top, the next ones go clockwise; the first tool you picked up is slot 0.)
5. **Holster.** Tap **Tab** and release without moving: the hand is empty. Hold Tab, move toward the tool in the hand, release: it is put away too.
6. **Full inventory.** Pick up tools until you have 4, then try a fifth: refused, it stays in the world.
7. **Use.** Equip a tool, aim at a target (for a quick test use `Requires Target = off` on a tool and watch the log / `On Used`): left mouse uses it; with `Use Mode = Hold` it repeats while held.
8. **Drop.** Hold Tab, move toward a tool, press **Q**: it flies out forward and lands near you, it does not hit you. Release Tab: the equipped tool is **not** put away. Pick it up again with E.
9. **Props take the hands.** With a tool in the hand, press E on a sunbed: the tool disappears from the hand and stays away after you place or drop the sunbed. Choose it again from the menu.
10. **Hands busy.** With a sunbed in the hand, press **Tab** (nothing opens) and press **E** on a tool (refused).
11. **Camera.** While Tab is held the camera does not turn; it turns again on release.

---

## 9. Troubleshooting (filter the Output Log with `LogSunsetCove`)

| Symptom | Likely cause |
|---|---|
| Nothing happens on **Tab** / left mouse / Q | the action is empty on the Tool Inventory component (a warning at Play says which), or its mapping is missing in `IMC_SunsetCove` |
| Left mouse (or Q) works for props but not for tools, or the other way round | the two components got **different** Input Action assets on the same key, and one swallows the key. Assign the same asset to both (section 5) |
| E does not pick the tool up | it is not an *ASunsetCoveTool* (the Blueprint parent is wrong), the box does not block `Visibility`, out of `Interaction Range`, or the hands hold props / the inventory is full (a refusal is reported) |
| The tool is picked up but you see nothing | it is hidden because it is stored, not equipped: choose it in the menu. Or the Skeletal Mesh Asset is not set (a warning says so) |
| The tool floats in front of the arms, not in the hand | neither `Tool_R` nor `Prop_R` exists on the arms (a warning says so): it is at the fallback offset |
| The tool sits wrong in the hand | the `Grip` bone and the hand socket do not share the same axes, or the grip is animated. Quick fix: **Hold Location / Rotation Offset** on the tool |
| Moving the mouse up selects the bottom sector | turn on **Invert Radial Y** |
| The wheel selects with the slightest movement / never | tune **Radial Radius** and **Radial Dead Zone** |
| The wheel selects an empty sector | an empty sector selects nothing; release on it = hands free |
| The camera keeps turning with the menu open | the character is not a `SunsetCoveCharacter` (the override lives there) |
| Two tools of the same kind cannot be carried | **Allow Duplicate Tools** is off, or they have the same **Tool Id**. Give each tool a unique id; leave it empty for no duplicate check |
| A use does nothing | `Requires Target` is on and there is no target (the target must have the **Tool Target Interface**, block the use channel and be within **Use Range**), the tool is on cooldown, or **Is Use Blocked** returns true |
| Dropped tool is stuck / flies through a wall | **Drop Wall Margin** and **Wall Trace Channel**; the box of the tool must have collision |

---

## 10. Later: the radial menu widget and the animations

The hooks already exist; see `Architecture_Overview.md`, sections 7.3 (the widget) and 7.4 (the animations). In short: **On State Changed** shows the widget, **On Radial Hover Changed** highlights a sector, and **On Tool Action** is where the arms montages are played.
