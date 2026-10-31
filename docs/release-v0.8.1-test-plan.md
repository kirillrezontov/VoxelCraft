# v0.8.1 release acceptance matrix

Record actual platform, compiler, driver, command, date and result. Pending rows
are release acceptance gaps, not passes inferred from compilation.

| Scenario | Expected result | Actual outcome |
| --- | --- | --- |
| Linux client + Linux server | Join, chunks, synchronized edits and private inventories | Passed Linux loopback: CPU shared edits/persistence plus all three Vulkan network scenarios (2026-10-29) |
| Windows client + Linux server | Protocol 3/mod fingerprint matches; same edits and avatars | Pending Windows host |
| Windows client + Windows server | MSVC archive extraction, two peers, saved profiles | Pending Windows host |
| Single-player Linux | Movement/collision/mining/crafting/combat/respawn | Passed scripted hardware Vulkan survival/creatures/environment/UI (2026-10-29); human play pending |
| Single-player Windows | Same behavior and assets from arbitrary working directory | Pending Windows host |
| Two-player physical LAN | Both avatars/edits, normal play for 30 minutes | Pending human LAN session |
| Save/reload and server restart | Exact blocks, pose, health/items, mod state restored | Passed Storage/PlayerSave/ServerHost and real Server.Process; packaged server restart (2026-10-29) |
| Abrupt client disconnect | Peer removed, profile retained, reconnect succeeds | Passed TCP/Host regression tests and bounded eight-client reconnect/save/reopen soak (2026-10-29) |
| Interrupted save/corrupt region | Staging ignored; explicit corruption; prior revision recovery | Passed corruption, interrupted staging, failed-save retry and explicit recovery regression tests (2026-10-29) |
| Window resize/minimize/restore | Correct extent, paused minimized drawing, clean shutdown | Passed Client.GpuSmoke and Client.SdlPlaytest on Radeon with zero Vulkan diagnostics (2026-10-29) |
| Fullscreen and alt-tab | Enter fullscreen, desktop switching, restore/exit without validation errors | Passed 30-frame fullscreen/no-VSync startup with zero validation diagnostics; manual alt-tab/restore transitions pending |
| Render distances 1, 3, 6; negative seams | Near-first bounded streaming, eviction and edited revisit | Passed CPU region/negative-coordinate/eviction tests and Vulkan streaming; manual radius 6 pending |
| Invalid JSON/Lua/dependency/texture | Clear diagnostic, no partial content/world corruption | Passed CPU Modding/ContentAtlas/PngTiles tests and demo-mod Vulkan crafting/network scenes |
| Malformed/truncated/flooded packets | Peer rejected, host stays available, bounded memory | Passed rate/flood/framing tests and 200 fragmented malformed sessions with valid join afterwards |
| Identity reuse after disconnect/restart | Missing/wrong token rejected; owner's token restores exact profile | Passed missing/wrong token, cross-name token rejection, case-insensitive owner restoration and restart |
| Release archives | Extracted server/client run outside source tree; licenses/shaders/libs included | Passed both extracted Linux archives; packaged client strict Vulkan smoke; server Unicode-path save/restart; Windows pending |
| Windows CI | MSVC client/server compile, CPU tests, separate archive smoke tests | Pending remote CI execution |
| Linux CI | GCC/Clang build, CPU tests, package smoke tests | Pending remote CI execution |

Useful commands: `ctest --preset debug`, `ctest --preset graphics`,
`ctest --preset asan-ubsan`, `ctest --preset tsan`,
`build/release/bin/vc_network_soak 1800`, `build/release/bin/vc_mesh_benchmark`.
Graphics tests require a live display and Khronos validation. Longer soak duration
accepts 10..14400 seconds. Tests use temporary directories and ephemeral ports.
Use a backed-up copy of existing worlds for manual compatibility checks.
