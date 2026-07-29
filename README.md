# 🌀 Echo Protocol

> **Navigate the dark. Survive the echoes.**
>
> A stealth/survival game where the only way to see is by making noise — but every noise reveals you to the things hunting you.

**Winner-ready · 1.44 MB Game Development Contest Entry**  
*Created by Zaid Shabir* ♥

---

## 🎮 The Core Mechanic

> ### **The only way to see is by making noise.**
> ### **But every noise helps the monsters find you.**

You are alone in a derelict space station. The lights are dead. The corridors are pitch black. Your only tool is a **sonar pulse** — press SPACE and a blue ring of light expands outward, briefly illuminating everything it touches.

But the pulse also travels as sound. And the Hunters are listening.

Every scan is a gamble. Every ping reveals the world — and your location. Navigate using fading blue wireframe memories. Move carefully. Survive.

---

## 📖 Story

*Echo Protocol* — a last-resort communication system designed to function even when all power is lost. A single pulse radiates outward, bouncing off walls, echoing through corridors, painting a ghostly picture of the world.

The station's automated defense systems were never shut down. The Watchers patrol. The Hunters listen. And they have been waiting for something to make a sound.

Find the relay room. Restore the console. Reach the airlock. Escape.

---

## 🎯 Objective

```
START → Find the relay room → Activate the console → Airlock unlocks → Reach the airlock → ESCAPE
```

| Phase | Description |
|-------|-------------|
| **1. Explore** | Move through 15–20 procedurally generated rooms connected by corridors |
| **2. Locate Relay** | Use sonar to find the relay room — it glows orange when your pulse reflects off it |
| **3. Activate** | Enter the relay room and **hold E for 2 seconds** — progress bar fills, alarm sounds, console turns green |
| **4. Escape** | Reach the airlock and hold position through a 3-second countdown. Door opens, walk into the light |
| **5. Survive** | Avoid the Watchers and the Hunter — getting caught means death. Every ping is a risk. |

---

## 🕹️ Controls

| Key | Action |
|-----|--------|
| **W A S D** | Move |
| **SPACE** | Emit sonar pulse (reveal surroundings — alerts enemies) |
| **E** | Activate relay console (hold for 2 seconds near relay room) |
| **TAB** | Open/close achievements menu (during gameplay or game-over) |
| **X** | Reset all achievements (two-step confirmation, from achievements menu) |
| **ESC** | Exit game (or close achievements menu if open) |
| **R** | Restart (from Game Over or Victory screen) |

---

## 👾 Enemies

### Watcher — The Patrol Unit
> *Slow. Methodical. Always watching.*

| Property | Value |
|----------|-------|
| Speed | 80 px/s |
| Hearing | 200 px (footsteps only — ignores sonar) |
| Behaviour | Patrols a 300px square route. Investigates footsteps. Searches for 2s then moves on. **25% chance** to guard a corridor doorway. **40% chance** to investigate the relay room. **After relay activation**: moves to cut off the airlock path. |

*Watchers make the station feel occupied. They don't chase aggressively — but they block your path and force you to time your movements.*

### Hunter — The Predator
> *Silent. Patient. Waiting for a sound.*

| Property | Value |
|----------|-------|
| Speed | 150 px/s (90 px/s when retreating) |
| Hearing | 450 px (540 px after relay activation — +20%) |

| State | Behaviour |
|-------|-----------|
| **IDLE** | Motionless in the dark. Nearly invisible. |
| **ALERT** | Hears your sonar. 0.6s dramatic charge-up. Turns toward you. |
| **RUSH** | Springs toward your last ping location at full speed. |
| **RETREAT** | If it loses you, retreats to a random room at 60% speed. Can re-engage mid-retreat if it hears another pulse. |

*The Hunter creates fear without scripts. One ping too many and the red glow at the edge of the screen turns into a death sentence.*

> **#8 Hunter Hesitation:** When the Hunter reaches the last ping location, there's a **15% chance** it pauses, slowly scans the area (0.6s), and *then* resumes the rush. This makes the Hunter feel intelligent instead of robotic — and the moment of silence before the sprint is terrifying.
> The hesitation can be interrupted by a fresh ping, instantly re-targeting the Hunter.

> **Late-game surprise — Hunter Mimics Sonar:** After relay activation, each time the player pings, the Hunter emits its **own sonar pulse** 1.8–2.2 seconds later from its current position. It briefly reveals itself — and also reveals the player. A chilling reminder that you are not alone in the dark.

---

## ✨ Features

