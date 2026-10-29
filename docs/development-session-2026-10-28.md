# Development session — 2026-10-28

Requested window: five hours starting **13:37:19 UTC** (18:37:19 Asia/Tashkent).
Feature development stops at **18:07:19 UTC**, leaving the final 30 minutes for
verification, documentation and handoff. Hard end: **18:37:19 UTC**.

This is an incremental work record; the final section will identify the exact
stopping point. The repository began with all project files untracked. No user
changes were reset and no commits were created. A complete initial source snapshot
is retained locally at `build/session-20261028/baseline.tar.gz`.

## Inspected baseline

- Read AGENTS.md, SPEC.md, README, ROADMAP, architecture/world-format/decisions,
  build configuration and the client, renderer, world, generator, player and input code.
- Existing v0.2 client already implements rays, editing, collision, jumping,
  buffered jumps, input snapshots and scripted SDL playtests. Some documentation
  predates the latter changes; these are baseline features, not work of this session.
- GCC Debug baseline build and all 43 CPU/CLI tests passed.
- Linux desktop, NVIDIA RTX 3050 and project-local validation layers are available.
  Windows tools/runner are not available; Windows execution remains unverified.

## Changes and checks

### Frustum culling

- Added `engine/renderer/frustum.hpp/.cpp`: CPU AABB/frustum testing against all
  six Vulkan clip planes, including zero-to-one near depth. Bounds are translated
  relative to the camera in double precision; a conservative tolerance prevents
  edge flicker from shader float rounding.
- `engine/renderer/vulkan_renderer.cpp`: compute each uploaded mesh's actual local
  bounds, reject non-finite vertex positions and skip fully invisible terrain draws.
  Selection and crosshair retain their existing rendering behavior.
- Renderer API and `client/main.cpp` report uploaded versus submitted mesh/triangle
  counts so diagnostics now describe actual visibility.
- Added `tests/frustum_tests.cpp` covering planes, touching/straddling bounds,
  perspective near/far rejection, camera rotation and remote camera-relative origins.
- Added new sources to CMake.
- GCC and Clang Debug: **46/46 CPU/CLI tests passed**, warning-free builds.
- Pinned-dependency Release: **2/2 hardware graphics tests passed**, zero core or
  synchronization validation warnings/errors including shutdown. Smoke diagnostics:
  28/97 terrain meshes and 76,792/321,000 triangles submitted at the sampled view.
  These are scene counters, not a general performance benchmark.

### Bounded background terrain generation

- Added `engine/voxel/terrain_worker.hpp/.cpp`: one stop-aware C++23 jthread,
  bounded/deduplicated requests and completed results, explicit ownership transfer,
  exception propagation and joined shutdown. No Vulkan/World access on the worker.
- Client initial generation polls desktop events, handles resize/close, presents
  the loading view and reports progress while terrain is generated in the worker.
- Added portable CMake Threads dependency and worker tests for capacity, duplicate
  requests, generation equivalence, invalid coordinates and idle/pending shutdown.
- GCC and Clang Debug: **48/48 tests passed**. Corrected a test-only dangling-else
  warning; subsequent builds were warning-free. Both hardware graphics tests passed
  with zero core/synchronization validation diagnostics, including shutdown.

### Persistent sandbox foundation

- Added `engine/core/binary.hpp`: explicit little-endian integers/IEEE doubles,
  bounded reader, length-prefixed strings and FNV-1a corruption checksums.
- Added `engine/voxel/storage.hpp/.cpp` and CMake `vc_storage`: world metadata,
  seed/generator version/bounds, regional namespaced palettes and RLE chunk data,
  immutable whole-world snapshots, directory-rename commit, previous revision
  retention, bounded decoding and explicit corruption/version failures. Missing
  identifiers retain their names as magenta solid fallback content.
- Added `engine/platform/file_lock.hpp/.cpp` and CPU-only `vc_files`: exclusive
  process-owned locks, using POSIX flock and Windows exclusive file handles. This
  replaced an intermediate directory lock before shipping the persistence slice,
  so crashes do not leave logical stale locks. Native code stays in platform.
- Added `game/player/save_state.hpp/.cpp`: versioned player camera, movement mode
  and selected material, with finite-coordinate/orientation validation. The later
  hotbar slice upgrades this payload to v2 and retains v1 reading/migration.
- Added `Window::userDataPath()` using SDL's platform-appropriate writable path.
- Client now creates/resumes the default user-data sandbox or `--world-dir PATH`,
  rejects conflicting seed/height flags, restores player state, and supports F5,
  60-second autosave, save-on-exit and `--no-save`. Bounded frame runs are temporary
  unless given an explicit world; smoke runs cannot target persistent worlds.
