# Engine Setup Guide — Prop Inventory System

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Systems:** `UPropInventoryComponent`, `UInteractionComponent`, `ASunsetCoveProp`
**Revision 2.** **Audience:** whoever sets the system up in the editor and tests it (no C++ needed), and the animator (section 2).
**Per l'animatore (in italiano):** `../Guida_Animatore.md`.
**Companion documents:** `Architecture_Overview.md` (how it works inside), `../../InteractionSystem/Interaction_System.md`, `../ToolInventorySystem/Engine_Setup_Guide.md` (the tools; it shares the left mouse and Drop actions with this system).

> **What changed in this revision (2)**
> - New base class **`ASunsetCoveProp`**: a prop is now a Blueprint child of it. The collision root, the physics, the skeletal mesh component, the grip and the interface are all built in C++. Nothing to restructure, no sockets to add in Unreal, no interface to implement (section 7).
> - **Sockets come from Blender.** The hand sockets (`Prop_R`, `Prop_L`) on the arms and the grip bone (`Grip_R`) on every prop are made by the animator and arrive with the FBX. Nothing is added by hand in Unreal any more (sections 2 and 7).
> - The prop is placed so its grip bone lands exactly on the hand socket: no hand-tuned offsets.
> - The hologram copies skeletal meshes too.

---

## 0. What you are setting up

When the steps below are done, in first person you can:

| Do this | What happens |
|---|---|
| Aim at a prop and press **E** | the prop jumps into a hand (right first, then left), its grip bone on the hand socket |
| Hold **left mouse** | a hologram of the prop follows your view, green = valid, red = invalid; the **mouse wheel** turns it |
| Release **left mouse** | the prop is placed at the hologram (only if it is green) |
| Hold **right mouse**, release | the power bar charges, rises and falls; releasing throws the prop (strongest at the peak) |
| Press **Q** | the prop falls from your hand, pushed gently forward |

With two props in hand, Place / Throw / Drop use the one you grabbed **last**.

What is left to do **in the editor** (Input Actions and the mapping context are assets, so they cannot be made by code):

