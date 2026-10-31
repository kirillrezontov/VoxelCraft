# Architecture: viewer, interaction and persistent sandbox

The implemented Milestone 1 target split, persistence owner and static mesher
contract are described in [modular architecture](modular-architecture.md), with
the [dependency graph](module-dependency-graph.md) and
[verification gate](modularization-progress.md). The paths below remain valid;
`engine/voxel` now spans independent voxel/generation/lighting/meshing/storage
targets. ClientApplication/GameSession remain the next extraction.

`client/main.cpp` composes local play; shared CLI/preferences and the connected
loop live in `client/options.*`, `preferences.*`, and `network_client.*`.
`server/main.cpp` composes the headless host and console lifecycle.
It owns the window before the renderer, ensuring GPU resources and the Vulkan
surface are destroyed before SDL's window and subsystem.

| Module | Responsibility | Dependencies |
| --- | --- | --- |
| `engine/core/log.hpp` | Small stderr logging interface | Standard library |
| `engine/platform` | SDL window, input snapshot, relative mouse, timing, assets, surface | SDL3, Vulkan headers for surface integration |
| `engine/platform/file_lock.*` | Process-owned save lock, released by the OS on exit/crash | Native OS APIs, isolated here; no SDL/Vulkan |
| `engine/platform/tcp.*`, `stop_signal.*` | Owning nonblocking sockets and console-stop scope | POSIX/WinSock/CRT APIs; no SDL/Vulkan |
| `engine/network` | Bounded framing, partial I/O and endpoint parsing | CPU platform sockets |
| `game/network` | Strict value protocol, content matching, mirrored remote state | CPU game/content/network services |
| `server/host.*` | Authoritative players, survival, streaming, actors, mods and snapshots | CPU services only |
| `engine/voxel` | Registry, coordinates, chunks, world, generator, CPU mesher | GLM and standard library |
| `engine/voxel/storage.*` | Versioned whole-world snapshots and compressed regions | CPU world, binary codec, platform file lock |
| `engine/renderer/mesh.hpp` | CPU vertex/index payload and integer mesh origin | GLM and standard library; no Vulkan |
| `engine/renderer/texture.*` | Validated CPU RGBA images and atlas UV layout | GLM and standard library; no Vulkan |
| `engine/renderer/vulkan_renderer.*` | GPU lifetime, uploads, rendering, presentation, readback | Platform, camera matrices, Vulkan |
| `game/player` | Camera, walking/free flight, fixed-step player controller | GLM, CPU collision; no SDL or Vulkan |
| `game/gameplay` | Placement/destruction rules and player overlap rejection | Voxel storage, CPU rays/collision; no SDL or Vulkan |
| `game/content` | Built-in block registration and procedural atlas pixels | Voxel registry, CPU render data |
| `game/ui` | Themed screens, item catalog, console, CPU widgets and cached TrueType HUD geometry | Gameplay, CPU mesh/atlas data; no SDL/Vulkan |

The generator never calls renderer APIs. Static voxels are contiguous 16-bit IDs,
not entities or heap objects. The mesher consumes chunks and adjacent world data,
produces CPU mesh data, and leaves GPU lifetime to the renderer. Block access is
centralized behind Chunk::get/set, leaving room for palette storage later.

## Data flow and ownership

The application registers content, generates all resident chunks, then meshes
them. Meshing after generation removes internal faces across chunk boundaries.
Only nonempty meshes are uploaded. World owns chunks in a coordinate-keyed map;
renderer owns uploaded buffers and never retains references to chunk storage.
The temporary CPU mesh collection is released after upload. One bounded terrain
worker copies an immutable generator and transfers completed chunks to the main
thread. Pending, in-flight and completed chunks share the capacity limit. The main
thread polls events and presents loading progress; World and Vulkan remain owned
by the main thread. The stop-aware worker joins before its queue data is destroyed.

The default resident region is horizontal chunks -3..3, vertical chunks -1..1.
This covers the current generator's full surface range. World bounds default to
-512..511 and are separate from residency. Missing chunks and coordinates beyond
vertical bounds sample as air, so the viewer has visible edges and an underside.
Live streaming retains the surface band and the player's vertical neighborhood
within a configured horizontal chunk radius. One bounded worker generates missing
chunks near-first; the main thread integrates two per frame. Eviction caches
edited/legacy chunks as RLE with a raw fallback bounded by the original cell-array
size. Untouched chunks are regenerated. Resident terrain is bounded; retained edits
still grow with building activity and have an explicit snapshot cap.

