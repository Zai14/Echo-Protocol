# Echo Protocol — Complete Project Overview

> Stealth/survival game where you navigate a dark procedural station using sonar pulses, evade hostile AI, and escape.

**Language:** C99  
**Graphics:** raylib 5.5 + GLSL 330 shaders  
**Target:** Windows 64-bit (MSVC / MinGW), cross-compilable from Linux  
**Build:** CMake ≥ 3.16 (always fetches raylib 5.5 from GitHub via FetchContent)  
**Exe size target:** < 1.44 MB (single floppy disk)  
**Core mechanic:** *The only way to see is by making noise, but every noise reveals you to enemies.*

---

## 1. Project Structure

```
EchoProtocol/
├── OVERVIEW.md              ← this file
├── README.md                ← build instructions
├── CMakeLists.txt           ← build system, size-optimised Release
├── .gitignore
│
├── assets/
│   ├── shaders/
│   │   └── darkness.fs      ← GLSL fragment shader (darkness + CRT effects)
│
├── cmake/
│   └── mingw-w64-x86_64.cmake  ← cross-compile toolchain file
│
├── include/                  ← 12 header files
│   ├── game.h               ← top-level Game struct, state machine, pacing timers
│   ├── player.h             ← WASD movement, circle collider
│   ├── renderer.h           ← off-screen render texture, darkness shader, sonar reveal bridge
│   ├── sonar.h              ← expanding reveal pulses (signature mechanic)
│   ├── echomemory.h         ← fading blue wireframe memory traces + corruption system
│   ├── soundprop.h          ← sound event propagation (footsteps, sonar, phantom whispers)
│   ├── enemy.h              ← Watcher + Hunter + Phantom AI, patrol routes, sound queries, retreat
│   ├── map.h                ← procedural station generator + wall collision query
│   ├── collision.h          ← circle-circle, circle-rect primitives
│   ├── procedural.h         ← xorshift64* seeded PRNG
│   ├── easing.h             ← shared easing utilities
│   ├── audio.h              ← procedural ambient audio system (all sounds synthesised)
│   └── achievements.h       ← 21 persistent achievements, save/load, popup + menu rendering
│
└── src/                      ← 12 source files
    ├── main.c               ← entry point, game loop, fullscreen toggle
    ├── game.c               ← orchestrator: init, restart, update, draw, HUD, shake, fade
    ├── player.c             ← WASD input, movement, glow rendering
    ├── renderer.c           ← render texture, shader init/uniforms
    ├── sonar.c              ← pulse emission, expansion, decay, cooldown
    ├── echomemory.c         ← grid-cell capture, blue wireframe fading, corruption, compaction
    ├── soundprop.c          ← event emit, lifetime update, spatial query
    ├── enemy.c              ← Watcher patrol/investigate/search, Hunter idle/alert/rush/retreat, Phantom wander
    ├── map.c                ← room placement, L-corridor connection, wall collision query
    ├── collision.c          ← circle-circle, circle-rect
    ├── procedural.c         ← xorshift64* RNG
    ├── audio.c              ← wave synthesis for hum, static, footsteps, heartbeat, phantom whisper, etc.
    └── achievements.c       ← 21 persistent achievements, save/load, popup and menu rendering
```

---

## 2. Core Game Loop (`src/main.c` → `src/game.c`)

```
ToggleFullscreen() → InitWindow(1280×720, borderless fullscreen, MSAA 4x)
  │
  └─ GameInit()
       ├─ PlayerInit()          → player at start room centre
       ├─ RendererInit()        → render texture + darkness shader
       ├─ AmbientAudioInit()    → synthesise all sounds, start in silence
       ├─ SonarInit()           → empty pulse list, cooldown ready
       ├─ EchoMemoryInit()      → empty cell array
       ├─ SoundPropInit()       → empty event list
       ├─ StationGenerate()     → 15–20 rooms + corridors (random seed)
       ├─ EnemyManagerAdd()     → 1 Hunter + N Watchers at station spawns
       ├─ Grace period (8s)     → sonar pulses are silent to enemies
       ├─ Hunter placed in nearest room to start  → guaranteed encounter
       └─ state = BOOT (title screen, 4.5s, SPACE to skip)
               
While running:
  GameUpdate()
    ├─ ESC → exit
    ├─ R (game-over/won) → GameRestart() — fresh station, fresh enemies
    ├─ SPACE → sonar pulse + shake + zoom + sound event + scan count
    ├─ Grace/hud timers count down
    ├─ PlayerHandleInput() + PlayerUpdate()
    ├─ Wall collision (slide along room/corridor walls using StationIsWalkable)
    ├─ SonarUpdate() + SoundPropUpdate()
    ├─ UpdateFollowCamera()     → spring zoom, screen shake
    ├─ Fade transition (fadeAlpha → fadeTarget smoothing)
    ├─ Footstep sound timer + sound event emit
    ├─ Nearest enemy distance cache (shared by audio + heartbeat)
    ├─ AmbientAudioUpdate()     → silence fade-in, hum, static, footsteps, heartbeat, machinery
    ├─ Sonar → Renderer bridge (reveal pulses → shader)
    ├─ Echo memory capture (grid cells within pulse radius)
    ├─ EnemyManagerUpdate()     → AI + sound queries + retreat + room roaming
    ├─ Relay activation         → hold E near console (2s), progress bar, alarm sound
    ├─ Alerts tracking           → counts each sonar pulse after grace period ends
    ├─ Difficulty scaling        → after relay: Hunter +20% hearing, heartbeat faster, machinery louder
    ├─ Death slow-mo (0.6s)     → freezes gameplay, grows red vignette, ESC to skip
    ├─ Game-over check (circle-circle collision) → triggers slow-mo death
    ├─ Airlock escape sequence  → alarm lights, countdown, final sonar pulse
    ├─ White-out escape         → door opens, player walks in, white light, fade to black
    └─ Win check (escape sequence complete → WON)
               
  GameDraw()
    Pass 1 — World (into render texture, camera-relative)
    ├─ StationDraw()            → room/corridor floors + walls (tinted by distance)
    ├─ Relay room orange glow   → additive BLEND, triggered by sonar pulse reflection
    ├─ Relay console             → blinking orange panel + sparks (inactive) / solid green (active)
    ├─ PlayerDraw()             → 5-layer cyan glow + core + outline
    ├─ Sonar pulse rings        → 3 additive glow circles + DrawRing + trailing wake
    ├─ EnemyManagerDraw()       → state-coloured circles + facing line
    └─ Airlock marker           → green rectangle outline
               
    Pass 2 — Darkness composite
    └─ RendererDrawDarkness()   → shader: darkness + personal glow
         + sonar reveal + ring-edge glow + vignette + CRT bands + flicker
               
    Pass 3 — Screen-space overlays
    ├─ Echo memory wireframe    → blue fading grid (additive blend)
    ├─ Heartbeat ring           → pulsing red ring when enemies < 220px
    ├─ Phantom silhouette       → faint purple figure drawn in echo memory overlay when phantom < 300px
    ├─ Phantom encounter text   → "*The memory is not your own.*" on first corrupted cell detection
    ├─ Airlock compass          → golden pulsing dot/arrow toward escape
    ├─ Hunter proximity glow    → directional red/orange glow at screen edge
    ├─ Sonar cooldown ring      → bottom-right ring fill indicator
    ├─ Death slow-mo            → growing red vignette + white flash (0.6s)
    ├─ Airlock escape sequence  → flashing red alarm borders + countdown text + door opening
    ├─ Relay HUD status         → "RELAY OFFLINE" (orange) / "RELAY ONLINE" (green)
    ├─ End statistics           → Alerts count + Rank (S/A/B/C) on game-over and win screens
    ├─ HUD text                 → tutorial (fades), first-ping flash, grace indicator
    └─ Fade overlay             → smooth black fade-in/out
```

