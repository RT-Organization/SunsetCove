# Engine Setup Guide — Day/Night Cycle

**Project:** Sunset Cove  |  **Engine:** Unreal Engine 5.8  |  **System:** `ADayNightManager`
**Revision 2.** **Audience:** level designers, lighting artists, anyone placing the system in a map.
**Companion document:** `Architecture_Doc_DayNightCycle.md` (how it works inside, full API, sun/moon visuals).

> **What changed in this revision**
> - All optional curves are documented with starting keys and the interpolation to use (section 5).
> - Auto exposure: recommendation, step-by-step setup, and the new exposure curves (sections 3 and 5).
> - The "gray world" bug is explained and fixed in code; recovery steps in section 8.
> - New editor buttons on the manager: **Refresh Lighting** and **Reset Post Process Overrides**.
> - Moon: new default intensity (0.05) and the settings that make it look like a moon (sections 2 and 9).
> - `SkyAtmosphereActor` is a plain *Actor* reference (your compile fix). Pick the Sky Atmosphere actor as before.

---

## 0. What you are setting up

The Day/Night Cycle is one actor, `BP_DayNightManager`, placed once in a gameplay level. It:

1. keeps the game clock (07:00 at the start of the day, then running through sunset into the night);
2. moves and dims the **Sun** and **Moon**, and drives the **Sky Light**, the **Sky Atmosphere** and a **Post Process Volume** from that clock;
3. announces clock events (hour changed, sunset started, night started, ...) so other systems can react.

It never touches the barrier, the visitors or the economy. Those systems listen to it.

### Files

```
Source/SunsetCove/
├── Public/Environment/DayNightCycle/
│   ├── DayNightManager.h                  ADayNightManager (the actor)
│   ├── DayNightCycleSubsystem.h           lookup: "where is the manager?"
│   └── Data/
│       ├── DayNightTypes.h                EDayNightPhase, EDayNightExposureMode
│       └── DayNightLightingProfile.h      UDayNightLightingProfile (data asset with all the curves)
└── Private/Environment/DayNightCycle/
    ├── DayNightManager.cpp
    └── DayNightCycleSubsystem.cpp

Content/Environment/DayNightCycle/
├── BP_DayNightManager
├── DA_DayNightLightingProfile_Default
└── Curves/   Curve_SunRotation, Curve_SunIntensity, Curve_SunColor, ...
```

---

## 1. Build the C++

1. Close the editor, copy the `Source/` files into the repository (same paths).
2. Right-click `SunsetCove.uproject` → **Generate project files** (new files need it).
3. Build the **SunsetCoveEditor** target (Development Editor), then open the editor.

No `Build.cs` change is needed.