World removal invalidates its GPU origin and resident face neighbors. A near-first
mesh queue normally builds two chunks/frame (four for edits), while always emitting
GPU removal messages. Pending stale generation never overwrites retained edits.
World edits invalidate affected meshes. Uploaded
meshes cache actual local vertex bounds and are tested against the camera's six
Vulkan clip planes before each draw. Culling stays camera-relative in double
precision with conservative float-rounding tolerances. Resource counts and
submitted mesh/triangle counts are separate diagnostics.

## Persistent sandbox

`WorldStore` owns an exclusive process lock on a chosen world directory. The
platform layer implements that lock with POSIX flock or a Windows exclusive file
handle; it is released on crashes without leaving a stale logical lock. The game
may load one committed revision, resolve its namespaced content palette, and
replace runtime IDs without changing world meaning. Unknown identifiers are
retained in visibly magenta solid fallback definitions.

Each save writes immutable region files and a manifest into a staging directory.
Renaming that directory to a fresh revision number commits all chunks and game
state together; the previous revision remains available. This avoids Windows's
different behavior for replacing existing files and prevents partial multi-region
updates. FNV-1a checksums detect accidental corruption; readers also validate sizes,
counts, coordinates, identifiers, runs and versions. Checksums are not authentication.
Files are closed and synchronized before publication. POSIX syncs staging and
parent directories around rename; Windows uses FlushFileBuffers and write-through
publication. Device/filesystem durability guarantees still depend on the platform.

The game owns the player's versioned binary payload. Storage treats it as bounded
opaque game data and never depends on camera/gameplay types. Loading is synchronous.
A main-thread snapshot merges resident edits with evicted retained chunks; one
bounded SnapshotWriter owns serialization and I/O. A completed result still counts
against capacity until consumed. Shutdown explicitly awaits and reports save errors;
destruction completes accepted work. Snapshot capture can still hitch. See
[world-format](world-format.md) and [ADR 0005](decisions/0005-world-snapshots.md).

## Coordinate and matrix conventions

World space is right-handed, +Y up; the default camera faces -Z. Increasing yaw
turns toward +X. Camera positions use double precision; block and chunk positions
use signed 64-bit integers. Mesh vertices are local floats in [0,32]. The renderer
subtracts camera position from each integer origin in double precision before
converting its offset to float. The view matrix contains orientation only.
This avoids large absolute float vertices, although exact sub-block camera
movement beyond double's integer precision is not guaranteed.

GLM matrices are column-major with column vectors: `clip = projection * view *
position`. Projection is right-handed with Vulkan depth [0,1], a 75-degree
vertical field of view, near=0.1 and far=1800. Projection Y is flipped for a positive
Vulkan viewport. Faces have outward counterclockwise winding; back faces are
culled. Cube face shading is a fixed multiplier, not a lighting simulation.

## Vulkan contract

Required: Vulkan 1.3, core `dynamicRendering`, `VK_KHR_swapchain`, graphics and
presentation queues (which may be separate), a BGRA8/RGBA8 SDR surface format,
and D32_SFLOAT or D16_UNORM depth attachment support. FIFO presentation is used.
SDL supplies platform surface instance extensions. No Vulkan 1.4 features are
required. Debug builds use `VK_LAYER_KHRONOS_validation` and `VK_EXT_debug_utils`
when both are available, otherwise log their absence. Vulkan errors include the
operation and result number. `--require-validation` requires and enables both
in any build type. Validation errors and warnings are counted atomically, with
the report checked after GPU teardown; any reported error makes the run fail.

One frame is in flight. A fence protects the command buffer, acquire semaphore
and shared depth attachment. Presentation semaphores are indexed by acquired
swapchain image, as described in the
[Khronos synchronization guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).
The fence is reset only after successful acquisition and recording. Resize and
out-of-date/suboptimal results request recreation. Zero-size/minimized windows
pause drawing. Device idle precedes swapchain destruction/recreation and shutdown;
this is the conventional baseline without swapchain-maintenance extensions.
The swapchain transition uses COLOR_ATTACHMENT_OUTPUT as both source and
destination stage, chaining it to the acquire semaphore wait at that stage.
Using TOP_OF_PIPE as the source produced a synchronization-validation
WRITE_AFTER_READ hazard and was corrected.