---

## 3. Core Mechanic Details

### Sonar Pulse (Signature Mechanic)
| Parameter | Value |
|-----------|-------|
| Max concurrent pulses | 3 |
| Expansion speed | 300 px/s (ease-out cubic) |
| Max radius | 400 px |
| Reveal persistence | 1.0 s after max radius (ease-out decay) |
| Cooldown | 2.0 s |
| **#1 Echo Drift** | 2% chance per ping that the origin drifts ±2–6px independently in X and Y.
                   Makes the player briefly question *"Did I move?"* — costs only 3 lines of code |
| **#5 Emergency Lights** | RED/BLACK screen flashes every 25–35s — brief red overlay pulses, pure atmospheric tension |
| **#15 Broken Monitor** | "ERROR" text flickering on a monitor in a random room, visible when sonar reveals the area |
| **#18 Loose Cable Sparks** | Random `zzzt` blue-white flash at a random wall position every 6–12s |
| Shader layers | 8 reveal circles + ring-edge glow in darkness shader |
| Shader flash | `sonarFlash` uniform (1.0 → 0 over ~0.17s) —
                  brief screen-wide brighten on each pulse emit |
| Draw layers | 3 additive glow circles + variable-thickness DrawRing + trailing wake +
                  **#6 Ring Distortion** (`sin(time × 120 + ratio × 2π) × 0.015` modulates ring
                  thickness — organic wobble makes the pulse feel alive) |
| **#20 Rare Event** | 1% chance per ping that a mysterious sonar pulse echoes back from
                    a random room centre. No enemy. No explanation. Just... a response.
                    The player immediately questions whether they're truly alone.
| **#6 Ring Distortion** | 100% of pings — DrawRing radius modulated by `sin(time × 120 + ratio × 2π) × 0.015`.
                    Creates a subtle, organic wobble that makes the pulse feel alive instead of mechanical |
| Shake (first ping) | 0.20s, 10px intensity, 1.04 zoom |
| Shake (subsequent) | 0.12s, 5px intensity, 1.025 zoom |
| Screen flash | SonarFlash uniform (1.0 → 0 over ~0.17s) |
| Cooldown UI | Bottom-right ring fill indicator |
| Grace period | 8s — sonar doesn't alert Hunters |
| **#3 Broken Sonar** | 5% chance per ping that the pulse is delayed by 0.5s.
                   Player hears nothing → silence → *ping*. Creates brief panic. |
| **Sonar Saturation** | Each ping adds +0.25 saturation; decays at 0.08/s (~12s for full clear)
                    Spamming 4+ pings fills screen with noise dots, making vision harder |
| **#9 Echo Trails** | 4 small blue particles trail behind each active sonar ring — visual reinforcement of wavefront motion |
| **#20 Power Drain** | Each sonar ping dims station lights for 3s — encourages careful scanning, punishes spam |

### Sonar Reflection
Room type at the pulse origin affects echo reveal strength *(all C-side, no shader changes)*:

| Room Type | Echo Strength | Behaviour | Player Interpretation |
|-----------|---------------|-----------|----------------------|
| **Metal rooms** (objective, relay, airlock) | **1.35×** (+35% brighter) | Strong, clear echo | *"This room is important."* |
| **Concrete rooms** (outer/far rooms) | **0.65→1.0×** (fades with distance) | Dull, quick fade | *"I'm deep in the station."* |
| **Corridors** | **0.7×** base + 2 stretched echoes at ±30% offset | Weak + elongated along corridor axis | *"This is a narrow passage."* |

**Implementation:** The sonar bridge computes a `reflectionMult` per pulse by checking the pulse origin against station room/corridor bounds. Final strength = `clamp(baseStrength × reflectionMult, 0, 1)`. Corridor pulses emit **2 secondary reveal circles** offset along the corridor's long axis (+30%, −30%) at 1.2× radius with 0.55/0.40 strength, simulating distorted echoes bouncing down narrow halls.

### Echo Memory
| Phase | Duration | Visual |
|-------|----------|--------|
| Full visibility | 0–0.7s | White-cyan outline (lerps to blue) |
| Blue wireframe | 0.7–3.7s | Blue wireframe fading to transparent |
| **Corrupted** (by Phantom) | — | **Magenta/purple** with oscillating glitch intensity, additive blend |
| Grid cell | 64×64 px | |
| Capacity | 1024 cells | Additive blend, screen-space overlay |

*When the Phantom passes through a revealed area, affected cells turn purple — the infection spreads through your memory of the station.*

