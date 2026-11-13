# Sea System — Architecture

**Project:** Sunset Cove | **Engine:** Unreal Engine 5.8 | **Code:** `Source/SunsetCove/{Public,Private}/Environment/Water/`
**Status (2026-11-12):** base built. Drives the waves, tide, shoreline queries and the sound of the waves.
Material, mesh, foam textures and effects are the next steps; see `Sea_Setup_Guide.md` for how to make them.
**Audience:** programmers and the technical artist. The editor steps live in the setup guide.

---

## 1. What we want

The sea lies in front of the cove: a landscape made by the artist (sand beach, high rocks all around). The water has to:

| Goal | How the system answers it |
|---|---|
| look like a real sea, not a flat plane | a sum of **Gerstner waves** moves the surface (vertex displacement in the material) |
| move like the sea: swell coming to the beach | waves have a **direction**: point the long swell at the beach |
| "breathe": go slowly up and down | a **tide**: the whole sea rises and sinks (amplitude and period are tunable) |
| white foam where the waves hit the sand | **shore foam** in the material: the shallower the water, the more foam, animated in bands that run up the beach |
| foam on the top of big waves | **crest foam**: the higher a point of the wave, the more foam |
| sound of the waves along a long beach | a looping waves sound on an emitter that **slides along the shoreline** to the point closest to the player |
| the rest of the game knows where the water is | **queries**: `GetWaterHeightAt`, `IsUnderwater`, `GetDistanceToShore` — computed with the **same formula** as the material |

The rule that keeps it simple: **C++ is the single source of truth.** It computes the wave parameters and the time, and hands
them to the material every frame. The material never invents its own waves, so what you see and what gameplay measures match.

---

## 2. The pieces

```
                      USunsetCoveSeaSubsystem (World Subsystem)   ◄── GetSea(this) from anywhere
                                 │ (one weak pointer)
                                 ▼
 ASunsetCoveSea  (one per level, placed in front of the beach; origin = still water level)
 ├─ SeaRoot
 ├─ SurfaceMesh ──► dynamic instance of M_Sea ◄── SeaTime, SeaTideOffset, SeaLevel, SeaWaveA0..3, SeaWaveB0..3 (every frame)
 │                                                 (the same values go to MPC_Sea if set: wet sand, foam on rocks...)
 ├─ Shoreline (spline drawn on the waterline)
 └─ ShoreAudioAnchor ──► looping cue Audio.Cue.Ambient.ShoreWaves (framework audio, Ambient slider, attenuation)

 Consumers (they only know the subsystem + the public queries):
   ASunsetCoveWindAmbience   GetDistanceToShore → wind louder by the sea          (AudioSystem/Ambience/)
   future: floating props    GetWaterHeightAt  → buoyancy
   future: swimming / wading IsUnderwater, GetWaterHeightAt
   future: visitors          GetDistanceToShore → "go for a swim" spots
```

| File | Role |
|---|---|
| `Environment/Water/SunsetCoveWaveTypes.h/.cpp` | `FSunsetCoveGerstnerWave` (the editable wave), the math (`PackWaves`, `GetDisplacement`, `GetHeightAt`) and the material contract |
| `Environment/Water/SunsetCoveSea.h/.cpp` | `ASunsetCoveSea`: components, tick (time, tide, parameters), queries, shore audio |
| `Environment/Water/SunsetCoveSeaSubsystem.h/.cpp` | the lookup (same pattern as `UDayNightCycleSubsystem`) |
| `AudioSystem/Ambience/SunsetCoveWindAmbience.h/.cpp` | the wind (it uses the sea, the sea does not know it) |

---

## 3. The wave model (the contract between C++ and the material)

A Gerstner wave moves each surface point on a small circle (up/down **and** towards the crest), which gives sharp crests and
wide troughs — the look of a real swell. For one wave (direction `D`, wavelength `L`, amplitude `A`, steepness `S`, `N` waves):

```
k     = 2π / L                      wave number
ω     = sqrt(g·k) · SpeedScale      deep water speed (g = 980 cm/s²): long waves travel faster
Q     = S / (k·A·N)                 lateral motion; the sum keeps the surface from folding over
f     = k · dot(D, P.xy) − ω·t
offset.xy = Q·A · D · cos(f)
offset.z  = A · sin(f)
surface = still level (actor Z) + Σ offset + tide
```

To keep the material cheap, C++ packs each wave into two float4:

| Parameter | Components | Meaning |
|---|---|---|
| `SeaWaveA{i}` (i = 0..3) | `(D.x, D.y, k, ω)` | direction, wave number, angular speed |
| `SeaWaveB{i}` | `(A, Q·A, 0, 0)` | height, lateral motion |
| `SeaTime` | scalar, seconds | the sea clock (game time × `WaveTimeScale`; pauses with the game) |
| `SeaTideOffset` | scalar, cm | the tide right now |
| `SeaLevel` | scalar, world cm | actor Z + tide (for wet sand / foam on other materials) |

Unused waves are sent with zero amplitude, so the material always sums four. Only the first 4 waves of the actor are used.

**Height queries.** Because Gerstner moves points sideways, the water right above a world point comes from a slightly different
rest point. `GetHeightAt` finds it with 3 fixed-point steps (exact enough for a gentle sea, a few microseconds per call).

