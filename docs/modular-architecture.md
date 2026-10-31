# Modular core foundation

Milestone 1 builds on `82d7f28`, the merge containing recovered baseline
`460a4f3` and the four recovery commits. The additional `5ab11af` tracks the two
user task notes. Those notes and the independent VoxelUI history are preserved.
See [verification and gate status](modularization-progress.md) before advancing.

## Implemented boundaries

Sources stay at their existing paths. A module is a CMake dependency and ownership
boundary, not a directory rename or a C++ language module.

| Target | Public operations / responsibility | Important dependencies |
| --- | --- | --- |
| `vc_options`, `vc_text` | C++23/options; UTF-8 and presentation values | GLM, standard library |
| `vc_render_data`, `vc_camera` | CPU meshes/images/atlas/frustum/overlay; camera values | GLM; no GPU APIs |
| `vc_voxel` | `BlockRegistry`, `Chunk`, `World`, raycast, collision | CPU render values, text; no workers or terrain policy |
| `vc_generation` (extracted) | `TerrainGenerator`, `TerrainWorker`, `ChunkStreamer`, retained edits, streaming region | `vc_voxel`, Threads |
| `vc_lighting` (extracted) | `Lighting::refresh`, light queries and immutable mesh snapshots | `vc_voxel` |
| `vc_meshing` (extracted) | CPU mesher, synchronous queue, snapshot worker/coordinator, static policies | `vc_voxel`, `vc_lighting`, Threads |
| `vc_storage` | `WorldStore`, `SnapshotWriter`, new `WorldPersistence` | `vc_voxel`, Threads; private platform files |
| `vc_files`, `vc_tcp`, `vc_stop_signal`, `vc_console_input` | Native file/socket/signal/stdin mechanics | Standard library/OS; stdin also uses text |
| `vc_network` | Framed transport and addresses | `vc_tcp` |
| `vc_content`, `vc_images` | Built-in definitions/CPU models/atlas; bounded PNG | Voxel/render values; PNG decoder private |
| `vc_gameplay` | Inventory, survival, interaction, controller, saves/profiles, spawn | Voxel/camera; generation for spawn sampling |
| `vc_simulation` | Living actors and day cycle | Gameplay; EnTT private |
| `vc_modding` | Existing declarative/Lua pack runtime | Gameplay/content; Lua/PNG/JSON private |
| `vc_commands` | Shared trusted command mutation context, parser and roles | Simulation/storage/text/files |
| `vc_protocol`, `vc_remote_session` | Validated protocol/content fingerprint; remote mirror and transport | Network/game/commands; remote links protocol |
| `vc_server_host` | Authoritative players/world/streaming/mods/save coordination | Protocol/storage; no lighting, meshing or client UI |
| `vc_settings`, `vc_ui_core`, `vc_game_ui` | Settings persistence and retained legacy UI | CPU files/text/game services |
| `vc_input`, `vc_platform`, `vc_renderer` | SDL input/window; Vulkan owner | Client only; renderer exposes CPU values |
| `vc_client_modules` (new interface target) | Host mesher selection | `vc_meshing`; client policy definition |
| `vc_client_runtime`, `voxelcraft` | CLI/preferences/remote loop; local composition | Explicit client services and selected mesher |

Previously `vc_voxel` built generation, streaming, lighting and all meshing workers.
Every storage/gameplay/network consumer inherited that entire link surface. Those
implementations now compile in separate libraries. The server executable's actual
link closure omits mesh, lighting, SDL, Vulkan, font and UI libraries. The test
executable deliberately exercises CPU UI and meshing too. `BUILD_TESTING=OFF`
allows a server-only default build to omit those test-only builds entirely.

The renderer's platform, camera, font and Vulkan links are now private; only CPU
render values are public. Hosts that use windows link `vc_platform` explicitly.
CPU render values retain their `engine/renderer` path; this path does not imply
a dependency on `vc_renderer` or Vulkan.

## Concrete service contracts

`engine/voxel/persistence.hpp` introduces `WorldPersistence(path)`. It owns a
locked `WorldStore` followed by a `SnapshotWriter`, so reverse destruction joins
the writer before releasing the lock, including constructor failure and exception
unwinding. Both local play and `Host::Impl` now own this service through one
`unique_ptr`; a temporary world has no service. It owns neither live world nor
gameplay state.