Buffers are move-only owners. The renderer's implementation owner exists before
initialization, so failed initialization destroys every already-created resource.
Shader modules have local exception cleanup; GPU buffers survive until pending
work is idle. Uploads use host-visible coherent memory, keeping this small static
scene simple. Device-local mesh staging, multiple frames in flight and allocation
pooling need profiling and are future work.

## v0.2 editing, selection and player movement

SDL events become an input snapshot in the platform layer. The client maps its
actions into player movement and content selection. The click that reacquires
capture is consumed, rather than editing terrain. Key/button actions ignore key
repeat; there is no held-button block editing. Gameplay does not consume SDL events.

`raycast` traverses cells using normalized DDA, with a six-unit reach in the client.
Axis-aligned rays have infinite crossing time for unused axes. All exactly tied
axes advance at an edge/corner, so cells touched only at an edge are skipped.
The reported placement face chooses X, then Y, then Z for ties. A ray beginning
on an integer plane and moving negatively starts in the cell it enters; starting
inside a block reports a zero normal and does not allow placement. Invalid or
unrepresentable inputs are rejected; traversal reach is bounded to 1024 units.

World::setBlock only changes resident cells inside vertical bounds. It marks its
chunk dirty and, for changed boundary faces, resident face neighbors. Corner
changes can invalidate four chunks, not diagonal neighbors. The client drains
this set, remeshes those chunks, and passes replacements to the renderer by
integer origin. Empty meshes remove GPU entries; formerly empty chunks add them.
The single submitted frame fence covers all terrain/model/HUD references; buffer
replacement waits for that fence, while swapchain recreation/shutdown still wait
for the device/presentation. Owned neighbor/lighting snapshots are meshed on one
bounded worker; revision tickets discard superseded edits, lighting and evictions.
GPU allocation/upload remains on the main thread and can still hitch.

Selection and crosshair geometry are small persistent GPU meshes. A push-constant
flag selects vertex-color shading for those overlays; terrain samples the atlas.
The selection outline is camera-relative and depth tested. The crosshair is drawn
last in screen space at near depth. Renderer APIs accept meshes and an optional
integer selection origin; no world or block-editing rules enter Vulkan code.

Collision uses a separate `solid` property from rendering opacity. The player is
a 0.6×1.8 AABB with eye height 1.62. Walking runs at 120 fixed steps/second, 5 units
per second, gravity 24 units/s² and an 8.2-unit/s grounded jump impulse. Frame deltas
are capped at 0.1 s. Swept Y/X/Z movement resolves contacts and slides along walls,
with a small contact gap to avoid numerical overlap. CPU tests cover floor,
ceiling, wall, high-speed and negative-coordinate contacts and frame-rate independence.

Missing chunks are closed to collision, while storage/meshing/raycast treat them
as unavailable air. Below minY is closed; above maxY is open. This prevents normal
walking into ungenerated data without implementing streaming. Collision APIs
bound body sizes and movement to keep CPU broad-phase work finite. Positions use
double precision (currently restricted to ±1e15 for collision); exact remote-world
player precision still needs future local-coordinate physics. There is no automatic
stepping, crouching, swimming, depenetration solver or dynamic rigid-body system.

See [ADR 0003](decisions/0003-interactive-world.md) for milestone scope and ownership.

## Creative inventory and HUD

The creative catalog derives from ItemRegistry and includes blocks, ordinary items,
tools and mod-defined items. Nine non-consuming item assignments persist by names.
Unknown saved assignments retain missing-item entries without inventing blocks.
Counted stacks and durability remain exclusively in SurvivalInventory.

The CPU controller owns screen/search/focus/drag state; SandboxUiLayout owns
constrained scaling. Separate view functions paint HUD, inventory, crafting,
settings, pause and console. SDL translates text events and navigation, with
logical-to-pixel pointer conversion. Menus isolate gameplay input. Solo Pause and
Settings freeze simulation; multiplayer menus only suspend local input.

The UI uses embedded licensed DejaVu fonts, a bounded stb_truetype atlas and
clipped UTF-8 geometry. Vulkan receives generic HUD meshes and a separate font
sampler at the frame fence. Geometry persists until visible state changes; only
visible console output and short notification transitions rebuild. Item icons use
the content atlas. See [the UI design](v0.8.2-ui-design.md) for layout/input details.

## Survival foundation

Items have a registry independent of block storage, using namespaced save keys.
Block items reference placement definitions; sticks, coal and pickaxes need no
world block. A 36-slot survival inventory owns quantities and tool durability.
Stack adds, transfers and recipes use transactional copies so failed collection or
crafting cannot lose ingredients or duplicate output. Survival and creative modes
reuse the same storage, collision, raycasting, placement and Vulkan code.

