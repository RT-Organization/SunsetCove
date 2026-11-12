# Audio — Sound List (research sheet)

**Project:** Sunset Cove | **Engine:** Unreal Engine 5.8
**What this is:** the shopping list for a sound research session. It covers every sound the game needs: what to look for, the exact file name to save it under, the folder it goes in, and the cue tag that will play it.
**After the session:** import the files as described in [`AudioSystem_Setup.md`](AudioSystem_Setup.md) § 4, or put them in the source folder and ask Claude to wire them (cue rows, Sound Cues, anim notifies).
**Source of the list:** GDD § Art Design > Audio (waves, wind, birds, leaves; footsteps on sand; objects placed or moved; barrier; tools; distinct signals for urgent requests, crabs, seagull, disturbed visitors, inspections; a calmer soundscape at sunset; **no voice dialogue**), plus the props, tools, adversities and events of the GDD.

---

## 1. Rules for every file

| Topic | Rule |
|---|---|
| **Name** | `S_<Area>_<Name>` (`S_Sunbed_Open`). Several takes of the same sound: `_01`, `_02`… (`S_Footstep_Sand_01` … `_05`). Loops end in `_Loop` (`S_Amb_Waves_Loop`). English, no spaces. |
| **Format** | WAV, 48 kHz (44.1 is fine), 16 or 24 bit. No MP3 / OGG (Unreal re-compresses at cook time). |
| **Channels** | **Mono** for anything that happens at a place in the world (footsteps, props, tools, NPCs, crabs, seagull, impacts). **Stereo** for music, ambience beds and UI. |
| **Trim** | No silence before the sound starts (a late start feels like lag); a short natural tail is fine. |
| **Level** | Peak around −1 dBFS, no heavy limiting. Do not try to balance sounds against each other: the volume categories and cue rows do that. |
| **Loops** | Must loop without a click or a gap (crossfaded loop point). Beds: 30 s – 2 min, so the repetition is not noticed. |
| **Variations** | Anything heard often (footsteps, impacts, coins, crab clicks) needs **3–5 takes**. Claude wraps them into one Sound Cue `SCue_<Name>` that picks one at random. (`SCue_`, because `SC_` is taken by the Sound Classes.) |
| **No voices** | No dialogue. Wordless reactions (grunts, laughs, "hmpf", children's shouts) are fine and wanted. |
| **License** | Commercial use without attribution is easiest (CC0, Sonniss GDC bundles, your own recordings). CC-BY is fine if you note the credit. **Avoid CC-BY-NC** (non-commercial) and anything "for personal use". Write every file in the credits sheet (§ 9). |

**Where to search (suggestions):** Freesound.org (filter License = CC0), the free Sonniss GDC Game Audio bundles (royalty-free), Pixabay sound effects, Zapsplat (check its license tier), Epic's Fab marketplace. Or record it yourself: sand footsteps and a folding beach chair are easy with a phone.

---

## 2. Folders

```
Content/Audio/
├── Ambient/            S_Amb_*            beds and spot sounds of the cove (stereo beds, mono spots)
├── Music/              S_Music_*          stereo tracks
├── SFX/
│   ├── Player/         S_Footstep_*, S_Player_*        the protagonist's body
│   ├── Props/<Item>/   S_Sunbed_*, S_Umbrella_*       carried objects
│   ├── Tools/<Item>/   S_Bucket_*, S_Spray_*, S_Toy_*, S_GarbageBag_*
│   ├── World/          S_Barrier_*, S_Bin_*, S_Cave_*  fixed objects of the level
│   ├── Visitors/       S_Visitor_*, S_Child_*          NPC reactions (wordless)
│   ├── Adversities/    S_Crab_*, S_Seagull_*, S_Wind_*
│   ├── Events/         S_Police_*, S_Boss_*, S_Bodyguard_*
│   └── Economy/        S_Coins_*, S_Fine_*
└── UI/                 S_UI_*             (Beach/ holds the current procedural UI sounds)
```

**Source files** (the WAVs as downloaded, before import) go in the repository folder `Audio (sources)/`, with the same sub-folders. It sits next to `Export (blender)/`, outside `Content/`, so the originals are kept but not cooked. Unreal remembers the source path for re-import.

---

## 3. Ambience (category Ambient) — MVP

The bed of the cove. It plays on every map through `Audio.Cue.Ambient.Beach` (already wired: just fill the Sound).

| # | File name | Folder | Cue tag | What to look for (search words) | Type | Hook today |
|---|---|---|---|---|---|---|
| A1 | `S_Amb_Waves_Loop` | Ambient | `Audio.Cue.Ambient.Beach` | gentle waves on a small sandy beach, soft shore wash, no surf crashing ("calm beach waves", "shore lapping") | stereo loop 60–120 s | **yes** |
| A2 | `S_Amb_Wind_Loop` | Ambient | `Audio.Cue.Ambient.Wind` | light coastal breeze, leaves rustling ("sea breeze", "wind in trees light") | stereo loop 60 s | needs a layer (see note) |
| A3 | `S_Amb_Seagulls_Distant_01`…`_04` | Ambient | `Audio.Cue.Ambient.Seagulls` | distant seagull calls, single calls not a flock | mono one-shots, 2–5 s | needs a random emitter |
| A4 | `S_Amb_Birds_Woods_Loop` | Ambient | `Audio.Cue.Ambient.Woods` | small songbirds in a pine wood behind the beach | stereo loop | needs a placed emitter |
| A5 | `S_Amb_Crowd_Beach_Loop` | Ambient | `Audio.Cue.Ambient.Crowd` | distant beach crowd walla, no understandable words ("beach crowd distant", "walla") | stereo loop 60 s | needs volume driven by visitor count |
| A6 | `S_Amb_Sunset_Loop` | Ambient | `Audio.Cue.Ambient.Sunset` | very calm evening shore: slow waves, crickets starting, no people | stereo loop | needs the 19:00 switch |
| A7 | `S_Amb_Night_Loop` | Ambient | `Audio.Cue.Ambient.Night` | night at the sea: soft waves, crickets, rare owl | stereo loop | needs the night switch |
| A8 | `S_Amb_Cave_Loop` | Ambient | `Audio.Cue.Ambient.Cave` | inside a seaside cave: muffled waves, drips, low room tone | stereo loop | needs a cave volume |

> Note: today one ambient layer plays at a time. Wind, crowd and seagulls on top of the waves need either a MetaSound that mixes the layers (Claude can build it from these files) or placed Ambient Sound actors. Find the files first; the mixing is a later task.

## 4. Music (category Music) — MVP: two tracks

| # | File name | Folder | Cue tag | What to look for | Type | Hook today |
|---|---|---|---|---|---|---|
| M1 | `S_Music_MainMenu_Loop` | Music | `Audio.Cue.Music.MainMenu` | warm, slow, acoustic: guitar / ukulele / soft keys, sunset mood ("lofi beach", "acoustic sunset chill") | stereo loop 2–3 min | **yes** |
| M2 | `S_Music_Gameplay_Loop` | Music | `Audio.Cue.Music.Gameplay` | light, positive, unobtrusive daytime music that you can listen to for an hour; low on melody so the beach stays audible | stereo loop 3–4 min | **yes** |
| M3 | `S_Music_Sunset_Loop` | Music | `Audio.Cue.Music.Sunset` | the emotional theme of the game: calm, a little melancholic, for 19:00 onwards | stereo loop | needs the 19:00 switch |
| M4 | `S_Music_Event_Police` | Music | `Audio.Cue.Music.Police` | comic tension: pizzicato / bassoon "sneaky" countdown | stereo loop 60 s | later (event) |
| M5 | `S_Music_Event_Boss` | Music | `Audio.Cue.Music.Boss` | comic brawl: fast, slapstick, cartoon fight | stereo loop 60 s | later (event) |
| M6 | `S_Music_Stinger_DayEnd` | Music | `Audio.Cue.Music.DayEnd` | short positive phrase (3–6 s) for the day summary | stereo one-shot | later |

Music loops: the loop point must fall on a bar line.

## 5. Player (category SFX) — MVP

| # | File name | Folder | Cue tag | What to look for | Type | Hook today |
|---|---|---|---|---|---|---|
| P1 | `S_Footstep_Sand_01`…`_06` | SFX/Player | `Audio.Cue.Player.Footstep.Sand` | bare feet / sandals on dry sand, soft crunch | mono, 0.3 s, 6 takes | needs notifies on the walk / run animations (`AS_Walk`, `AS_Run`) |
| P2 | `S_Footstep_WetSand_01`…`_04` | SFX/Player | `Audio.Cue.Player.Footstep.WetSand` | footsteps on wet, packed sand near the water | mono, 4 takes | needs surface detection |
| P3 | `S_Footstep_Water_01`…`_04` | SFX/Player | `Audio.Cue.Player.Footstep.Water` | wading in shallow water up to the knees | mono, 4 takes | needs surface detection |
| P4 | `S_Footstep_Rock_01`…`_04` | SFX/Player | `Audio.Cue.Player.Footstep.Rock` | steps on rock / inside the cave | mono, 4 takes | needs surface detection |
| P5 | `S_Player_Jump` / `S_Player_Land_01`…`_03` | SFX/Player | `Audio.Cue.Player.Jump` / `.Land` | jump effort without voice (cloth + sand), landing thud in sand | mono | needs notifies on `AS_Jump*` |
| P6 | `S_Player_Grab_01`…`_03` | SFX/Player | `Audio.Cue.Player.Grab` | hands grabbing an object, cloth movement | mono | notify on `AS_Arms_*_Grab_*` |
| P7 | `S_Player_ThrowWhoosh_01`…`_03` | SFX/Player | `Audio.Cue.Player.Throw` | arm swing whoosh, medium | mono | notify on `AS_Arms_*_Throw_*` |
| P8 | `S_Player_ThrowCharge_Loop` | SFX/Player | `Audio.Cue.Player.ThrowCharge` | soft rising "charge" tone or creak while the power bar fills (comic) | mono loop | needs the power-bar hook |

## 6. Props (category SFX) — MVP: Sunbed and Umbrella

Each prop has the same set; the Sunbed is the model.

| # | File name | Folder | Cue tag | What to look for | Type | Hook today |
|---|---|---|---|---|---|---|
| R1 | `S_Sunbed_Open` | SFX/Props/Sunbed | `Audio.Cue.Prop.Sunbed.Open` | folding beach lounger unfolding: metal / plastic clacks + fabric | mono 1–2 s | notify on `AS_Sunbed_Open` |
| R2 | `S_Sunbed_Close` | SFX/Props/Sunbed | `Audio.Cue.Prop.Sunbed.Close` | the same lounger folding shut | mono 1–2 s | notify on `AS_Sunbed_Close` |
| R3 | `S_Sunbed_Place_01`…`_03` | SFX/Props/Sunbed | `Audio.Cue.Prop.Sunbed.Place` | lounger set down on sand (soft thump + creak) | mono | notify on `AS_Arms_Sunbed_Place_*` |
| R4 | `S_Sunbed_Impact_01`…`_04` | SFX/Props/Sunbed | `Audio.Cue.Prop.Sunbed.Impact` | lounger hitting sand / a person after a throw (comic clatter) | mono | needs a physics impact hook |
| R5 | `S_Umbrella_Open` / `_Close` | SFX/Props/Umbrella | `Audio.Cue.Prop.Umbrella.Open` / `.Close` | beach umbrella canopy opening (fabric whoomp + ribs click) / closing | mono | notify on the umbrella animations |
| R6 | `S_Umbrella_Plant_01`…`_03` | SFX/Props/Umbrella | `Audio.Cue.Prop.Umbrella.Place` | umbrella pole pushed / twisted into sand | mono | notify on `AS_Arms_Umbrella_Place_*` |
| R7 | `S_Umbrella_Impact_01`…`_03` | SFX/Props/Umbrella | `Audio.Cue.Prop.Umbrella.Impact` | umbrella falling / thrown onto sand | mono | needs a physics impact hook |
| R8 | `S_Prop_PlaceInvalid` | SFX/Props | `Audio.Cue.Prop.PlaceInvalid` | soft "nope" tick when the hologram is red (not a buzzer) | stereo short | needs the placement hook |
| R9 | `S_Prop_Rotate_01`…`_03` | SFX/Props | `Audio.Cue.Prop.Rotate` | tiny ratchet / click per wheel step while rotating the hologram | stereo very short | needs the rotate hook |

## 7. Tools (category SFX) — MVP

| # | File name | Folder | Cue tag | What to look for | Type | Hook today |
|---|---|---|---|---|---|---|
| T1 | `S_ToolWheel_Open` / `_Close` | UI | `Audio.Cue.UI.ToolWheelOpen` / `.ToolWheelClose` | soft whoosh / breeze, same family as the UI sounds | stereo short | needs the wheel hook |
| T2 | `S_ToolWheel_Select` | UI | `Audio.Cue.UI.ToolWheelSelect` | soft tick per highlighted tool | stereo very short | needs the wheel hook |
| T3 | `S_Tool_Equip_01`…`_02` | SFX/Tools | `Audio.Cue.Tool.Equip` | quick cloth + handle grab | mono | notify on `AS_Arms_*_Equip_*` |
| T4 | `S_Bucket_Scoop_01`…`_03` | SFX/Tools/Bucket | `Audio.Cue.Tool.Bucket.Use` | plastic beach bucket scooping sand (+ a crab inside: a clack) | mono | notify on `AS_Bucket_Use` |
| T5 | `S_Spray_Use_01`…`_02` | SFX/Tools/Spray | `Audio.Cue.Tool.Spray.Use` | aerosol deodorant spray, a short burst | mono 0.5–1 s | notify on `AS_Spray_Use` |
| T6 | `S_Toy_Squeak_01`…`_03` | SFX/Tools/Toy | `Audio.Cue.Tool.Toy.Use` | rubber squeaky toy / rattle (comic) | mono | notify on `AS_Toy_Use` |
| T7 | `S_GarbageBag_Pickup_01`…`_03` | SFX/Tools/GarbageBag | `Audio.Cue.Tool.GarbageBag.Use` | plastic bag rustle + paper / can dropped inside | mono | notify on `AS_GarbageBag_Use` |
| T8 | `S_Tool_UseFail` | SFX/Tools | `Audio.Cue.Tool.UseFail` | dull "nothing happens" (empty swing, soft thud) | mono | needs the refusal hook |

## 8. World, visitors, adversities, events, economy

### 8.1 World (category SFX) — MVP

| # | File name | Folder | Cue tag | What to look for | Type |
|---|---|---|---|---|---|
| W1 | `S_Barrier_Open` / `S_Barrier_Close` | SFX/World | `Audio.Cue.World.Barrier.Open` / `.Close` | wooden / metal boom barrier lifting and dropping, a bit rusty | mono 1–2 s |
| W2 | `S_Bin_Dump_01`…`_02` | SFX/World | `Audio.Cue.World.Bin.Dump` | bag of rubbish thrown into a wheelie bin / dumpster lid | mono |
| W3 | `S_Cave_Bed_Sleep` | SFX/World | `Audio.Cue.World.Sleep` | lying down on a straw bed + a calm "time passes" swell | stereo 3–4 s |
| W4 | `S_World_ClosingBell` | SFX/World | `Audio.Cue.World.ClosingWarning` | the 18:00 warning: a soft hand bell / wind chime | stereo 2 s |
| W5 | `S_World_DayStart` | SFX/World | `Audio.Cue.World.DayStart` | morning: rooster far away or a gentle chime | stereo 2–3 s |

### 8.2 Economy (category SFX / UI) — MVP

| # | File name | Folder | Cue tag | What to look for | Type |
|---|---|---|---|---|---|
| E1 | `S_Coins_Pay_01`…`_03` | SFX/Economy | `Audio.Cue.Economy.Pay` | a few coins into a hand / tin box at the barrier | mono |
| E2 | `S_Coins_Tip` | SFX/Economy | `Audio.Cue.Economy.Tip` | a single bright coin + small sparkle | stereo |
| E3 | `S_Fine_Stamp` | SFX/Economy | `Audio.Cue.Economy.Fine` | a fine: rubber stamp slam + comic "sad trombone" (short) | stereo |

### 8.3 Visitors (category SFX, wordless) — after the MVP NPC work

| # | File name | Folder | Cue tag | What to look for | Type |
|---|---|---|---|---|---|
| V1 | `S_Visitor_Happy_01`…`_04` | SFX/Visitors | `Audio.Cue.Visitor.Happy` | content hums, small "mm!", light laughs (adult, mixed) | mono |
| V2 | `S_Visitor_Annoyed_01`…`_04` | SFX/Visitors | `Audio.Cue.Visitor.Annoyed` | sighs, "tsk", "ugh" (no words) | mono |
| V3 | `S_Visitor_Angry_Leave_01`…`_03` | SFX/Visitors | `Audio.Cue.Visitor.AngryLeave` | grumble + stomping away | mono |
| V4 | `S_Visitor_Request` | UI | `Audio.Cue.UI.Request` | the "urgent request" signal (GDD): a short, friendly two-note ping, audible over the beach | stereo |
| V5 | `S_Visitor_Disgusted_01`…`_03` | SFX/Visitors | `Audio.Cue.Visitor.Disgusted` | "eww" reactions to the smell (no words) | mono |

### 8.4 Adversities (category SFX)

| # | File name | Folder | Cue tag | What to look for | Type |
|---|---|---|---|---|---|
| D1 | `S_Crab_Skitter_Loop` | SFX/Adversities | `Audio.Cue.Adversity.Crab.Move` | tiny claws clicking on sand, small skitter | mono loop |
| D2 | `S_Crab_Pinch_01`…`_03` | SFX/Adversities | `Audio.Cue.Adversity.Crab.Pinch` | claw snap (+ visitor yelp, wordless) | mono |
| D3 | `S_Crab_Caught_01`…`_02` | SFX/Adversities | `Audio.Cue.Adversity.Crab.Caught` | crab dropping into a plastic bucket | mono |
| D4 | `S_Seagull_Call_Close_01`…`_03` | SFX/Adversities | `Audio.Cue.Adversity.Seagull.Call` | loud, cheeky seagull close up (the thief) | mono |
| D5 | `S_Seagull_Steal` | SFX/Adversities | `Audio.Cue.Adversity.Seagull.Steal` | wing flaps + snatch of a paper bag / crisps | mono |
| D6 | `S_Seagull_Hit` | SFX/Adversities | `Audio.Cue.Adversity.Seagull.Hit` | comic squawk + feathers puff + flaps away | mono |
| D7 | `S_Wind_Gust` | SFX/Adversities | `Audio.Cue.Adversity.Wind.Gust` | strong gust (3–5 s) | stereo |
| D8 | `S_Child_Scream_Loop` | SFX/Adversities | `Audio.Cue.Adversity.Child.Scream` | a child crying / tantrum, wordless, loopable, annoying but comic | mono loop |
| D9 | `S_Child_Calm` | SFX/Adversities | `Audio.Cue.Adversity.Child.Calm` | child giggle after getting the toy | mono |
| D10 | `S_Smell_Loop` | SFX/Adversities | `Audio.Cue.Adversity.Smell` | comic "stink" buzz: flies buzzing around | mono loop |

### 8.5 Events (category SFX) — later

| # | File name | Folder | Cue tag | What to look for | Type |
|---|---|---|---|---|---|
| X1 | `S_Police_Whistle` | SFX/Events | `Audio.Cue.Event.Police.Arrive` | police whistle, short | mono |
| X2 | `S_Police_Countdown_Tick` | SFX/Events | `Audio.Cue.Event.Police.Tick` | clock tick for the countdown (last 10 s) | stereo |
| X3 | `S_Boss_Arrive` | SFX/Events | `Audio.Cue.Event.Boss.Arrive` | expensive car door slam + many footsteps | mono |
| X4 | `S_Punch_01`…`_04` | SFX/Events | `Audio.Cue.Event.Punch` | cartoon punch / bonk (not realistic) | mono |
| X5 | `S_Bodyguard_KnockedOut_01`…`_03` | SFX/Events | `Audio.Cue.Event.KnockOut` | body falling on sand + cartoon "boing" | mono |
| X6 | `S_Event_Success` / `S_Event_Fail` | UI | `Audio.Cue.UI.EventSuccess` / `.EventFail` | short positive jingle / comic fail jingle | stereo |

## 9. Credits sheet

Keep one line per file in `Audio (sources)/Credits.csv` (create it the first time):

```
File,Source (site / pack / own recording),Author,License,URL,Date
S_Amb_Waves_Loop.wav,Freesound,someuser,CC0,https://freesound.org/...,2026-11-13
```

It is needed for the game's credits and to prove the license later.

## 10. What happens after the research

1. Put the WAVs in `Audio (sources)/<folder>/` with the names above and fill the credits sheet.
2. Either import them yourself (`AudioSystem_Setup.md` § 4: import into the `Content/Audio/...` folder of the table, set the Sound Class, add the tag and the cue row), or tell Claude "the sounds are in Audio (sources)" and it will:
   - import them and set Sound Class, looping and streaming,
   - build the `SCue_*` random containers for multi-take sounds,
   - create the `Audio.Cue.*` tags and the `DA_AudioConfig` rows,
   - add the anim notifies where the hook column says "notify",
   - and list the hooks that still need code ("needs ... hook"), as a separate task.

**MVP order:** A1, M1, M2, P1, R1–R3, R5–R6, T3–T7, W1, W4, E1, then the UI replacements if the procedural UI sounds should be swapped.
