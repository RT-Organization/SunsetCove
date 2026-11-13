# Sea System — Unreal Setup Guide

**What this is:** the editor steps to turn the C++ base (`ASunsetCoveSea`, see `Sea_Architecture.md`) into a sea that looks good:
the mesh, the material, the foam, the sound. Follow it in order; each step works on its own, so you can stop after any of them.
**Folders:** `Content/Environment/Water/` (mesh, material, textures, MPC, Blueprint), sounds in `Content/Audio/`.

---

## 0. Shopping list (what you need, and where to find it for free)

| Thing | Type | Needed for | Where to get it (free) |
|---|---|---|---|
| `SM_SeaPlane` | Static Mesh, 100 × 100 cm, 256 × 256 subdivisions | the waves (vertices to move) | make it in Unreal (step 1), or in Blender (Plane, subdivide, apply) |
| `MPC_Sea` | Material Parameter Collection | sharing the sea values with other materials | make it (step 2) |
| `M_Sea` + `MI_Sea` | Material (Single Layer Water) + instance | the look | make it (steps 3-5) |
| `T_Water_N_A`, `T_Water_N_B` | 2 tileable **water normal maps** | ripples, glitter | Fab (free "water normal"), ambientCG / Poly Haven (CC0), or the engine Water plugin content if enabled |
| `T_Foam` | tileable **foam texture** (grey/white mask) | shore + crest foam | ambientCG / Fab ("sea foam", "foam mask"); a cloudy noise texture works to start |
| `ATT_ShoreWaves` | Sound Attenuation | waves louder near the water | make it (step 8) |
| (later) `NS_ShoreSpray` | Niagara system | splashes on the beach | Fab free water splash packs, or the Niagara templates |

Texture import settings: normal maps → *Compression: Normalmap*; foam → *Compression: Masks* (or Grayscale), sRGB **off**.

---

## 1. The sea plane mesh (`SM_SeaPlane`)

1. **Modeling Mode** (top toolbar mode selector) > **Create** > **Plane**.
2. Width 100, Depth 100, **Width Subdivisions 256, Depth Subdivisions 256**. Click in the level, then **Accept**.
3. Save the generated mesh as `Content/Environment/Water/SM_SeaPlane`, delete the actor it placed.
4. Open the mesh: *Nanite* off (WPO on a dense regular plane is cheaper without it), *Collision* none.

The sea actor scales this 100 cm plane to its `Surface Size` (default 1 km × 1 km). For a nicer look keep the waves area
around 400 m (`Surface Size` 40000) and add a second flat plane for the horizon later.

---

## 2. The parameter collection (`MPC_Sea`)

Content Drawer > right click > *Materials* > **Material Parameter Collection**, name `MPC_Sea`, open it and add **exactly**:

| Scalar parameters | Vector parameters |
|---|---|
| `SeaTime`, `SeaTideOffset`, `SeaLevel` | `SeaWaveA0`, `SeaWaveA1`, `SeaWaveA2`, `SeaWaveA3`, `SeaWaveB0`, `SeaWaveB1`, `SeaWaveB2`, `SeaWaveB3` |

The sea actor writes them every frame (if a name is missing it logs one warning). The sea material can read either these or
the same names as material parameters — this guide uses the **collection** (one place, also usable by the landscape).

---

## 3. The sea material (`M_Sea`) — settings

Create a Material `M_Sea`. In its Details:

| Setting | Value |
|---|---|
| Material Domain | Surface |
| Blend Mode | Opaque |
| **Shading Model** | **Single Layer Water** |
| Tangent Space Normal | **off** (we output a world-space normal) |
| Usage: Used with Static Mesh | on |

Single Layer Water adds an output node, **Single Layer Water Material**, with *Scattering Coefficients*, *Absorption
Coefficients*, *Phase G* and *Color Scale Behind Water*: that is where the water colour comes from (not Base Color).

---

## 4. The material graph