- `load(BlockRegistry&) -> optional<WorldArchive>` is synchronous and resolves the
  saved palette after content registration. It rejects a pending save/result.
- `requestSave(World, BlockRegistry, WorldMetadata, string) -> bool` transfers
  owned snapshot values to the existing bounded worker. False means busy; a
  completed but unconsumed result still counts as busy.
- `saving()`, `takeSaveResult()`, `waitForSave()` retain existing polling versus
  explicit shutdown-wait behavior. `SnapshotResult::rethrowIfFailed()` surfaces
  I/O errors. Poll/consume results and report failures before session destruction;
  destruction finishes an accepted write but cannot report its error to the UI.
- `directory()` is a borrowed const path valid for the service lifetime.

All service methods are owner-thread operations, not a concurrent API. Once a
save result is consumed, the worker has completed all store I/O. `load` cannot
race it under this contract. Low-level `WorldStore`/`SnapshotWriter` remain
available for existing consumers and tests; the hosts no longer coordinate their
relative lifetimes manually.

Other real contracts are retained: `GameSettings` validation/load/save,
`ModRuntime` content installation/restore/dispatch, command `Context` and its
validated sinks, `RemoteSession` intent/mirror APIs, and `VulkanRenderer`'s CPU
payload API and explicit `shutdown`/validation report. No abstract service tree,
service locator, unused resource interface or speculative session API was added.

## Ownership and destruction

| Owner today | Owned state and required order |
| --- | --- |
| Local `run` | Window before renderer. Content registered before palette loading; metadata/world before player restoration. UI borrows inventories/settings; callbacks remain scoped to the loop. |
| `World` | Compact static chunks, revisions and dirtiness; only owner thread mutates it. Static cells are not actors. |
| `ChunkStreamer` | Borrows live world; owns generator, retained edits and terrain worker. Worker owns immutable generation data and joins before its queues die. Streamer dies before World. |
| Mesh coordinator | Owns worker/tickets. Each job copies center/face neighbors and relevant lighting. No live World/Lighting pointers cross threads. Worker joins before its immutable registry/queues die. |
| `WorldPersistence` | Owns store/lock and worker. Save jobs own world, registry and payload copies, so they do not borrow session services. |
| Local player / simulation | Controller owns camera; inventory/survival/actors/day/mods stay owner-thread state. Saved player payload remains game-owned. |
| Remote `runRemote` | Owns `RemoteSession`, local content and presentation mirrors; server remains authoritative. Meshing consumes mirror snapshots. Disconnect occurs before renderer/window teardown. |
| `Host::Impl` | Owns content, world/persistence, generator/streamer, actors/day, connections and saved profiles. Connections die before referenced profile map/content; streamer dies before World. |
| Renderer | Owns GPU resources and borrows Window. Mesh/HUD upload copies CPU values. Explicit shutdown drains/releases GPU state before inspecting validation and before Window dies. |

No `ClientApplication`, reusable `GameSession`, `ScreenManager` or VoxelUI runtime
exists yet. In particular each current client loop still constructs its own
window/renderer and starts with a world/connection. Milestone 2 must separate that
lifetime; see [the concrete handoff](next-milestone-client-sessions.md).

## Compatibility and limits

Terrain versions 1–3, world/region format 1, player readers 1–10, network protocol
4 and mod API 1 are unchanged. Generation, voxel loops, light propagation and
mesh geometry algorithms are preserved. The default policy remains greedy;
the explicit culled build uses the pre-existing reference algorithm.

External save/mod/network data still reaches the same bounded validators and
atomic storage routines. The persistence service adds exclusion around loading
while saving; it does not weaken validation or give scripts new access. Worlds,
paths and pack directories are host-selected; there is no new resource resolver,
native execution boundary or sandbox claim.

The [link checker](../cmake/ModuleBoundaries.cmake) rejects project link cycles,
graphics/Lua/mod/UI leakage into CPU core targets, and UI/meshing/lighting leakage
into server targets. It checks direct and transitive PUBLIC/PRIVATE link edges;
it is not a full C++ include analyzer. Shared source-root include directories are
retained for compatibility. Public headers are source contracts, not stable ABI.
The gameplay target still contains spawn and save-value helpers; protocol still
couples content fingerprints to modding. These are real dependencies to revisit
only when another concrete consumer justifies smaller targets.