- Restored obstructed/unavailable walking positions enter flight for recovery.
- Added `tests/temp_directory.hpp`, storage/player/binary tests and
  `tests/client_persistence.cmake`. Tests cover negative regions, partial heights,
  reordered runtime IDs, missing content, compression, concurrent lock rejection,
  checksummed structural corruption, failed-save rollback, previous snapshots,
  configuration conflicts and executable reopen after forced process termination.
- Updated CMake project version to **0.3.0 development**. No stabilized release or
  Windows execution is claimed.
- GCC and Clang Debug: **56/56 tests passed** at the persistence checkpoint.
  CPU-only Clang ASan/UBSan/LSan: **56/56 passed**, no findings. Pinned Release:
  **3/3 hardware graphics tests passed**, all reported zero validation diagnostics.

### Creative inventory and visible hotbar

- Added `game/gameplay/inventory.hpp/.cpp`: a real creative catalog of registered
  non-air blocks, nine assignable/clearable slots, number/wheel selection and
  namespaced save/restore. Materials are infinite creative supplies; survival item
  quantities are deliberately not claimed.
- Added registry size access for catalog enumeration.
- Added `game/ui/sandbox_ui.hpp/.cpp`: shared CPU menu controller, scalable hit
  regions, paginated catalog, textured isometric block icons, numbered hotbar,
  selected-material label, save-status notices and built-in bitmap text geometry.
- Player payload v2 persists nine assignments and selected index, and reads v1
  selected-material saves. Missing hotbar-only names also retain fallback content.
- Platform input now handles E/Escape menu actions, slots 1..9, fractional/flipped
  wheel input, UI primary/secondary clicks and F5 outside capture. Pointer mode
  prevents UI clicks recapturing/editing terrain. Focus loss clears pending UI
  actions, held controls and wheel accumulation. Logical SDL pointer coordinates
  convert to window pixels for high-DPI hit testing.
- Renderer receives generic colored/textured HUD meshes and uses a dedicated
  depth-disabled pipeline. HUD buffer replacement is transactional and occurs only
  for menu/assignment/status/viewport changes. Crosshair hides while inventory opens.
- Client composes the shared UI controller, pauses movement commands while menus
  are open, and retains gravity. Catalog clicks assign the selected slot; right
  click on a hotbar slot clears it; empty slots cannot place terrain.
- Added `tests/inventory_tests.cpp`, extended input/player tests, and
  `tests/ui_playtest.cpp` plus opt-in CTest `Client.Inventory`. The hardware test
  exercises pixel-scaled clicks, capture transitions, save/reload of assignments,
  actual world placement, clearing and empty-slot rejection.
- GCC Debug: **64/64 CPU/CLI tests passed**, warning-free. New inventory graphics
  test passed with **zero validation warnings/errors** including shutdown. Its
  inventory image was inspected and showed readable labels, textured icons and
  selected-slot highlighting. Follow-ups: GCC/Clang **64/64 passed**, CPU-only
  ASan/UBSan/LSan **64/64 passed**, and all **4/4 graphics tests passed**.
- No packages were installed. The host's ImageMagick converted PPM readbacks to
  PNG for inspection because Pillow is not installed.

### Basic settings

- Added `game/settings.hpp/.cpp` and `vc_settings`: validated FOV (50..110 degrees),
  mouse sensitivity (0.0005..0.01 radians/pixel) and invert-Y, versioned/checksummed
  binary settings, staged replacement, previous-file backup and backup recovery.
- Camera now uses configured FOV/sensitivity/inversion; invalid non-finite aspect,
  FOV and sensitivity are rejected at camera boundaries.
- Extended the shared UI with an F10 settings menu, actual decrement/increment,
  inversion, reset-defaults and close buttons. Changes apply immediately and save
  automatically during normal play; failures produce a visible status and log.
- Added explicit `--settings-file`, `--no-settings`, `--fov`, `--sensitivity`,
  `--invert-y` and `--normal-y` options. Bounded/smoke runs do not read/write user
  settings unless an explicit settings path is supplied.
- Added `tests/settings_tests.cpp`, invalid-FOV CLI coverage and extended the
  hardware UI test through settings clicks, persistence and configured SDL look.
- GCC Debug: **69/69 tests passed**. Settings/Inventory hardware playtest passed
  with zero validation diagnostics, including teardown; the settings image was
  inspected. Clang **69/69 passed**, ASan/UBSan/LSan **69/69 passed**, no findings.
