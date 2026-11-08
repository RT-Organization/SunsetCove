# Architecture Overview — Arms Animation System (props and tools)

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **Module:** `SunsetCove`
**Audience:** programmers who build on the system. To set it up in the editor and to drop in the animator's files: see `Engine_Setup_Guide.md`.
**Companion documents:** `../InventorySystem/PropInventorySystem/Architecture_Overview.md`, `../InventorySystem/ToolInventorySystem/Architecture_Overview.md`, and the animator's contract `../InventorySystem/Guida_Animatore.md` (Italian, by design).

---

## 1. Purpose and boundaries

The animator delivers one animation per **item**, per **action** and per **hand** (`AS_Arms_<Item>_<Action>_<Hand>`). This system plays the right one when the player grabs, places, throws, drops, equips, holsters or uses something, and decides **when the item really moves** (attaches, detaches, appears, launches) so that it lands on the right frame of the animation.

| It does | It does NOT do |
|---|---|
| choose the animation by item id, action and hand, from a data asset | decide whether an action is allowed (the inventories do) |
| keep the free arm in its idle / walk / run while the other arm holds or uses an item | change the base locomotion (the base arms Animation Blueprint does, untouched) |
| play one-shots (grab, throw...) and hold poses (hold, throw charge) | animate the item itself (props and tools have their own skeleton and `PickedUpAnimation` / `PlacedAnimation`) |
| tell the inventories when the physical effect happens (delay or notify) | replicate anything (single player) |
| do nothing, silently, for anything that is missing | require an editor asset per animation (no `AM_` montage exists) |

Nothing in `Plugins/CoreFramework` was changed. The dependency direction is **one way**:

```
ASunsetCoveCharacter  ──creates & wires──►  UArmsAnimationComponent ──listens to──► UPropInventoryComponent / UToolInventoryComponent
UPropInventoryComponent / UToolInventoryComponent  ──know only──► FActionCommitTiming (a tiny generic type), never the animation layer
UArmsAnimationComponent ──reads──► UArmsAnimationSet (data asset)  ──writes variables──► UArmsAnimInstance (post-process ABP)
```

---

## 2. Findings about the existing animation setup

| Question | Answer |
|---|---|
| Which mesh and ABP drive the arms? | `ABaseCharacter::FirstPersonMesh` (framework) with the mesh asset `SM_Character` (skeleton `SK_Character`, project content) and the base arms ABP. It was `ABP_SunsetCoveCharacter` and is now `ABP_FPArms_Base`, a child of the framework `UAnimationInstance` (locomotion domains, later swimming). It lives in **project** content, not in the plugin. |
| Can the project extend it without editing the plugin? | Yes. This system does it with a **post-process Animation Blueprint** on the same mesh, which receives the pose of the base ABP. The base ABP is not edited. |
| Skeleton | `Root` > `Arm_L/R` > `UpperArm_L/R` > `Hand_L/R` > fingers and `Prop_L/R`; `Twist_L/R` is a child of `Arm_L/R`; `CB_Hand_L/R` > `Ik_Hand_L/R` and `Pole_L/R` are children of `Root`. There is no clavicle: the top of each arm is `Arm_L/R`. |
| Framework montages | `ABaseCharacter::PlayActionMontages` plays authored montage assets in the shared `DefaultSlot` group and replicates. It is not used here (section 5). |

The branch filter of each arm in the Layered Blend is therefore **`Arm_R`** and **`Arm_L`**, which contain the twist bone, the hand, the fingers and the hand sockets. `CB_Hand_*`, `Ik_Hand_*` and `Pole_*` are outside them: they matter only if the animator keys them (setup guide, section 4).

---

## 3. Files and folder layout