### 4.1 Wave displacement (World Position Offset)

1. Add a **Collection Parameter** node for each of `SeaTime`, `SeaTideOffset`, `SeaWaveA0..3`, `SeaWaveB0..3` (Collection = `MPC_Sea`).
2. Add a **Custom** node named `GerstnerOffset`, Output Type **CMOT Float 3**, inputs (in this order, names exact):
   `P` (← *Absolute World Position*, option *Excluding Material Offsets*), `T` (← SeaTime), `A0 A1 A2 A3 B0 B1 B2 B3` (← the vectors).
   Code:

```hlsl
float4 A[4] = { A0, A1, A2, A3 };
float4 B[4] = { B0, B1, B2, B3 };
float3 O = 0;
for (int i = 0; i < 4; i++)
{
    float f = A[i].z * dot(A[i].xy, P.xy) - A[i].w * T;
    float c = cos(f);
    O.x += B[i].y * A[i].x * c;
    O.y += B[i].y * A[i].y * c;
    O.z += B[i].x * sin(f);
}
return O;
```

3. `GerstnerOffset + (0, 0, SeaTideOffset)` → **World Position Offset**.

This is the same formula as `SunsetCoveWaves::GetDisplacement` in C++. If you change one, change the other.

### 4.2 Normal

A second **Custom** node `GerstnerNormal`, Output **CMOT Float 3**, same inputs:

```hlsl
float4 A[4] = { A0, A1, A2, A3 };
float4 B[4] = { B0, B1, B2, B3 };
float3 N = float3(0, 0, 1);
for (int i = 0; i < 4; i++)
{
    float f = A[i].z * dot(A[i].xy, P.xy) - A[i].w * T;
    float kA = A[i].z * B[i].x;
    N.x -= A[i].x * kA * cos(f);
    N.y -= A[i].y * kA * cos(f);
    N.z -= A[i].z * B[i].y * sin(f);
}
return normalize(N);
```

Detail ripples: sample `T_Water_N_A` and `T_Water_N_B` with **World Aligned** UVs (Absolute World Position XY / 800 and / 1500),
each panning in a different direction with `SeaTime`, blend them (*BlendAngleCorrectedNormals*), transform to world
(*Transform Vector* Tangent → World) and blend with `GerstnerNormal`. Expose the ripple strength as a scalar parameter.
Plug the result into **Normal**. (Tip: for performance, compute `GerstnerNormal` per vertex through a *Vertex Interpolator*.)

### 4.3 Colour (Single Layer Water output)

Starting values to tune in `MI_Sea` (tropical cove: clear turquoise shallows, deep blue far out):

| Output | Start with |
|---|---|
| Base Color | very dark blue-green (0.0, 0.03, 0.04) — mostly hidden by scattering |
| Specular / Roughness | 0.5 / 0.04 (glossy water) |
| Scattering Coefficients | (0.02, 0.08, 0.09) — the turquoise |
| Absorption Coefficients | (0.40, 0.09, 0.06) — red is absorbed first, so depth becomes blue |
| Phase G | 0.1 |
| Color Scale Behind Water | 1 |

### 4.4 Shore foam (the white where waves hit the sand)

```
WaterDepth = SceneDepthWithoutWater − PixelDepth                (both nodes exist in a Single Layer Water material)
ShoreMask  = 1 − saturate(WaterDepth / FoamDepth)               FoamDepth ≈ 60 cm
Bands      = saturate(sin((WaterDepth / BandWidth − SeaTime · BandSpeed) · 2π) · 0.5 + 0.5)    BandWidth ≈ 40, BandSpeed ≈ 0.3
Foam       = ShoreMask · lerp(1, Bands, 0.6) · T_Foam(world UV / 300, panning slowly) → step-ish with a Smoothstep
```

Use `Foam` to lerp **Base Color** towards white (0.9) and **Roughness** towards 0.6, and to lower the scattering (foam is
opaque). The bands make lines of foam that run towards the beach and fade, like waves washing up.