### 🌀 Sonar Pulse (Signature Mechanic)
- Expanding blue ring with multi-layer additive glow
- Variable ring thickness (thick near origin, thin at max range)
- **#6 Ring Distortion** — Ring radius modulated with `sin(time)`, creating a subtle organic wobble that makes the pulse feel alive
- **#9 Echo Trails** — 4 small blue particles trail behind each active sonar ring, fading as the ring expands
- Trailing wake effect behind the wavefront
- **#20 Power Drain** — Each sonar ping dims the station lights for 3s — encourages careful scanning
- **#1 Echo Drift** — 2% chance per ping that the origin drifts ±2–6px independently in X and Y. The player wonders: *"Did I move?"*
- **Sonar Reflection** — echo strength varies by room type:
  - **Metal rooms** (relay, airlock): 1.35× brighter
  - **Concrete rooms** (outer): 0.65–1.0× duller
  - **Corridors**: 0.7× + stretched echoes varying by aspect ratio (long = 3 echoes, short = 2)
- **#20 Rare Event**: 1% chance per ping that a mysterious sonar pulse echoes back from somewhere else in the station. No enemy. No explanation. Just... a response.
- Camera shake + zoom pulse on every ping
- Screen-wide CRT flash via shader
- 2-second cooldown — every ping must count
- 8-second grace period before pulses alert enemies
- **#3 Broken Sonar** — 5% chance per ping that the pulse is delayed by 0.5s. Player presses SPACE → nothing happens → then PING. Creates a moment of panic and uncertainty.
- **#5 Emergency Lights** — RED/BLACK screen flashes every 25–35s — 3 rapid pulses of red overlay, pure tension
- **#18 Loose Cable Sparks** — Random `zzzt` blue-white flash at a random wall position every 6–12s
- **#15 Broken Monitor** — "ERROR" text flickering on a monitor in a random room, visible when sonar reveals the area

### 🔵 Echo Memory
- Objects revealed by sonar leave behind blue wireframe traces
- 3-second fade from bright white-cyan to transparent
- Like navigating using fading photographs of where you've been
- 64×64 pixel grid cells, up to 1024 active

### 🏚️ Procedural Station
- 15–20 rooms generated fresh each run (seeded RNG)
- L-shaped corridors connect rooms
- Random relay room, random airlock position
- Hunter always spawns nearest to your start room — guaranteed encounter
- Each run gets a unique **Station ID** (e.g. "STATION KX-4721")
- **#17**: Each facility also gets a procedural **name** from a pool of 15 (`ORPHEUS`, `EREBUS`, `NEMESIS`, etc.) — players remember "my run on Facility TITAN"
- **#26 Hidden Seed Challenge**: Seeds divisible by 777 show `[ANOMALY]` on the title screen — a rare discovery for dedicated players
- **#19 Station Age**: Every station has a seed-derived age (1–100 years). Older stations experience more frequent electrical flickers, sparks, and CRT glitches — the infrastructure degrades visibly.
- **#18 Dynamic Room Names**: When entering a room, a procedural sector label appears top-left (e.g. "SECTOR C-12 — Maintenance", "SECTOR A-07 — Cryogenics"). Names are deterministic per seed — no two runs feel the same.
- **#11 Station Motto**: Each facility also displays a unique motto ("We Listen.", "No Signal Returns.", "The Dark Remembers.") — adds identity and atmosphere at zero size cost
- **#18**: One random room per station contains a **lore terminal** — a one-line story fragment revealed by sonar pulse
- **#15 Impossible Room**: 0.2% chance per run that a single non-important room becomes marked as an **anomaly**. The room looks normal, but when a sonar pulse passes through, the player sees: *"This room should not exist."* — No explanation. No gameplay. Just an easter egg that makes players wonder.
- **#22 Hidden Observation Room**: 0.5% chance per run that a room has one-way glass. When a sonar pulse passes through, the player sees: *"Behind the glass... a silhouette."* — Was it always there? Players will share screenshots.
- **#10/#15 Water & Rust**: Each run randomly marks 0–2 rooms as **wet** (footsteps 1.8× louder — splashing) and 0–2 rooms as **rusty** (footsteps 2× louder — creaking metal). Forces the player to choose between quiet routes and fast routes.
- No two runs are the same

### 🎬 Dynamic Title Screen
- **GAME_STATE_BOOT** shows "ECHO PROTOCOL" title card
- Auto-sonar pulses from random room centres every 1.5–3s
- Reveals the procedural station behind the logo — teaches the mechanic before gameplay starts
- **#19**: Shows attempt count and outcome ("Attempt 3 — Lost Contact" / "Recovered") — after every death or escape, the title remembers your last run
- 4.5s auto-advance or SPACE to skip