- Saved inspected hotbar/inventory/settings images in `docs/voxelcraft-v03*.png`.

### Horizontal and vertical terrain streaming

- Added `World::remove` and modified-chunk tracking. Removal dirties its GPU
  origin and resident face neighbors; edits remain distinguishable from generation.
- Added `engine/voxel/streaming.hpp/.cpp`: near-first bounded terrain requests,
  player/surface-band residency, horizontal and vertical movement, compact RLE edit
  retention with a raw worst-case fallback, cache restoration and stale-job rejection.
- New saves merge evicted retained chunks with resident edits. Untouched terrain
  regenerates from seed/version; legacy full-world snapshots retain all their data.
  Resident chunks are bounded; the edit cache grows with edited chunks and the
  snapshot format's 8,192 saved-chunk limit remains explicit.
- Added `engine/voxel/mesh_queue.hpp/.cpp`: coalesced dirty work, near-first
  two-chunk frame budget (four during edits), and unconditional GPU removal messages.
- Client now generates around the actual spawn/saved camera, including high-bound
  player headroom, runs streaming through gameplay, saves retained edits and shows
  terrain/mesh backlog plus cached edit payload bytes in diagnostics. Unsupported
  saved heights are rejected before unsafe client coordinate conversions.
- Added `tests/streaming_tests.cpp`: compact worst-case round trips, unload dirty
  neighbors, extreme coordinates/height bands, eviction/revisit, edit-only saves,
  teleport cancellation, legacy retention and mesh queue budget/removal behavior.
- Added `tests/streaming_playtest.cpp` and opt-in `Client.Streaming`: real GPU
  resource add/remove/replacement during horizontal/vertical travel; compares
  settled GPU resource/triangle counts against a complete CPU world remesh, then
  saves, reloads and revisits evicted edits. Passed in 2.50 seconds with zero
  validation diagnostics. Its terrain/HUD image was captured and inspected.
- GCC/Clang Debug: **75/75 tests passed**. ASan/UBSan/LSan: **75/75 passed**, no
  findings. Full pinned Release graphics suite: **5/5 passed**, zero Vulkan
  validation warnings/errors across the strict tests, including shutdown.
- Added ADR 0006 documenting real memory/latency limits and future disk paging.

### Cross-platform path polish

- Added `pathText` alongside `utf8Path`; source/log boundaries explicitly use UTF-8
  rather than Windows' locale-dependent `path::string()` conversion.
- Applied it to settings/world paths, snapshot directory scanning, native locks,
  shader/screenshot errors and client logs. Added a Unicode world-path save/load
  regression test. Windows execution remains unverified.

### Verified v0.3 checkpoint

- Final GCC and Clang Debug checkpoint: **76/76 CPU/CLI tests passed**, warning-free.
- CPU-only Clang ASan/UBSan/LSan: **76/76 passed**, no findings.
- Pinned Release: **81/81 tests passed**, including **5/5 strict hardware graphics
  tests**. No Vulkan validation diagnostics were reported by the strict runs.

### v0.4 resources and survival foundation

- Expanded built-in content with logs, planks, leaves, cobblestone, coal ore and
  iron ore; the procedural atlas now has twelve tiles with gutters (216×18 RGBA).
- Generator v2 preserves v1 heights/layers and adds deterministic global-grid
  complete trees and underground ore veins. Partial world bounds do not truncate
  trees. Generator v1 remains intact; metadata supports both versions. New worlds
  default to v2; `--generator-version` configures new worlds and conflicting
  existing-world changes are rejected.
- Added `game/gameplay/items.hpp/.cpp`: namespaced item definitions, quantities,
  stack limits, tool tiers/speeds/durability, 36-slot survival inventory, transactional
  collection/transfer/crafting, unknown-item preservation and four real recipes.
- Added `game/gameplay/survival.hpp/.cpp`: held timed mining, target/tool/slot
  cancellation, required harvest tiers, resource drops, full-inventory protection,
  tool wear/breakage, 20 health, fall damage, death and inventory-preserving respawn.
- Added `game/player/spawn.hpp/.cpp`: searches supported clear spawn cells near the
  origin, consulting saved edits and rejecting unsupported player coordinate ranges.
- Player controller now reports landing impacts. Collision distinguishes real
  solids from unavailable streamed contacts; unknown contacts freeze vertical
  velocity without granting grounding/jumps or inflicting fall damage.