```
Source/SunsetCove/
├── Public/Core/Utils/SunsetCoveActionCommit.h         EActionCommitMode, FActionCommitTiming: the "when does the item move" contract
├── Public/Interfaces/ItemIdentityInterface.h          IItemIdentityInterface: GetItemId() (Sunbed, Bucket...)
├── Public/AnimationSystem/ArmsAnimation/
│   ├── ArmsAnimationTypes.h                           EArmSide, EArmsAnimAction, EArmHeldPose, FArmsClip and the item/action structs
│   ├── ArmsAnimationSet.h                             UArmsAnimationSet: the data asset (item > action > hand > animation), editor helpers
│   ├── ArmsAnimInstance.h                             UArmsAnimInstance: the graph variables of ABP_FPArms_Items
│   ├── ArmsAnimationComponent.h                       UArmsAnimationComponent: events in, animations out
│   └── AnimNotify_ArmsItemCommit.h                    the notify "Arms Item Commit"
├── Private/AnimationSystem/ArmsAnimation/             the matching .cpp files
└── (modified, additive) Gameplay/Props/SunsetCoveProp.*  PropId + IItemIdentityInterface
                         Gameplay/Tools/SunsetCoveTool.h  IItemIdentityInterface (ToolId)
                         InventorySystem/.../*InventoryComponent.*, *InventoryTypes.h   commit timing (section 6)
                         Character/SunsetCoveCharacter.*  creates and wires the component
```

Editor assets (the setup guide creates them): `ABP_FPArms_Items`, `DA_ArmsAnimations`, two slots on `SK_Character`, and the imported `AS_Arms_*` sequences.

---

## 4. The two kinds of animation

| | Poses | One-shots |
|---|---|---|
| Actions | `Hold`, `PlaceAim`, `ThrowCharge` | `Grab`, `Place`, `Throw`, `Drop`, `Equip`, `Holster`, `Use`, `Pickup`, `Open`, `Close` |
| How they play | variables of `UArmsAnimInstance` selected by a *Blend Poses by Enum* node in the graph | montages built **at run time** from the sequence, played in the slot of the arm |
| Duration | until something changes them | their own length |
| Why | a pose has no end, loops (`Hold`) or must stay on its last frame (`ThrowCharge`) until the release | blend in/out, interruption, the looping `Use` of the spray and the automatic return to the held pose are exactly what montages do |

The slot source in the graph is the held pose, so after a one-shot the arm blends back to `Hold` by itself (or to locomotion after a release).

**Why not authored `AM_` montages:** 40 animations would need 40 montage assets, each a manual step, for no gain. `UAnimInstance::PlaySlotAnimationAsDynamicMontage` builds the same montage from the sequence in memory, and the notifies placed on the sequence still fire. **Why not the framework `PlayActionMontages`:** it needs montage assets, uses the shared `DefaultSlot` group (so a right-hand animation would interrupt a left-hand one) and replicates, which a single-player game does not need.

### 4.1 The graph (`ABP_FPArms_Items`, a post-process Animation Blueprint)

```
Input Pose ─► cached pose "Base"
Layered Blend per Bone   (Base Pose = "Base")
  Layer 0, right arm, filter bone Arm_R :  Slot "ArmItem_R"  ◄─ Blend Poses by EArmHeldPose (RightArmPose)
  Layer 1, left arm,  filter bone Arm_L :  Slot "ArmItem_L"  ◄─ Blend Poses by EArmHeldPose (LeftArmPose)
       None -> "Base" | Hold -> Sequence Player(HoldSequence, loop) | PlaceAim -> Sequence Player(PlaceAimSequence, loop)
       ThrowCharge -> Sequence Player(ThrowChargeSequence, no loop: stays on its last frame)
  Blend weights = 1 (constant)
─► Output Pose
```

The **free arm keeps its locomotion** because each layer only overrides the bones of its own arm. Each arm has **its own slot group** (`ArmsRight`, `ArmsLeft`), so montages of the two arms never interrupt each other. The graph is cheap: with no item the layer is the base pose itself.

