# Footsteps — Architecture and Setup

**Project:** Sunset Cove | **Engine:** Unreal Engine 5.8 | **Code:** `Source/SunsetCove/{Public,Private}/AudioSystem/Footsteps/`
**Status (2026-11-13):** built with placeholder sounds (procedural, `Content/Audio/SFX/Footsteps/`). Replace them with real
recordings when you have them: same names, re-import, nothing else changes.

---

## 1. What it does

The player hears a step every stride while walking or running, louder when running, softer when crouched, and a landing sound
after a jump or a fall. The sound depends on what is under the feet:

| Under the feet | How it is known | Sound row |
|---|---|---|
| the sea (shallow water on the beach) | the sea is asked directly (`USunsetCoveSeaSubsystem` → `IsInsideSeaArea` + `GetWaterHeightAt`): the water surface is more than 3 cm above the feet | **Water** |
| the landscape (sand, mud...) | a short trace under the feet returns the **Physical Material** of the painted layer | that layer's surface |
| a mesh (rocks, a wooden deck) | same trace: the Physical Material of the mesh's material | that surface |
| anything without a Physical Material | — | **Default** (= sand: it is a beach) |

---

## 2. The pieces

```
Project Settings > Physics > Physical Surface:  SurfaceType1 Sand · 2 Mud · 3 Rock · 4 Wood · 5 Water   (Config/DefaultEngine.ini)
Content/Environment/PhysicalMaterials/:         PM_Sand · PM_Mud · PM_Rock · PM_Wood · PM_Water          (one Surface Type each)
Content/Audio/SFX/Footsteps/<Surface>/:         S_Step_<Surface>_01..04, S_Land_<Surface>_01              (SC_SFX)
Content/Audio/DA_Footsteps (USunsetCoveFootstepSet): one row per surface → step sounds + landing sounds + volume + variation

BP_Character > Footsteps (USunsetCoveFootstepComponent)
   every stride on the ground ─► DetectSurface(): sea? → Water : trace (physical material) → surface
                              ─► DA_Footsteps row ─► random sound (never the same twice in a row), random pitch / volume
                              ─► UAudioFrameSubsystem::PlaySound2D (category SFX: SFX + Master sliders apply)
                              ─► OnFootstep(Surface, Location, bLanding)   (for sand puffs, splashes, footprints later)
   LandedDelegate (fell faster than 350 cm/s) ─► landing sound, louder the harder the fall
```

| Class | Role |
|---|---|
| `USunsetCoveFootstepSet` (`DA_Footsteps`) | data only: rows `Surface → Steps[], Landings[], Volume, PitchVariation, VolumeVariation`, a `Default` row, the volume `Category` |
| `USunsetCoveFootstepComponent` | rhythm (distance-based strides), ground detection, playback, `OnFootstep`; `PlayFootstep(bLanding, VolumeScale)` for anim notifies |
| `ASunsetCoveSea` | answers whether a point is in the water (`IsInsideSeaArea`, `GetWaterHeightAt`) |

Steps come from the **distance walked**, not from an animation, so they work in first person with no body animation.
If one day the body animation should drive them: turn `Auto Footsteps` off and call **Play Footstep** from an anim notify.

---

## 3. Setup in the editor

### 3.1 Already done

- Physical surfaces Sand / Mud / Rock / Wood / Water (Project Settings > Physics > Physical Surface).
- `PM_Sand`, `PM_Mud`, `PM_Rock`, `PM_Wood`, `PM_Water` in `Content/Environment/PhysicalMaterials/`.
- `DA_Footsteps` filled with the placeholder sounds; `BP_Character > Footsteps > Footstep Set = DA_Footsteps`.

### 3.2 When the artist's landscape arrives (the important step)

The landscape decides sand / mud **per painted layer**:

1. In the landscape's **Layer Info** assets (one per paint layer, e.g. `LI_Sand`, `LI_WetSand`, `LI_Mud`): set **Phys Material** to
   `PM_Sand`, `PM_Mud`... (open the Layer Info asset, or Landscape mode > Paint > the layer's info).
2. Wet sand can stay `PM_Sand` (it is still sand), or get `PM_Mud` for a squelchy shore line.
3. Rocks and cliffs (static meshes): in their **material** (or the mesh's collision override), *Phys Material* = `PM_Rock`.
4. A wooden deck / pier: `PM_Wood` (add sounds for it first, 3.4).

The trace uses the *Visibility* channel; landscapes and meshes block it by default. Where layers blend, the **dominant** layer
at that point decides.

### 3.3 Replace the placeholder sounds

The placeholders are synthesized (good enough to hear the system, not final). Good free sources: Sonniss GDC audio bundles
(royalty free), freesound.org (check each licence: CC0 / CC-BY), Fab. For each surface, 4-6 **short single steps** (0.2-0.4 s,
mono, trimmed tight, no long tail) and 1-2 landings. Either re-import over the same names, or drop new files in the folder and
put them in the rows of `DA_Footsteps`. Set *Sound Class* = `SC_SFX` on new files.

### 3.4 A new kind of ground (gravel, wet rock, wood...)

1. Project Settings > Physics > Physical Surface: a new name on the next free Surface Type.
2. Duplicate a `PM_*`, set its *Surface Type*.
3. `DA_Footsteps` > Surfaces > add a row: Surface + sounds.
4. Put the physical material on the landscape layer / mesh material.

---

## 4. Tuning (`BP_Character` > Components > Footsteps)

| Property | Default | Meaning |
|---|---|---|
| Walk / Run Stride Length | 150 / 220 cm | distance between two steps; shorter = faster rhythm |
| Walk / Run Speed | 250 / 600 cm/s | speeds treated as walking / running (in between: blended) |
| Min Speed | 40 cm/s | slower than this counts no steps |
| Walk / Run Volume | 0.6 / 1.0 | loudness walking / running |
| Crouch Volume Scale | 0.45 | crouched steps |
| Landing Min / Full Volume Fall Speed | 350 / 900 cm/s | a landing sound only after a real fall, louder when harder |
| Ground Trace Length | 60 cm | how far under the feet the ground is searched |
| Sea Water Surface | SurfaceType5 (Water) | the row used when the feet are in the sea |
| Min Water Depth | 3 cm | the sea counts only when its surface is this far above the feet |
| Play 2D | on | your own feet, flat; off for other characters (3D at the feet) |
| Draw Debug | off | ground trace (green / red) and the surface name of each step |

In `DA_Footsteps`, per row: `Volume` (sand 0.7, mud 0.9, rock 0.8, water 1.0 today), `Pitch Variation`, `Volume Variation`.

---

## 5. Limits

- Single player: the player's steps are local 2D sounds. Visitors can use the same component with `Play 2D` off.
- The sea test is "the water surface is above the feet": deep water is not special-cased (swimming does not step: the component
  only counts steps while the character moves on the ground).
- Physical surfaces are project-wide names in `DefaultEngine.ini`: keep the order (Sand = SurfaceType1 ...), assets store the index.