Mining accumulates work while a captured held edit targets the same block/tool.
Changing target, tool/slot, focus or capture cancels progress. The capture click is
not armed for mining until a later editing press. Game content sets hardness,
harvest tiers and drops. Finished collection checks inventory space before changing
World. Tools wear only after an actual block removal. Placement consumes one item
only after gameplay placement succeeds.

The player controller reports resolved landing speed. Unknown streamed contacts
freeze vertical velocity and do not create grounding or impacts, avoiding false
fall damage and invisible-floor jumps. Survival applies damage once on real
landings, keeps inventory on death, and searches a clear origin-area spawn for R.
The UI owns no physics; it displays health, backpack, recipes and death state.

HUD foreground text renders after icons. The renderer's generic progress indicator
uses persistent meshes and push-constant scaling, with no gameplay/mining logic or
per-frame buffer reallocation. Survival state is stored in player payload v3;
earlier creative payloads remain readable. Generator v2 adds trees/ores while v1
is kept intact for old worlds. Smelting, hunger, combat and world item entities are
not implemented by this foundation.

## Texture atlas

The content layer generates a deterministic 306×18 RGBA atlas: seventeen 16² tiles
with a one-texel gutter on every side. Tiles include white fallback, stone, dirt,
grass top/side, log side/end, planks, leaves, cobblestone coal/iron ore, sand, sandstone, snow, water and lamps.
Grass bottom reuses dirt. Each definition stores six
runtime tile IDs in +X/-X/+Y/-Y/+Z/-Z order; the mesher creates local face UVs
with the upper grass fringe at V=0. The white tile preserves vertex tint support.
UV bounds, invalid tile IDs, fringe orientation and gutter pixels are CPU-tested.

The renderer accepts generic RGBA pixels, not game-specific materials. It stages
them into a device-local R8G8B8A8_SRGB image, transitions to shader-read layout,
and binds a combined image/sampler descriptor in the fragment shader. Texture
resources and descriptors outlive swapchain recreation and are destroyed after
pending work finishes. This requires sampled-image and transfer-destination
support for RGBA8 sRGB in addition to the device features above.

Sampling is nearest, without anisotropy or mipmaps; no optional device features
are required. Gutters duplicate edge/corner pixels to isolate tiles. Distance
aliasing is a known limitation. Built-in textures remain code-generated. Mod PNG tiles use a hash-pinned
PNG-only decoder with bounded dimensions/file/decoder allocations. A CPU
ContentAtlas combines built-in and external tiles, preserving built-in indices
and rebuilding gutters; the client supplies its image and layout consistently
to the renderer, mesher and HUD.
See [ADR 0002](decisions/0002-texture-atlas-and-validation.md).

There is no generic rendering backend, general job system or ImGui overlay.
Lua serves controlled mod hooks; EnTT serves dynamic actors only. Other dependencies will be introduced for concrete
milestones. See [the decisions](decisions/0001-v01-foundation.md).

## World expansion and lighting

Generator v3 adds climate biomes, cave fields, rivers, mountains, snow and small
ruins while preserving v1/v2. Streaming expands the v3 surface band to -32..159.
The CPU Lighting service derives direct sky visibility and level-15 emissive BFS
from resident cells. Geometry and lighting dirty sets feed the same budgeted mesh
queue. Game DayCycle supplies sunlight/ambient/sky data; Vulkan owns only generic
render parameters. Player payload v4 persists simulation time.

Opaque/transparent indices share vertices but use separate pipelines. Water draws
with depth testing and alpha blending after opaque geometry, sorted by chunk
distance. Its non-solid fluid definition drives swimming/fall cushioning and
underwater atmosphere. Flow, drowning, sky diffusion and sun shadow maps remain
planned. See [ADR 0008](decisions/0008-world-expansion-lighting.md).

## Living world

`game/simulation/creatures.*` hides an EnTT registry behind value APIs. Dynamic
actors use stable save IDs, voxel AABB sweeps and a bounded fixed-step update.
Grazers wander/flee; stalkers chase and damage nearby survival players at night.
Melee targets respect voxel occlusion and tools wear only on successful attacks.
Dropped items remain physical until successful transactional pickup; raw meat heals.
Actor payload v5 shares transactional snapshots with terrain and player state.