A Sequence Player without a sequence shows the reference pose, so `UArmsAnimInstance::SetArm` guarantees that a pose is never selected without its sequence (`ThrowCharge` and `PlaceAim` fall back to `Hold`, `Hold` falls back to `None`, and unselected poses get the `Hold` sequence).

---

## 5. Data flow: event, then mapping, then animation

```
PropInventory / ToolInventory
    │  ActionBeginHook (action accepted, effect pending)        delegates (OnSlotsChanged, OnStateChanged, OnEquippedToolChanged, OnToolAction)
    ▼                                                                    ▼
UArmsAnimationComponent                                         UArmsAnimationComponent
    item id  = IItemIdentityInterface::GetItemId(actor)             ResyncArm: the inventories say what each hand holds
    clip     = UArmsAnimationSet::FindClip(item, action, hand)       ApplyArm: pose + sequences -> UArmsAnimInstance variables
    montage  = PlaySlotAnimationAsDynamicMontage(clip, slot)
    answer   = FActionCommitTiming (when the item moves)
```

| Event | Effect on the arms |
|---|---|
| hook: `Grab`, `Place`, `Throw`, `Drop` (props), `Pickup`, `Equip`, `Holster`, `Drop` (tools) | start the one-shot of that item, action and hand; answer with the commit timing of the clip |
| `OnSlotsChanged`, `OnEquippedToolChanged` | `ResyncArm`: a hand holds an item -> pose `Hold`; empty -> `None`. A prop wins over a tool in the right hand |
| prop `OnStateChanged(Placing)` / `(ChargingThrow)` | pose `PlaceAim` / `ThrowCharge` on the hand of the active prop |
| prop `OnStateChanged(Idle)` | back to `Hold` **on the next tick** (a one-shot timer): a `Place` or `Throw` follows in the same call (the inventory sets the state to Idle first) and must take over the arm, or a new aim starts at once |
| release actions (`Place`, `Throw`, `Drop`, `Holster`) | the arm keeps its pose while the release animation plays and is re-read from the inventory when the montage **starts blending out**, so it never snaps to locomotion under its own animation |
| tool `UseStarted` / `UseEnded` | the `Use` one-shot; a clip marked *loop while active* (the spray) repeats until `UseEnded` stops it |
| tool menu opened / closed | optional `Open` / `Close` clips of the generic id `ToolMenu` |

A tool swap is one `OnEquippedToolChanged`: only the `Equip` of the new tool is animated (the old tool is hidden at once), because a `Holster` and an `Equip` in the same frame would cut each other in the same slot.

### 5.1 Missing animation is a safe no-op

No data asset, item, action, hand or sequence: nothing plays, nothing crashes, nothing is logged per event. One line at start says what is not set up (no set, no post-process ABP, slot missing on the skeleton). `bLogMissingClips` lists every missing clip once, for a test session. The data asset can also stand in a **debug placeholder** for every empty clip, to see all the events before a single animation exists.

---

## 6. Commit timing: when the item really moves

**The rule: the logical state is immediate, only the physical effect can wait.** The moment the player acts, the hand is occupied (Grab) or free (Place, Throw, Drop), the tool is equipped or holstered, the slot is filled or emptied, and `OnSlotsChanged` fires. That never depends on an animation. What can be delayed is the physical effect: attaching to the socket, detaching, moving to the placement transform, launching, showing or hiding a tool.

Per clip, in the data asset: **Immediate** (the default, same as before this system), **After Delay** (seconds) or **On Notify** (the animation says when). `On Notify` is the best practice: it follows the animation if its speed or length changes. It has a **timeout** (default: the clip length plus 0.1 s), so a lost notify cannot leave an item stuck.

```
player acts ─► inventory accepts: slots change NOW ─► ActionBeginHook ─► the animation starts, answers with the timing
                                                          │
                          Immediate ◄─────────────────────┤
                          AfterDelay: timer ──────────────┤──► the physical effect happens (CommitGrab / CommitPlace / CommitRelease / CommitDrop...)
                          OnNotify: UAnimNotify_ArmsItemCommit ─► CommitPendingAction(), or the timeout
```

