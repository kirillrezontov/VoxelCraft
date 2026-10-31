# VoxelCraft Development Roadmap

## Current Milestone: v0.8.2 — UI/UX and Developer Console (release candidate)

- [x] Baseline audit/build and 185-test regression baseline
- [x] Licensed embedded sans/mono TrueType fonts, UTF-8/Cyrillic, bounded glyph cache
- [x] Shared theme, constrained layouts, CPU widgets, focus, tooltips and retained HUD
- [x] Full ItemRegistry creative catalog, pickaxes, search/categories and mod metadata/icons
- [x] Authoritative drag/split/quick transfer, read-only recipe eligibility and crafting UI
- [x] SDL text console, selection/clipboard, bounded history/output and live completion
- [x] Shared parser/registry and real local/server gameplay/admin commands
- [x] Per-player modes, safe teleport/spawn points, retained counted inventory and durability
- [x] Protocol 4 bounded requests/replies, authenticated persisted roles and audited operations
- [x] Nonblocking headless stdin console, EOF/service mode and graceful signal shutdown
- [x] Solo Pause/Settings simulation freeze, multiplayer input isolation and persisted UI scale
- [x] Player payload 10 migration from 1..9; world format 1 and mod API 1 retained
- [x] CPU/network/restart/identity/flood regressions and SDL/Vulkan console playtest
- [x] GCC/Clang/sanitizer builds, licensed asset packaging and user/developer documentation
- [ ] Windows/MSVC execution and sustained human cross-platform playtesting (unverified locally)

Detailed task coverage and acceptance evidence: [v0.8.2 implementation record](docs/v0.8.2-implementation.md).
Repository completeness and clean-checkout evidence are recorded separately in
the [baseline recovery record](docs/baseline-recovery.md).
Weather and full Lua command registration are optional, explicitly deferred.

## v0.8.1 — Stabilization (historical release-candidate foundation)

- [x] Baseline audit and complete CPU/CLI regression run
- [x] Profile-specific private-server credentials; explicit trusted-LAN mode
- [x] Protocol 3 diagnostics, idle cleanup, bounded client residency and shutdown notification
- [x] Durable region/manifest writes and atomic directory publication
- [x] Owned, bounded asynchronous terrain/meshing/snapshot work
- [x] Opaque greedy meshing, tiled UVs and surface-equivalence regressions
- [x] Worker/render/save/network diagnostics and reproducible mesh benchmark
- [x] Single-frame fence replacement of terrain/HUD GPU buffers
- [x] Window size, fullscreen and VSync startup settings
- [x] ASan/UBSan and isolated TSan presets; bounded network stress tooling
- [x] CPack client/server archives, dependency notices and extracted-archive tests
- [x] Linux/Windows CI packaging plus optional hardware Vulkan workflow
- [ ] Windows MSVC build/CPU tests/archive checks actually executed on Windows
- [ ] Windows gameplay and cross-platform human LAN acceptance
- [ ] Remote CI results and public distribution license selection

Detailed task coverage, measurements, commands and remaining blockers are tracked
in [docs/v0.8.1-stabilization.md](docs/v0.8.1-stabilization.md). Release-candidate
implementation does not constitute final Windows or human playtest acceptance.


The persistent creative sandbox is implemented, and the optional survival slice
now provides a real resource/tool progression. These are development checkpoints;
sustained human playtesting and Windows execution remain pending.

### Implemented v0.4 foundation

- [x] Namespaced item registry and independent stack quantities
- [x] Nine-slot survival hotbar and 27-slot backpack with safe transfers
- [x] Timed held-button mining, visible progress and capture/focus cancellation
- [x] Block drops, required harvest tiers and inventory-full protection
- [x] Transactional recipes: logs → planks → sticks → wooden/stone pickaxes
- [x] Tool durability, breakage and one-item placement consumption
- [x] 20 health, fall damage, death and safe R respawn with inventory kept
- [x] Explicit creative/survival modes, saved health/items/durability and migration
- [x] Generator v2 trees, coal and iron deposits; generator v1 compatibility
- [x] Log, plank, leaf, cobblestone and ore textures plus tool/item icons
- [x] Unavailable streamed floors pause falling without damage or fake grounding
- [x] CPU and strict SDL/Vulkan progression, save/reload and death/respawn tests
- [ ] Sustained human survival playtest and balance polish

### Implemented v0.5 foundation

- [x] Generator v3 plains, forest, desert and mountain biomes, snow and rivers
- [x] Coherent caves, biome-dependent trees and small stone ruins with lamps
- [x] Frozen generator v1/v2 compatibility and expanded bounded surface residency
- [x] Separate translucent mesh stream, static water, swimming and underwater fog
- [x] Directional sunlight, direct sky visibility, propagated block lamps and fog
- [x] Saved 20-minute day/night cycle, repeatable CLI time controls
- [x] Craftable lamps and sand/sandstone/snow/water content
- [x] CPU tests, strict environment graphics checks and generated-world captures
- [ ] Sustained human exploration/playtest and Windows execution
- [ ] Fluid flow, sky-light diffusion, sun shadows and precise transparency sorting