- Input exposes armed held mining, C crafting and R respawn. Capture-click holds
  cannot mine; release/focus/menu transitions cancel held work.
- Extended the shared UI with actual survival stack counts, backpack swaps/single
  transfers/quick moves, tool icons/durability bars, resource-consuming crafting,
  recipe availability, health and death/respawn screens.
- Renderer now accepts a foreground HUD layer for text/counts over icons and a
  generic progress indicator using persistent meshes and per-frame push constants.
  Gameplay still never calls Vulkan directly. Crosshair/progress hide in menus.
- Client adds saved `--survival`/`--creative` modes, disables survival flight,
  consumes placement items only after success, connects mining/health/respawn,
  saves quantities/durability, and reports visible collection/tool/death feedback.
- Player payload v3 adds mode, health and 36 namespaced item stacks; v1/v2 remain
  readable and migrate to creative mode with full health and empty survival items.
- Added resource/survival tests and migration/menu/spawn/input/contact coverage.
  Added `tests/survival_playtest.cpp`/`Client.Survival` with actual SDL held mining,
  log→plank→stick→wood/stone pickaxe progression, iron harvesting, tool wear,
  backpack transfers, save/reload, fall death and R respawn. Later extended it to
  verify that placement consumes one item and mining it back restores the material.
- Extended executable persistence checks for saved survival mode, rejected flight,
  and preserved legacy generator v1. Added ADR 0007 for rules and scope.
- CMake project version is **0.4.0 development**, not a stabilized release.
- GCC/Clang: **89/89 tests passed**, warning-free. ASan/UBSan/LSan: **89/89 passed**,
  no findings. First complete pinned Release checkpoint: **95/95 passed**, including
  **6/6 hardware graphics tests**. The survival run completed in 8.55 seconds with
  zero Vulkan validation diagnostics; follow-up placement/CLI additions are checked
  separately in the session logs.
- Mining/crafting/death images were captured under strict validation; mining and
  crafting images were inspected, showing progress, quantities, tool icons,
  durability, ingredient/availability labels and health. Saved under `docs/voxelcraft-v04*.png`.
- The full executable persistence test then passed in 4.68 seconds, including
  creation/reopen of an explicit generator-v1 world. An actual new generator-v2
  client view ran 60 frames at radius 4 and captured natural trees, with zero core
  or synchronization validation diagnostics. Generation/meshing/upload for that
  run were 1082.8/230.2/74.2 ms; these describe that host/scene, not a general target.

### Save failure on close

- Normal window close now attempts its save before discarding World. If saving
  fails, the application remains open, preserves in-memory edits, releases capture
  and shows retry/discard controls. F5 retries and explicit Ctrl+Shift+Q exits
  without saving. Bounded automation still reports save failures as a failed run.
- Input preserves that explicit discard intent across capture/focus changes, with
  CPU coverage. The interactive failed-close recovery itself has not been manually
  exercised; storage failed-write/rollback behavior is covered separately.

### Documentation

- README and ROADMAP distinguish implemented v0.3 work from remaining settings,
  streaming, manual playtests and Windows verification.
- Updated `docs/architecture.md`, `docs/roadmap.md` and `docs/world-format.md` with
  real ownership, format layout/limits/recovery, culling, worker, persistence and UI.
- Added ADR 0004 (culling/worker) and ADR 0005 (regional transactional snapshots).

### v0.5 world expansion and lighting

- Added `engine/voxel/lighting.*`: derived direct sky columns and bounded emissive
  flood propagation, with seam/roof/wall/source removal and relight invalidation.
- Extended block definitions/vertices/meshes with fluid/emission/opacity, normals,
  light and separate transparent indices. Vulkan draws opaque then sorted
  transparent chunks, supports transparent-only lifecycle, directional lighting,
  warm block light and horizontal/underwater fog. Shader push constants fit the
  Vulkan-guaranteed 128-byte minimum. A first hardware run caught a stale hardcoded
  attribute count; fixed it to the actual array size and reran strict validation.
- Expanded content to seventeen atlas tiles: sand, sandstone, snow, water and lamp.
  Lamps consume planks/coal through the fifth recipe. The crafting panel fits five.
- Added swimming, slow sinking, surface escape, underwater atmosphere and water
  fall cushioning. Survival raycasts skip fluids and placement can replace water;
  creative can target/remove water directly.
- Generator v3 adds coherent climate biomes, mountains/snow, river valleys/static
  water, 3D caves, biome tree density and deterministic small stone ruins with lamp
  rewards. Frozen generator v1/v2 samples remain tested. Expanded v3 streaming
  surface band to -32..159. New worlds default to v3; smoke retains the v2 scene.
