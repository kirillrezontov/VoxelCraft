# Five-hour roadmap implementation — 2026-10-29

User-authorized window: **2026-10-28 20:19:15 UTC → 2026-10-29 01:19:15 UTC**
(01:19:15 → 06:19:15 Asia/Tashkent). Stop adding features at **00:49:15 UTC**
and reserve the final 30 minutes for checks, documentation and handoff.

The session started with the previous continuation's uncommitted v0.7 changes.
They are preserved; a complete starting-source archive is under
`build/timed-20261029/starting-source.tar.gz`. No system packages were installed.
This record is incremental; the final stopping point is not yet reached.

## Mod state migrations

- Added optional manifest `state_version` (default 1) and player payload v7.
  Payload v6 mod records migrate to schema 1; v1..v6 remain readable.
- `on_migrate` transforms a pack's string state before load hooks. It receives
  old/new pack versions and state versions, and permits only state APIs.
- Downgrades, absent migrations, Lua failures and resource-limit violations
  reject startup before a new snapshot is saved. Migration uses fresh Lua VMs
  and commits all pack state/VMs/diagnostics together, preserving the old session
  if any pack fails. Missing pack state retains its schema metadata.
- GCC Debug checkpoint: **120/120 CPU/CLI tests passed**, including legacy v6,
  successful/repeated/missing/downgraded/failed migrations and VM rollback.

## Declarative mining rules

- Added mod block `mining` declarations: break time, tool/tier, optional drop and
  quantity, with strict numeric/content validation and transactional installation.
- Survival collects the declared quantity through the existing full-inventory
  transaction, retaining the block/tool when collection cannot fit.
- GCC Debug checkpoint: **122/122 CPU/CLI tests passed**. Coverage includes
  slower bare-hand breaking without harvest, stone tool tier/wear, quantities,
  inventory-full protection, no-drop rules and invalid declarations.

## External PNG textures and shared atlas

- Added hash-pinned PNG-only stb_image decoding (16×16 tiles, 128 KiB input,
  1 MiB decoder budget), portable contained asset paths and UTF-8 support.
- Added ContentAtlas: fixed built-in IDs, namespaced appended textures, row
  packing, regenerated gutters and one layout shared by terrain/HUD/Vulkan.
- The demo now includes a cyan PNG tile and pickaxe harvest rule. Added CPU
  coverage for exact built-in preservation, repacking, gutters, atlas capacity,
  dependency textures, invalid paths/symlinks, malformed/oversized PNGs and
  excessive inflate data followed by valid decoding.
- GCC Debug checkpoint: **128/128 CPU/CLI tests passed**. Added a separate
  `graphics` CMake preset. GCC and Clang ASan/UBSan/LSan each passed
  **128/128 CPU/CLI tests**; all **8/8 strict Vulkan tests passed** on the
  isolated AMD/Wayland display, with zero validation diagnostics.

## Current work

The executable persistence test verified schema upgrade, once-only migration and
refused downgrade without losing saved state. The survival GPU test verifies the
custom PNG block, crafting/placement/mining/tool wear and Lua state. Captures:
[crafting](voxelcraft-v07-crafting.png), [custom block](voxelcraft-v07-mod-block.png).

The previous Wayland restore limitation was fixed by requesting activation after
SDL restore; the isolated smoke now passes, including minimize/restore and
shutdown. Desktop policy still controls whether activation is granted.

Next: v0.8 portable networking, headless authoritative simulation and connected
play. Networking remains planned until the corresponding code and checks land.
Windows and sustained human playtesting are still unverified.


## v0.8 transport and simulation groundwork

- Added owning nonblocking TCP streams/listeners. WinSock/POSIX details stay in
  `engine/platform/tcp.cpp`; Windows links `ws2_32` and remains unexecuted.
- Added bounded length framing with partial I/O, per-pump byte budgets, queue
  limits/backpressure and explicit truncated/malformed stream errors.