### Implemented v0.6 foundation

- [x] EnTT dynamic actors; static blocks remain in compact chunks
- [x] Wandering/fleeing grazers and night-chasing hostile stalkers
- [x] Swept voxel collision, gravity and obstacle jumps for creatures/drops
- [x] Occluded melee targeting, held attack cooldowns, tool damage/wear
- [x] Physical collectible drops, full-inventory retention and explicit population caps
- [x] Raw meat food healing and hostile contact damage
- [x] Stable actor identities, saved motion/health/loot and payload v1..v4 migration
- [x] Animated reusable models with culled/batched Vulkan instance rendering
- [x] CPU and strict SDL/Vulkan combat, loot, food and actor persistence tests
- [ ] Sustained human gameplay/balance and Windows execution
- [ ] Pathfinding, richer behaviors, dynamic body collisions and an ecosystem

### Implemented v0.7 foundation

- [x] Deterministic pack discovery with exact dependencies, API/version validation
- [x] Declarative namespaced blocks, items, tools and transactional recipes
- [x] Paged crafting menu for added recipes and successful-craft notifications
- [x] Controlled Lua load/tick/place/break/craft hooks with staged gameplay changes
- [x] Lua memory/instruction limits, hook rollback and script-disable diagnostics
- [x] Client mod options, content-before-save resolution and persistent pack state
- [x] Player payload v6 with v1..v5 reading and missing-mod data retention
- [x] Example glowstone pack and API/content documentation
- [x] CPU/sanitizer coverage and strict SDL/Vulkan modded crafting/persistence checks
- [ ] Manual modded/survival playtests and Windows execution
- [x] Explicit state schemas, transactional Lua migrations and downgrade rejection
- [x] Declarative mining time, required tool/tier, drops and quantities
- [x] External 16×16 PNG tiles, bounded decoder and shared repacked atlas
- [x] Explicit API-1 development compatibility policy

### Implemented v0.8 foundation

- [x] Dedicated 60 Hz server with no SDL/Vulkan initialization or linkage
- [x] Portable nonblocking TCP, bounded framing and backpressure
- [x] Content/version handshake and server-owned player movement/edit validation
- [x] Up to eight players with independent persistent names/poses/hotbars
- [x] Multiple streaming centers, revisioned compressed chunks and unloads
- [x] Shared day/creature state, visible animated player models and connected Vulkan client
- [x] F5/autosave/console-interrupt snapshots and reconnect persistence
- [x] Two real loopback players and strict SDL/Vulkan shared custom-block playtest
- [x] Server-authoritative survival mining/crafting/health/private inventories and respawn
- [x] Bounded eight-client connection/edit/save/restart soak tooling
- [ ] Client prediction and extended latency/bandwidth tuning
- [x] Per-profile private-server credentials (v0.8.1)
- [ ] Encryption, internet hosting and server administration
- [ ] Windows execution and sustained human LAN playtesting

### Next development step

Execute the Windows/MSVC workflow and manual cross-platform acceptance matrix.
Resolve the release blockers in [the stabilization record](docs/v0.8.1-stabilization.md)
and the [v0.8.2 acceptance record](docs/v0.8.2-implementation.md) before a stable release.

## v0.3 — Persistent Sandbox (implemented development foundation)

The interactive slice is implemented and passes CPU and scripted SDL/Vulkan
playtests. Sustained human playtesting and Windows execution remain pending;
neither v0.2 nor v0.3 is claimed as a stabilized release.

### Implemented v0.3 foundation

- [x] Camera-relative frustum culling with submitted mesh/triangle diagnostics
- [x] One bounded CPU terrain worker with responsive startup events/progress
- [x] Versioned world metadata, seed, generation version and height bounds
- [x] Palette/RLE-compressed 4³-chunk regional snapshots with namespaced block keys
- [x] Atomic whole-snapshot commit, previous revision retention and checksum validation
- [x] Missing content retained as visibly magenta solid blocks
- [x] OS-owned exclusive session lock, released on process termination
- [x] Player position/orientation/movement-mode/selected-material persistence
- [x] Default user-data world, explicit world directory, temporary mode, F5 and autosave
- [x] CPU round-trip/corruption tests and executable create/reopen graphics checks
- [x] Basic creative inventory catalog and visible nine-slot textured hotbar
- [x] Pointer-mode inventory interaction, number/wheel selection and saved assignments
- [x] F10 settings menu with real FOV, sensitivity and invert-Y controls
- [x] Versioned user settings, backup recovery, defaults and CLI overrides
- [x] Bounded horizontal/vertical terrain streaming with compact retained edits
- [x] Near-first frame-budgeted meshing and GPU removal on chunk eviction
- [x] Save/reload/revisit coverage for evicted edits and legacy snapshots

### v0.3 verification and limits

See the
[session record](docs/development-session-2026-10-28.md) for executed checks and the
latest stopping point. Modified chunks persist across streaming; bounded background
meshing and snapshot I/O are implemented in v0.8.1.