Behavior of the inventories (the hook is a C++ delegate, `ActionBeginHook`; nothing changes when it is not bound):

- **Prop inventory:** `Grab` (attach and `OnPropGrabbed`), `Place` (detach and move to the hologram transform captured at the release), `Throw` and `Drop` (detach and launch). The speed is decided at the release; the place the prop leaves from (and the wall-safety trace) is decided at the commit, where the hand is by then. `OnPropAction` still means "the physical effect took place", so the rest-spot logic is unchanged.
- **Tool inventory:** `Pickup` (attach and hide), `Equip` (show), `Holster` (hide), `Drop` (detach and launch). The equipped tool changes at once; a tool whose `Equip` is pending cannot be used yet. A swap, a drop of the equipped tool and "hands become busy" hide the old tool immediately (only the new `Equip` is timed). With auto-equip on, the first `Pickup` goes into the hand at once and the `Equip` is the timed moment.
- **Flushing:** a new action, `CancelCurrentAction`, `EndPlay` and the unpossess apply a pending effect immediately, so the world and the slots always agree. A prop or tool destroyed while pending is skipped safely.
- **Public additions:** `CommitPendingAction()` (BlueprintCallable) and `IsCommitPending()`. `TryGrabProp`, `ConfirmPlacement`, `TryPickupTool`... now mean "accepted"; the effect may follow slightly later.
- `Use` is not timed (the moment a bucket catches a crab is a natural next step with the same mechanism).

---

## 7. Item identity

`IItemIdentityInterface::GetItemId()` is the key. `ASunsetCoveTool` returns its `ToolId`; `ASunsetCoveProp` returns its `PropId`, and when `PropId` is empty the Blueprint name without `BP_` and `_C` (`BP_Sunbed` gives `Sunbed`, the animator's rule). Set `PropId` by hand on a Blueprint named differently (`BP_Sunbed_Proto`). A Blueprint prop that implements `IPropInterface` by hand and does not implement this interface has no id and plays no arm animation (no error).

The ids are the animator's names: `Sunbed`, `Umbrella`, `Bucket`, `Spray`, `Toy`. The radial menu clips use the generic id `ToolMenu`.

---

## 8. Extending

| Goal | How |
|---|---|
| A new item (a net, a towel) | Give its Blueprint a `PropId` / `ToolId`; add the id to `Items` in `DA_ArmsAnimations` and fill the clips (or import `AS_Arms_<Item>_<Action>_<Hand>` and press *Populate From Naming Convention*). **No code.** |
| A new action type | Add a value to `EArmsAnimAction` (and to the animator's guide), hook it where the event happens. |
| A tool held in the left hand (`Tool_L`, the crab) | The data and the channels already have a left side; a left-hand tool needs the Tool Inventory to know the hand. |
| Another commit moment (the effect of `Use`) | Reuse `FActionCommitTiming`: ask the hook when the effect of the use must land. |
| The prop travels to the hand instead of teleporting | **Future work, requested by the project owner.** `FUTURE:` comments mark the place: `CommitGrab` (props) and the physical pickup in `TryPickupTool` (tools). |

---

## 9. Assumptions and limits

- **Single player.** Nothing replicates.
- **One tool hand.** Tools use the right hand. `Tool_L` is reserved.
- **Both arms can hold a prop at the same time**, each with its own animation; actions act on the active (last grabbed) prop.
- **Blend times of the poses are in the Animation Blueprint**, per pin, not per item. Blend times of the one-shots are per clip.
- **Notifies must be on the sequence** (the `AS_` asset). Whether they survive a re-import of the FBX is to be confirmed in the editor.
- **Compilation:** this was written while the editor held a Live Coding session, so the first build still has to be run (see the hand-over notes for the result). **Not verified in the editor:** the graph, the slots, the montage playback in the post-process instance, the bone filters and the feel of the timing need a play test. The checklist is in the setup guide.