1. Build the C++ (section 1)
2. Make sure the arms and the props come from Blender with the right names (section 2: the animator's part)
3. Create **5 Input Actions** and the **mapping context** (sections 3, 4)
4. Assign them on **BP_Character** (section 5)
5. Optional: the **hologram material** (section 6)
6. Make a **prop**: one Blueprint, one mesh (section 7), then test (section 8)

You never touch `Plugins/CoreFramework`.

---

## 1. Build the C++

1. Close the editor.
2. Right-click `SunsetCove.uproject` > **Generate Visual Studio project files** (new source files were added).
3. Build the **SunsetCoveEditor** target (Development Editor), then open the editor.

After the build, `BP_Character` shows two new components in its **Components** panel, **Interaction** and **Prop Inventory**. They come from C++. (If you do not see them, check that `BP_Character` derives from `SunsetCoveCharacter`: Class Settings > Parent Class.)

---

## 2. Names that must match between Blender and Unreal (the animator's part)

Everything that connects a prop to the hand is made in Blender and imported with the FBX. The code only needs the **names** to match.

### 2.1 The arms (first-person character)

| In Blender | Name | Notes |
|---|---|---|
| a **bone** or a **socket** on the right hand | `Prop_R` | where a prop is held in the right hand |
| a **bone** or a **socket** on the left hand | `Prop_L` | same, left hand |

- **A bone is the safest choice**: a bone child of the hand bone, named `Prop_R`, works with no import settings. A bone **or** a socket both work, the code uses whichever it finds by that name.
- A **socket** in Blender is an Empty parented to the hand bone, named with the prefix `SOCKET_` (for example `SOCKET_Prop_R`). Unreal strips the prefix on import and the socket is named `Prop_R`.
- Place it where the palm holds an object and give it the orientation you want a held prop to have. **The prop's `Grip_R` bone (2.2) will be aligned to it axis for axis.**
- When the arms are exported again, re-import over the existing arms mesh: the sockets and bones of the new file are the ones used. There is nothing to redo in Unreal, and **do not add sockets by hand in the skeleton any more**: the ones in the FBX are the truth.
- If the animator prefers other names, change them on the component: `BP_Character` > **Prop Inventory** > `PropInventory|Hands` > **Right / Left Hand Socket Name**.

### 2.2 Every prop

| In Blender | Name | Notes |
|---|---|---|
| a **bone** (or a socket) of the prop's skeleton | `Grip_R` | the point and orientation by which the hand holds the prop |
| (optional) a second one | `Grip_L` | only if the left hand must hold it differently (default name of the left grip); set it in **Prop|Grip > Left Grip Socket Name** |

- **Align it like the hand socket.** The system puts the prop so that its `Grip_R` has the same position **and orientation** as `Prop_R` / `Prop_L`. If the animator builds both from the same convention (for example, both with the Y axis along the fingers), a prop sits right in the hand with no offset at all.
- **The grip should not move in the prop's animations** (or only rest where it is held). The system reads the position of the bone at the moment the prop is grabbed. A good habit: make the grip a child of the root bone that is never keyed.
- The prop's skeleton can contain any other bones the animations need (the umbrella's ribs, the sunbed's legs).
- If the bone is not called `Grip_R`, change it per prop: Blueprint > Class Defaults > **Prop|Grip > Grip Socket Name**.

### 2.3 Check it quickly

Open the imported skeleton, expand the **Skeleton Tree**: you should see `Prop_R` and `Prop_L` (arms) or `Grip_R` (prop), as bones or with the socket icon. If one is missing the Output Log says so at Play (filter `LogSunsetCove`):

- arms: *PropInventory: hand socket(s) or bone(s) [Prop_R, Prop_L] not found on 'FirstPersonMesh'...*
- prop: *Prop 'BP_X': grip bone or socket 'Grip_R' does not exist on skeleton of 'SK_X'...*

### While the Blender work is not ready yet

You can test now. A missing hand socket makes props hang at a **fallback offset** from the arms mesh (one warning), and a missing grip bone makes the **center of the prop** sit on the hand socket (one warning). Tune the fallback on the Prop Inventory component: `PropInventory|Hands` > **Right / Left Hand Fallback Location**, **Hand Fallback Rotation**. Defaults put the props about 45 cm in front, 25 cm to the side and 30 cm below the view. For a prop, **Prop|Grip > Hold Location / Rotation Offset** nudges it without going back to Blender.

### First-person view is required

The props ride on the **arms**, which only exist in first person. On `BP_Character` > Class Defaults > **Character|Camera**:

- **Default Perspective** = `First Person`
- **Allow Perspective Toggle** = off

(To carry on the visible body in third person instead, set `Hand Mesh Source = Body Mesh` on the Prop Inventory component; the body skeleton then needs the same two names.)

---

## 3. The five Input Actions

Create them in `Content/InventorySystem/PropInventorySystem/Input/` (and the interact one in `Content/InteractionSystem/Input/`). Right-click > **Input > Input Action**.

| Asset | Value Type | Triggers | Notes |
|---|---|---|---|
| `IA_Interact` | Digital (bool) | none | grab and every future interaction |
| `IA_PropPlace` | Digital (bool) | **none** | hold = hologram, release = place |
| `IA_PropThrow` | Digital (bool) | **none** | hold = charge, release = throw |
| `IA_PropRotate` | **Axis1D (float)** | none | mouse wheel |
| `IA_PropDrop` | Digital (bool) | none | |

**Do not add triggers** (Hold, Pressed, Tap...) to `IA_PropPlace` and `IA_PropThrow`: the system needs *Started* on press and *Completed* on release. Leave the other settings at their defaults.

**`IA_PropPlace` (left mouse) and `IA_PropDrop` (Q) are also used by the Tool Inventory** ("use tool" and "drop tool"). Assign the same assets there; see `../ToolInventorySystem/Engine_Setup_Guide.md`, section 5.

---

## 4. The mapping context (keys)

The framework already has a context (`IMC_Character`). **Do not edit it** (it is inside the plugin). The project has its own slot for extra actions: `SunsetCoveMappingContext` on `BP_Character`, added with priority 1 (higher than the framework's 0).

1. Create `IMC_SunsetCove` (right-click > **Input > Input Mapping Context**), for example in `Content/Character/Input/`. (If the project already has a context for its own actions, add to it instead.)
2. Add the mappings:

| Action | Keyboard & mouse | Controller (GDD proposal) | Modifiers / triggers on the mapping |
|---|---|---|---|
| `IA_Interact` | `E` | Gamepad Face Button Bottom (A / Cross) | none |
| `IA_PropPlace` | Left Mouse Button | Gamepad Right Trigger (R2 / RT) | none |
| `IA_PropThrow` | Right Mouse Button | Gamepad Left Trigger (L2 / LT) | none |
| `IA_PropRotate` | **Mouse Wheel Axis** | Gamepad D-pad Right (+1) and D-pad Left (**Negate** modifier) | wheel: none. D-pad: add trigger **Pressed**, so one press = one step |
| `IA_PropDrop` | `Q` | Gamepad Face Button Top (Y / Triangle) | none |

3. If the wheel turns the hologram the wrong way, add a **Negate** modifier to the Mouse Wheel mapping.
4. Gamepad triggers are analog: they fire *Started* when pressed past the actuation threshold (default 0.5) and *Completed* when released, which is what is needed.

### Key conflicts with the framework

The framework context uses some keys already (Shift, Ctrl, Space were confirmed; the Dash / Dodge keys could not be read from the asset). Because our context has priority 1, **our mappings win** a conflict. To check: open `IMC_Character` (read only) and look for `Q`, `E`, left / right mouse. If Dash or Dodge is on `Q`, `Q` will now drop props instead. Fix by re-mapping *our* key (change `IA_PropDrop`'s key here), never by editing the plugin asset.

The pause key is owned by the PlayerController's own context (`Esc`). No conflict.

---

## 5. Assign everything on BP_Character

Open `BP_Character` (`Content/Character/Blueprints/`).

1. **Class Defaults** > category **SunsetCove|Input**: **Sunset Cove Mapping Context** = `IMC_SunsetCove`. (**Sunset Cove Mapping Priority** stays at 1.)
2. **Components** panel > select **Interaction**. In Details > **Interaction|Input**: **Interact Action** = `IA_Interact`.
3. Select **Prop Inventory**. In Details > **PropInventory|Input**:
   - **Place Action** = `IA_PropPlace`
   - **Throw Action** = `IA_PropThrow`
   - **Rotate Action** = `IA_PropRotate`
   - **Drop Action** = `IA_PropDrop`
4. Compile and save.

If one of the actions is left empty, the Output Log shows one warning at the start of Play (*'...' has no Place Action...*) and only that action is disabled.

---

## 6. The hologram material (optional)

Without it the hologram is visible when valid and **hidden** when invalid (you get a warning once).

1. Create a **Material**, for example `M_PropHologram` in `Content/InventorySystem/PropInventorySystem/Materials/`.
2. In its Details: **Blend Mode** = *Translucent*, **Shading Model** = *Unlit*, **Two Sided** = on.
3. **Usage**: tick **Used with Skeletal Mesh** (props are skeletal meshes; without this flag the material shows as the default grid on them). In the Material editor: Details > *Usage* > *Used with Skeletal Meshes*.
4. Add a **VectorParameter** named exactly `HologramColor` (default a light green). Connect its RGB to **Emissive Color**.
5. Add a **Constant** (for example 0.35) to **Opacity**. (Optional: multiply with a Fresnel for a glassy edge.)
6. Save. On the Prop Inventory component: **PropInventory|Placement|Hologram** > **Hologram Material** = `M_PropHologram`.

If you name the parameter differently, set **Hologram Color Parameter Name** to match. The valid / invalid colors are **Valid Hologram Color** and **Invalid Hologram Color** on the same component.

---

## 7. Making a prop (one Blueprint, one mesh)

1. Content Browser > right-click > **Blueprint Class** > in *All Classes* search **Sunset Cove Prop** > create, for example `BP_Sunbed` in `Content/Gameplay/Props/`.
2. Open it. In the **Components** panel select **PropMesh** and, in Details > **Mesh** > **Skeletal Mesh Asset**, pick the prop's skeletal mesh (the one with the `Grip_R` bone).
3. Compile and save. **That is all.** The Blueprint already has:

| What | Done by C++ |
|---|---|
| a **collision box as the root**, Movable, with the `PhysicsActor` collision preset (blocks the interaction trace and the world) | yes |
| the box **sized to the mesh** and the mesh centered in it (in the viewport you see the box wrap the mesh) | yes (`Auto Fit Collision To Mesh`) |
| the mass | yes (`Mass Kg`, default 10) |
| the grip: the `Grip_R` bone is aligned to the hand socket when carried | yes |
| the Prop Interface (grab, place, throw, drop, hologram) | yes |
| the hologram: a copy of the skeletal mesh, in its current pose | yes |

Then drop it in the level (**do not move PropMesh in the viewport**: with Auto Fit it is re-centered automatically; to move it yourself turn Auto Fit off, see below). A prop in the level is solid and stays put until grabbed (**Start Simulating Physics** is off by default).

### 7.1 Properties of the Blueprint (Class Defaults)

| Category | Property | Default | What for |
|---|---|---|---|
| `Prop\|Grip` | `Grip Socket Name` | `Grip_R` | bone / socket of the prop skeleton held by the hand (right hand, and left unless the next one is set) |
| | `Left Grip Socket Name` | none | a different grip for the left hand (optional) |
| | `Hold Location Offset`, `Hold Rotation Offset` | 0 | a nudge relative to the hand socket, without going back to Blender |
| `Prop\|Collision` | `Auto Fit Collision To Mesh` | on | box from the mesh bounds; mesh centered in it |
| | `Collision Padding` | 2 cm | the box is a bit larger than the mesh: easier to aim at |
| | `Collision Box Extent` | 50 cm | half size of the box when Auto Fit is off |
| `Prop\|Physics` | `Mass Kg` | 10 | how heavy it is when pushed by other physics bodies once loose. The throw speed does **not** depend on it |
| | `Start Simulating Physics` | off | on = it falls and can be pushed from the start; off = it stays where it was put |
| `Prop\|Rules` | `Can Be Picked Up` | on | off = the player cannot grab it (broken, in use). Settable at runtime |
| `Prop\|Animation` | `Picked Up Animation`, `Placed Animation` | none | an animation asset played once on the mesh when grabbed / placed |

### 7.2 Animations when placed

Two ways, use one:

- **Simple:** set **Placed Animation** (for example `AS_Umbrella_Open`) and, if you want, **Picked Up Animation** (`AS_Umbrella_Close`). They play once when the prop is placed / grabbed, and the mesh stays in the last pose. These use a single animation on the mesh, so leave **PropMesh > Animation Mode** as *Use Animation Asset* (or no Animation Blueprint).
- **Animation Blueprint:** give **PropMesh** an Animation Blueprint (Details > Animation) and drive it from the Blueprint events **On Grabbed**, **On Dropped**, **On Placed**, **On Thrown** (right-click in the Event Graph; they have the player as *Holder*). Leave the two animation assets empty.

### 7.3 Sunbeds and visitors

Add **Sunset Cove Rest Spot** components to the Blueprint (children of **PropMesh** or the root). The character disables them while the prop is in your hands and enables them again when you **place** it (a dropped or thrown prop stays disabled until placed).

### 7.4 When Auto Fit is not what you want

Untick **Auto Fit Collision To Mesh**: set **Collision Box Extent** yourself and place **PropMesh** inside the box by hand. Everything else works the same.

### 7.5 Not a skeletal prop? Not a problem

A prop with only a static mesh can still be made: parent **Actor**, add the **Prop Interface** in Class Settings, and give it a **Movable** static mesh or box as the **root** with simple collision (see `Architecture_Overview.md`, 4.2). `ASunsetCoveProp` is the easy way, not the only way.

---

## 8. Test it

### 8.1 Checklist

Put two props in a test level, press Play.

| # | Do | Expect |
|---|---|---|
| 1 | Look at a prop within ~2.5 m, press **E** | it jumps to the right hand, `Grip_R` on `Prop_R` |
| 2 | Look at a second prop, press **E** | it jumps to the left hand |
| 3 | Look at a third prop, press **E** | nothing (hands full) |
| 4 | Hold **left mouse** | hologram of the **second** prop (last grabbed) follows the view, green on flat ground |
| 5 | Scroll the wheel while holding | it turns in 15° steps |
| 6 | Aim at a wall, or inside a rock | hologram turns red (or disappears without a material) |
| 7 | Release over a red spot | nothing happens; the prop stays in the hand |
| 8 | Release over a green spot | the prop is placed there, upright, and stays; its Placed Animation (if any) plays |
| 9 | Hold **right mouse** for 2 s, release | it is thrown; peak charge goes farthest; it falls and tumbles as **one solid object** |
| 10 | Press **Q** | the last grabbed prop falls forward gently; the other becomes the active one |
| 11 | Hold left mouse, then press right mouse | the placement is cancelled and the charge starts |
| 12 | Grab with the right hand while looking at a wall point-blank, then **Q** | the prop falls in front of the wall, not inside it |
| 13 | Run forward and throw | the prop does not hit you |

### 8.2 See the charge value (no UI needed)

On `BP_Character` > **Event Graph**: *Event BeginPlay* > **Get Prop Inventory** > **Assign On Throw Charge Changed** > on the event, **Print String** with the float. You see 0 → 1 → 0 → 1... while the right mouse button is held, and a final 0 on release.

### 8.3 Debug drawing

Tick **Draw Debug** on the Prop Inventory component (`PropInventory|Debug`): a yellow point where the surface was found and the obstacle box (green = valid, red = invalid). On the Interaction component (`Interaction|Debug`) it draws the interaction trace: green = focused, orange = hit but not interactable, red = nothing. Output Log filter: `LogSunsetCove`.

---

## 9. Tuning cheat sheet

Prop Inventory component values are editable while playing (select the character in the *World Outliner* during PIE).

| I want... | Change |
|---|---|
| a stronger / weaker throw | Prop Inventory: `Throw > Max Throw Speed` / `Min Throw Speed` |
| the bar to charge faster / slower | `Throw > Charge Time To Peak` |
| the bar to hold at the top instead of falling | `Throw > Ping Pong Charge` off |
| a flatter / higher arc | `Throw > Throw Pitch Offset Degrees` |
| less / more tumble | `Throw > Throw Spin Degrees Per Second` |
| the wheel to turn 5° / 45° per notch | `Placement > Rotation Step Degrees` |
| to place farther away | `Placement > Max Placement Distance` |
| placement allowed on steeper ground | `Placement > Max Surface Slope Degrees` |
| the prop to tilt to a slope | `Placement > Align To Surface Normal` |
| props to be pushable after placing | `Placement > Simulate Physics After Place` |
| a bigger / smaller grab reach | **Interaction** component: `Interaction Range`, `Trace Radius` |
| a gentler / harder drop | `Drop > Drop Forward Speed`, `Drop Up Speed` |
| one prop held a bit higher / turned | that prop: `Prop|Grip > Hold Location Offset / Hold Rotation Offset` (or fix the grip bone in Blender) |
| a prop easier to aim at | that prop: `Prop|Collision > Collision Padding` |

---

## 10. Troubleshooting (Output Log, filter `LogSunsetCove`)

| Symptom | Cause | Fix |
|---|---|---|
| Nothing happens on **E**, no log | `IA_Interact` not set, or not in the mapping context | sections 3-5. Warning *'Interaction: ... has no Interact Action'* confirms it |
| Aiming at the prop does not focus it (Draw Debug on the Interaction component shows orange or red) | the prop is not a `Sunset Cove Prop` and has no Prop Interface, or does not block Visibility, or is out of range | make it a child of `Sunset Cove Prop` (section 7) |
| *'Prop X has no Skeletal Mesh Asset on its PropMesh component'* | mesh not set | section 7, step 2 |
| *'Prop X: grip bone or socket 'Grip_R' does not exist on skeleton of ...'* | the skeleton has no bone with that name | the animator adds it in Blender (section 2.2), or set `Grip Socket Name` to a bone that exists |
| *'PropInventory: hand socket(s) or bone(s) [...] not found on 'FirstPersonMesh''* | the arms mesh has no `Prop_R` / `Prop_L`, or the arms have no skeletal mesh asset, or the names differ | section 2.1; or fix the names in `PropInventory|Hands` |
| The prop is in the hand but turned or offset wrongly | the `Grip_R` bone and the hand socket have different axes in Blender | align them in Blender (section 2), or nudge with the prop's `Hold Location / Rotation Offset` |
| The prop jumps when grabbed after being placed | the grip bone moves in the placed animation | keep the grip unkeyed (section 2.2) |
| Warning *'no Hologram Material'* | optional material missing | section 6 |
| Hologram looks like a gray grid | the hologram material lacks *Used with Skeletal Meshes* | section 6, step 3 |
| Hologram is always red | no flat surface in range, an obstacle, or the prop's `Is Placement Blocked` returns true | lower `Max Surface Slope`, move, enable `Draw Debug` to see the box; if the beach is a single mesh try `Ignore Support Surface Actor` off |
| Hologram is red only near rocks or walls | working as designed (obstacle box) | shrink with `Obstacle Box Shrink` |
| The prop floats when placed | its animated mesh extends below the box, or `Collision Padding` is large | the placement uses the bounds of all components: lower the padding, check the mesh in the Blueprint viewport |
| A thrown prop does not fall | collision of the root altered in the Blueprint (the box must keep the `PhysicsActor` preset) | restore the preset on **CollisionBody** |
| The throw hits me | `Release Pawn Ignore Seconds` too low for a big prop | raise it |
| **Q** does something else (dodge) | the framework context binds Q | remap `IA_PropDrop` (section 4) |
| The wheel rotates the wrong way | | add **Negate** to the wheel mapping |
| Warnings about missing actions after possessing | actions not assigned in the BP | section 5 |

---

## 11. Later: connecting the charge bar and the animations

**Charge bar (the only UI of this system).** Create the widget, then on `BP_Character` (or the HUD):
- **Assign On State Changed**: show the widget when `New State = Charging Throw`, hide it otherwise.
- **Assign On Throw Charge Changed**: set the bar percent to the float (0..1).

**Arms animations (one-shots for the player).** **Assign On Prop Action**: *Switch on Action* (Grab / Place / Throw / Drop), then **Play Action Montages** (an existing function on the character) with the arms montage for that action. The action has already taken effect when the event fires; if the animation must delay it, that is a small change inside the component (see `Architecture_Overview.md`, 9.1). The Animation Blueprint of the arms can read `Get State`, `Is Holding Any Prop` and `Is Hand Occupied` to choose hold poses.

**Tools.** The Tool Inventory is a separate system (`../ToolInventorySystem/`). It is refused while `Is Holding Any Prop` is true: the character does that wiring, nothing to do here.

---

## 12. What was and was not verified

- The C++ of the systems and of `ASunsetCoveProp` was written against the UE 5.8 headers and built with the Development Editor target (see the build result in the hand-over notes).
- **Not run in the editor** by the author: how a real `Grip_R` bone and `Prop_R` socket line up with your Blender rig, hologram appearance, throw feel, placement obstacle detection on your actual beach geometry, and the Auto Fit box on your real meshes. Walk through section 8.1 once with a real prop and tune with section 9.