### 🔊 Procedural Audio (All Synthesised — Zero Audio Files)
| Sound | Source | Behaviour |
|-------|--------|----------|
| Electrical hum | 50 Hz sine wave | Pitch + volume increase with enemy proximity (**Adaptive Hum**) |
| Ambient static | White noise | Pitch increases when Hunter is near |
| Metal footsteps | 600 Hz sine burst | Triggered by movement |
| Heartbeat | 40 Hz sine thump | 75–120 BPM, faster when enemies close; 43% faster after relay |
| CRT interference | Noise burst on sonar ping | — |
| **Panic Breathing** | Low modulated noise | Starts when Hunter < 180px, pitch ramps with proximity |
| **#13 False Heartbeat** | Single heartbeat thump | Randomly triggers every 30–36s even when no enemy is nearby. Player freezes, looks around, sees nothing. Psychological horror. |
| **#11 Announcements** | Procedural word synthesis | Distorted speaker phrases every 20–60s ("...Attention... Sector... offline...") — pure atmosphere |
| **#14 Radio Burst** | Random voice fragments | Rare distorted transmissions every 35–40s ("...HEL—", "...DON'T—", "...IS ANYONE—"). Never complete. |
| # **Distant machinery | 60 Hz square wave | 2× louder + more frequent after relay |
| Silence | First 3 seconds | Pure silence to build atmosphere |

### 🌑 Darkness Shader
- 3% ambient brightness floor
- 90px personal glow bubble around the player
- Up to 8 sonar reveal circles per frame + ring-edge wavefront glow
- CRT scanline bands + flicker + chromatic shift + edge static
- Quadratic vignette darkening at screen edges
- Screen-wide flash on sonar pulse

### 💓 Game Feel & Screen-Space Effects
| Effect | Trigger |
|--------|---------|
| Screen shake | Sonar pulse (10px first ping, 5px subsequent) |
| Camera zoom | Sonar pulse (1.04× first ping, 1.025× subsequent) |
| Heartbeat ring | Enemy within 220px |
| Red edge glow | Hunter in ALERT or RUSH state |
| Golden compass | Points toward airlock after first ping |
| **#7 Light Leak** | Tiny white flicker pulses under the closed airlock door — a reminder that escape is possible |
| **#12 Moving Shadows** | During CRT flicker events, a brief shadow shape appears near the edge of the screen for a single frame. Nothing is actually there. |
| **#16 CRT Burn-in** | Brief white-blue screen flash overlay (0.06 alpha) after each sonar ping or CRT flicker — simulates screen persistence |
| **#13 Reactor Pulse** | A subtle 2% global brightness surge every 12s — like huge generators thrumming deep in the station |
| Orange room glow | Sonar reflects off relay room |
| Console sparks | Blinking orange panel near relay room |
| **#5 Terminal Numbers** | "NODE 14", "SECTOR 07", "TEMP 281K" displayed on relay console — changes colour from orange to green on activation |
| **#5 Security Cameras** | Red blinking LEDs at the top of every room. Blink at 2Hz with room-unique phase. Visible when sonar reveals the area. They do nothing — but players always think they're dangerous. |
| Activation progress | Green bar fills as you hold E at relay console |
| **Near Miss** | Hunter rushes within 10px → 0.2s slow-mo burst + heart thump |
| **#3 Corrupted Compass** | Airlock compass jitters erratically when Hunter < 120px — player panics |
| **Room Wall Glow** | Cyan additive wall outline when sonar pulse passes through — the station lights up where you scan |
| **Player Trail** | 3 faint blue circles trail behind movement — gives sense of motion in darkness |
| **Pulse Particles** | 8 dots scatter from the sonar ring edge as it expands — adds life to the pulse |
| **Alert Flash** | Expanding white ring bursts from an enemy when it detects you — dramatic feedback |
| **Airlock Beacon** | Pulsing green vertical beam + ring glow above and below the escape door — always visible |
| **#8 Ceiling Dust** | When the Hunter rushes within 200px of the player, small dust particles fall from the ceiling. Makes the Hunter feel powerful and the station feel aged. |
| **#6 Ceiling Pipes** | 2–3 thin procedural horizontal lines + occasional steam puffs near room ceilings — adds depth |
| **#7 Floor Numbers** | Tiny painted text ("B12", "A07", "HX4") in bottom corner of every room — adds realism |
| **Sonar Saturation** | Spamming sonar fills screen with noise — vision degrades |
| Death slow-mo | 0.6s red vignette + white flash + CRT distortion |
| **Last Echo** | Sonar pulses keep expanding during death — player watches the last ripple |
| **Panic Breathing** overlay | Dark vignette + breathing when Hunter < 150px |
| **#12 Panic Vision** | Player glow radius **shrinks** when Hunter < 150px — exactly when you most need to see |
| Alarm lights | 8Hz flashing red borders during airlock escape |
| **#16 Dynamic Ending** | Escape screen varies by alert count — ≤1 alert = silent dark exit, ≥3 = frantic red alarms |
| White-out escape | Door opens, player walks in, bright white light, fade to black |
| End rank + **#15 Ω (ECHOLESS)** | S/A/B/C rank + **Ω tier** for 0 alerts, ≤3 scans, <2 minutes |
| **#25 Final Black Screen** | Brief 1s black pause before game-over stats — dramatic beat before failure |
| Cooldown ring | Bottom-right ring fill indicator |
| **Station ID + Facility Name** | "STATION KX-4721 — FACILITY ORPHEUS" in top-right corner

