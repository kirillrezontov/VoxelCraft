# Milestone 2 handoff: ClientApplication and GameSession

Status: historical Milestone 1 handoff. The follow-up is now implemented; see
[ClientApplication/GameSession](client-sessions.md) and its verification/limitations.
The original plan below is preserved for comparison. Begin only after
the verification gate in [the progress record](modularization-progress.md) passes.
Use the installed integration and architecture skills; preserve the legacy UI.

## Exact interfaces available now

* `client::Options` / `parseOptions` in `client/options.hpp`: CLI seed/bounds,
  local/remote selection, rendering startup options, save/mod/settings/identity paths.
* `client::Preferences loadPreferences(const Options&)` in `client/preferences.hpp`:
  active `GameSettings` and optional persistence path. Uses Window's user-data path.
* `ContentServices` in `game/content/services.hpp` (`vc_game_content`): nonmovable
  owner; `blocks()`, `mods()`, `items()`, `recipes()`, `mining()` const/mutable access;
  `rebuildGameplay()` after saved/network palette extension and before consumers.
* `WorldStore::load(BlockRegistry&) -> optional<WorldArchive>` / `save(...) -> revision`,
  `SnapshotWriter::request(World, BlockRegistry, WorldMetadata, string) -> bool`,
  `busy/takeReady/wait`, and `SnapshotResult::rethrowIfFailed()` in `vc_storage`.
  Save jobs own snapshots, not live session pointers; the store outlives the writer.
* `TerrainGenerator`, `ChunkStreamer`, `Lighting`, `BackgroundMeshQueue` in the
  generation/streaming/lighting/meshing targets. Streamer borrows World; mesh workers
  own immutable copies; both join on destruction. The alternative generator seam
  covers generation jobs only; the game still uses its versioned concrete generator.
* `netgame::RemoteSession` in `game/network/client.hpp` (`vc_remote_session`):
  constructor borrows mutable block registry; `pump`, `open`, `welcome`, `state`,
  `world`, `camera`, dirty chunks/notices/replies, intent/inventory/hotbar/command/save,
  and `disconnect`. The server owns gameplay; the client mirrors authoritative data.
* `server::Host` / `HostConfig` in `server/host.hpp`: existing headless authority,
  `tick/save/shutdown/console/ready` and state/port/fingerprint access. Do not embed
  a second authority in a remote session or change protocol 4 as part of UI extraction.
* `VulkanRenderer` in `engine/renderer/vulkan_renderer.hpp`: Window-borrowing owner;
  upload/update meshes, HUD/models/instances, draw/resize/shutdown/report. It has no
  GameSession or VoxelUI contract. `Window::poll()` returns aggregated `Input`.

The codecs no longer link the mod runtime. Content fingerprinting is implemented
by `vc_game_content`, with declaration in `game/network/content_fingerprint.hpp`.
Compute it before palette fallback registration. `vc_client_runtime` currently
exposes option/preferences/remote-loop helpers and privately links their concrete
dependencies. `voxelcraft` explicitly links local composition dependencies.

## Proposed ownership

Create one `ClientApplication` that owns Window, renderer, preferences/settings
persistence, application event loop, screen navigation and an optional uniquely
owned GameSession. Construct Window before renderer and destroy it last. It must
render a menu without constructing content for a world, loading a save or connecting.

The session boundary owns content, authoritative local or mirrored remote state,
player/inventory/day/actors, world/metadata, generation/streaming/meshing work and,
for local play, the store/snapshot writer. Expose concrete coarse operations for
tick, state queries, actions, save/disconnect and shutdown. A small tagged owner or
variant can select local/remote implementations; avoid an abstract interface for
each gameplay type and avoid dispatch per voxel. Share existing inventory, commands,
simulation and mesh algorithms; do not duplicate them in two new loops.

The renderer persists across sessions. Session geometry/atlas/models must be removed
or replaced before session resources disappear. The current renderer owns uploaded
bytes; its single-frame fence/wait behavior is the existing retirement mechanism.
Do not call `shutdown()` merely to return to a menu: introduce a tested clear/update
path using its existing methods, preserving renderer validation through final shutdown.
The atlas is session-specific (mods), so menu assets must have independent ownership.