> **Note on the Sky Atmosphere reference.** In the first version of the code I included `Engine/SkyAtmosphere.h`, which is the wrong header: the
> `ASkyAtmosphere` class is declared in `Components/SkyAtmosphereComponent.h` (this is what Epic's API reference shows for 5.1 and 5.4; I could not
> check 5.8's page). Your fix, a generic `AActor*`, is correct and works fine: the manager finds the Sky Atmosphere component on whatever actor you
> assign, and the log warns at start if that actor has none. You can keep it as it is.

---

## 2. Prepare the level lighting

The project renders with **Lumen** and **Virtual Shadow Maps**, and has `r.AllowStaticLighting=False`: **every light in the cove must be Movable.**

Open the gameplay level. Delete leftover default lights, sky or fog, then add the actors below and rename them clearly (`DL_Sun`, `DL_Moon`, `SkyAtmosphere`, `SkyLight`, `PPV_DayNight`).

### 2.1 Actors

| Actor | Required? | Purpose |
|---|---|---|
| Directional Light — **Sun** | yes | main light |
| Directional Light — **Moon** | recommended | light of the night |
| Sky Atmosphere | recommended | sky and sunset colors |
| Sky Light | recommended | ambient light that follows the sky |
| Post Process Volume | recommended | exposure, saturation, warmth |
| Exponential Height Fog / Volumetric Cloud | optional | scenery; they react to the lights on their own |

### 2.2 Settings per actor

**`DL_Sun`**

| Setting | Value |
|---|---|
| Mobility | **Movable** |
| Atmosphere and Cloud → **Atmosphere Sun Light** | ✔ on |
| Atmosphere and Cloud → **Atmosphere Sun Light Index** | **0** |
| Cast Shadows | ✔ on |
| Cast Cloud Shadows | ✔ on, if you use Volumetric Clouds |
| Rotation | leave it: the manager overwrites it every frame |

**`DL_Moon`**

| Setting | Value |
|---|---|
| Mobility | **Movable** |
| Atmosphere Sun Light | ✔ on |
| Atmosphere Sun Light Index | **1** (Unreal supports two atmosphere lights: "a sun and a moon") |
| **Atmosphere Sun Disk Color Scale** | **start at 0.2** (RGB). This multiplies the brightness of the moon's visible disk. **0 hides the disk entirely** (use that if you add a moon mesh, see section 9) |
| Light Color | slightly blue, for example R 0.6 · G 0.7 · B 1.0 |
| Cast Shadows | your call: on gives moon shadows on the sand, off saves one shadow-casting light at night |

**`SkyAtmosphere`** — defaults are fine. Note the **Mie Scattering Scale** value (default about 0.004) if you plan to use the sunset-glow curve.

**`SkyLight`**

| Setting | Value |
|---|---|
| Mobility | **Movable** |
| **Real Time Capture** | ✔ on |
| Source Type | Captured Scene |

**`PPV_DayNight`**

| Setting | Value |
|---|---|
| **Infinite Extent (Unbound)** | ✔ on |
| Everything else | leave it at default. Do **not** tick overrides for Exposure, Saturation or Temp by hand: the manager writes them, and it warns at start if the volume is not Unbound |

---

## 3. Auto exposure: should you enable it?

**Recommendation: yes, enable it, but constrain it with the manager's exposure range.**

Why it is worth it for Sunset Cove:

- **The cave.** The player's base is a dark cave next to a bright beach. Eye adaptation (bright when you step outside, dark when you walk in) is exactly the effect that sells that transition, and a fixed exposure cannot do it.
- **The sunset.** Looking toward the sun versus away from it changes the scene's brightness a lot; adaptation smooths that out.
- **Less hand-balancing.** With a fixed exposure every lighting change must be matched by hand. With auto exposure the brightness of the picture is anchored, and you art-direct with a compensation curve.

What can go wrong, and how this setup handles it:

| Risk | Handling |
|---|---|
| **The night gets bright.** Auto exposure brings any dark scene up toward mid-gray, so a night would look like a day | The **Min EV100 offset curve** limits how far the eye may brighten. It is the main "how dark is the night" control |
| Exposure "pumps" when the player looks around | slow the adaptation speeds, keep a narrow range, use the Histogram method |
| Tuning takes longer | calibrate **one** number (`DayExposureEV100`), the curves are offsets from it |

If the team prefers a locked, graphic look with no adaptation at all, keep auto exposure off. The new range curves then do nothing, and you rely on the light curves plus `ExposureBiasCurve`.

### 3.1 Enable it (step by step)

1. **Project Settings → Engine → Rendering → Default Settings.**
2. Tick **Auto Exposure**. (This is `r.DefaultFeature.AutoExposure`, currently `False` in `DefaultEngine.ini`.)
3. Leave **Extend default luminance range in Auto Exposure settings** ticked: the project already has it on. It makes the volume's Min/Max values mean **EV100**, which is what the manager's curves assume. The manager warns at start if it is off.
4. Select `PPV_DayNight`. Under **Lens → Exposure**, set the metering mode to **Auto Exposure Histogram**. (Or leave it and set **Exposure Mode = Histogram** in the lighting profile, so the manager sets it.)
5. Leave **Speed Up / Speed Down** at their defaults to begin with. If the picture reacts too nervously, lower them a little.
6. In the editor viewport open the **Perspective menu → Exposure** and tick **Game Settings**. Without this the editor viewport uses its own exposure and ignores the volume.
7. Make sure the volume is the **only** source of exposure: no other unbound volume with exposure overrides, and no post-process settings on the character's camera (a camera's own settings win over volumes).

### 3.2 Calibrate `DayExposureEV100`

The range curves are offsets from one number: the EV100 at which a sunny noon looks right.