- Added CPU `game/simulation/day_cycle.*`: a 20-minute cycle, atmospheric colors,
  sun direction and clock. Player payload v4 persists time and reads versions1..3.
  Client adds `--time`/`--freeze-time`, biome/time diagnostics and underwater fog.
- Fixed initial spawn search running unnecessarily for saved players/flight.
- Added `tests/lighting_tests.cpp`, `tests/expansion_tests.cpp` and strict hardware
  `tests/environment_playtest.cpp`. GCC: 97/97 CPU/CLI tests passed. All seven
  graphics tests passed with zero validation diagnostics. Day/night brightness,
  enclosed lamp lighting, water and transparent-only add/remove were exercised.
- Actual generator-v3 client runs captured river, desert, snow and ruin views,
  each with explicit save/reopen and 30 frames, zero validation warnings/errors
  including shutdown. Captures are `docs/voxelcraft-v05-*.png`.
- Added ADR0008 and updated README, both roadmaps, architecture and save format.
- Water remains static, transparency sorts chunks, sky light is direct vertical
  visibility and sun shadows are not implemented. These are explicit limits.

### v0.6 living-world foundation

- Added pinned EnTT 3.14.0 (exact installed package or fetched source), isolated
  inside `game/simulation/creatures.*`. Static blocks remain chunk arrays.
- Added stable actor save records and validation in `game/simulation/entity_data.*`.
  Player payload v5 persists namespaced actor kind, ID, motion, health and loot,
  reads v1..v4 and retains unknown dropped item keys. Unknown actor types fail.
- Added wandering/fleeing grazers and night-chasing/contact-damaging stalkers,
  fixed-step gravity/voxel sweeps, obstacle jumps, bounded natural spawning,
  occluded ray targeting, attack cooldowns, pickaxe damage and durability.
- Added physical collectible drops, full-inventory retention, retained distant
  drops and explicit 32-creature/32-drop caps. Killing blows stop at the drop cap
  so loot cannot be discarded. Far creatures despawn beyond 128 horizontal blocks.
- Added raw meat, a visible inventory icon and right-click food healing (+3 HP).
- Added `game/content/creature_models.*`: colored cuboid animals/enemies/loot with
  per-vertex sinusoidal limb motion. Renderer accepts generic reusable models and
  CPU instance values, batches visible instances by model, rotates normals and
  updates a coherent instance buffer only after the frame fence. No per-frame
  terrain mesh rebuilding/upload is needed for actors. Terrain/HUD use identity
  instance zero. Buffer growth/removal and conservative culling are exercised.
- Client integrates population, combat, pickups, food, death handling, actor save,
  lighting and visible/dynamic diagnostics. Smoke suppresses natural spawning.
- Added six CPU creature tests; all 103 GCC CPU/CLI tests passed at this checkpoint.
  The first strict SDL creature test passed in 1.55 s, zero Vulkan diagnostics.
  It drives capture-safe held combat/pursuit, tool wear, physical loot/food,
  save/reload identity and instance buffer/culling lifecycle. Captures were inspected
  and retained as `docs/voxelcraft-v06-creatures.png`/`...-combat.png`.
- Strict existing GPU smoke also passed after the instance/shader changes.
- v0.5 Clang and ASan/UBSan/LSan checkpoint: 97/97 each, no findings.
- Removed obsolete mesher face shading constants and added an explicit controller
  exception header. Fixed a compiler warning for a shadowed instance variable and
  a test macro's ambiguous else; third-party EnTT headers are system includes.
- Added ADR0009 and updated README, roadmap, building, architecture and save format.
- Pathfinding, dynamic body collisions, ecosystems, hunger and smelting remain
  unimplemented; automatic AI random/cooldown state is not persisted.

## Historical stopping point

This record stopped after the v0.6 checkpoint above. The transferred source also
contained an unfinished v0.7 mod runtime without client/persistence/test wiring.
Work resumed on the other machine on 2026-10-29; see the
[continuation and current handoff](development-session-2026-10-29.md).
The original timed development window is no longer active.

Known limits: synchronous saves/remeshing, no mipmaps, fluid flow/drowning,
full sky diffusion/sun shadows, hunger/smelting, mod runtime or networking.
Snapshots do not guarantee fsync/power-loss durability; latest corruption fails
explicitly and backup recovery is manual. Windows execution and sustained human
playtesting remain pending.