## Concrete code to migrate

1. Extract the body of anonymous `run(const Options&)` in `client/main.cpp` into a
   local session owner, retaining its exact startup order. Its save/awaitSnapshot,
   mod dispatch, commandContext and notification lambdas currently capture stack
   locals. Replace long-lived captures with session methods and short-lived trusted
   action contexts. Keep the CLI smoke path behavior and final save/discard policy.
2. Split `client::runRemote` in `client/network_client.cpp`: Window/renderer/settings
   move to the application; `RemoteSession` handshake, authoritative inventory
   updates, pose revisions, interpolation and intent sending stay in the session.
   Handshake palette mapping precedes `content.rebuildGameplay()`. Remote UI mutations
   remain real inventory/command intents, never local authority.
3. Keep `Host::Impl` in `server/host.cpp` independent. Its `ContentServices` and
   worker lifetime order provide a concrete reference, not a reason to route local
   play through networking. Share gameplay helpers where extraction proves useful.
4. Move repeated settings notifications/persistence and validation reporting to the
   application. Keep current backup/error behavior and command-line overrides.

## ScreenManager and input seam

Add a small application ScreenManager for menu/loading/gameplay/error transitions;
it may initially adapt `SandboxUiController`. Current controller construction
requires an inventory, so do not create a fake authoritative inventory to display
the application menu. Keep world HUD/inventory/crafting/console with live session
bindings; application menu/pause navigation can be a separate small adapter.

Before VoxelUI integration, Window needs an ordered raw SDL event seam **inside**
polling. Polling SDL a second time loses the events already consumed by `poll()`.
Preserve current aggregated gameplay input while introducing optional host routing.
Future VoxelUI pointer events need logical coordinates; current Input cursor values
are framebuffer pixels. DPI scaling belongs to a single defined conversion path.
Keep focus/text/IME/clipboard/capture ordering. Clear held mining, movement and mouse
actions when opening a screen, losing focus, failing connection or ending a session.
Solo pause/settings freeze simulation; multiplayer menus block input while pumping
network state. Screen callbacks must detach before session destruction.

## Loading, cancellation and shutdown

Use explicit idle/loading/active/stopping/error states. Build a new session privately
and publish it only after validation; failed/cancelled startup releases its store
lock and joins accepted jobs. Initial terrain and remote handshakes currently run
their own polling/present loops; migrate them into incremental application ticks so
cancel/resize/quit remain responsive. Do not add blocking waits to every frame.

On leave: block new actions, remove session UI bindings, request/observe local save
or send remote disconnect, finish accepted saves and report errors, join mesh/terrain
workers, clear renderer session data, release session/content and show main menu.
Do not silently discard a save because teardown is inconvenient. The explicit
Ctrl+Shift+Q discard path stays explicit. Constructor failure and shutdown during
generation, pending mesh completion or snapshot writing need the same ordering.

## Required verification and risks

Keep the current full CPU/server, sanitizer and 12-test graphics suites. Add repeated
menu → new/load local world → save → menu cycles in one process; world-lock release
and reload of existing formats; remote connect/disconnect/reconnect and failed auth;
cancel generation/handshake; save failure with retry; pending jobs at teardown;
switch different mod atlases/content; resize/minimize/restore from menu and gameplay;
settings changes surviving transitions; console/inventory authority and input capture.
Run Vulkan synchronization validation through final application shutdown. Check CPU
memory/worker counts and renderer mesh/model counts after repeated cycles.

Risks: captured UI/command pointers into destroyed locals, stale numeric IDs after
content rebuild, renderer content persisting across sessions, silent asynchronous
save failure, remote mirror accidentally becoming authoritative, duplicated simulation,
and frozen networking while menus are open. The target graph does not need another
foundation rewrite before this work: add an application composition target if it
reduces executable logic, then extract one local lifecycle slice before remote.
Native SDK, VoxelUI backend/design and legacy UI removal remain later milestones.