### Darkness Shader (`assets/shaders/darkness.fs`)
| Layer | Effect |
|-------|--------|
| Ambient darkness | 3.0% brightness floor |
| Personal glow | Smoothstep bubble (r=90px) around player, +22% brightness |
| Sonar reveal | Up to 8 circles with inner fill + ring-edge wavefront glow |
| Sonar flash | Screen-wide brightness on pulse emit |
| Vignette | Quadratic darkening (65% at corners) |
| CRT bands | Two drifting sine-wave horizontal bands (8% + 13% res freq) |
| Edge static | Increased noise near screen borders |
| Flicker | ±1.5% at 3.7Hz + ±0.5% at 7.1Hz, random phase resets |
| Colour shift | Blue-green (cyan) CRT phosphor at periphery |

---

## 4. Sound Propagation (`soundprop.h` / `soundprop.c`)

| Event | Radius | Intensity | Lifetime |
|-------|--------|-----------|----------|
| `FOOTSTEP` | 80 px | 0.30 | 0.3 s |
| `SONAR_PULSE` | 400 px | 1.00 | 2.0 s |
| `PHANTOM_WHISPER` | 130 px | 0.45 | 0.6 s |

Capacity: 16 events, compacted each frame.  
Enemies query via spatial overlap: `SoundPropQuery(cx, cy, hearingRadius, ...)`.  
*Note: `PHANTOM_WHISPER` events are ignored by Watchers — the phantom's presence is purely atmospheric.*

---

## 5. Enemies (`enemy.h` / `enemy.c`)

### Watcher (1–2 per game, depending on room count)
| Property | Value |
|----------|-------|
| Speed | 80 px/s |
| Radius | 14 px |
| Hearing | 200 px (ignores sonar pulses) |
| Patrol | 4-point 300px square route (150px radius) |
| Spawn | Up to 2 Watchers in random non-start rooms |

**State machine:**
```
PATROL → (arrive) → PAUSE_WAYPOINT [0.6–1.4s]
  → 75% PATROL (next waypoint)
  → 25% GUARD CORRIDOR → INVESTIGATE (blocks doorway)

PATROL → (hear footstep) → INVESTIGATE → (arrive) → SEARCH [2.0s scan]
  → 70% PATROL
  → 30% ROAM:
       ├─ After relay activation: 100% AIRLOCK GUARD → INVESTIGATE
       ├─ Before relay: 40% RELAY ROOM → INVESTIGATE
       └─ Otherwise: random room → INVESTIGATE
```

**Visuals:** State-reactive red circles + facing direction line.  
- PATROL: `(200,50,50)`
- PAUSE: `(180,60,60)`
- INVESTIGATE: `(230,70,70)` — brighter when hunting
- SEARCH: `(160,50,50)`

### Hunter (1 per game, spawns in room nearest to start)
| Property | Value |
|----------|-------|
| Speed | 150 px/s (90 px/s when retreating) |
| Radius | 12 px |
| Hearing | 450 px (only SONAR_PULSE) |

**State machine:**
```
IDLE → (hear sonar) → ALERT [0.6s charge, colour brightens]
  → RUSH [sprint toward pulse]
  → (timeout/arrive) → **15%** HESITATE [0.6s pause, slow scan, then re-RUSH]
  → **85%** RETREAT [move to random non-start room at 60% speed]
  → (arrive) → IDLE
       ↑ (can re-engage mid-retreat if new pulse heard)
```

**Visuals:** State-reactive red circles + facing direction line.  
- IDLE: `(80,25,25)` — barely visible in the dark
- ALERT: Dynamic, brightens from `80→235` over 0.6s with 18Hz pulse
- RUSH: `(240,60,60)` + core `(255,80,80)` — blazing
- **HESITATE**: Mid-brightness red with 12Hz sine pulse — `(200×ht, 50, 50)`, pauses and scans
- RETREAT: `(120,35,35)` — dimming down

**Game Over:** `CollisionCircleCircle(enemy, player)` → slow-mo death sequence (0.6s) → `GAME_STATE_GAME_OVER`.

### Phantom — The Echo Anomaly (0–1 per game, 30% chance)
> *It exists only in what you remember. It corrupts what you have seen.*

| Property | Value |
|----------|-------|
| Speed | 35 px/s |
| Radius | 10 px |
| Behaviour | Wanders the station aimlessly, moving between random rooms |
| Visibility | **Never seen directly** — only leaves purple wireframe trails in echo memory |
| Threat | Cannot kill the player. Corrupts revealed echo memory cells with magenta/purple noise. |
| Spawn | 30% chance per run (60% on ANOMALY seeds — seed % 777 == 0) |

**State machine:**
```
PHANTOM_WANDER → (arrive at room centre) → PHANTOM_PAUSE [3–7s]
  → pick new random room → PHANTOM_WANDER
```

**Behaviour:**
- Every 0.3s, the phantom corrupts up to 9 echo memory cells near its current position
- Corrupted cells render as **magenta/purple** with oscillating glitch intensity
- Every 8–14s, emits a `PHANTOM_WHISPER` sound event (inaudible to Watchers)
- When the phantom is within 300px of the player:
  - A **faint purple silhouette** is drawn in the echo memory overlay layer
  - A **procedural whisper sound** plays (0.5s modulated noise with sibilance, very quiet)
- On first detection of a corrupted cell: encounter text "*The memory is not your own.*"
  and the **"Memory Leak"** achievement unlocks

**Design purpose:** The Phantom is pure psychological horror. It can't hurt you — but it corrupts your map, distorts your view of explored areas, and whispers in the dark. Players will question what's real. The purple corruption spreading through previously safe blue wireframes creates a sense of contamination without any direct threat.

> **#8 Hunter Hesitation:** When the Hunter reaches the last ping location (RUSH timer still active),
> there is a **15% chance** it enters HESITATE instead of immediately retreating. It pauses for 0.6s,
> slowly rotates as if scanning, then resumes the rush (`alertPulseLife + 0.5s` remaining).
> A fresh pulse interrupts the hesitation and instantly re-targets the Hunter.
> This single state makes the Hunter feel intelligent and unpredictable — the silence before
> the charge creates genuine tension. Draw colour: pulsing red at mid-brightness (12Hz sine).

> **Hunter Mimics Sonar (late-game):** After relay activation, each time the player pings, the Hunter emits its **own sonar pulse** 1.8–2.2s later from its current position. The fake pulse briefly reveals the Hunter's silhouette — and also reveals the player. A chilling reminder that you are not alone.