### 4.5 Crest foam (whitecaps)

Pass `GerstnerOffset.z` from the vertex shader with a **Vertex Interpolator**, then
`CrestMask = saturate((OffsetZ − CrestStart) / CrestRange)` × `T_Foam` → add to `Foam`. Start with CrestStart = 12, CrestRange = 10.

### 4.6 Parameters to expose (for `MI_Sea`)

Ripple strength / tiling / speed, colours of 4.3, FoamDepth, BandWidth, BandSpeed, foam tiling, CrestStart, CrestRange.
Create **`MI_Sea`** (right click `M_Sea` > Create Material Instance) and tune there, not in `M_Sea`.

---

## 5. Place and set the sea

1. Optional: Blueprint `Content/Environment/Water/BP_Sea`, parent **Sunset Cove Sea** (to keep the tuned values in one asset).
   Otherwise drag **Sunset Cove Sea** from *Place Actors* directly.
2. Place it in the level: **its Z is the still sea level** (where the water touches the sand), X/Y in front of the beach.
3. Details:

| Category | Property | Set to |
|---|---|---|
| Components > SurfaceMesh | Static Mesh | `SM_SeaPlane` |
| Sea \| Surface | Surface Material | `MI_Sea` |
| Sea \| Surface | Surface Size | 40000 × 40000 to start (the area with waves) |
| Sea \| Surface | Parameter Collection | `MPC_Sea` |
| Sea \| Waves | Waves | turn **Direction Degrees** of the first (long) wave so it travels **towards the beach** (0 = +X); keep the other two within ±40° of it |
| Sea \| Tide | Tide Amplitude / Period | 10 cm / 240 s to start; 0 = no tide |
| Sea \| Editor | Animate In Editor | on while tuning (waves move in the viewport) |

4. **Shoreline**: select the actor, select the *Shoreline* spline, move its two points onto the waterline of the beach, Alt-drag
   a point to add more, follow the curve of the beach at the water level. It only needs to cover where waves break.

Wave tuning cheat sheet: bigger sea → raise the first wave's Amplitude (≤ Wavelength / 20); sharper crests → Steepness up;
calmer → Wave Time Scale < 1 or lower amplitudes; storms later via the `SetWaves` function.

---

## 6. Wet sand on the landscape (optional, when the artist's landscape material exists)

In the landscape material: Collection Parameter `SeaLevel` (MPC_Sea), `Wet = 1 − saturate((AbsoluteWorldPosition.Z − SeaLevel) / 40)`;
use `Wet` to darken Base Color (× 0.6) and lower Roughness (→ 0.3). The wet band then follows the tide.

---

## 7. Sounds (all through `DA_AudioConfig` > Cues)

Sound assets (`Content/Audio/Ambient/`; already set up on 2026-11-12):

| Sound asset | Looping | Sound Class | Cue (category Ambient) | Cue volume |
|---|---|---|---|---|
| `S_AmbMainMenu_Waves_LoopA` | **on** | `SC_Ambient` | `Audio.Cue.Ambient.MainMenu` | 0.9 |
| `S_AmbGameplay_Waves_Loop` | **on** | `SC_Ambient` | `Audio.Cue.Ambient.ShoreWaves` (attenuation: built into the sea, or `ATT_ShoreWaves`, step 8) | 1.0 |
| `S_Amb_Wind_Loop` | **on** | `SC_Ambient` | `Audio.Cue.Ambient.Wind` | 0.3 (it is a detail) |

Who plays them:

| Cue | Played by |
|---|---|
| `Audio.Cue.Ambient.MainMenu` | `BP_GameMode_MainMenu` > Map Audio > **Ambient Cue** |
| `Audio.Cue.Music.MainMenu` | `BP_GameMode_MainMenu` > Map Audio > **Music Cue** — just set the cue's **Sound** when you have the track (looping, Sound Class `SC_Music`) |
| `Audio.Cue.Ambient.ShoreWaves` | the sea actor (nothing to set; `Shore Waves Cue` empty = this one) |
| `Audio.Cue.Ambient.Wind` | the wind actor (step 9) |

