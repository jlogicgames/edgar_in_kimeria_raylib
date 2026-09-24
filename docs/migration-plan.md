# Migration plan: Rust/Bevy → C99/raylib/Flecs

Reference implementation: `../edgard_in_kimeria_rs` (Rust 2024, Bevy 0.19, about 7,600
lines across `src/`, `tests/` and `assets/shaders/`). The raylib port must reach
**feature parity** with it: everything in the Rust README's *Feature parity* list and its
*Deliberate design choices*. It does **not** carry over the Rust version's bugs. The Rust
README keeps several of them on purpose as *Faithfully preserved oddities*; section 3.10
lists which ones this port fixes and how. The port also picks up three open tickets from
the sibling ports (section 6).

Structure borrowed from `../Edgard_arcade/docs/parity-plan.md` (the Godot port's plan).
That plan compared two existing ports. This one plans a rewrite from an empty repository,
so it adds an architecture section (2) and a constants appendix (A) that the rewrite can
be checked against.

Legend: **[PARITY]** port as-is from Rust · **[FIX]** Rust behaviour that is a bug; the
port behaves correctly instead, and the change goes into the README's deviations list ·
**[NEW]** not in Rust; comes from a ticket, a new platform, or a decision in section 7.

---

## 1. Scope

### 1.1 Target platforms

| Target | Backend | Status in Rust | Plan |
|---|---|---|---|
| macOS | raylib `PLATFORM_DESKTOP` (GLFW, OpenGL 3.3) | shipped | **in scope** |
| Web (GitHub Pages) | raylib `PLATFORM_WEB` (Emscripten, WebGL 1) | shipped via Trunk | **in scope** |
| iOS | raylib `PLATFORM_DESKTOP_SDL` on SDL, OpenGL ES (section 1.4) | none | **in scope**, starting with a feasibility spike |
| Linux, Windows | raylib `PLATFORM_DESKTOP` | shipped | out of scope for now |
| Android | raylib `PLATFORM_ANDROID` | none | out of scope for now |

Out-of-scope platforms aren't blocked: the code stays portable C99 behind raylib, and
nothing platform-specific leaks out of `platform_*.c` (section 2.3). Adding one later is a
build target plus a settings path, not a port.

### 1.2 Toolchain

- **C99** for all first-party code: `-std=c99 -Wall -Wextra -Wpedantic -Werror` (clang).
- **raylib 5.5**, pinned via CMake `FetchContent` to a tag, never a branch. Needed 5.x
  APIs: `LoadSoundAlias` (overlapping one-shots), `ToggleBorderlessWindowed`,
  `GetApplicationDirectory`, and the SDL platform backend for iOS.
- **Flecs v4** (latest v4.x release, pinned), vendored as its amalgamated `flecs.c` /
  `flecs.h` under `third_party/flecs/`. Flecs is written in C99. It's built with
  `FLECS_CUSTOM_BUILD`, keeping only the addons the game uses: `FLECS_SYSTEM`,
  `FLECS_PIPELINE`, `FLECS_LOG`, and in dev builds on macOS `FLECS_REST` plus `FLECS_STATS`
  for the Flecs Explorer (section 3.9, I7).
- **yxml** (MIT, single file, C89), vendored under `third_party/yxml/` for `.tmx`/`.tsx`
  parsing. The maps load through Tiled's own format (section 3.1), and a streaming parser
  keeps the code small without a hand-rolled XML reader to debug. `cute_tiled.h` only
  reads JSON maps, which would mean converting the maps.
- **CMake ≥ 3.24**: the Xcode generator with `CMAKE_SYSTEM_NAME=iOS` for iOS;
  `emcmake cmake` for web.
- No other first-party dependencies: random numbers, the string table, the INI settings
  file and the test harness are written in-tree.

**Warning flags and `third_party/`.** `-Werror` covers everything under `src/` and
`tests/`. The vendored `flecs.c` and `yxml.c` compile with their upstream defaults instead.
This is a different category, not an exemption for convenience: they're unmodified
upstream releases pinned by version. A warning in them can only be fixed by forking, and a
fork is what vendoring a pinned release avoids. The headers are included with
`-isystem` so they don't hide warnings in our code either.

### 1.3 Assets

Copy `assets/` from the Rust repository: `audio/`, `fonts/`, `images/`, `tiles/`. Replace
`shaders/*.wgsl` with GLSL (R6). One map edit is deliberate: see L3. For iOS touch
controls (decision D10), bring `Joystick.png`, `Knob.png` and `JumpButton.png` over from
the Flutter original's `assets/images/HUD/`.

### 1.4 iOS risk

raylib has no official iOS platform, so this is the plan's largest unknown and is tackled
first (Phase 0b), before any gameplay code depends on it.

- **Route A (preferred):** raylib's `PLATFORM_DESKTOP_SDL` backend on SDL for iOS, built
  with `GRAPHICS_API_OPENGL_ES3`. SDL provides the window, the touch input, MFi/Xbox/PlayStation
  gamepads through GameController, the app lifecycle, and `SDL_GetBasePath` for bundle
  resources. OpenGL ES has been deprecated on iOS since iOS 12 but still ships and runs.
- **Route B (fallback):** the same SDL backend with OpenGL ES provided by **ANGLE over
  Metal**, if Route A hits a driver problem or a future iOS removes OpenGL ES. The game's
  code doesn't change between the two routes; only the linked GL library does.
- **Spike exit criteria:** the Phase 0 scene (a tile, a sprite, a shader, a sound)
  running on a physical iPhone and the simulator, at 60 fps, with a touch and a paired
  gamepad both reaching `InputFrame`. If neither route passes, stop and re-plan iOS before
  Phase 1.

---

## 2. Architecture: Flecs

Bevy gives the Rust port an ECS, a schedule, states, an asset server, a render graph and
`bevy_ui`. Flecs replaces the ECS, the schedule, states and events; raylib replaces
rendering, audio, input and windowing. Rows marked *load-bearing* change gameplay if
they're ported loosely.

### 2.1 Bevy → Flecs mapping

| Bevy concept | Flecs / C99 design |
|---|---|
| `#[derive(Component)]` structs | `ECS_COMPONENT_DECLARE` / `ECS_COMPONENT_DEFINE` on plain structs: `Position`, `BoxSize`, `Velocity`, `Facing`, `Hitbox`, `Gravity`, `Grounded`, `ContactState`, `ActorStateC`, `AnimPlayer`, `ZLayer`, and so on. Marker components (`Player`, `Enemy`, `Bat`, `Checkpoint`) are `ECS_TAG`s. |
| `GamePos` in Tiled space, `sync_transforms` | Keep **Tiled space** (y down, top-left anchor) in `Position`. raylib's 2D space is also y down, so there's no projection step; the concept is deleted rather than ported. |
| Plugins | Flecs **modules** (`ECS_MODULE`), one per Rust plugin: `CoreModule`, `LevelModule`, `PlayerModule`, `EnemyModule`, `ItemsModule`, `ObjectsModule`, `EffectsModule`, `UiModule`, `AudioModule`, `DevModule`. `ECS_IMPORT` them in `main.c`. |
| Systems + queries | `ECS_SYSTEM` with query signatures. Single-threaded (no `ecs_set_threads`): raylib isn't thread-safe, and determinism matters more than parallelism for a game this size. |
| Schedule sets (`PreUpdate`, `FixedUpdate`, `Update`, `PostUpdate`) | Three custom pipelines, each selecting systems by a phase tag, run explicitly from the main loop (section 2.2). |
| `Time<Real>`, `Time<Virtual>`, `Time<Fixed>` | A `GameTime` singleton: `real_dt`, `virtual_dt`, `time_scale`, `fixed_dt = 1/60`. Each system reads the clock it needs. |
| Commands, `try_despawn` | Flecs defers structural changes made inside a running system and applies them at the end of it, as Bevy does for commands. `ecs_delete` on an entity that is already gone is a no-op, so no `try_` variant is needed. *load-bearing:* inside one system run, an entity deleted earlier in the same iteration is still visible, so gameplay checks `Dying` tags rather than liveness, mirroring Rust's `Without<Dying>` filters. |
| `LevelEntity` + despawn on reload | A `LevelEntity` tag; level unload is `ecs_delete_with(world, LevelEntity)`. |
| `AppState` + `DespawnOnExit(state)` | `AppState` singleton plus one entity per state. Menu entities carry the pair `(DespawnOnExit, <state>)`; leaving a state runs `ecs_delete_with(world, ecs_pair(DespawnOnExit, state))`. |
| `run_if(in_state(...))` | Each system is registered with a `RunIn` state mask. `app_set_state()` calls `ecs_enable(world, system, mask & state)` on every registered system, so disabled systems cost nothing. |
| `OnEnter` / `OnExit` | `app_set_state()` calls the enter/exit functions registered per state. |
| Observers (`PlayerKilled`, `EnemyStomped`, `CheckpointReached`) | Flecs custom events: an `ecs_entity_t` per event, `ecs_emit` to raise it, `ECS_OBSERVER` to handle it. `EnemyStomped` is emitted **on the enemy entity** with a `{ bool by_attack; }` payload, like Bevy's `EntityEvent`. |
| Messages (`TriggerActivated`, `LoadLevel`, `AdvanceLevel`) | `TriggerActivated` becomes a custom event with a `{ char target_id[32]; }` payload, observed by walls, escalators and torches. `LoadLevel` / `AdvanceLevel` set a `PendingLevel` singleton that the pre-update pipeline handles at the start of the next frame, matching Rust's `PreUpdate` placement. |
| `PlaySound` messages | A direct `audio_play(name, volume)` call; audio isn't an ECS concern. |
| `CollisionWorld` snapshot per fixed step | The same: the first fixed-phase system copies every active block, escalator and falling platform into a `CollisionWorld` singleton. *load-bearing:* resolution order plus break-on-first-contact decides where the actor ends up, and Flecs iterates by table (archetype), so an Actionable wall would come after the plain blocks. Give each spawned object a `SpawnIndex` (its position in the Tiled object list) and sort the snapshot by it, which is deterministic across runs and platforms. |
| `AnimationSet` / `AnimationPlayer` / `ActorState` | `AnimSet` is a pointer to a static clip table indexed by `ActorState`; `AnimPlayer { current, frame, elapsed, finished }`. A state change resets playback, and `finished` is polled, as in Rust. |
| `Anchor::TOP_LEFT` + `flip_x` | `DrawTexturePro` with a negative source width to flip. The box stays at `Position`, and the hitbox is mirrored *inside* the box (`hitbox_left_x` / `mirrored_pos`). *load-bearing.* |
| Parent/child (`ChildOf`) | Flecs `EcsChildOf`. Deleting a parent deletes its children, as Bevy does: the torch glow, the firefly halo, and the falling platform's warning torch. |
| Asset server + `Loading` state | Synchronous loading at startup into an `Assets` singleton. On failure, print the full resolved path and exit with an error, as the Rust panic does. |
| `GameSettings`, `GameProgress` | Singletons. |
| `bevy_ui` | Immediate-mode UI drawn by systems in the render pipeline (section 5); widget state such as focus, easing and entrance timing lives in components on menu entities. |
| `rand` | A seeded xorshift32 in `rng.c`. |

### 2.2 Frame pipeline

The main loop owns ordering and runs three Flecs pipelines plus rendering. Each pipeline
is built with `ecs_pipeline_init` from a phase tag, and phases inside it are ordered with
`EcsDependsOn`, so the order below is data, not convention.

```
frame():
  real_dt = min(GetFrameTime(), 0.25)
  ecs_run_pipeline(world, PrePipeline, real_dt)        // 1–2
  accumulator += real_dt * time_scale
  while (accumulator >= 1/60):                          // 3
      ecs_run_pipeline(world, FixedPipeline, 1/60)
      accumulator -= 1/60
  ecs_run_pipeline(world, UpdatePipeline, real_dt)     // 4–5
  render()                                              // 6
```

1. **Pre** (`PrePhase`): handle `PendingLevel` (unload, load, snap the camera).
2. **Input** (`InputPhase`): build the `InputFrame` singleton from keyboard, touch, every
   connected gamepad, and the capture script's overlay (I5). No system calls `IsKeyDown`
   directly.
3. **Fixed** (`FixedCollect` → `FixedActors`): collision snapshot, then player physics, bat
   movement and ground-mob AI. *load-bearing:* bullet time runs **fewer steps**, it doesn't
   shrink the step.
4. **Update**, phases in Rust's chained order: `PlayerLogic` (`read_input → bullet_time →
   player_routines → resolve_attack → apply_facing`), `ItemLogic` (`collect_items →
   touch_bombs → touch_checkpoints → track_trigger_overlap`), `EnemyLogic` (`touch_damage →
   red_mob_attack_state → despawn_dead_enemies`), `ObjectLogic` (escalators, falling
   platforms), `Effects` (particles, torches, fireflies, shockwaves, explosions, fog,
   ripples), `CameraPhase` (follow, backdrop), `UiLogic`.
5. **Animation** (`AnimPhase`, real time).
6. **Render**, outside Flecs' progress loop but using Flecs queries: world into the render
   texture in explicit layer order (sky, tiles, actors, torches, effects), then the post
   pass, then UI and HUD. Explicit layer passes replace Bevy's z sort, and there's no depth
   buffer.

Within one phase, Flecs runs systems in declaration order, so each module declares its
systems in the order listed. That's the equivalent of Rust's `.chain()`.

### 2.3 Source layout

```
CMakeLists.txt
src/
  main.c              world setup, module imports, frame loop; emscripten main loop on web
  app.c/.h            AppState, RunIn masks, pipelines, clocks
  platform_macos.c    settings path, asset root
  platform_ios.c      settings path, asset root, lifecycle (pause on background)
  platform_web.c      canvas resize, start screen gate (T3)
  components.h        every component and tag
  input.c/.h          InputFrame from keyboard, touch, gamepad and capture script
  assets.c/.h         textures, sounds, fonts, shaders, anim clip tables
  anim.c/.h
  collision.c/.h      pure functions: overlaps, resolve_horizontal/vertical, gravity
  tmx.c/.h            yxml-based .tmx/.tsx reader
  mod_level.c mod_player.c mod_enemy.c mod_items.c mod_objects.c
  mod_effects.c mod_camera.c mod_ui.c mod_audio.c mod_dev.c
  render.c/.h         world RT, post pass (ripple + chroma), letterbox, layer passes
  touch.c/.h          on-screen controls (iOS, D10)
  l10n.c/.h           Msg table EN/UK
  settings.c/.h       settings.ini load/save (T2)
  rng.c/.h
third_party/flecs/  third_party/yxml/
assets/             copied from Rust; shaders/ holds GLSL
tests/              test_collision.c, test_triggers.c, test_tmx.c, test_l10n.c, test_settings.c
ios/                Info.plist, launch screen, asset catalog
web/shell.html
.github/workflows/  ci.yml, deploy-web.yml
docs/
```

Gameplay modules and `collision.c` may use raylib's value types (`Vector2`, `Rectangle`)
but never a function that needs a window. The tests (section 8) create a Flecs world,
import the gameplay modules and run systems without opening a window, the way Rust's
`tests/triggers.rs` builds a headless `App`.

---

## 3. Feature inventory

Each row is one feature, with the Rust source to check against. The numbers are in
appendix A.

### 3.1 Level data and tilemap

| # | Kind | Feature |
|---|---|---|
| L1 | PARITY | Parse `forest-1.tmx` and `forest.tmx`: one CSV tile layer (`Background`), one external tileset (`Forest.tsx`, 20 columns, 16 px tiles, `Tileset.png`), and the object layers `SpawnPoints` and `Collisions`. Mask Tiled's flip bits off each gid, and fail loudly on any encoding other than CSV. `level.rs` |
| L2 | PARITY | Object fields: `type` (the class), `name` (the Collectable kind and the Trigger/Actionable id), `x`, `y`, `width`, `height`. Properties `offNeg`, `offPos` (int **or** float), `isVertical`, `Intensity`, and `type` (the Actionable kind: `Torch` / `Wall`). Unknown classes log a warning. |
| L3 | FIX | Rust ignores object-layer offsets, because `forest.tmx`'s `SpawnPoints` has a stray `offsety="-16"` that the entity positions were never authored against. The port **honours offsets**, as Tiled defines them, and removes the stray attribute from its copy of `forest.tmx`, so every entity lands where it does today. Check: the F1 overlay in `forest` matches a Rust capture pixel for pixel. |
| L4 | PARITY | Collision block classes: `Platform`, `QuickSand` and `Wall`; anything else is `Solid`. |
| L5 | PARITY | Level rotation `["forest-1", "forest"]`; the checkpoint wraps back to the first level. |
| L6 | PARITY | Tile rendering: draw only the visible tile range, nearest filtering. The map size (in pixels) feeds the firefly area. |

### 3.2 Player (`player.rs`)

| # | Kind | Feature |
|---|---|---|
| P1 | PARITY | 48×48 box (the Tiled size is ignored), hitbox (18, 26, 11, 22). Input is read per frame and consumed by the fixed step. |
| P2 | PARITY | Held jump: holding the key auto-hops. Jump force 260; in quicksand ×0.1. Jump sound. |
| P3 | PARITY | Coyote rule: grounded until `vy > 9.8·15`. |
| P4 | PARITY | Movement 100 px/s, ×0.1 in quicksand, plus the velocity of the escalator being ridden (see O8). Facing follows input only. |
| P5 | PARITY | Clamber: a `Wall` hit while airborne sets `clambering`, and vy ×0.1 each step. Wall jump: vy = −182, vx = ±130, with the push direction chosen by the input/facing rule. 0.1 s input lockout. |
| P6 | PARITY | Attack: only when grounded, not jumping and not clambering. Freezes horizontal movement for the whole 7-frame clip. Attack box per `attack_rect`. Hits the first overlapping enemy each frame. |
| P7 | PARITY | Death plane at y > 380. |
| P8 | PARITY | Routines: `Active → Dying` (Hit clip) `→ Reappearing` (Appear clip at the start position, facing right, zero velocity) `→ Active`. `LeavingLevel` counts 3 s on **real** time, then advances the level. |
| P9 | FIX | **Three deaths, not four.** Rust decrements `lives` only while it's above 0, an off-by-one that allows a fourth death. The port ends the run when the third death's reappear finishes. |
| P10 | FIX | **Lives persist across levels.** Rust stores them on the player, which is re-spawned for every level, so lives quietly refill at each checkpoint. They move into `GameProgress`, and a new run resets them. |
| P11 | PARITY | Invulnerability (F2 / `EIK_INVULNERABLE`) ignores every kill. |
| P12 | PARITY | Bullet time: if the player's hitbox is within 50 px (rect-to-rect distance) of any bat, `time_scale` is 0.5. It resets to 1.0 on leaving `Playing`. |
| P13 | FIX | Gravity is expressed **per second** (588 px/s² = 9.8 × 60) and multiplied by the fixed dt, instead of Rust's 9.8 added per step. At the fixed 60 Hz this is the same arc to within float rounding (the ported test compares with a tolerance); it stops the value silently changing meaning if the step rate ever does. Jump force, terminal velocity and the coyote threshold stay velocities. |

### 3.3 Enemies (`enemy.rs`)

| # | Kind | Feature |
|---|---|---|
| E1 | PARITY | Bat: patrols along one axis at 50 px/s between `pos ∓ off·16`. No gravity. |
| E2 | PARITY | Bat contact kills the player unless the player is mid-attack. |
| E3 | PARITY | Yellow and Red mobs: idle unless the player's x is in `[x − offNeg·16, x + offPos·16]` and they overlap vertically; then they run toward the player at 80 px/s. `move_direction` lerps toward the target direction at 0.1 per step and drives facing only. Terrain collision uses the shared resolver. |
| E4 | PARITY | Yellow mob contact: a stomp if the player is falling onto it (bounce vy = −260, bounce sound), otherwise the player dies. |
| E5 | PARITY | Red mob: if the player is inside ±65 px of its reference x, it starts its attack (4 frames × 0.2 s), freezes, and kills the player on every step while the player stays in range. |
| E6 | FIX | **After an attack the Red mob walks back to its spawn x** at its run speed, instead of Rust's `x += 300` teleport. The Flutter source's own comment says the intent was "back to initial position after attack". Same decision as the Godot port (its E5). |
| E7 | FIX | **Red mob body contact behaves like the Yellow mob's:** a stomp kills it and bounces the player; any other contact kills the player. In Rust the player's contact check simply skips Red mobs, although the Red mob's stomp handling exists. Same decision as the Godot port (its E6). |
| E8 | PARITY | A killed enemy plays its Hit clip, then is deleted. It's tagged `Dying` and excluded from AI and contact meanwhile. |
| E9 | FIX | Circle hitboxes (bat r = 8, coins, bombs) use a real circle-vs-rectangle test (`CheckCollisionCircleRec`) instead of Rust's bounding box. The difference is under a pixel at the corners, but the correct test costs nothing in raylib. |

### 3.4 Items and interactables (`items.rs`, `objects.rs`)

| # | Kind | Feature |
|---|---|---|
| O1 | PARITY | Coin: +1 to the coin counter, collect sound, screen ripple at its centre. |
| O2 | NEW | Heart: collect sound and a shockwave ring, **plus +1 life, capped at 3** (decision D9, matching the Godot port's O4). At full lives the heart is still collected and still plays the shockwave. Rust grants nothing. |
| O3 | PARITY | Bomb: bounce sound, explosion shader at its centre, bomb removed, player killed. |
| O4 | PARITY | Checkpoint: invisible, hitbox 16×32. Triggers the player's `LeavingLevel` routine (disappear sound and clip, then 3 s). Visible only in the F1 overlay. |
| O5 | PARITY | Trigger: invisible; F1 shows it. Tracks which trigger id the player stands in. Interact (L/C/North, or the touch button) emits `TriggerActivated(id)`. |
| O6 | PARITY | Actionable Wall: a `Wall`-kind block, removed permanently when triggered. |
| O7 | PARITY | Actionable Torch: starts lit only if `Intensity > 0` (default 0). A toggle switches between off and intensity 200. |
| O8 | FIX | Escalator: patrols one axis at 50 px/s over `off·32`; a horizontal escalator flips its sprite at each end; a trigger toggles running/idle, changing the clip. **A vertical escalator carries its rider vertically:** Rust adds only the x velocity, so on a descending escalator the rider free-falls onto it every step and flickers into the Falling animation. The port moves a grounded rider by the escalator's per-step displacement on both axes. Check: ride the `forest-1` escalators with F1 on; the state stays Idle/Running. |
| O9 | PARITY | Falling platform: landing on it from above starts a 1 s warning (a torch with intensity 5, the Falling clip). The torch then goes out, the platform drops 200 px linearly over 1.5 s, and the platform and torch are deleted. Both timers run on virtual time. |
| O10 | FIX | **Touching a falling platform from below or the side no longer kills.** Rust flags any overlap while not falling as `hit_platform_from_below` and kills the player, so a jump that clips the platform's edge is fatal. The port treats it as a solid block from below (the player bumps their head, vy = 0) and from the sides. |
| O11 | PARITY | Plain `Torch` objects default to intensity 80. |

### 3.5 Effects (`effects/`)

| # | Kind | Feature |
|---|---|---|
| X1 | PARITY | Torch: a flickering green glow disc plus four emitters (core flame, embers, sparkles capped at 40 per burst, grey smoke). Particles move linearly from→to, shrink and fade. |
| X2 | PARITY | Generated textures: `glow` (32 px, falloff 2) and `dot` (8 px, falloff 8), built pixel by pixel at startup, never shipped as files. |
| X3 | PARITY | Level fireflies: 24 in `forest`, black 3 px dots on quadratic Bézier flights of 3–7 s, hidden for 1–2 s between flights, fading in over the first half and out over the second. |
| X4 | PARITY | Menu fireflies: 24, gold, a 7 px glow core with a ×4 halo, a 3 Hz pulse with a per-firefly phase, and a UI exclusion rectangle. The wander area tracks the visible view. |
| X5 | PARITY | Fog shader: menu-only backdrop (Main, About, Options), stretched to the visible area, with the aspect-ratio correction using the real size. |
| X6 | PARITY | Shockwave shader: a ring of radius 64 and width 8 over 0.6 s, straight alpha. |
| X7 | PARITY | Explosion shader: 64 px over 0.7 s. Keep the `1.0 - smoothstep(0.0, 0.18, x)` form, because reversed-argument `smoothstep` is undefined in GLSL too. |
| X8 | PARITY | Screen ripple (post pass): 0.75 s, max radius 300 px, strength 12 fading with progress, frequency 60, decay 20, aspect-corrected. Only one ripple is live at a time. |
| X9 | PARITY | Chroma "poison" glitch (post pass): random intervals that get stronger over time. Off by default; `EIK_CHROMA_GLITCH` enables it. It resets on entering `Playing` and shares the ripple's pass. |
| X10 | PARITY | Sky backdrop: tiled horizontally, scrolling 5 px/s and wrapping at one tile, drawn behind the world, covering the whole visible area. Visible only in `Playing`, `Paused` and `GameOver`. |

### 3.6 Rendering

| # | Kind | Feature |
|---|---|---|
| R1 | PARITY | Logical resolution 640×360 with Bevy's `ScalingMode::AutoMin`: at least 640×360 world units are visible, and a window with a different aspect ratio shows **more** world along the free axis. This matters on iPhones (about 19.5:9). Size the world `RenderTexture2D` to `window / scale`, where `scale = min(w/640, h/360)`, and recreate it on resize or rotation. |
| R2 | PARITY | Nearest filtering everywhere, including the world render texture when it's scaled up. |
| R3 | PARITY | Camera: target top-left = `(x − 200 − 11, y − 200)` when facing right, `(x − 400 − 33, y − 200)` when facing left. Moves toward the target at up to 500 px/s. Snaps on level load. Menus reset it to the origin. |
| R4 | PARITY | Layer order, back to front: sky, tiles, actors, torches, shockwave and explosion, fog (menus). |
| R5 | PARITY | Post pass: world RT → `screen_effects.fs` (ripple + chroma) → window, letterboxed. The UI is drawn after, at window resolution. |
| R6 | PARITY | Shaders written once in GLSL, with a version header prepended at load: `#version 330` on macOS, `#version 300 es` on iOS (OpenGL ES 3), `#version 100` with `precision mediump float` on web (WebGL 1). Port from the libGDX GLSL (`fog`, `shockwave`, `bomb_explosion`) and from the WGSL (`screen_effects`). A shader that fails to compile logs its info log and that effect stops drawing; the game keeps running. |
| R7 | PARITY | Straight alpha blending for the shockwave and torch glow, not additive. |
| R8 | NEW | iOS safe area: the world fills the whole screen; the HUD, touch controls and menus stay inside the safe-area insets (the notch and home indicator). |

### 3.7 Audio (`audio.rs`)

| # | Kind | Feature |
|---|---|---|
| S1 | PARITY | One-shots: `jump`, `hit`, `collect`, `bounce`, `disappear`, `button_click` (8-bit PCM WAV). A `LoadSoundAlias` pool of 4 per sound lets the same sound overlap. |
| S2 | PARITY | Menu music: `main_menu.mp3`, streamed and looped while on Main, About or Options. Fades in over 2 s from silence and out over 1 s on leaving. Moving between menu screens doesn't restart it. |
| S3 | PARITY | `play_sounds` and `sound_volume` settings. The hover blip plays at 0.35 volume. |
| S4 | NEW | iOS: the audio session category is "ambient" (respects the silent switch, mixes with the user's music). Audio pauses when the app goes to the background and resumes on return. |

### 3.8 UI, menus, localization (`ui.rs`, `localization.rs`)

| # | Kind | Feature |
|---|---|---|
| U1 | PARITY | Main menu: breathing title (11% of viewport width), Play, About, Options, Exit, and a navigation hint. Full-screen panel at 35% black over the fog and fireflies. **Exit is hidden on web and iOS** (iOS apps don't quit themselves). |
| U2 | PARITY | About: a 580×460 panel with a heading, the body text and Back. |
| U3 | PARITY | Options: heading, "Language" label, one button per language (the active one prefixed `> `), Back, hint. Choosing a language returns to the main menu. T2 adds a display row on macOS. |
| U4 | PARITY | Pause: "Pause Menu", Resume, Exit to Menu, hint, controls help. Esc, gamepad East or Select resumes. The HUD stays visible. |
| U5 | PARITY | Game over: "Game Over", Play Again, hint. |
| U6 | PARITY | HUD: coin icon (the 16×16 top-left frame of `Items.png`, drawn at 32×32) at (10, 10), then the count at font size 24. Removed on returning to the menus. |
| U7 | PARITY | Buttons: 220×75, radius 4, white; hovered or focused 0.85 grey; black text. Eased scale: 1.08 hovered or focused, 0.94 pressed, ease rate 14. |
| U8 | PARITY | Entrance animation: each element fades and rises 14 px over 0.35 s (smoothstep), staggered 0.08 s by order. |
| U9 | PARITY | Focus: mouse hover or touch takes focus. Arrows, Tab/Shift+Tab and the D-pad move it, wrapping. Enter, Numpad Enter, Space, gamepad South or Start activate; a tap activates. The first button is focused by default. A focus change plays the hover blip; activation plays the click. |
| U10 | PARITY | Fonts: `NanoPlus.ttf` for button labels, `QuestSquare.ttf` for everything else, with QuestSquare at larger nominal sizes to match. **Load with explicit codepoints**: raylib loads only ASCII by default, which would drop every Cyrillic glyph. Collect the codepoints from the whole message table with `LoadCodepoints`, and load each font at each size used (point filter). |
| U11 | PARITY | Localization: all 15 `Msg` strings in English and Ukrainian, copied verbatim into a `static const char *MSG[MSG_COUNT][LANG_COUNT]` table, plus the new keys below. |
| U12 | FIX | **Esc / gamepad B goes back** from About and Options to the main menu. The hint on every menu promises it in both languages, but Rust handles it only in Pause. |
| U13 | FIX | **Play from the main menu starts a fresh run**: coins, lives and level all reset, exactly like Play Again. Rust resets nothing on Play, so coins from an abandoned run carry into the next one. Exit to Menu doesn't need to reset anything itself, because every new run starts from Play or Play Again. |
| U14 | NEW | Controls help and About text on iOS describe the touch controls instead of keys: new `Msg` keys, EN and UK, picked by platform. |
| U15 | NEW | HUD lives row under the coin counter: one heart icon per remaining life (the `Items.png` heart frame at (0, 16), drawn at 32×32). Rust has no lives display; now that hearts heal (O2) and lives carry across levels (P10), the player needs to see them. Same as the Godot port's HUD. |

### 3.9 Input and dev tools (`player.rs`, `dev.rs`)

| # | Kind | Feature |
|---|---|---|
| I1 | PARITY | Keyboard: A/D and ←/→ move; J/Z jump; K/X attack; L/C interact; Esc pauses. |
| I2 | PARITY | Gamepad (every connected pad): D-pad left/right or left stick x beyond ±0.3 moves; South jumps; West or East attacks; North interacts; Start pauses. raylib names: `RIGHT_FACE_DOWN`/`LEFT`/`RIGHT`/`UP`, `MIDDLE_RIGHT` (Start), `MIDDLE_LEFT` (Select). On iOS these come from GameController through SDL. |
| I3 | PARITY | Dev keys F1 debug draw (green collision blocks, red hitboxes, plus checkpoints and triggers), F2 invulnerability, F3 shockwave and ripple at the player, F4 advance level, F5 trigger the checkpoint routine. Available in every macOS and web build, as in Rust. Not reachable on iOS without a keyboard, which is fine. |
| I4 | PARITY | Env switches: `EIK_INVULNERABLE`, `EIK_DEBUG_DRAW`, `EIK_CHROMA_GLITCH`, `EIK_DEBUG_TILEMAP`. |
| I5 | PARITY | Capture harness: `EIK_CAPTURE=<dir>`, `_INTERVAL`, `_SHOTS`, `_LEVEL`, `_DELAY`, `_INPUT` (`run`, `left`, `fx`, `cycle`, `checkpoint`, `pause`), and `_ABOUT` (dev builds only, `-DEIK_DEV`). Writes `frame-NN.png` with `TakeScreenshot`, then exits. Scripted input goes through the `InputFrame` overlay, because raylib can't inject key events. macOS only. |
| I6 | NEW | FPS overlay (F6 or `EIK_SHOW_FPS`), top-right, on every screen. Rust has none, but ticket T1's acceptance criteria check frame pacing against one. |
| I7 | NEW | Flecs Explorer in macOS dev builds: `-DEIK_DEV` enables `FLECS_REST`, so the running world can be inspected at flecs.dev/explorer. This replaces the entity `Name`s the Rust port kept for Bevy's inspector. |
| I8 | NEW | iOS touch controls (decision D10): a virtual joystick bottom-left (`Joystick.png` base, `Knob.png` knob; horizontal deflection beyond the same ±0.3 dead zone as the stick moves), and jump, attack and interact buttons bottom-right, plus a pause button top-right. Jump is held like the key (auto-hop); attack and interact fire on touch-down. Multi-touch: each control tracks its own touch id, so the player can run and jump at once. Shown only on iOS while `Playing`, hidden while a gamepad is connected, drawn inside the safe area at about 40% opacity. |

### 3.10 Rust's "preserved oddities" and what the port does

The Rust README lists these as bugs kept on purpose. This port doesn't inherit them.

| Rust oddity | Port | Row |
|---|---|---|
| Four deaths before game over | Fixed: three | P9 |
| Red mob teleports 300 px right after attacking | Fixed: walks back to its spawn x | E6 |
| Object-layer offsets are ignored | Fixed: honoured, and the stray offset is removed from the map | L3 |
| Gravity added per step, not scaled by dt | Fixed: per-second value times dt; same arc at 60 Hz | P13 |
| Escalators carry only x velocity, even vertical ones | Fixed: riders follow both axes | O8 |
| Touching a falling platform from below kills | Fixed: it's solid from below and the sides | O10 |
| Circle hitboxes tested as bounding boxes | Fixed: real circle test | E9 |
| Checkpoints and triggers are invisible | **Kept.** This is design, not a bug: their art was removed on purpose. F1 shows them. | O4, O5 |

Bugs that the Rust README doesn't list, found while writing this plan: lives refilling per
level (P10), the Red mob's missing contact check (E7), Esc/B not going back (U12), and
Play not resetting the run (U13). All are fixed.

---

## 4. Implementation plan

Each phase leaves something runnable and ends with a named check. Phase 0 proves all three
platforms before any gameplay code depends on them.

### Phase 0a: Scaffold (macOS, web)
**Status:** Complete.

1. CMake project; raylib 5.5 via `FetchContent`; vendored Flecs (custom build) and yxml;
   warning flags as in 1.2.
2. `main.c`: a Flecs world, three empty pipelines, the frame loop from 2.2, and a
   1280×720 window titled "Edgard in Kimeria" that draws one sprite. The asset root comes
   from `EIK_ASSET_ROOT`, else `GetApplicationDirectory()/assets`; loading fails loudly with
   the full path.
3. Web: `emscripten_set_main_loop`, `web/shell.html` with a canvas that fills the viewport
   and a resize callback (Rust's `fit_canvas_to_parent`), `--preload-file assets`.
4. CI (`ci.yml`) on a macOS runner: build macOS and web, run the tests.
5. **Check:** CI green; the sprite shows on macOS and in the browser.

### Phase 0b: iOS spike (section 1.4)
**Status:** Implementation complete; physical-device acceptance remains pending.

1. raylib's SDL backend with `GRAPHICS_API_OPENGL_ES3`, built for iOS from CMake's Xcode
   generator; an `.app` bundle with `assets/` as resources.
2. The Phase 0a scene plus one shader, one sound, a touch point and a gamepad reading.
3. Add an iOS simulator build (unsigned) to CI.
4. **Check:** the spike exit criteria in 1.4, on a physical device. If Route A fails, try
   Route B; if both fail, stop and re-plan.

### Phase 1: Collision core and tests
**Status:** Complete.

1. Port `core/collision.rs` into `collision.c` function for function: `overlaps`,
   `hitbox_left_x`, `resolve_horizontal`, `apply_gravity`, `resolve_vertical` (with its
   `VerticalOutcome`), and `mirrored_pos`. Apply the O10 fix here, and the P13 unit change.
2. Port all 11 tests from `tests/collision.rs` into `tests/test_collision.c`. Change the
   two that encode fixed bugs (the falling-platform outcome and the gravity-per-step test)
   to assert the corrected behaviour, and add one test each for O10 and E9.
3. **Check:** all tests pass under `ctest`.

### Phase 2: Level loading, tilemap, camera
**Status:** Complete.

1. `tmx.c` built on yxml (L1, L2, L3 with offsets honoured), and `test_tmx.c` asserting the
   object counts, classes and spawn order of both maps.
2. `mod_level.c`: spawn by class (L4, L5), collision blocks with `SpawnIndex`, map size,
   `ecs_delete_with(LevelEntity)` on unload.
3. The world render texture with AutoMin sizing (R1, R2), tile drawing (L6), sky backdrop
   (X10), camera follow and snap (R3), and the F1 overlay (blocks only for now).
4. **Check:** both levels render with the correct collision boxes under F1; a capture with
   `EIK_CAPTURE_LEVEL=1` matches the Rust build's at the same settings.

### Phase 3: Player and the fixed-step loop
**Status:** Complete.

1. `GameTime` and the fixed accumulator (2.2).
2. `input.c`: keyboard and gamepad (I1, I2), and the capture script overlay.
3. `anim.c` and the player clip table (A.2), including the attack clip wrapping at 4 per row.
4. `mod_player.c`: P1–P13. Lives in `GameProgress`.
5. **Check:** `EIK_CAPTURE_INPUT=run` and `left` on both levels; wall jumps in `forest-1`;
   quicksand in `forest`; jump arcs compared frame by frame against Rust; the run ends on
   the third death, and lives don't refill at a checkpoint.

### Phase 4: Enemies and bullet time
**Status:** Complete.

1. Bat, Yellow and Red mobs (E1–E9), the sword, `EnemyStomped` as a Flecs event.
2. Bullet time (P12) through `time_scale`; animation keeps real-time speed.
3. **Check:** a bat pass slows physics but not animation; the Red mob's swing kills
   mid-animation, and it then walks back to its spawn point; the Red mob can be stomped.

### Phase 5: Items, objects, triggers
**Status:** Complete.

1. O1–O11, and `TriggerActivated` as a Flecs event observed by walls, escalators and torches.
2. Port `tests/triggers.rs` (5 tests) against a headless world: a wall is removed by a
   matching id and not by another; an escalator toggles; a torch toggles and relights at
   200; only the targeted torch changes. Add a test for vertical escalator carry (O8).
3. **Check:** tests pass; in `forest-1` the trigger opens the wall and toggles the torch; a
   falling platform warns, drops and disappears; a heart restores one lost life and none
   beyond 3; `EIK_CAPTURE_INPUT=checkpoint` advances
   after 3 s.

### Phase 6: Effects and shaders
**Status:** Implementation complete; iOS simulator acceptance is pending because this checkout's
local Xcode toolchain cannot find an iOS C compiler.

1. GLSL ports with the per-platform version header (R6).
2. Particles, torch, fireflies, fog, shockwave, explosion, and the post pass with ripple and
   poison glitch (X1–X9).
3. **Check:** `EIK_CAPTURE_INPUT=fx` frames next to Rust's; F3 shows both the shockwave and
   the ripple; `EIK_CHROMA_GLITCH=1` shows the recurring glitch; the same effects render on
   the iOS simulator.

### Phase 7: Audio
1. S1–S4: the alias pool, music with fades keyed to menu states, and the iOS audio session.
2. **Check:** by ear. Overlapping coin pickups don't cut each other off; Main → About →
   Main doesn't restart the music; Play fades it out over 1 s; on iOS the silent switch
   mutes it and backgrounding pauses it.

### Phase 8: UI, menus, localization, HUD
1. `l10n.c` (U11, U14) and font loading with Cyrillic codepoints (U10).
2. Immediate-mode widgets with focus, eased scale and entrance animation (U7–U9), drawn
   inside the safe area (R8).
3. Screens U1–U6, the HUD lives row (U15), the menu backdrop, the state wiring, and the
   fixes U12 and U13.
4. **Check:** every screen in both languages with no missing glyphs; keyboard-only,
   gamepad-only and touch-only walk-throughs; Esc/B backs out of About and Options; coins
   are 0 after Exit to Menu → Play.

### Phase 9: iOS touch controls (D10)
1. Art: `Joystick.png`, `Knob.png` and `JumpButton.png` from the Flutter original. Attack,
   interact and pause have no art: until new art exists, draw them as `JumpButton.png`
   tinted per button with a small glyph on top (sword, hand, pause bars). New art is a
   drop-in replacement, not a code change.
2. `touch.c`: I8 in full, feeding `InputFrame`. Shown only
   on iOS while playing, and hidden while a gamepad is connected.
3. **Check:** both levels completed on an iPhone with touch alone, then with a gamepad
   alone; running while jumping and attacking works (multi-touch); connecting a gamepad
   hides the controls and disconnecting shows them again; the pause button opens the pause
   menu over the T1 glitch.

### Phase 10: Dev tools and capture harness
1. I3–I7 complete.
2. **Check:** each `EIK_CAPTURE_INPUT` mode runs and exits by itself; the Flecs Explorer
   shows the live world in a dev build.

### Phase 11: Tickets T3, T2, T1
Section 6. T3 goes first, because audio initialisation moves.

### Phase 12: Release
1. macOS: a signed, notarized `.app` with `assets/` in its `Resources/`; the asset root
   falls back to the bundle's resource path. Verify by launching from Finder.
2. iOS: a signed build installable through TestFlight. Store submission isn't part of this
   plan.
3. Web: `deploy-web.yml` with `setup-emsdk`, uploading `index.html`, `.js`, `.wasm` and
   `.data` to Pages on push to `main`.
4. README: build commands per platform, controls, env switches, and a **Deviations from
   Rust** section listing every FIX row and every NEW row.

### Phase 13: Parity sign-off
1. Walk through the Rust README's feature parity list and section 3 of this plan; tick
   each row with the capture, test or device check that proves it.
2. Side-by-side capture runs: the same `EIK_CAPTURE_*` settings on both builds, every
   mode, both levels. Every difference is either a FIX row or gets fixed.

---

## 5. UI layout notes

Bevy lays menus out with flexbox in logical window pixels. The port lays them out by hand
in the same units:

- Enable `FLAG_WINDOW_HIGHDPI` and draw UI in logical pixels, so sizes match Bevy's
  `Val::Px` on Retina screens and iPhones.
- A panel is a column centred in the (safe-area) window: padding 10, row gap 20, radius
  20. Its size is either fixed (400×300, or 580×460 for About) or full-screen for the main
  menu. Children are stacked and centred; there's no general flexbox.
- On iPhones in landscape, 580×460 doesn't fit in about 390 logical pixels of height.
  Panels scale down uniformly to fit the safe area; buttons keep a minimum touch target of
  44×44 logical pixels.
- The title's font size is `0.11 × window width` (Bevy's `FontSize::Vw(11.0)`), so it stays
  on one line in narrow windows. Load it at a few sizes and pick the nearest.
- Rounded rectangles: `DrawRectangleRounded` with `roundness = 2r / min(w, h)`.

---

## 6. Tracked tickets

| # | Source | Requirement |
|---|---|---|
| T1 | [edgard_in_kimeria_java#3](https://github.com/jlogicgames/edgard_in_kimeria_java/issues/3) | **Chromatic-aberration glitch while paused, on the game view only.** While paused, the frozen world renders through the chroma shader. The pause menu on top is completely unaffected. The effect stops as soon as the game resumes, with nothing left behind. A static or slow idle animation are both acceptable. No frame-pacing regression, checked with the FPS counter. |
| T2 | [edgard_in_kimeria_rs#22](https://github.com/jlogicgames/edgard_in_kimeria_rs/issues/22) | **Fullscreen by default on desktop and mobile, with a windowed opt-out in the menu.** Web is excluded. The setting persists across sessions. |
| T3 | [edgard_in_kimeria_rs#21](https://github.com/jlogicgames/edgard_in_kimeria_rs/issues/21) | **Web-only start screen.** One button (e.g. "Play") whose click is the user gesture browsers require before audio. Audio and game start after the click. Desktop and mobile skip it. |

### T3: web start screen
1. `APP_WEB_START` is the initial state when `PLATFORM_WEB` is defined; macOS and iOS
   start in `APP_MAIN_MENU`.
2. Load textures, fonts and shaders before the click, but call **`InitAudioDevice()` and
   load every sound and the music stream only after it**, so the AudioContext is created
   inside the user gesture. Rust's `index.html` AudioContext-wrapping script isn't needed
   and isn't ported.
3. The screen: black background and one button styled like the menu buttons, labelled with
   `Msg::Play` in the current language. It accepts a click, a tap, Enter/Space or gamepad
   South. Check Safari explicitly, because its gesture rules are the strictest.
4. **Check:** in a fresh browser profile the console has no autoplay warnings, the menu
   music fades in right after the click, and macOS and iOS never show the screen.

### T2: fullscreen default and windowed opt-out
1. `settings.c`: a small INI file (`display=fullscreen|windowed`, `language=en|uk`) at
   `$HOME/Library/Application Support/edgard_in_kimeria/settings.ini`. The same path works
   on macOS and inside the iOS app container, because `HOME` points at the container there.
   A missing or corrupt file falls back to defaults and is rewritten on the next save.
   Saving the language too is an extension of Rust, where it resets every launch
   (decision D6).
2. **macOS:** on startup, apply the saved mode (default fullscreen) with borderless
   windowed fullscreen (`ToggleBorderlessWindowed`), not exclusive `ToggleFullscreen`: it
   doesn't change the display mode, and AutoMin scaling (R1) handles any aspect ratio.
   Options gets a "Display: Fullscreen / Windowed" button, with two new `Msg` keys in both
   languages. Unlike the language buttons, it toggles in place and keeps focus. Switching
   to windowed restores a 1280×720 window centred on the current screen (decision D5); the
   window remains resizable.
3. **iOS:** always fullscreen, landscape-only, with the status bar hidden; no toggle is
   shown. That meets the ticket, because an iOS app has no windowed mode to opt into.
4. **Web:** no toggle; the setting is ignored.
5. **Check:** first launch on macOS is fullscreen; toggle, quit and relaunch comes back
   windowed; deleting the file brings back fullscreen; the language survives a relaunch on
   all three platforms.

### T1: pause glitch on the game view only
1. The frame pipeline (R5) already keeps the world and the UI apart: the world, including
   the sky, is drawn into the world render texture, the post pass draws it to the screen,
   and **the HUD and pause menu are drawn afterwards**. The glitch can't reach the menu,
   and no extra framebuffer is needed. That's the libGDX ticket's FBO sketch, which this
   design already has.
2. While `AppState == PAUSED`, `render.c` sets `chroma_intensity = 1` and a fixed
   `chroma_shift` (start at the poison maximum, 0.010, and tune by eye). `time` keeps
   advancing on the real clock, so the shader's `sin(time·15)` shake gives a slow idle
   wobble. The world stays frozen, because the fixed and gameplay systems are disabled in
   `PAUSED` through their `RunIn` masks.
3. On resume the next frame writes the gameplay values again: intensity 0, unless the X9
   poison glitch is enabled and active. It's one uniform write, so nothing lingers.
4. X9 and T1 share the shader. Pause wins while paused, and the poison schedule's clock
   doesn't advance while paused.
5. iOS: backgrounding the app pauses the game (S4), so returning shows the pause menu over
   the glitched world instead of resuming mid-jump.
6. **Check:** the ticket's four acceptance criteria. For frame pacing, compare the F6
   overlay paused and unpaused: the post pass runs every frame either way, so there
   should be no difference.

---

## 7. Decisions

| # | Question | Decision |
|---|---|---|
| D1 | Port Rust's "preserved oddities"? | **Settled: no.** Bugs aren't ported; section 3.10 lists each one. Design choices that only look odd (invisible checkpoints and triggers) stay. |
| D2 | Lives reset on every level | **Settled: fixed** (P10). |
| D3 | Esc/B back on About and Options | **Settled: fixed** (U12). |
| D4 | Coins carry over into a new run | **Settled: fixed** (U13). |
| D5 | Windowed size for T2 on macOS | 1280×720, Rust's default window. Open, but low stakes. |
| D6 | Persist the language as well as the display mode? | Yes; same file. Open, but low stakes. |
| D7 | Platforms | **Settled:** macOS, web and iOS in scope; Linux, Windows and Android out for now (section 1.1). |
| D8 | Dev keys F1–F5 in release builds | Keep them in every macOS and web build, as Rust does. `-DEIK_NO_DEV_KEYS` removes them if needed. |
| D9 | Should a Heart grant a life? | **Settled: yes, matching Godot:** +1 life, capped at 3, plus the shockwave (O2), and a lives row on the HUD (U15). |
| D10 | iOS controls | **Settled: on-screen touch controls** (I8, Phase 9), with gamepads also supported. Attack, interact and pause use placeholder art until new art exists. |
| D11 | ECS | **Settled: Flecs** (section 2). |

---

## 8. Testing strategy

- **Unit tests** (`ctest`, headless Flecs world, no window): collision (11 ported, 2
  updated for fixed bugs, 2 new), triggers (5 ported, 1 new), TMX parsing, l10n
  completeness (every `Msg` non-empty in every language), and a settings INI round-trip.
- **Capture runs** replace manual play for anything visual on macOS, as in Rust. Keeping
  the harness's env interface identical lets the two builds be compared frame by frame;
  expected differences are exactly the FIX rows.
- **Device checks** on iOS for touch, safe area, audio session and lifecycle; there is no
  capture harness there.
- **CI** on a macOS runner: macOS build and tests, web build, and an unsigned iOS simulator
  build.

---

## 9. Dropped or changed on purpose

- **Hot reload** (`--features dev`). This is dev tooling, not a player-facing feature, and
  a C rebuild is fast. A shader reload key (F7) is a cheap later addition if tuning needs
  it.
- **Asynchronous asset loading and the `Loading` state.** raylib loads synchronously; the
  assets total a few MB.
- **`index.html` AudioContext wrapper.** Superseded by T3.
- **Linux, Windows and Android builds.** Out of scope for now (D7).

---

## Appendix A: Constants to carry over

### A.1 Physics and gameplay

| Constant | Value | Source |
|---|---|---|
| Fixed step | 60 Hz; real delta capped at 0.25 s | `core/mod.rs` |
| Gravity | 588 px/s² (Rust: 9.8 per step); terminal 300; jump force 260 | `core/mod.rs`, `player.rs` |
| Player move speed | 100 px/s | `player.rs` |
| Player box / hitbox | 48×48 / (18, 26, 11, 22) | `player.rs` |
| Wall jump | vy = −260·0.7, vx = ±260·0.5, lockout 0.1 s | `player.rs` |
| Coyote threshold | vy > 9.8·15 | `player.rs` |
| Death plane | y > 380 | `player.rs` |
| Lives | 3, game over on the third death (P9) | `player.rs` |
| Bullet time | range 50 px, scale 0.5 | `player.rs` |
| Attack box | x offset `16 − ox + w` (right) / `ox − 20 + w` (left); y `oy − 14`; size 37 × (h + 14) | `player.rs` |
| Checkpoint delay | 3 s real time | `player.rs` |
| Bat | 50 px/s, range unit 16 px, circle r = 8 | `enemy.rs` |
| Mob | 80 px/s, hitbox (10, 6, 14, 26), range unit 16, lerp 0.1 | `enemy.rs` |
| Red mob | attack range ±65 px; walks back to spawn x at 80 px/s after attacking (E6) | `enemy.rs` |
| Stomp bounce | vy = −260 (mobs only) | `enemy.rs` |
| Escalator | 50 px/s, range unit 32 px | `objects.rs` |
| Falling platform | delay 1 s, 200 px over 1.5 s, warning torch intensity 5 | `objects.rs` |
| Checkpoint hitbox | 16×32 | `items.rs` |
| Torch default intensity | 80 (Torch), 0 (Actionable Torch), relit at 200 | `level.rs`, `torch.rs` |
| Torch glow | radius `clamp(4 + i·0.06, 3, 40)`, alpha `clamp(18 + i·0.22, 8, 220)/255`, colour (0.4, 1, 0.6) | `torch.rs` |
| Camera | follow 500 px/s, left 200, up 200, hitbox width 11 | `camera.rs` |
| Sky scroll | 5 px/s | `camera.rs` |

### A.2 Animation clips (frame size, origin, frames, step, loop)

| Sheet | Clip | Spec |
|---|---|---|
| `hero/Player.png` (48×48) | Idle | row 9, 4, 0.1 |
| | Running | row 0, 4, 0.1 |
| | Jumping | row 8, 1 |
| | Falling | row 4, 1 |
| | Hit | row 4, 2, once |
| | Attacking | row 1, 7 frames wrapping at 4 per row, once |
| | Appearing | row 3, 4, once |
| | Disappearing | row 6, 4, looping |
| | Climbing | row 3, 1 |
| `enemy/Bat.png` (16×16) | Idle / Running / Hit | y 0 / 32 / 64; 5 / 5 / 4; 0.03; Hit once |
| `enemy/yellow_mob.png` (48×32) | Idle / Running / Hit | rows 5 / 1 / 4; 4 each; 0.05; Hit once |
| `enemy/Mobs.png` (48×32) | Idle / Running / Hit / Attacking | rows 5 / 1 / 4 / 2; 4 each; 0.1, Attacking 0.2; Hit and Attacking once |
| `Items.png` (16×16) | Bomb / Coin / Heart | (32, 0) 2 @ 0.1 / (0, 0) 2 @ 0.3 / (0, 16) 2 @ 0.3 |
| `objects/Grey Off.png` | Escalator Idle | 32×16 declared, clamped to 32×8; 1 frame |
| `objects/Grey On (32x8).png` | Escalator Running | 8 frames, 0.05 |
| `objects/FallingOn.png` | Idle / Falling | 4 frames; 0.1 / 0.3 |

Frame rectangles are clamped to the image, because two sheets declare frames taller than
the file. raylib wouldn't reject the out-of-range rectangles, but clamping keeps the
transparent padding identical to Rust.

### A.3 Effects

| Effect | Values |
|---|---|
| Ripple | 0.75 s, max radius 300 px, strength 12 × (1 − progress), frequency 60, decay 20, phase `time·6` |
| Chroma poison | interval 1–3 s, duration 0.2–0.5 s (+ up to 1.5 s with poison), shift 0.002 → 0.010 at 0.0005/s, shake `sin(t·15)·shift·0.25` |
| Chroma on pause (T1) | intensity 1, shift 0.010 to start, tuned by eye |
| Shockwave | 0.6 s, radius 64, ring 8, 128×128 quad |
| Explosion | 64 px, 0.7 s |
| Fog | uniforms `ground_pos 0`, `ground_add 0`, `fade 1`; size = visible area |
| Level fireflies | 24, black, 3 px `dot`, flight 3–7 s, hidden 1–2 s, control ±30, end ±20 |
| Menu fireflies | 24, (1, 0.8, 0.2), 7 px `glow` core, halo ×4 at 0.35 alpha, pulse 3 Hz, exclusion (−120, −140)–(120, 170) |
| Torch emitters | core every 0.04–0.16 s; embers 3–7 every 0.02–0.14 s; sparkles every 0.08–0.26 s, count `clamp(i·(0.8–1.2), 10, 300)` then capped at 40; smoke every 0.22–0.72 s, life 2–4 s |

### A.4 UI

| Element | Values |
|---|---|
| Colours | panel black; main-menu panel black at 35%; text white; button white; hovered or focused 0.85 grey; button text black |
| Panel | 400×300 (About 580×460), padding 10, gap 20, radius 20; scaled down to fit the iOS safe area |
| Button | 220×75, radius 4; font 40 (Play), 28 (others), 24 (language) |
| Font sizes | title 11 vw; About heading 42, body 30; other headings 24; Language label 16; hint and help 14; HUD 24 |
| Animation | ease 14/s; scale 1.08 / 0.94; stagger 0.08 s; appear 0.35 s; rise 14 px; breathe ±3.5% at 2 rad/s |
| Music | fade in 2 s, fade out 1 s; hover blip at 0.35 volume |