**Difficulty Scaling (after relay activation):**
- Hunter hearing radius: 450px → **540px** (+20%)
- Heartbeat cooldown: 0.50–0.80s → **0.35–0.56s** (×0.70, ~43% faster)
- Mechanical rumble volume: 0.025–0.055 → **0.050–0.080** (2× louder)
- Mechanical rumble interval: 8–18s → **6–16s** (more frequent)

---

## 6. Procedural Station (`map.h` / `map.c`)

| Parameter | Value |
|-----------|-------|
| Rooms | 15–20 rectangles, 80–180 px each |
| Room padding | 30 px minimum gap |
| Placement | Random near existing room, 50 attempts max |
| Corridors | L-shaped, 48 px wide |
| RNG | xorshift64*, deterministic per seed |
| Special rooms | Start (idx 0), Transmitter (idx furthest from start), Relay (1 per game, closest to start), Airlock (1 random) |
| Notes | The relay room now has gameplay weight: player must **activate the relay console** before the airlock unlocks. An **orange sonar-reflection glow** hints the room's location. |
| Enemy spawns | Up to 3 random non-start rooms. First spawn = Hunter, rest = Watchers |
| Floor colour | `(15, 20, 30)` — shifts blue by distance (+12 B, −12 R) |
| Wall colour | `(60, 80, 110)` — shifts blue by distance (+15 B, −15 R) |
| Wall collision | `StationIsWalkable(cx, cy, radius)` — circle-vs-all-rects test |

**Runtime room properties (set per-run from seed):**
| Property | Selection | Effect |
|----------|-----------|--------|
| **#10 Water Puddles** | 0–2 random non-start rooms | Footsteps in these rooms are **1.8× louder** (interval halved, radius ×1.8, intensity ×1.8). Player must decide: fast path through water or quiet detour. |
| **#15 Rusty/Damaged Floors** | 0–2 random non-start rooms | Footsteps are **2× louder** — creaking metal. Same mechanic as water, different flavour. |
| **#19 Station Age** | Derived from `seed % 100 + 1` (1–100 yrs) | `ageGlitchMult = 1.0 + age/100`. Older stations have more frequent electrical flicker events. Timer between flickers = `(5–13s) / ageFreq`. First flicker timer also respects age. |

---

## 7. Procedural Audio (`audio.h` / `audio.c`)

All sounds synthesised at runtime by manually filling 16-bit sample buffers with `sinf`, `GetRandomValue`, and `fmodf` (raylib 5.5 removed the `GenWave*` helpers from the public API).  
Zero audio files shipped. Sample rate: 11025 Hz.

| Sound | Synthesis | Volume | Behaviour |
|-------|-----------|--------|----------|
| Electrical hum | 50 Hz sine | 0.020–0.040 | Continuous, **Adaptive**: pitch (1.0→1.08) + volume
                   increase when Hunter < 350px. Fades in after 3s silence |
| Static | White noise | 0.008 | Continuous, **Adaptive**: pitch (1.0→1.05)
                   when Hunter is near. Fades in after 3s silence |
| Metal footsteps | 600 Hz sine, 0.06s | 0.040 | Triggered by movement timer (0.35s interval) |
| Heartbeat thump | 40 Hz sine, 0.12s | 0.015–0.050 | Triggered by enemies < 220px,
                   speed varies: 75 BPM (far) → 120 BPM (close).
                   After relay: 43% faster (×0.70 cooldown) |
| **Panic Breathing** | Low modulated noise, 1.5s | 0.0–0.060 | Starts when Hunter < 180px, pitch+volume
                   ramp with proximity. Creates psychological urgency. |
| **Phantom Whisper** | Modulated noise + 2000Hz sibilance, 0.5s | 0.006–0.018 | Triggered when phantom < 300px, cooldown 1.5–3s.
                   Very quiet breathy whisper, pitch varies ±5% per play. |
| CRT interference | Noise burst, 0.15s | 0.050 | Triggered by sonar pulse |
| Distant machinery | 60 Hz square, 1s | 0.025–0.055 | Random interval 8–18 seconds.
                   After relay: 2× louder (0.050–0.080), interval 6–16s |
| Silence intro | — | — | First 3 seconds: pure silence |
| **#13 False Heartbeat** | Single 40 Hz thump, 0.12s | 0.015–0.050 | Randomly triggers every 30–36s even when no enemy is
                    nearby (nearestEnemyDist > 300px). Sets `nearestEnemyDist = 50` for one frame,
                    triggering one heartbeat thump through the audio system. Player freezes, looks
                    around, sees nothing. Pure psychological horror. |

---

## 8. Screen-Space Overlays (Game Feel)

### Airlock Compass
- Appears after first sonar ping
- Golden pulsing dot if airlock is on-screen
- Golden direction arrow at screen edge if airlock is off-screen
- Gives the player a constant sense of direction without a minimap
- **#3 Corrupted Compass:** When the Hunter is closer than 120px, the compass screen-position
  jitters by up to 10px per frame (random direction). The player sees the arrow shake erratically
  at the exact moment they most need to orient — creates panic without any extra UI

### #1: Room Wall Glow
- When a sonar pulse's wavefront passes within 40px of a room centre, the room's walls flash cyan
  using additive blending (`(150, 210, 255)` at 15% alpha, 2px thickness)
- The glow only appears while a pulse is actively passing through — no persistent state needed
- Makes the sonar feel tactile and responsive: the station literally lights up where you scan
- Costs ~15 lines, runs inside the world-space draw pass so the darkness shader properly obscures it

### #2: Fading Player Trail
- The player's last 5 positions are stored in a ring buffer (shifted each frame when moving >1px/s)
- Up to 3 fading circles are drawn at stored positions with decreasing alpha (`(160,200,255)` at 4% max)
- Creates a subtle sense of motion in the otherwise static darkness
- World-space draw — the trail is only visible inside your personal glow bubble
- Costs ~15 lines total (5 in update, 10 in draw)

### #3: Pulse Edge Particles
- While a sonar ring is expanding (<95% of max radius), 8 small cyan dots scatter outward from the ring circumference
- Each dot's radial offset is modulated by `sin(time × 3 + particleIndex)` for organic scatter
- Alpha fades as the pulse approaches max range (`0.08 × (1 - ratio) × 0.6`)
- Creates the impression that the pulse is disturbing dust and particles as it radiates outward
- Costs ~15 lines, purely draw-side, no new state