**Normals.** The material computes them from the same sum (formula in the setup guide), then adds small detail normal maps for
ripples. The waves are for shape and motion; the normal maps give the glitter.

---

## 4. Rendering plan (to be built, see the setup guide)

| Layer | Technique | Why |
|---|---|---|
| shading model | **Single Layer Water** (UE's water shading: absorption, scattering, refraction, Lumen reflections) | physically based water colour by depth: turquoise in the shallows, deep blue far away |
| shape | World Position Offset from the Gerstner sum (Custom HLSL node, 4 waves) + tide | matches C++ exactly |
| mesh | `SM_SeaPlane`: a 100 × 100 cm plane with 256 × 256 subdivisions, scaled by `SurfaceSize` | WPO needs vertices; ~1.5 m spacing over 400 m is enough for a swell |
| far sea | a second, flat, big plane to the horizon (no WPO, same material with waves off) | cheap horizon |
| small detail | two tiling normal maps panning in different directions, blended with the wave normal | ripples and sun glitter |
| colour by depth | handled by Single Layer Water absorption + `SceneDepthWithoutWater` for shallow tint | clear shallows over the sand |
| **shore foam** | `WaterDepth = SceneDepthWithoutWater − PixelDepth`; foam where depth < FoamDepth, cut into bands that move towards the beach over time, masked by a tiling foam texture | the white line where the waves hit the landscape, no extra geometry |
| crest foam | wave height (from the WPO Z, passed by a vertex interpolator) above a threshold × foam texture | whitecaps on big waves |
| wet sand (optional) | landscape material reads `SeaLevel` from `MPC_Sea`: darker sand below SeaLevel + a few cm | the beach is wet where the tide was |
| spray (later) | Niagara system along the shoreline spline, spawning where waves break | splashes |

Why not the engine's **Water plugin** (`WaterBodyOcean`)? It is powerful (waves, shore foam, buoyancy, landscape carving) but it
is a large experimental system that owns the landscape integration and is harder to bend to a small stylised-realistic cove and to
keep in sync with our own gameplay queries. This system stays small, readable and fully ours. If one day the Water plugin is
preferred, `ASunsetCoveSea` can keep its role (queries, shoreline, audio) and read the heights from the plugin instead of its own
waves; nothing else in the game would change.

---

## 5. The shoreline

A spline component on the sea actor, drawn by the level designer along the waterline of the beach (at the still water level).
It is not rendered. It answers:

- `GetClosestShorePoint(Location)` / `GetDistanceToShore(Location)` (horizontal distance);
- where the waves sound plays (section 6);
- later: where to spawn shore spray, which visitors walk to, where foam decals go.

It does **not** need to follow the rocks; only the part of the coast where waves break (the beach) matters.

---

## 6. Sound

| Sound | Where it plays | How |
|---|---|---|
| Waves on the beach (`Audio.Cue.Ambient.ShoreWaves` → `S_AmbGameplay_Waves_Loop`) | the sea actor, `ShoreAudioAnchor` | one looping cue attached to the anchor; every frame the anchor slides (smoothly) to the shoreline point closest to the listener, at sea level + 30 cm. The cue's **attenuation** makes it full near the water and faint inland; a wide *non-spatialized radius* makes it surround you near the water instead of coming from one point |
| Wind (`Audio.Cue.Ambient.Wind` → `S_Amb_Wind_Loop`) | `ASunsetCoveWindAmbience` placed in the level | 2D loop, volume = shore proximity × shelter (rays around the ear: high rocks close around you → quiet) |
| Main menu waves (`Audio.Cue.Ambient.MainMenu` → `S_AmbMainMenu_Waves_LoopA`) | `BP_GameMode_MainMenu` > Map Audio > Ambient Cue | framework ambient layer, 2D, crossfaded |
| Main menu music (`Audio.Cue.Music.MainMenu`) | `BP_GameMode_MainMenu` > Map Audio > Music Cue | already wired: fill the cue's Sound when the track exists |

All of them go through the CoreFramework audio subsystem, so the Ambient / Music sliders of the Options menu apply.

---

## 7. Roadmap

| Phase | Content | Who |
|---|---|---|
| 1 (done) | C++ base: waves, tide, parameters, queries, shoreline, waves sound, wind | code |
| 2 | `SM_SeaPlane`, `MPC_Sea`, `M_Sea` (WPO, normals, Single Layer Water colour), `MI_Sea`, placed and tuned on the real landscape | editor (setup guide sections 2-6) |
| 3 | shore foam bands + crest foam with real foam textures; wet sand on the landscape | editor |
| 4 | Niagara shore spray on the spline; underwater post process; a far horizon plane | editor + small code (spline sampling for spray) |
| 5 | gameplay: buoyancy for thrown props (`GetWaterHeightAt`), wading / swimming (`IsUnderwater`, framework swimming domain), tide linked to the Day/Night clock, weather presets (`SetWaves`) | code |

---

## 8. Limits and assumptions

- One sea per level (a second one is ignored with a warning).
- The surface is a plane: no rivers, no waterfalls. Waves are a deep-water model; real waves slow and steepen in the shallows —
  the shore foam hides that, a "shoaling" factor can be added later in the material (scale amplitude by depth).
- The tide moves the water, not the shoreline spline: with a tide of ±10 cm the waterline barely moves on a gentle beach. A big
  tide would need the spline to move or a second "high tide" line.
- Single player: everything is local, computed from game time.