`BP_GameMode_Gameplay` has **no** Ambient Cue: in the cove the ambience is in the world (sea + wind), and the empty cue stops the
menu waves when the game starts.

## 8. `ATT_ShoreWaves` (Sound Attenuation) — optional

The sea already gives the waves sound these values by itself (*Sea | Shore Audio > Shore Attenuation* on the actor, used when the
cue and the sound asset have no attenuation). Make the asset only to share or tune them outside the actor: `Content/Audio/Attenuation/ATT_ShoreWaves`
(right click > *Audio* > *Sound Attenuation*), then set it as the cue's *Attenuation Override* (it then wins over the actor values):

| Setting | Value | Why |
|---|---|---|
| Attenuation Shape | Sphere | |
| Inner Radius | 500 | full volume within 5 m of the waterline |
| Falloff Distance | 4500 | fades out over the next 45 m: silent at the high rocks of a 50 m cove |
| Attenuation Function | Natural Sound | |
| Enable Spatialization | on, **Non Spatialized Distance Max 1200 / Min 400** | close to the water the sound surrounds you instead of coming from a point |
| Enable Air Absorption | on (defaults) | far waves sound duller |

## 9. The wind (`Sunset Cove Wind Ambience`)

Drag **Sunset Cove Wind Ambience** from *Place Actors* into the gameplay level (anywhere; one per level; **not** in the main menu).

| Property | Default | Meaning |
|---|---|---|
| Shore Volume / Inland Volume | 1.0 / 0.35 | multipliers of the cue volume at the water / far inland |
| Shore Near / Far Distance | 5 m / 40 m | where the change happens (a 50 m cove: inland volume near the rocks) |
| Use Shelter | on | rays around the ear: rocks close around you lower the wind |
| Shelter Trace Count / Length | 8 / 15 m | how many rays, how far a rock still shelters |
| Sheltered Volume Scale | 0.05 | volume when fully walled in by the rocks (0 = silent) |
| Draw Debug | off | on = see the rays (red = blocked) and the current values |

The rocks and the landscape shelter automatically (they block *Visibility* like every mesh). Without a sea in the level the
wind simply stays at the inland volume.

---

## 10. Test checklist

| Do | Expect |
|---|---|
| Main menu | soft waves (and the music once its cue has a sound); Options > Audio > Ambient changes the waves |
| Start a game | the menu waves fade out; near the water the shore waves come in, the wind fades in |
| Walk along the beach | the waves stay beside you (turn on the sea's **Draw Debug**: a cyan sphere follows you on the shoreline) |
| Walk inland | waves fade with distance, wind gets quieter |
| Stand under the high rocks | the wind almost disappears (wind **Draw Debug**: most rays red) |
| Sea in the viewport | with the material set, waves move without pressing Play |

## 11. Troubleshooting

| Symptom | Cause |
|---|---|
| Water is flat | `SM_SeaPlane` not set (engine plane has no vertices), or the material lacks the WPO graph, or `MPC_Sea` not set on the actor |
| Waves look different from where things float | the material formula differs from the C++ one (section 4.1) |
| Log: "the collection lacks some parameters" | add the missing names to `MPC_Sea` (section 2) |
| Log: "no waves sound" / "nothing to play" | the tag or the cue row is missing in `DA_AudioConfig` |
| Waves sound is everywhere at the same volume | the sound asset has an attenuation with Spatialization off, or the sea's Shore Attenuation was edited to not attenuate |
| Waves sound comes from a wrong place | the Shoreline spline is not on the waterline |
| No shore foam | the material is not Single Layer Water (no `SceneDepthWithoutWater`), or FoamDepth too small |