### #4: Enemy Alert Flash
- When a Hunter enters ALERT state, a 0.5s timer captures its position
- An expanding white ring (`DrawRing` with additive blending) radiates from the enemy — starts at 60px, shrinks to 0
- Creates dramatic feedback: the player sees *exactly* where the threat is charging up
- The flash re-triggers every 0.5s while the Hunter stays in ALERT, creating a repeating pulse effect
- Costs ~12 lines (update detection + draw ring)

### #5: Airlock Beacon Pulse
- A pulsing green vertical beam (`(80, 255, 150)` at 6% alpha, 4px wide, 16px tall) extends above and below the closed airlock door
- Pulses at 2Hz synced with the existing green glow ring
- Makes the escape destination visible from farther away, especially when the sonar reveals it
- The airlock already had a pulsing green `DrawRing` beacon — the vertical beam gives it more presence
- Costs ~10 lines, additive blending inside world-space pass

### Hunter Proximity Edge Glow
- When Hunter is in ALERT or RUSH state
- Directional glow at the screen edge facing the Hunter
- ALERT: dim orange `(200,80,30)`, radius 50px
- RUSH: bright red `(255,50,50)`, radius 80px
- 4Hz pulsing for urgency
- Multi-layer additive glow (4 concentric circles)

### Sonar Reflection (Room-Type Echo Variation)
- Pulse reveal strength varies by room type at the pulse origin
- **Metal rooms** (objective, relay, airlock): 1.35× multiplier — bright, satisfying reveal
- **Concrete rooms** (outer/far rooms): 0.65–1.0× multiplier (fades with index/total ratio) — dull echo
- **Corridors**: 0.7× multiplier + 2 stretched secondary pulses along corridor axis at 0.55/0.40 strength
- Purpose: Players learn to read the station through **echo quality** rather than sight alone
- All logic is C-side — the shader receives modified `(x, y, radius, strength)` vec4 values unchanged

### Relay Room Orange Glow (Sonar Echo Hint)
- Relay room walls glow **orange** when a sonar pulse's wavefront passes through it
- Trigger: `fabsf(pulseOriginDist - currentRadius) < 60px` → sets `relayGlowTimer = 2.5s`
- Visual: 3px orange wall outline `(255,150,50)` + subtle orange floor tint, BLEND_ADDITIVE
- Animation: Pulsing at 3Hz, decays as timer counts down
- Purpose: Gives the player a **direction to explore** without a minimap or quest marker
- Only active while relay is not yet activated (`relaysActivated < 1`)
- Rendered inside the world pass → darkness shader properly obscures it

### Station ID & Facility Name
- Displays "STATION KX-XXXX" and "FACILITY ORPHEUS" in the top-right corner during gameplay
- The 4-digit number is derived from the procedural seed (`stationSeed % 10000`)
- The facility name is chosen from a pool of 15 procedural names (`ORPHEUS`, `EREBUS`, `NEMESIS`, `VEGA`, `TITAN`, `IXION`, `HADES`, `LUX`, `UMBRA`, `SOL`, `NOX`, `LYNX`, `AURORA`, `HELIX`, `KARMA`)
- **Purpose**: Gives procedural levels more identity — players remember
  *"My run on Facility EREBUS was brutal."*
- **#11 Station Motto:** Below the facility name, a procedurally chosen motto appears:
  *"We Listen."*, *"No Signal Returns."*, *"Beneath the Static."*, *"Echoes Never Fade."*,
  *"Last Light."*, *"The Dark Remembers."*, *"Silence is Survival."*, *"Do Not Answer."*, *"Listen Closely."*, *"The Station Remembers."*
  - Selected by `facilityNameIdx % 10`, so it's deterministic per run
  - Tiny 9px text at very low alpha — barely visible, purely atmospheric
  - Costs virtually zero size (a single string table + one DrawText call)
### Sonar Cooldown Ring
- Bottom-right of screen
- Faint background ring + partial fill ring showing remaining cooldown
- Center dot pulses brighter as cooldown nears end
- Only visible during active cooldown

### #13: Reactor Pulse
- A subtle 2% global brightness surge every 12 seconds — like huge generators thrumming deep in the station
- Sets `sonarFlash = 0.03f` briefly when no other flash is active — barely perceptible but adds life
- Costs ~5 lines of code, zero new state beyond a reusable timer

### #16: CRT Burn-in
- Brief white-blue screen flash overlay (0.06 alpha, 0.2s) after each sonar ping or CRT flicker event
- Simulates screen persistence on old CRT monitors — the image lingers for a fraction of a second
- Triggered by `spacePressed` or `flickerType == 8`
- Draws a full-screen rectangle at very low alpha — almost invisible but perceptible in peripheral vision

### Heartbeat Ring
- Pulsing red ring at screen centre
- Activates when nearest enemy < 220px
- 90 BPM rhythm (matching audio heartbeat)
- Intensity scales with proximity

### Death Flash & Last Echo
- Brief semi-transparent red overlay (`180,20,20` at 25% alpha)
- Drawn on top of the game-over black background
- Decays over 0.4s
- **Last Echo**: During death slow-mo, sonar pulses keep expanding at half speed.
  The player watches the last blue ripple pass over their dead body.
  Pulses continue revealing the empty station — then darkness returns.
  Makes death feel cinematic, not mechanical.

### Near Miss (0.2s Slow-Mo Burst)
- Activated when a rushing Hunter passes within 10px of the player but misses
- **0.2s slow-motion**: gameplay briefly decelerates
- Camera flash + heartbeat thump sound
- Gives the player a surge of adrenaline without any scripted sequence
- Makes narrow escapes feel dramatic

### Sonar Saturation (Noise Overlay)
- Each sonar ping adds +0.25 saturation (capped at 1.0)
- Saturation decays at 0.08/s — takes ~12s of silence to fully clear
- At high saturation: screen fills with randomly positioned noise dots
- **Strategic implication**: Spamming sonar to stay visible is punished.
  The player must balance seeing vs. keeping their vision clean.
- Only active in GAME_STATE_PLAYING

