# Modular core foundation (Milestone 1)

This is the implemented CMake/source architecture, built on the recovered baseline
merged at `82d7f28`. It is ordinary C++23 libraries, not C++ language modules or a
stable binary SDK. No source directories were relocated. The existing game,
Vulkan renderer, Lua/data mods and legacy UI remain active.

## Findings and decisions

The baseline already separated sockets, files, persistence, gameplay, simulation,
SDL input/window, CPU render data and Vulkan. Its graph was acyclic; it did not
need a new universal engine registry. However, `vc_voxel` owned world storage,
generation, streaming, lighting, collision, raycasts, meshing and workers. Any
consumer of persistence acquired the entire voxel implementation. `vc_protocol`
publicly linked `vc_modding` because content fingerprinting lived beside packet
codecs, pulling Lua/PNG/JSON into protocol consumers. Client/server content setup
was repeated with important ordering requirements expressed only in call sites.
Content metadata also included texture implementation declarations for one integer
type. The client local/remote loops still combine session and application state.

We split the actual engine responsibilities into `vc_voxel`, `vc_generation`,
`vc_streaming`, `vc_lighting`, `vc_meshing`, and `vc_physics`, keeping existing
implementations. `vc_core` owns UTF-8 and presentation validation (`vc_text` remains
a source-compatible CMake alias). `TextureTile` lives in `resource_types.hpp`; core
and world headers no longer include the texture API. `vc_render_types` supplies
CPU interchange headers; `vc_render_data` implements texture/overlay/frustum work.
These CPU types do not imply Vulkan ownership. The Vulkan loader and legacy font
implementation are private renderer dependencies.

See [the target graph](module-dependency-graph.md) for direct links and checks.
Platform mechanics remain in the existing files/TCP/console/signal/window targets;
combining them would make the headless server inherit desktop dependencies.
Lua remains private to `vc_modding`, not a dependency of engine mechanisms or packet
encoding. EnTT remains private to actor simulation. Static blocks remain 32³ arrays
of 16-bit IDs. No per-cell dispatch or additional per-cell allocation was added.

## Concrete service contracts

| Contract | Operations and authority | Lifetime / thread |
| --- | --- | --- |
| `ContentServices` (`game/content/services.hpp`, `vc_game_content`) | Owns built-in blocks, `ModRuntime`, items, recipes and mining rules; `rebuildGameplay()` publishes a complete rebuilt set after palette resolution | Noncopyable/nonmovable session owner; main thread; references to registry objects remain stable, entry references/IDs must be reacquired after rebuild |
| `WorldStore` (`vc_storage`) | Exclusive directory lock, bounded/versioned load, atomic revision save; accepts opaque game payload | Unique owner; must outlive `SnapshotWriter`; no simultaneous caller load/save while writer is busy |
| `SnapshotWriter` (`vc_storage`) | One owned world/registry snapshot, request/poll/wait/error result | Owns joining worker; accepted save finishes during destruction; caller must observe failures explicitly |
| `GameSettings`, `loadSettings`, `saveSettings` (`vc_settings`) | Validated values, backup recovery, atomic settings persistence | CPU service; UI currently holds the active settings values; no second authoritative settings store was added |
| `ModRuntime` (`vc_modding`) | Pack validation, registration, restore/save, bounded Lua events and diagnostics | Owned inside content; callbacks run with explicit short-lived `ModContext`; no native loader or unloading |
| `server::Host` / `netgame::RemoteSession` | Authority and local transport pump versus remote mirror/intent submission | CPU owners; host/session must die before referenced content; remote clients do not acquire world authority |
| `VulkanRenderer` | Explicit upload/update/draw/resize/shutdown/report; consumes owning CPU values | Main thread, borrows live Window; after shutdown only report/destruction are valid |

`ContentServices` is used by **all three** compositions: local client, remote client
and dedicated server. Register packs before `WorldStore::load` or handshake palette
mapping. Compute the network fingerprint before adding missing-content fallbacks.
Restore mod state, rebuild gameplay content, then restore hotbars/inventories/actors
and start gameplay. Remote handshake may append missing block definitions, hence
its second rebuild. Registry object addresses stay stable; rebuilding is a loading
operation, not live content reload. No worker or UI may read during rebuilding.
Resource resolution remains the existing bounded pack loader and platform asset
paths; a general resource-provider abstraction is deferred until there is a real
consumer. Diagnostics remain existing log/error/value reporting.

## Ownership and shutdown

| Owner | Owns / borrows | Required destruction order |
| --- | --- | --- |
| Local client `run()` | Window; content; renderer; World, store, player, inventory, simulation; streamer, mesh queue, snapshot writer, UI | UI/callback closures before referenced gameplay; snapshot writer before store; streamer before world; renderer before Window; content after consumers |
| Remote `runRemote()` | Window, content, renderer, `RemoteSession`, UI and replicated view/simulation data | Disconnect, release UI/meshing/mirror before content; renderer before Window |
| `Host::Impl` | Content, authority world/player/actors, store/writer, generator/streamer, listener/connections | Connections/listener first; streamer before world; writer before store; content last among game services |
| `BasicTerrainWorker<G>` | Generator value, bounded request/result queues, `jthread` | Last member is thread: stop-aware wait wakes and thread joins before queues/generator destruction |
| `MeshWorker` | Registry/atlas copies and immutable neighborhood/light jobs | Thread joins before queues/content; no borrowed World reaches the worker |
| `SnapshotWriter` | Snapshot values; borrows WorldStore | Joins/completes accepted work before store lock destruction |
| Vulkan implementation | Instance/device/surface/swapchain/fence/buffers/textures | Completion/waits precede resource release; existing single-frame-fence behavior retained |

The scope destruction order and existing explicit shutdown paths already satisfy
these dependencies. Content ownership is now consolidated; application/session
ownership has deliberately not been rewritten in this milestone. Content does not
retain UI, renderer or World pointers. `unique_ptr`, stack values and references
are sufficient; no shared ownership or global service locator was introduced.

Future `ClientApplication` will own the persistent Window/renderer/settings/UI.
Future `GameSession` will own session content/world/network/simulation/jobs. Before
destroying a session, detach UI callbacks, drain/observe saves, join workers, and
remove renderer resources referencing session content. There is no VoxelUI runtime
in the current executable. See [the concrete handoff](next-milestone-client-sessions.md).

## Compatibility and trust

World/region format 1, terrain versions 1–3, player payload 10 with prior readers,
protocol 4, mod API 1, namespaced identifiers and input/UI behavior are unchanged.
Content fingerprint encoding was moved byte-for-byte to `content_fingerprint.cpp`;
its declaration remains reachable from the protocol header without including the
mod runtime. Existing consumers constructing mods must include its header and link
the appropriate content target explicitly.

External mod data/PNG/Lua, saved palettes and network intent retain their current
validation, size/allocator/instruction limits, rollback and server authority. This
refactor does not add parsers or weaken budgets. Native executable mods and safe
hot unloading are not implemented. The compile-time generator runs trusted C++
code and is not a security boundary.

## Deliberate limitations

Public include roots still expose repository paths rather than an installed SDK;
the dependency guards enforce target links, not C++ visibility or arbitrary
generator-expression/external dependency analysis. Gameplay still links generation
and privately streaming through spawn validation. Protocol still shares command
and simulation value APIs. These real dependencies are documented rather than
hidden behind empty interfaces. Menus still start with a world, and local/remote
loops still need extraction in Milestone 2.

Verification and preservation evidence: [progress record](modularization-progress.md).