The v0.1 implementation is complete under SPEC §35: the Linux application is
buildable, tested and hardware-validated, with Windows CMake/CI preparation done.
Windows compilation/runtime remain unverified; this is not a claim of a Windows release.

### Implemented v0.2 foundation

- [x] Grid DDA raycasting with six-block reach, negative coordinates and corner/grid-plane tests
- [x] Visible block selection outline and crosshair
- [x] Single-click block destruction and adjacent placement
- [x] Grass/dirt/stone selection using keys 1/2/3
- [x] Rejection of occupied, unloaded, out-of-height and player-overlapping placement
- [x] World-owned dirty chunk tracking, including neighbors at changed faces
- [x] GPU replacement/addition/removal of only affected chunk meshes
- [x] Player AABB collision, wall sliding, gravity, grounded jumping and fixed-step walking
- [x] F toggles free flight; entering walking mode inside solid terrain is rejected
- [x] CPU tests for rays, editing, dirty neighbors, collision, jumping and frame-rate independence
- [x] Strict GPU smoke coverage of jump/landing, seam edits, selection and GPU mesh lifecycle
- [ ] Sustained manual mouse/keyboard and building playtest
- [ ] Broader collision/control polish following playtesting
- [ ] Windows build/runtime verification when a runner is available

The interactive slice is running; v0.2 is not marked release-complete.
The historical v0.2 slice kept fixed residency; current v0.3 streams terrain.
Missing data still blocks walking until nearby generation completes.
Sustained manual walking/building tests and Windows execution remain pending.
See [v0.2 verification](docs/verification-v02.md) for executed checks and limits.

## v0.1 — Completed World Viewer Foundation

The original milestone checklist and [verification record](docs/verification.md)
are retained below. Its implementation is the basis of v0.2.

### Tasks
- [x] CMake project setup (presets, pinned FetchContent dependencies, installed-dependency option)
- [x] SDL3 integration (events, relative mouse, high-DPI window, resize/minimize)
- [x] Vulkan initialization (1.3, device capability checks, optional validation)
- [x] Vulkan rendering pipeline (dynamic rendering, textured indexed meshes, depth)
- [x] First-person camera (WASD, mouse look, vertical free flight)
- [x] Block registry (namespaced identifiers, duplicate/unknown handling)
- [x] Chunk storage (32³, compact 16-bit IDs, 64-bit coordinates, configurable height)
- [x] Procedural terrain generation (seeded coherent noise, grass/dirt/stone)
- [x] Chunk mesh generation (six faces, neighbor-aware visibility, winding tests)
- [x] Render generated voxel terrain (fixed surface region; no streaming)
- [x] Basic debugging and diagnostics (FPS, frame time, camera, chunks, triangles, startup timing)
- [x] Linux build verification (GCC and Clang; hardware Vulkan smoke test)
- [x] Windows CI build configuration (prepared; Windows compilation/runtime unverified)
- [x] Unit tests (CTest + GoogleTest; graphics-independent core)
- [x] Texture atlas (procedural 16² tiles, per-face materials/UVs, padded edges, staged sRGB upload)
- [x] Core and synchronization validation (project-local Khronos layer; acquire/transition hazard fixed)
- [x] Opt-in GPU CTest and strict validation mode (checks errors through resource destruction)

### v0.1 verification and baseline limitations

The hardware smoke test exercises rendering, scripted camera motion, swapchain recreation,
minimize/restore, mouse capture and image readback. Captured terrain was inspected.
Core and synchronization validation pass on the RTX 3050 with a downloaded,
checksum-verified project-local layer: 0 errors and 0 warnings, including shutdown.
Windows remains unverified until CI executes. The v0.1 baseline omitted texture mipmaps,
frustum culling, streaming, collision, block interaction and persistence. It drew every nonempty
mesh in a fixed region, default 7×7×3 chunks; world bounds remain -512..511.

## Future Milestones

### v0.3 — Persistent Sandbox (active above)
Saving/loading, creative inventory/hotbar and settings implemented; stabilization continues.

### v0.4 — Survival (active above)
Resource/tool progression implemented; human playtesting and balance remain pending.

### v0.5 — World Expansion (implemented foundation above)
Biomes, caves, trees, small ruins, direct sky/block lighting and static water.

### v0.6 — Living World (implemented foundation above)
EnTT actors, basic animal/enemy AI, combat, loot, food and animated instances.

### v0.7 — Modding (implemented foundation above)
Declarative content, Lua hooks, mod discovery and saved state; stabilization continues.

### v0.8 — Multiplayer
Dedicated creative/survival server, networking and synchronization implemented
above; stabilization continues.

### v0.9 — Advanced Voxels
Composite blocks, multiblock objects, custom shapes.

### v0.10 — Advanced Graphics
PBR, shadows, water, LOD, GPU optimizations.

### v1.0 — First Full Release
Polishing, optimization, Windows/Linux distribution.