### #11: Station Announcements
- Rare, procedurally generated distorted speaker announcements
- Word pools combine randomly: "...Attention... Sector... offline..." or "...Warning... Power... critical..."
- Fire every 20–60s during gameplay, display for 3s
- Centered near the bottom of the screen, faded and subtle
- **No gameplay impact** — pure atmosphere to make the station feel alive and abandoned

### #14: Radio Bursts
- Random distorted radio transmissions — never complete, never useful
- Fragments pool: "...HEL—", "...DON'T—", "...IS ANYONE—", "...THEY'RE—", "...RUN—", "...ECHO—", "...SECTOR—", "...LAST—"
- Fire every 35–40s during gameplay, display for 2s
- Rendered near center-bottom of screen, just above the announcement overlay
- Same system as announcements — no new infrastructure, just a second string pool and timer
- **Purpose**: Makes the player feel like someone else was here. And they're gone now.

### #18: Procedural Lore Logs
- One random non-important room per station contains a lore terminal
- When a sonar pulse reveals the room, a one-line log fragment appears for 4s
- Pool of 10 fragments: "DAY 17 — We heard it again." / "DO NOT PING. It learns." / "Do not answer the echoes."
- Centered below announcements, softly pulsing
- **No collectibles** — just atmosphere and environmental storytelling

### #18: Dynamic Room Names
- When the player enters a room, a procedural sector label appears top-left for 3s
- Format: `"SECTOR {A–E}-{01–99} — {RoomType}"`
- Room type pool: Corridor, Maintenance, Storage, Habitation, Cryogenics, Hydroponics, Comms, Engineering, Medbay, Observatory
- Room name derived from `stationSeed + roomIdx` — deterministic per run
- Detection: checks which room rectangle the player's position falls inside (after wall collision)
- **No gameplay impact** — makes the station feel like a real facility with sectors

### #15: Impossible Room
- 0.2% chance per run (`GetRandomValue(0, 499) == 0`) that a non-important room
  becomes marked as an anomaly
- When a sonar pulse passes within 30px of the room centre, the player sees:
  *"This room should not exist."* with a subtitle *"— Echo Anomaly Detected —"*
- Overlay displays for 4s, soft purple-white text centered on screen
- The room looks completely normal. There is no gameplay difference.
- **Easter egg** — pure mystique. Players will post screenshots wondering
  if it's a bug or intentional. That ambiguity is the point.

### #22: Hidden Observation Room
- 0.5% chance per run that a non-important room has one-way glass
- Same detection pattern as Impossible Room (pulse within 30px → reveal)
- When revealed: *"Behind the glass... a silhouette."* appears for 3s
- The silhouette is never explained. It's never seen again.
- **Easter egg** — makes players question what they saw

### #12: Moving Shadows
- During CRT flicker events (`flickerType == 1 || 8`), a brief shadow shape appears near the
  screen edge for a single frame
- A 15–18s timer picks a random screen-edge position and stores it
- On the next CRT flicker frame, the shadow is drawn as a dark circle (`(80,40,40)` at 12% alpha)
- The position is cleared after one frame, so the shadow never persists
- Player glances over — nothing there. *Was that a person?*
- **No gameplay impact** — pure atmospheric horror, costs ~10 lines

### #6: Ceiling Pipes
- 2–3 thin procedural horizontal lines drawn near the top of every room (`pipeY = room.y + 6 + pi × 5`)
- Occasional steam puffs: `sin(time × 1.5 + room offset) > 0.85` → small white circle fades near ceiling
- Draws 1.0px-thick lines in dark grey (`(60,65,70)` at 20% alpha) — subtle enough not to distract
- Adds visual depth to otherwise empty room ceilings, makes station feel structurally real
- Costs ~25 lines, computed entirely from existing room geometry

### #7: Floor Numbers
- Tiny painted text (7px font) in the bottom-right corner of every room
- Code pool: `"B12"`, `"A07"`, `"HX4"`, `"C09"`, `"D03"`, `"E11"`, `"F08"`, `"G02"`
- Selection: `(stationSeed + roomIdx) % 8` — deterministic per run
- Drawn at `(room.x + room.w - 22, room.y + room.h - 10)` with dark grey `(100,105,110)` at 15% alpha
- **No gameplay impact** — adds realism and makes the station feel like a real facility with markings
- Costs ~10 lines, zero new state

### #8: Ceiling Dust
- When the Hunter is in RUSH state within 200px of the player, 6 small dust particles
  drift down from the top of the screen
- Particles animate via `fmodf(time × speed + offset, 1.0)` — procedural, no particle system needed
- Alpha fades as particles approach the bottom of the drop zone
- Creates a subtle environmental cue that the Hunter is close and moving fast
- Subtle enough that players don't consciously register it — but they *feel* it

### #5: Security Cameras
- Every non-start room has a small security camera on the ceiling (top-centre of room)
- Red LED blinks at 2Hz with room-unique phase offset (`sinf(time × 2 + ri × 1.7)`)
- Two draw layers: dim glow (`(200,30,30)` at 60% alpha) + bright core (`(255,80,80)` at full)
- World-space draw — properly obscured by the darkness shader until sonar reveals the area
- **Cameras do nothing.** They never detect the player, never trigger alarms.
  But players don't know that. Every blinking LED adds tension.
- Costs ~15 lines, zero new assets

### #12: Panic Vision
- When the Hunter is closer than 150px, the player's personal glow radius **shrinks** proportionally
- At 0px distance: 25px minimum (tiny bubble)
- Exactly when the player most needs to see, their vision collapses inward
- Creates intense panic without any scripted event

### Grace Period Indicator
- Shows "PULSE IS SILENT — ENEMIES CANNOT HEAR"
- Appears after the first sonar ping, during the 8s grace period
- Fades out as grace timer approaches 0
- GRAY text, modest alpha (max 0.5), positioned below the tutorial text

### Death Slow-Mo Sequence
- Triggered by enemy collision
- 0.6s slow-motion: gameplay freezes, ESC to skip
- Bright white flash on first frame (CRT distortion via `sonarFlash` shader)
- Growing red vignette that fills the screen from the center
- After 0.6s, transitions to standard GAME OVER screen
- Makes death feel visceral rather than an instant cut