Game content builds cuboid models. The renderer accepts reusable generic meshes
and instance values, culls conservatively, batches by model, rotates normals and
applies per-vertex sway/instance phase. One coherent instance buffer updates after
the frame fence; terrain/HUD use identity instance zero. Vulkan never knows EnTT
handles or creature rules. See [ADR0009](decisions/0009-living-world.md).


## Modding

`game/modding/runtime.*` owns pack discovery, validated declarative content and
one bounded Lua state per scripted pack. It depends on CPU gameplay, not platform,
renderer or EnTT. The client registers packs before loading world palettes,
installs items/recipes before restoring inventories, and dispatches notifications
after successful player actions. UI reports successful crafted item IDs by value.

Lua callbacks stage block edits, an inventory copy, namespaced persistent state
and messages. Only a successful protected call commits them; errors disable the
script and preserve other packs. World-owned dirty tracking drives existing
lighting/mesh queues after script edits. Scripts cannot touch Vulkan or World
ownership. Player payload v7 shares mod state/schema metadata with transactional
world snapshots, retaining missing pack records and reading previous versions.
`on_migrate` runs state-only schema upgrades in fresh VMs and commits the entire
restored mod set atomically. Declarative mining rules install after items, using
the gameplay-owned validation and collection transaction. ContentAtlas and PNG
loading remain CPU services; only the renderer uploads atlas pixels.
See [modding](modding-design.md) and [ADR0010](decisions/0010-modding-foundation.md).

## Authoritative multiplayer

Host owns a shared CPU world and up to eight independent controllers/survival
states. The streaming worker serves the union of player regions. World-owned
data revisions change on insert/edit, independently of neighbor mesh dirtiness.
The server sends revisioned compressed chunks, unloads and 20 Hz snapshots.
The client mirrors values, owns rendering/lighting/meshing, smooths authoritative
motion and responds immediately to look input. Movement prediction remains planned.

Client commands contain intent and recipe/slot requests; quantities, placement,
mining work, damage and recipes are validated and applied by Host. The shared UI
can send inventory transactions to the server without mutating its item mirror;
offline play executes those same transactions directly. Health/items are private
per-recipient snapshots. Named profiles persist with world/mod/actor state in
payload v9, while retaining earlier formats and offline base-player state.
The standalone host has no SDL/Vulkan dependency. Native socket/signal code stays
in the platform layer. See [multiplayer](multiplayer.md) and
[ADR0013](decisions/0013-authoritative-multiplayer.md).


## v0.8.1 stabilization decisions

Opaque full-cube faces use slice masks keyed by block ID and face illumination.
Masks are allocated only for visible slices; compatible rectangles merge without
crossing material/light boundaries. Vertices carry optional atlas bounds and local
repeat coordinates, so the fragment shader repeats each 16² tile with its original
orientation. Transparent and non-opaque cubes keep their existing unit-face path.
The original culled mesher remains selectable for equivalence tests/benchmarks.
Non-cubic geometry does not exist yet and requires its own future specialized path.
Chunk arrays stay at 65,536 bytes; a palette rewrite is deferred. Save format and
modding API are unchanged. See [ADR0014](decisions/0014-stabilization.md).

Protocol 3 adds an optional private profile credential to Hello. Host validates it
before selecting/creating a profile; credentials live in operator-owned files,
never world payloads. External unauthenticated binds require explicit trust opt-in.
A single server thread owns world/player/mod/simulation state; network clients
send only validated intent/transactions. Platform file sync and Windows UTF-8
process configuration remain outside gameplay. CPack installs separate Client and
Server components, with detected redistributables, licenses and compiled shaders.

## v0.8.2 shared commands and authority

`game/commands` provides bounded parsing, typed errors, registry metadata,
permission filtering, completion and reusable callbacks. Context contains trusted
references to existing world/player/inventory/actor/day-cycle services and narrow
save/broadcast/administration sinks. Local owner, remote requests and server stdin
use these same definitions at their simulation mutation boundary. Gameplay has no
Vulkan or UI dependency. C++ definitions can be extended without parser changes;
full Lua command registration remains optional future work.

Protocol 4 adds sequenced command requests and structured replies, mode/hotbar/
pose revisions and server-selected permissions in snapshots. A request cannot
supply issuer or role. Only authenticated profile keys can receive persisted roles;
console alone grants/revokes. Resource caps, audit redaction, migration and syntax
are documented in [commands](v0.8.2-commands.md). Player payload 10 appends explicit
per-profile mode and spawn fields; versions 1..9 and metadata/regions 1 remain readable.