---

## 🎨 Visual Style

**No textures. No sprites. No image assets.**

Everything is drawn procedurally using raylib's drawing primitives — circles, lines, rectangles, rings, and GLSL shader effects. The visual identity comes from:

- **Colour**: Cyan sonar, red enemies, orange relay, green airlock
- **Lighting**: Additive blending for all glow effects
- **Animation**: Pulsing rings, expanding wavefronts, drifting noise
- **Shader**: CRT scanlines, vignette, flicker, chromatic aberration

---

## 🏗️ Technical Overview

```
Language    : C99
Graphics    : raylib 5.5 + GLSL 330 shaders
Target      : Windows 64-bit (MSVC / MinGW)
Build       : CMake ≥ 3.16
Exe size    : < 1.44 MB (single floppy disk)
```

### Architecture

```
main.c          → Entry point, game loop, fullscreen toggle
game.c          → Orchestrator: init, update, draw, HUD, state machine
player.c        → WASD movement, glow rendering
sonar.c         → Pulse emission, expansion, decay, cooldown
echomemory.c    → Grid-cell capture, wireframe fading, compaction
enemy.c         → Watcher + Hunter AI, patrol, sound queries, retreat
map.c           → Procedural station generator, wall collision
renderer.c      → Render texture, darkness shader, sonar bridge
audio.c         → Manual wave synthesis (all sounds procedurally generated)
soundprop.c     → Sound event propagation, spatial queries
collision.c     → Circle-circle, circle-rect primitives
procedural.c    → xorshift64* seeded PRNG
easing.h        → Shared easing utilities
achievements.c  → 20 persistent achievements, save/load, popup notifications
```

### State Machine

```
BOOT ──→ PLAYING ─────→ GAME_OVER ──→ EXIT
  │          │               ↑
  │          └── (airlock) ─→ WON ─────┘
  │                           ↓
  │                    (R key → GameRestart)
  └── (4.5s or SPACE)
```

---

## 🔧 Build Instructions

### Requirements
- CMake ≥ 3.16
- C99 compiler (MSVC, MinGW-w64, or GCC)
- Git (for CMake FetchContent to pull raylib 5.5)

### Windows (MSVC)
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Windows (MinGW)
```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

### Cross-compile to Windows from Linux
```bash
sudo apt install mingw-w64
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
```

---

## 📦 Size Optimisation

| Technique | Effect |
|-----------|--------|
| `-Os -flto -s` | Aggressive size optimisation + link-time optimisation |
| `-ffunction-sections -fdata-sections` + `--gc-sections` | Strips unused code |
| Zero image/audio assets | All procedural — no embedded files |
| Single shader (3.1 KB) | One `.fs` file for all darkness/CRT effects |
| 11025 Hz sample rate | Minimal audio quality for size |
| Compact memory pools | Enemies: 12, Sound events: 16, Memory cells: 1024 |

---

## 🏆 Contest Fit

**Echo Protocol** was designed from day one for the **1.44MB Game Development Contest**.

| Requirement | Echo Protocol |
|-------------|---------------|
| Exe size < 1.44 MB | ✅ Verified Release build |
| No external assets | ✅ All procedural — zero images, zero audio files |
| Original mechanic | ✅ Sonar pulse — the only way to see |
| Fun in 5 minutes | ✅ First death in 30s–3min, first win in 3–10min |
| Memorable moment | ✅ First sonar ping — screen shakes, world reveals, Hunter stirs |
| Replayable | ✅ Procedural station + enemy placement = different every run |

---

## 🧠 Design Principles

1. **The only way to see is by making noise.** Every system reinforces this trade-off. No minimap, no wallhacks, no free information.
2. **Procedural everything.** Zero assets. Visuals come from Draw* calls, lighting, animation, and shaders. Audio comes from `sinf()` and `GetRandomValue()`.
3. **Atmosphere over UI.** No health bars, no quest markers, no tutorials after 8 seconds. Direction comes from a golden compass. Danger comes from a red glow at the edge of the screen.
4. **Silence is gameplay.** The game opens with 3 seconds of pure silence. Every footstep, every hum, every heartbeat is earned.
5. **Elegance through constraint.** 1.44 MB forces every byte to matter. No wasted code. No unused systems. Every mechanic strengthens the core loop.

---

> *"I could only see by making noise — but every noise helped the monsters find me."*
>
> That's the only idea you need to remember. The rest is survival.
