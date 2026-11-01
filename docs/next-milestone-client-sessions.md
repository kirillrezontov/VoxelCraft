# Milestone 2 handoff: ClientApplication and GameSession

**Implemented:** see [ClientApplication and GameSession](client-sessions.md).
The pre-extraction handoff below is retained as historical design context.

The previous environment blockers were closed by the 2026-11-01 unrestricted
rerun: **222/222 CPU/CLI, 12/12 graphics, 203/203 ASan/UBSan with leaks enabled,
and both extracted packages passed**. See [the gate recheck](modular-core-gate-recheck.json).
The original record below preserves the earlier run and its limits.

This is the next implementation slice, not an API already present. Begin only
after the outstanding Milestone 1 networking/graphics verification gate in
[progress](modularization-progress.md) is closed on a capable runner. Keep legacy
UI active while extracting sessions; VoxelUI integration is a later milestone.

## Interfaces available now

| Existing interface | Use in the next extraction |
| --- | --- |
| `voxel::WorldPersistence(path)` (`vc_storage`) | Local session owns it. Register content before `load(blocks)`; validate CLI overrides against metadata. `requestSave` takes owned values; poll `takeSaveResult` or check `waitForSave` before leaving. `saving` includes unconsumed results. Destructor joins before releasing the lock. |
| `voxel::World`, `TerrainGenerator`, `ChunkStreamer` (`vc_voxel`, `vc_generation`) | Local state/generation/retained edits. Streamer borrows World and owns its worker; retain restored chunks before streaming. `snapshot` includes retained edits; `sample` checks resident/retained/generated data for spawn validation. |
| `PlayerController`, inventories, `SurvivalState`, `LivingWorld`, `DayCycle` | Authoritative local state. Keep existing transactions/commands. Restore item/recipe/mining registries before inventory/player validation. |
| `ModRuntime(root, blocks)` | Blocks/atlas before palette resolution; items/mining before inventory restoration; saved mod state before dispatch. No native loader exists. |
| `netgame::RemoteSession` (`vc_remote_session`) | Existing handshake/pump, intent/commands, notices/replies, world mirror and server state. Do not duplicate server simulation locally. |
| `client::CoreMesher`, `CoreMeshQueue`, `CoreBackgroundMeshQueue` | Host-selected static policies; local/remote sessions own coordinators over their respective worlds. Lighting stays CPU-side. |
| `VulkanRenderer(Window&, ImageRgba, validation, vsync)` | Application ownership independent of session; mesh/HUD/model upload, draw, `shutdown`, then `validationReport`. No DrawList backend or atlas replacement API exists. |
| `GameSettings`, `loadSettings`, `saveSettings`; `client::loadPreferences(Options)` | Reuse validation, backup recovery and CLI overrides. Move persistent settings outside session; no duplicate authoritative store. |
| `SandboxUiController` / `SurvivalUiContext` | Existing screens/input/notifications/console/crafting adapters. Inventory pointers and remote inventory-action lambda borrow session state. |
| `commands::Registry` / `Context` | Reuse trusted references/mutation sinks. Remote UI sends intent; server state supplies permissions. |

Read the exact headers above. These are source-level C++ APIs, not stable plugin
ABI tables. Gameplay still includes generation for spawn sampling; no additional
module extraction is required before starting sessions.

## Proposed ownership

Construct application members in this order: platform/window, preferences and
content/resources, renderer, application navigation/UI, optional session. Destroy
in reverse, disconnecting UI callbacks before session state dies; renderer drains
before Window dies. Prefer unique ownership. Content must precede renderer
because its constructor requires the mod atlas. The first slice can keep one
installed content/atlas set for the application lifetime, with restart required
for pack changes. Arbitrary per-world atlas replacement does not exist. Decide
how missing saved IDs affect the session registry copy without invalidating that
application content.

`ClientApplication` should poll/draw without a session, using a menu camera and no
terrain meshes. `ScreenManager` should explicitly represent route/overlays/back/
loading/error/quit transitions. Current `UiScreen` has Gameplay, Inventory,
Crafting, Settings, Pause and Console; there is no main menu, world selection or
navigation stack. Add the minimum transitional navigation proving enter/leave,
without starting the complete UI rewrite.