- Added strict value-based gameplay messages, content fingerprints, compact
  palette/RLE chunk transfer with server-to-client runtime-ID mapping, state,
  input, hotbar, notices and controls. No renderer/platform handles enter messages.
- Player payload v8 adds up to 32 case-insensitive named creative profiles with
  camera/flight/hotbar state; old payloads remain readable and local saves retain
  multiplayer profiles. Legacy test fixtures are now independent of the current
  encoder's trailing fields.
- Chunk streaming supports the union of up to eight players' regions. Process-
  local chunk data revisions avoid confusing neighbor mesh invalidation with
  data changes. Retained edits can be sampled for distant profile restoration.
- GCC Debug checkpoint: **144/144 CPU/CLI tests passed**. Multi-player creature
  targeting/loot/attack cooldown support and the console stop scope are being
  completed before the host executable is connected.


## Headless authoritative host and creative connection

- Added a real console host using the shared world, streaming, player collision,
  creatures, day cycle, mods and snapshot storage; native networking remains in
  the platform layer. A bounded 90-tick run generated terrain and shut down
  cleanly. Linux `ldd` confirms no SDL/Vulkan dependency in the server binary.
- Added CPU RemoteSession world/state mirroring for both tests and the coming
  graphical client. The host accepts intent rather than client positions, checks
  command sequence/rate/content/identity, and distributes revisioned chunks and
  player/actor snapshots with bounded transport queues.
- Added per-attacker combat cooldowns and multi-player creature targeting/loot,
  safe spawn selection against other player bodies and cached terrain edits,
  and a console signal scope for save-on-interrupt.
- The initial network mode is creative; opening a survival world is explicitly
  refused. Per-name profiles are local/LAN identities, not authenticated accounts.
- Snapshot game data is now bounded to 256 KiB to hold all 32 worst-case named
  profiles without undermining the previous 1 MiB manifest bound.
- Host integration tests exercise two real loopback sessions, shared edits,
  independent motion, saving/reopening, content/name rejection and input timeout.
  GCC Debug passed **157/157 CPU/CLI tests** after connected-client integration.
- The real client executable connected to the standalone server and rendered
  90 frames with streamed terrain and zero Vulkan diagnostics. A strict graphics
  test now runs two actual TCP sessions against Host, draws another player,
  places/removes the demo's PNG-textured block for both clients, transmits SDL
  movement intent, saves, restarts and restores the independent profile.
  [Players](voxelcraft-v08-players.png), [shared edit](voxelcraft-v08-edit.png).
- This is a development foundation, not a stabilized multiplayer release.
  Next: server-authoritative survival and further transport/play verification.

## Connected survival and executable lifecycle

- Server authority now includes timed mining, harvest/tool wear, item placement,
  transactional recipes/backpack operations, food, fall/creature damage, death
  and inventory-preserving respawn. UI transactions can be sent to Host without
  mutating the client inventory mirror. Each peer receives only its own stacks.
- Payload v9 adds profile health/items and reads v8 creative profiles with empty
  survival inventory/full health. The 512 KiB game payload cap accommodates all
  32 worst-case profiles; region format remains version 1. Local saves preserve
  named profiles and the server preserves the independent offline base player.
- GCC Debug and full Clang ASan/UBSan/LSan each passed **164/164 CPU/CLI tests**.
  A new strict graphics test exercised real SDL held mining, two-log→wooden-tool
  crafting, stone harvest/wear, backpack commands, independent peer state, fatal
  fall, death UI, R respawn and F5 save over TCP; zero Vulkan diagnostics.
- The standalone server plus two standalone graphical clients passed an actual
  process test, including matched mod textures and mismatched-content rejection.
  The real survival client separately rendered 90 frames with zero diagnostics.
- A real headless process test passed bounded save/reopen, failed-startup snapshot
  preservation, exclusive session locking, SIGTERM save and crash-lock release.
  Signal/crash checks run on POSIX; portable bounded lifecycle checks run on Windows.
- Added a server-only CMake preset and optional eight-player real-time soak tool.
  Long-run transport/edit/reconnect verification is the next check.