1. In the profile, leave **Exposure Min/Max Offset Curve** empty for now.
2. In the editor set the manager's **Current Time In Hours** to about 13.
3. On `PPV_DayNight`, tick the overrides for **Min EV100** and **Max EV100** and give them the **same value** (Epic's docs: eye adaptation is disabled when Min = Max, so this pins the exposure).
4. Change that value until the sand and rocks look right at noon. **Lower EV100 = brighter picture, higher = darker.** A first guess for a 10 lux sun is around 1.
5. Write that number into the profile's **Day Exposure EV100**.
6. On the volume, untick the Min/Max overrides (or press **Reset Post Process Overrides** on the manager). Then assign the offset curves (section 5).

> Epic's issue tracker has an open entry about inconsistent clamp limits on these fields (UE-218856, targeted at 5.8). If the editor refuses a value you type,
> that is probably the reason, not your setup.

---

## 4. Create the Lighting Profile

The look of the day lives in a **data asset** made of curves.

1. Create `Content/Environment/DayNightCycle/`.
2. Right-click → **Miscellaneous → Data Asset** → **DayNightLightingProfile** → `DA_DayNightLightingProfile_Default`.
3. For each curve: right-click → **Miscellaneous → Curve** → **CurveFloat** (or **CurveLinearColor** for the sun color). Put them in `Content/Environment/DayNightCycle/Curves/`.
4. Open the profile and drag each curve into its slot. Create `BP_DayNightManager` and bind it (sections 6 and 7).

### 4.1 Rules that apply to every curve

- **X axis = hour, 0 to 24** (13.5 = 13:30). Fit the view to 0–24 (**View → Fit**).
- **A slot left empty means "not driven".** So does a curve **with no keys**: the manager ignores it and writes a warning to `LogSunsetCove`. (An empty curve would read 0 at every hour; see section 8 for what that did.)
- **Add keys one curve at a time** and look at the viewport after each (press **Refresh Lighting**, section 8.2).
- **Loop the day.** The value at hour **24 must equal the value at hour 0** (the sun rotation: plus 360). Make the night a flat plateau at both ends (for example 0–5.4 and 22–24 the same value), so the curve has no kink at midnight.
- **Add a key:** right-click on the curve → **Add Key**, then type its **Time** and **Value** in the fields of the Curve Editor.
- **Set interpolation:** select the key → right-click (or the toolbar) → **Interpolation** (Linear / Cubic / Constant) and **Tangent** (Auto / User / Break).
- **Never use Constant (stepped).** It makes the light jump.
- After placing cubic keys, **look for overshoot**: a curve that dips below its lowest key or rises above its highest between two keys. Fix it by switching that key to Linear, or by flattening its tangent.

### 4.2 Interpolation at a glance

| Curve | Interpolation | Why |
|---|---|---|
| `SunRotationCurve` | **Linear** on every key | constant angular speed. Cubic can overshoot on values past 360 and make the sun slow down or move backward |
| `SunColorCurve` | **Linear** | cubic overshoot gives negative or oversaturated colors, which show as color flicker |
| `SunIntensityCurve` | **Cubic, Auto tangents** | smooth ramps, flat plateaus. Check there is no dip below 0 |
| `MoonIntensityCurve` | **Cubic, Auto** | the moon fades in and out gently |
| `SkyLightIntensityCurve` | **Cubic, Auto** | smooth ambient change |
| `MieScatteringScaleCurve` | **Cubic, Auto** | one smooth bump around sunset |
| `ExposureMinOffsetCurve`, `ExposureMaxOffsetCurve` | **Cubic, Auto** | smooth range; the eye adaptation smooths it further |
| `ExposureBiasCurve` | **Cubic, Auto** | smooth |
| `SaturationCurve` | **Cubic, Auto** | smooth |
| `WhiteTemperatureCurve` | **Cubic, Auto** | smooth Kelvin shifts, no visible steps |

---

## 5. The curves, one by one

All numbers are a **starting point for art**, tuned for sunrise about 06:30 and sunset about 20:00. Format: `hour → value`.

### 5.1 `SunRotationCurve` (float) — Linear

Position of the sun on its arc, in degrees: 0 = rising, 90 = highest, 180 = setting, 270 = lowest.

`0 → 248.6` · `6.5 → 360` · `13.25 → 450` · `20 → 540` · `24 → 608.6`

Hour 24 = hour 0 + 360, so the day loops. **Golden hour tip:** to make the sunset last, add keys between 19:00 and 20:30 where the curve rises more slowly (fewer degrees per hour). Keep it always rising: a falling stretch means the sun moves backward.

### 5.2 `SunIntensityCurve` (float, multiplier 0–1) — Cubic, Auto

`0 → 0` · `5.4 → 0` · `6.5 → 0.15` · `7 → 0.35` · `9 → 0.8` · `11 → 1` · `15.5 → 1` · `18 → 0.8` · `19 → 0.45` · `20 → 0.2` · `20.5 → 0.06` · `21.1 → 0` · `24 → 0`

Do **not** drop it to 0 at the horizon: the sky keeps glowing after the sun sets, and that glow is lit by the sun light. Fade out slowly until about 21:00. (The manager hides a light whose multiplier is 0, so it costs no shadow rendering.)

### 5.3 `SunColorCurve` (linear color) — Linear

An extra tint for the sun. The atmosphere already reddens the sun near the horizon, so keep this **subtle**; if the sunset looks too red, raise Green and Blue by 0.1. Keys exist on the **R, G and B channels** at the same times (the manager reads them as a color; A stays 1).

| Hour | 0 | 6.5 | 8 | 10 | 12 | 16 | 18 | 19 | 20 | 21 | 24 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| R | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 0.9 | 1 |
| G | 0.40 | 0.55 | 0.82 | 0.95 | 0.97 | 0.92 | 0.78 | 0.62 | 0.45 | 0.38 | 0.40 |
| B | 0.20 | 0.30 | 0.62 | 0.88 | 0.92 | 0.80 | 0.55 | 0.35 | 0.25 | 0.25 | 0.20 |

### 5.4 `MoonIntensityCurve` (float, multiplier) — Cubic, Auto

`0 → 1` · `4.5 → 1` · `6.5 → 0` · `20 → 0` · `22 → 1` · `24 → 1`

Pair it with **Moon Max Intensity = 0.05** (profile). The moon is an atmosphere light: the sky is lit by it like it is lit by the sun, so a high value makes the night sky look like a dim day. Raise it only until the sand is readable.

### 5.5 `SkyLightIntensityCurve` (float, multiplier) — Cubic, Auto

The ambient light. Never let the night reach 0.

`0 → 0.08` · `5.4 → 0.08` · `6.5 → 0.4` · `8 → 1` · `17 → 1` · `19.5 → 0.5` · `21 → 0.1` · `22 → 0.08` · `24 → 0.08`

### 5.6 `MieScatteringScaleCurve` (float, absolute) — Cubic, Auto

The glow and haze around the sun. This is an **absolute** value, not a multiplier: read the **Mie Scattering Scale** on your SkyAtmosphere (default about 0.004) and scale every number below by *(your value ÷ 0.004)*.

`0 → 0.004` · `6 → 0.004` · `6.5 → 0.006` · `8 → 0.004` · `17 → 0.004` · `19 → 0.008` · `20 → 0.009` · `21 → 0.006` · `22 → 0.004` · `24 → 0.004`

The manager only updates the atmosphere when the value moves by more than 1 %, because each change rebuilds atmosphere data. If you see stutter while scrubbing time, remove this curve.

### 5.7 `ExposureMinOffsetCurve` and `ExposureMaxOffsetCurve` (float, in stops) — Cubic, Auto

Offsets from **Day Exposure EV100** (section 3.2). Only used with auto exposure enabled.

- **Min** = how far the eye may *brighten* a dark scene. A **lower** value allows a brighter picture. **This is your main "how dark is the night" control.**
- **Max** = how far the eye may *darken* a bright scene.
- The manager keeps Max ≥ Min. If they ever meet, adaptation is off at that hour.

**Min offset:** `0 → −3` · `5.4 → −3` · `6.5 → −2.5` · `8 → −2` · `18 → −2` · `20 → −2.5` · `21.5 → −3` · `24 → −3`

**Max offset:** `0 → −1` · `5.4 → −1` · `6.5 → 0` · `8 → +1` · `18 → +1` · `19.5 → +0.5` · `21.5 → −1` · `24 → −1`

*Reading it:* by day the eye may brighten up to 2 stops (stepping into the cave) and darken 1 stop. At night it may brighten up to 3 stops and is held from getting darker than 1 stop below the day setting. **Want a darker night? Raise the night Min offset toward −1.5. A brighter night? Lower it toward −4.**

*Example with a calibration of 1.0:* the day Min is 1.0 − 2 = −1 and the day Max is 1.0 + 1 = 2.

### 5.8 `ExposureBiasCurve` (float, EV) — Cubic, Auto

Exposure compensation added on top: positive = brighter. **It replaces the compensation of the volume and of the project.** `DefaultEngine.ini` has `r.DefaultFeature.AutoExposure.Bias=1.0`, so if the picture gets about one stop darker than before you assigned this curve, that is why: author the day around **1.0**.

`0 → 0.5` · `5.4 → 0.5` · `6.5 → 0.8` · `8 → 1.0` · `17 → 1.0` · `19.5 → 1.3` · `21 → 0.8` · `22.5 → 0.5` · `24 → 0.5`

The bump at sunset keeps the glow bright; it also works with auto exposure off.

### 5.9 `SaturationCurve` (float, 1 = unchanged) — Cubic, Auto

`0 → 0.85` · `5.4 → 0.85` · `7 → 1.0` · `17 → 1.0` · `19.5 → 1.1` · `21.5 → 0.85` · `24 → 0.85`

> **Never leave this curve empty or without keys.** Saturation 0 is a black-and-white world. The manager now ignores a key-less curve, but this was the cause of the gray world (section 8).

### 5.10 `WhiteTemperatureCurve` (float, Kelvin) — Cubic, Auto

6500 = neutral. In Unreal's white balance a **higher value looks warmer**, so sunrise and sunset go above 6500 and the moonlit night goes below. The manager keeps the value between 1500 and 15000. Check the direction in the viewport the first time: if sunset looks blue, mirror the numbers around 6500.

`0 → 5800` · `5 → 5800` · `6.5 → 7500` · `8.5 → 6600` · `12 → 6500` · `17 → 6800` · `19.5 → 8000` · `21 → 6800` · `22.5 → 5800` · `24 → 5800`

### 5.11 Other fields of the profile

| Field | Starting value | Note |
|---|---|---|
| Sun Max Intensity | 10 | set it to the sun intensity the level has today |
| **Moon Max Intensity** | **0.05** | was 0.5 in the first version: too bright. Tune by eye |
| Sky Light Max Intensity | 1 | |
| Exposure Mode | Leave As Is, or Histogram | see section 3.1 |
| Day Exposure EV100 | 1 | calibrate (section 3.2) |

---

## 6. Create and place the manager

1. **Add → Blueprint Class** → search **DayNightManager** → `BP_DayNightManager` in `Content/Environment/DayNightCycle/`.
2. Drag it into the gameplay level. **One per level.**
3. Select it from the Outliner (it has no mesh).

---

## 7. Bind the references and adjust the variables

In the **Details** panel everything is under the **DayNightCycle** category.

**References**

| Field | Assign | Required? |
|---|---|---|
| Sun Directional Light | `DL_Sun` | **yes** |
| Moon Directional Light | `DL_Moon` | recommended |
| Sky Light Actor | `SkyLight` | recommended |
| Sky Atmosphere Actor | `SkyAtmosphere` | optional (only used by the Mie curve). It is a generic actor field: pick the Sky Atmosphere actor |
| Post Process Volume | `PPV_DayNight` | optional |
| Lighting Profile (*Lighting* section) | `DA_DayNightLightingProfile_Default` | recommended |

Sun and Moon must be **two different actors**. Assign the volume **last**, after the profile is filled in (see the rollout order in 8.3).

**Buttons** (*Lighting* section of the Details panel, editor only)

| Button | Does |
|---|---|
| **Refresh Lighting** | reapplies the light for the current time. **Press it after editing any curve asset** (curve assets do not notify the manager) |
| **Reset Post Process Overrides** | switches off every post process override the manager writes on the volume (exposure method, exposure range, compensation, saturation, white balance) |

**Time**

| Variable | Default | What it does |
|---|---|---|
| Real World Day Duration Minutes | 12 | real minutes for *Start Hour → Main Span End Hour* (07:00 → 19:00). GDD test range 10–15 |
| Start Hour | 7 | morning start, and where *Reset Day* goes back to |
| Main Span End Hour | 19 | end of the span the duration refers to. Clock speed only |
| Time Speed Multiplier | 1 | extra speed, for tests (try 10) |
| Current Time In Hours | 7 | the clock, 13.5 = 13:30. **Drag it in the editor to preview the light** |
| Is Time Paused | on | on = frozen. A gameplay system calls *Start Time* |
| Snap To Start Hour On Begin Play | on | the game always starts at *Start Hour*; a preview time never leaks into the game |

**Phases and events**

| Variable | Default | What it does |
|---|---|---|
| Sunset Start Hour | 19 | Sunset phase and *On Sunset Started* begin here |
| Night Start Hour | 21 | Night phase and *On Night Started* begin here (proposal, not in the GDD) |
| Time Reached Targets | empty | hours that fire *On Time Reached*; gameplay systems add their own |

**Sun Path**

| Variable | Default | What it does |
|---|---|---|
| Sun Path Azimuth Degrees | 0 | world yaw the sun **rises** from. **Rotate it until the sun sets over the sea** |
| Sun Path Inclination Degrees | 20 | tilts the arc; noon height = 90 − this |
| Moon Orbit Offset Degrees | 180 | 180 = moon always opposite the sun |
| Fallback Sunrise / Sunset Hour | 6.5 / 20 | only used while there is no *Sun Rotation Curve* |

---

## 8. The gray world: why it happened, how to fix it

### 8.1 Why

You connected the Post Process Volume and the whole world went gray. **The cause is almost certainly a curve that is assigned in the profile but has no keys** (probably the Saturation curve).

- A curve asset with no keys **reads 0 at every hour**.
- In the first version of the code an *assigned* curve was always treated as "driven". So it wrote **Saturation = 0** into the volume, and saturation 0 is a black-and-white world. (An empty White Temperature curve would also have written 0 Kelvin, which is an extreme tint; an empty Sun Intensity curve would have switched the sun off.)
- It showed up **the moment you connected the volume** because in the editor the manager applies the light on every Details-panel change, so the broken values were written into the volume right away.
- The values were written into the volume's own settings, and a volume in the level is **saved with them**. So even after the curve is fixed, the volume keeps the gray override until you clear it.

It was not that the "post-process curves were not configured": an **unassigned** slot is fine and writes nothing. The bug was the *assigned but empty* case, which the code did not guard against. It does now: any curve with no keys is ignored, and a warning names it in the Output Log (filter `LogSunsetCove`).

### 8.2 Fix it now (in this order)

1. Update the code (`DayNightManager.h/.cpp`, `DayNightLightingProfile.h`, `DayNightTypes.h`) and rebuild.
2. Select `BP_DayNightManager` → Details → press **Reset Post Process Overrides**. This clears the saturation, white balance, exposure overrides the manager had written on the volume.
3. Save the level.
4. Open the profile and either **add keys** to each curve you assigned (section 5), or **clear the slot**.
5. Press **Refresh Lighting**. The world should be in color again.

To confirm the diagnosis before step 2: select the volume and look at **Color Grading → Global → Saturation**. If its override is ticked and the value is 0, this was it.

### 8.3 How to roll it out safely

1. Assign **no** Post Process Volume. Fill the light curves first and check the sun, moon and sky.
2. Add **one post process curve at a time**, in this order: Saturation, White Temperature, Exposure Bias, then the exposure range. After each: press **Refresh Lighting** and scrub **Current Time In Hours** from 0 to 24.
3. Assign the Post Process Volume.
4. After any problem, **Reset Post Process Overrides** gives you a clean volume.

### 8.4 If it is still gray

- Another Post Process Volume (or the character camera) has its own settings with a higher priority.
- The viewport is not showing game settings (Perspective menu → **Show → Post Processing** must be on).
- The Saturation curve has keys but the **values** are 0, or the keys are at the wrong **hours** (for example 0 to 1 instead of 0 to 24).

---

## 9. The moon is too bright: quick fixes you can do today

No new assets needed. The moon looked "sun-like" for three reasons, each with its own control:

| Cause | Fix |
|---|---|
| Moon Max Intensity was **0.5** (5 % of the sun). The moon is an atmosphere light, so at that strength the sky is lit like a dim day | set **0.05** (new default). Raise it only until the sand is readable |
| The moon's visible **disk** is drawn like a sun disk | on `DL_Moon` set **Atmosphere Sun Disk Color Scale** to about 0.2. **0 removes the disk** |
| With auto exposure, a dark night is brightened | limit it with the Min offset curve (section 5.7) |

Also try: a blue Light Color, and a smaller **Light Source Angle** on the moon if the disk looks like a hot spot.

For a *distinct* sun and moon made from real assets (textures, meshes, star domes), see the section *Sun and Moon visuals* in `Architecture_Doc_DayNightCycle.md`.

---

## 10. Test it

**Editor preview (no Play).** Drag **Current Time In Hours** from 0 to 24 and check 07:00, 13:00, 19:00, 20:00, 21:00 and 02:00. After editing a curve press **Refresh Lighting**. Put the time back to 7 (or leave *Snap To Start Hour On Begin Play* on).

**Play In Editor.** The day starts **paused at 07:00**. To run the clock: press **F8**, select the manager and untick **Is Time Paused**; or in the level Blueprint: *Event BeginPlay → Get Day Night Manager → Start Time* (test only). Raise **Time Speed Multiplier** to see the evening quickly. Filter the Output Log by **`LogSunsetCove`**.

**Blueprint hook-up.** *Get Day Night Manager* → **Is Valid** → drag from the node → **Assign On Hour Changed** (or *On Sunset Started*, *On Night Started*, *On Time Reached*, *On Day Reset*). *Get Formatted Time* returns `"HH:MM"`. *Register Time Target* (for example 18.0) then react in *On Time Reached*.

**Eye adaptation after a jump.** *Reset Day* and *Set Time Of Day* jump the clock; the exposure then eases to the new level at the volume's adaptation speed. If that looks bad (waking up in a blown-out morning), cover the jump with a fade.

---

## 11. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Whole world gray | a curve assigned with no keys wrote 0 saturation | section 8 |
| Sun does not move, log says *SunDirectionalLight is not assigned* | reference empty | assign `DL_Sun` |
| Clock never advances | **Is Time Paused** is on (default) | something must call *Start Time*; for tests untick it |
| Sun moves but the sky has no sunset colors | light not marked **Atmosphere Sun Light**, or Sun and Moon share an index | Sun index 0, Moon index 1 |
| Sky pure black at night, you wanted twilight | Sun curve reaches 0 too early | keep it above 0 until about 21:00 |
| Night looks like day | auto exposure brightens it, or Moon Max Intensity too high | raise the night Min offset curve; set Moon Max Intensity to 0.05 |
| Night is pitch black | Min offset too high, or Moon/Sky Light too low | lower the night Min offset (toward −4) |
| Exposure does nothing | auto exposure is off in the project, or the viewport is not on **Game Settings** | section 3.1, steps 2 and 6 |
| Exposure behaves oddly in the dark | the volume's **Histogram Min EV100** is too high for the scene | lower Histogram Min EV100 on the volume (community reports use values around −16), or use the Basic method |
| Log: *… exposure range curves are in EV100, but 'Extend default luminance range' is off* | project setting | tick it (section 3.1, step 3) |
| Log: *… is assigned but has no keys* | empty curve | add keys or clear the slot |
| Log: *… is not Infinite Extent (Unbound)* | volume has a limited box | tick Infinite Extent |
| Log: *… has no Sky Atmosphere component* | wrong actor in *Sky Atmosphere Actor* | pick the Sky Atmosphere actor |
| Log: *… is not Movable* | light Mobility is Static/Stationary | set **Movable** |
| Ambient light does not follow the time | Sky Light **Real Time Capture** is off | turn it on |
| Sun sets over the land | level orientation | change *Sun Path Azimuth Degrees* |
| Curve edited, viewport unchanged | curve assets do not notify the manager | press **Refresh Lighting** |
| Stutter while scrubbing time | Mie curve rebuilding the atmosphere | remove or flatten the Mie curve |
| Game starts at the wrong hour | *Snap To Start Hour On Begin Play* is off and a preview time was left | turn it on |
| Log: *… is already the day/night manager* | two managers in the level | delete one |