### Airlock Escape Sequence & White-Out Ending
- Triggered by reaching the airlock (80px zone)
- 3-second countdown with **flashing red alarm borders** at 8Hz
- Displays "DOOR OPENS IN 3... 2... 1..." countdown text
- "HOLD POSITION" prompt — player must stay near the door
- **Final sonar pulse** emitted automatically on arrival (counted in stats)
- **#7 Light Leak:** Even before the airlock sequence begins, a tiny white flicker pulses under
  the closed airlock door (3Hz, small 8×2px rectangle at `airlockY + 18`). Only visible after
  the first sonar ping. Subtle enough that players discover it over time — a reminder that
  escape is possible
- **#16 Dynamic Ending**: The escape background varies by alert count:
  - **≤1 alert**: Door opens in silence. Dark, quiet fade. The station simply dies.
  - **2 alerts**: Normal escape with warm green glow and alarm lights.
  - **≥3 alerts**: Frantic red alarm background. Hunter silhouette visible in white light at door frame. Barely escape.
- After countdown: **white-out sequence** begins (2.5s):
  - Door slides open with bright white interior light
  - Player automatically walks toward the door
  - Sonar flash on first frame
  - **Fade to black** during the last 0.5s
- Transitions to ESCAPED victory screen with full statistics
- Turns a simple "you win" into a dramatic escape moment that varies by how you played

### Relay Console & Activation
- **Blinking orange console** with animated screen + 3 procedural sparks
- **"Hold E to activate"** hint text appears when player is within 120px
- **2-second hold** with green progress bar below the console
- Progress **decays at 1.5× rate** if player releases E or walks away
- On activation: **camera shake + sonar flash + alarm sound**
- Console **transitions orange→green** with steady pulsing glow
- Relay status changes from "RELAY OFFLINE" → "RELAY ONLINE / AIRLOCK UNLOCKED"
- **#5 Terminal Numbers:** Small atmosphere text displayed beside the relay screen:
  - **Before activation** (orange): "NODE 14", "SECTOR 07", "TEMP 281K", "STATUS UNKNOWN"
  - **After activation** (green): "NODE 14", "SECTOR 07", "TEMP 281K", "STATUS ONLINE"
  - Changes colour with the console (orange→green), adds a sense of systems rebooting
  - Text changes indexed per run via `stationSeed` — never useful, always atmospheric

### #25: Final Black Screen
- Brief 1-second black pause before the game-over stats screen appears
- Set via `game->finalBlackTimer = 1.0f` at the moment of death
- Draws a full-screen black rectangle fading from 0→1 over the first 0.5s
- Creates a dramatic beat: *silence → black → "GAME OVER"*
- Costs ~5 lines, uses `fadeTarget`/`fadeAlpha` mechanism

### #12: Exit Door Breathing
- When the player is within 500px of the closed airlock, a small white mist circle pulses at the base of the airlock door
- Timer resets every 2–5s near the door, every 5s when far
- In the draw layer: when `exitMistTimer < 1.0s`, draws a small white circle (`(220,230,240)` at 8% alpha) at `airlockY + 16` with radius 6–10px modulated by `sin(time × 4)`
- Screen-space coordinate — properly converted from world via `GetWorldToScreen2D`
- The door literally breathes — a subtle reminder that escape is possible
- **Costs ~15 lines** total across update and draw

### #26: Hidden Seed Challenge — [ANOMALY]
- Seeds where `stationSeed % 777 == 0` are flagged as anomalous
- On the title screen, a purple `[ANOMALY]` label is drawn below the subtitle at `sh/2 - 38`
- Uses the same seed-derived subtitle system — no new infrastructure
- **Roughly 0.13% of runs** — players who discover it will share and discuss
- Additional echo ghosts spawn more frequently in ANOMALY seeds (extra atmosphere)
- **Costs ~5 lines** — the field `bool anomalySeed` is set once per run and checked on the title screen draw

### #4: Machinery Wake-up
- After the relay is activated (`relaysActivated >= 1`), existing flicker events become more intense:
  - Regular flickers (types 1–3) gain +0.15 `sonarFlash` — brighter, more dramatic
  - Power failures (type 4) last 30% longer — up to 4s max
  - Creates a tangible sense that the station is *reacting* to the relay activation
- Costs ~10 lines, modifies existing flicker event processing

### #3: Sonar Echo Delay
- When the player pings in a **large room** (area > 20,000 sq px = ~140×140px), a delayed secondary pulse is emitted
- Delay: `area / 40000.0` seconds — ranges from ~0.5s (small) to ~1.6s (max rooms)
- When the timer fires: emits a weak sonar pulse from the room centre with 40% brightness flash
- Guarded by `lastPingRoomIdx = -1` to prevent multiple fires on consecutive frames
- Purpose: Players subconsciously learn room size by echo timing — a skill that develops over time
- Costs ~25 lines

### End Statistics & Rank
- Both **GAME OVER** and **ESCAPED** screens show:
  - Survival time (MM:SS)
  - Scans used
  - **Alerts triggered** (sonar pulses after grace period)
  - **Rank** (S/A/B/C/Ω) computed from: `scans×2 + alerts×3 + minutes×2`
    - **Ω** (white-cyan "ECHOLESS"): 0 alerts, ≤3 scans, <2 minutes — the perfect silent run
    - **S** (gold): ≤ 6 points — minimal scans, no alerts, fast escape
    - **A** (green): ≤ 14 points
    - **B** (blue): ≤ 24 points
    - **C** (gray): > 24 points

---

## 9. Controls

| Key | Action |
|-----|--------|
| **WASD** | Move player (220 px/s, wall-collided) |
| **SPACE** | Emit sonar pulse (reveals area + shake + zoom + sound) |
| **E** | Activate relay console (hold for 2s near relay room) |
| **ESC** | Exit game |
| **R** | Restart from game-over or victory screen (new station, new enemies) |

---

## 10. Game State Machine

```
       BOOT ──→ PLAYING ─────→ GAME_OVER ──→ EXIT
         │          │               ↑
         │          └── (airlock) ─→ WON ─────┘
         │                           ↓
         │                    (R key → GameRestart)
         └── (4.5s or SPACE)
```