Start with a concrete local session owner. Candidate declaration order:
registry/content references, World/metadata/persistence, generator/player and
gameplay state, streamer/lighting/mesh coordinator, session UI adapters. Preserve
each borrower's order relative to its owner. `WorldPersistence` already makes
writer/store lifetime inseparable; shutdown must still observe errors rather
than relying on its nonthrowing destructor.

Represent local/remote alternatives at a coarse host boundary (for example a
variant) after identifying shared operations: update/input, UI snapshots/CPU
render output, leave request and shutdown progress. Do not introduce voxel-level
virtual dispatch or force mirrors into local authoritative simulation types.
No universal service registry is necessary.

## Exact code to migrate

`client/main.cpp::run` currently combines:

1. Window/preferences/content/renderer construction and `WorldPersistence` opening.
2. Metadata/palette load and player decode; bounds/mode validation and mod restore;
   initial terrain generation while polling/presenting.
3. Streamer/lighting/mesh coordinator creation and initial geometry preparation.
4. Player/inventory/actors/day restoration, mining installation and legacy UI.
5. Stack-capturing `dispatchMod`, `finishSnapshot`, `awaitSnapshot`, `save` and
   `commandContext` lambdas; input/simulation/render/autosave loop.
6. Ordinary-exit save (with explicit quit-without-save preserved), capture release
   and renderer shutdown/validation.

Move application owners out of phase 1; move phases 2–5 into local loading/update/
exit phases. Replace `awaitSnapshot`'s nested poll/draw loop with application-owned
shutdown progress. Retain the session on save failure for retry or explicit
discard. Initial generation already polls quit/resize: turn it into cancellable
progress while preserving save completion and worker joining. No detached threads.

`client/network_client.cpp::runRemote` separately constructs Window, preferences,
mods/atlas/renderer, then RemoteSession. Its handshake pumps while drawing. Its
gameplay loop reads server snapshots, sends actions/hotbar/inventory/command
intent, updates mirrors/lighting/meshes and draws actors/UI. Move transport/mirrors
and per-connection presentation into a remote session; reuse application owners.
Disconnect that session, clear its GPU meshes/HUD/instances and input state, then
return to menu. `RemoteSession` is a transport owner, not the proposed general
GameSession. Keep `Host::Impl` authoritative; do not recreate it inside the remote
client.

## Navigation, errors and shutdown

Reset held mining/actions, capture and text focus on menu/focus/session changes.
Preserve solo Pause/Settings simulation freeze and remote menus' input blocking.
Prevent a menu-closing click from reaching gameplay. Window polling aggregates
SDL events; the later VoxelUI adapter needs an ordered sink before aggregation,
not a second `SDL_PollEvent` drain.

Use host generation/session tokens for async completions. Invalidate callbacks
and pending navigation before referenced inventory/mod/world state dies. Failed
loading destroys partial state, releases lock/workers and leaves menu usable.
Local leave flushes/checks snapshots; remote leave disconnects and never saves
its mirror as an authoritative world. Cancel/join CPU work and drain/remove GPU
references before destroying adapters or their borrowed Window state.

## Required next tests and risks

- Start without a world; enter/leave/repeat local sessions in one process. Verify
  locks release, accepted saves finish and edits reload.
- Cancel/fail load; inject save failure and verify retry/explicit discard; shut
  down during generation, meshing and save.
- Connect/refusal/handshake mismatch, disconnect/reconnect/repeated sessions;
  private inventories, roles, notices and authority remain correct.
- Menu click suppression, held mining/focus loss, cursor/text/capture, persisted
  settings, Unicode console, solo pause versus remote menus.
- Renderer survives session changes: clear old meshes/HUD/models/instances;
  resize/minimize/restore during progress; validate final GPU shutdown.
- Existing CPU/CLI, sanitizer and graphics suites plus clean committed client/
  server builds and tracking audits.

Main risks are stack-capturing lambdas, renderer/atlas lifetime, previous-session
GPU state, failure paths destroying unsaved state and duplicated ownership in the
two loops. Retained edits/snapshot caps remain existing limits. No native loader,
VoxelUI dependency, persistence format change or renderer rewrite is necessary.