- **BOOT**: Dynamic title screen with "ECHO PROTOCOL" logo + procedural glow effects.
  - **Auto-sonar pulses** emit from random room centres every 1.5–3s, revealing the
    procedural station behind the logo — teaches the core mechanic before gameplay starts.
  - **#19 Adaptive Title**: If not the first attempt, shows "Attempt N — Lost Contact" (after death)
    or "Attempt N — Recovered" (after escape). Every death and escape is remembered.
  - 4.5s auto-advance, or SPACE to skip.
  - Transitions to PLAYING with `fadeAlpha = 0.0f` (no stale black overlay).
- **PLAYING**: Normal gameplay with all systems active.
  - *Relay sub-state*: Hold E near relay console for 2s with progress bar. Decays if released.
  - *Death sub-state*: 0.6s slow-motion with red vignette + CRT flash before transitioning to GAME_OVER.
  - *Escape sub-state*: 3s countdown with alarm lights + final sonar pulse → white-out (2.5s): door opens, player walks in, white light, fade to black → WON.
- **GAME_OVER**: Enemy caught player. Shows "GAME OVER" with survival time + scan count + alerts + rank. R to restart, ESC to exit.
- **WON**: Airlock reached. Shows "ESCAPED" with time + scan count + alerts + rank. R to restart, ESC to exit.
- **EXIT**: Clean shutdown, `CloseAudioDevice()`, `UnloadShader()`, `UnloadRenderTexture()`

### GameRestart()
Resets all gameplay state without re-initializing renderer or audio:
- New station seed → new layout
- Hunter relocated to nearest room to start (guarantees encounter)
- Fresh 8s grace period, fresh 8s tutorial timer
- Camera, sonar, echo memory, sound propagation all reset
- `fadeAlpha = 0.0f` (no stale black overlay)

---

## 11. Pacing & First-Time Experience

| Time | Event | Player Thought |
|------|-------|----------------|
| Time | Event | Player Thought |
|------|-------|----------------|
| T+0s | Title screen: "ECHO PROTOCOL" with pulsing glow ring + **auto-sonar pulses** reveal the station behind the logo every 1.5–3s. | *"What is this game?"* |
| T+1s | Pure silence. Fade-in completes. | *"The silence is intentional..."* |
| T+3s | Hum + static fade in. Title still visible with station sonar reveals. | *"The station feels alive."* |
| T+4.5s | Title fades out. Game begins. Station ID: "STATION KX-XXXX" appears. | *"OK, let's explore."* |
| T+5s | Tutorial text appears: "WASD to move | SPACE to ping" | *"Basic controls."* |
| T+5s+ | Player moves. Metal footsteps click. Tiny light bubble follows. | *"I can only see right around me."* |
| T+8s | Tutorial text begins to fade out gracefully. | *"The game trusts me."* |
| T+8s+ | **First SPACE press**: screen SHAKES, camera ZOOMS, world REVEALED. Blue ring expands. CRT burst. "SONAR PING" flash label. `scansUsed` increments. | *"THIS is the mechanic!"* |
| T+8s+ | Airlock compass appears (golden dot/arrow). Grace indicator shows "PULSE IS SILENT". Echo memory wireframe lingers. | *"I know where to go for now."* |
| T+8s+ | Grace period ends (8s). | *"Every ping is now a risk."* |
| T+13s | Player pings again. Hunter (one room away) hears it. ALERT — red glow at screen edge. | *"Something is coming."* |
| T+16s | Heartbeat starts (75→120 BPM, faster as Hunter closes). Hunter reaches RUSH — bright red edge glow intensifies. | *"RUN!"* |
| Death | Enemy touches player. **0.6s slow-motion**: white flash, red vignette grows from center. CRT distortion. | *"...caught me."* |
| Death+ | Game Over screen: survival time + scans + alerts + rank. | *"One more try."* |
| Relay | Player finds relay room. **Orange glow** reveals it. **Hold E** for 2s — progress bar fills. Console turns solid green. Alarm sounds. | *"Airlock is open!"* |
| Relay+ | Difficulty scales: Hunter hearing +20%, heartbeat faster, machinery louder. Watchers move toward airlock. | *"The station knows I'm coming."* |
| Escape | Player reaches airlock. **Alarm lights** flash red. "DOOR OPENS IN 3... 2... 1..." Final sonar pulse. | *"Almost there!"* |
| Escape+ | **Door opens** — white light floods in. Player walks forward. Fade to black. "ESCAPED" with time + scans + alerts + rank. | *"I made it."* |

---

## 12. Build & Size Optimisation

**CMakeLists.txt flags:**

| Compiler | C flags | Linker flags |
|----------|---------|--------------|
| GCC/Clang | `-Os -flto -s -ffunction-sections -fdata-sections` | `-Wl,--gc-sections -Wl,-s -flto` |
| MSVC | `/O1 /GL /Gy /Gw` | `/LTCG /OPT:REF /OPT:ICF` |

**Memory pools (after optimisation):**
- Enemies: 12 (down from 32)
- Sound events: 16 (down from 64)
- Memory cells: 1024 (down from 2048)
- Sample rate: 11025 Hz (down from 22050)
- Sonar reveal pulses: 8 (sent to shader each frame)

**Size savings:**
- Zero image/audio assets shipped (all procedural)
- Single shader file (3.1 KB)
- LTO eliminates dead code across translation units
- `--gc-sections` strips unused linker sections

---

## 13. Dependencies

| Dependency | Purpose | How obtained |
|-----------|---------|-------------|
| **raylib 5.5** | Windowing, input, rendering, audio, maths | `FetchContent` from GitHub (always, to guarantee API compatibility) |
| **OpenGL 3.3** | GLSL shader support | Via raylib |
| **winmm / gdi32 / opengl32** | Windows system libs | Linked on Windows |

No other libraries. No external assets. Single-binary deployment.

---

## 14. Design Principles

1. **The only way to see is by making noise.** Every system reinforces this trade-off.
2. **Procedural everything.** Zero image/audio assets — visuals come from raylib Draw* calls, lighting, animation, and shaders. Audio comes from GenWave* synthesis.
3. **Atmosphere over UI.** No minimap, no quest markers, no health bars. Direction is given through the airlock compass (diegetic). Danger is communicated through edge glow + heartbeat.
4. **Silence is gameplay.** The game defaults to pure silence. Every sound is earned.
5. **Under 1.44 MB.** Aggressive size optimisation without sacrificing visual quality.
