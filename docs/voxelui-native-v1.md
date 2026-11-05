# Linux native VoxelUI v1 — November 6, 2026

The existing menu, status/error screens, gameplay HUD and pause UI now render
DrawLists directly through the host's Vulkan device. `VC_VOXELUI_NATIVE` defaults
ON when VoxelUI integration is enabled. The software bridge remains explicitly
selectable for reference comparisons; native rendering does not create or upload
a full-screen software UI surface.

Starting heads for this continuation were VoxelCraft `3018d99` on
`perf/linux-voxelui-v1` and VoxelUI `b006f61` on `feat/native-vulkan-geometry`.
Earlier initial branches, build-discovery root cause and package repair are in
[the original report](voxelui-linux-v1.md). No resets, user changes overwritten,
Windows work, system package installation or remote pushes occurred. Native source
is VoxelUI `6fcd602`; `a1cb8d3` adds installed native Debug/Release CI. Both game
workflows now pin `a1cb8d394efcd57a1fd5d289493d541dee74c08c`. Push the VoxelUI commits
before running remote VoxelCraft CI; local commits are not remotely available yet.

## Renderer and lifetime

All eight command variants are implemented: square/rounded rectangles and borders,
images, text, vector geometry, clip and transform stack pushes/pops. Source painter
order is retained in a shared indexed draw; translucent commands are not sorted.
The GPU samples existing glyph masks from a shared atlas, including Cyrillic.
Caret, selection and composition visuals remain ordinary DrawList geometry.

SVG uses cached even-odd trapezoid tessellation, including concave contours, holes
and intersections, plus gradient, stroke, embedded-image and clipping support.
There are no arbitrary-polygon triangle fans. Glyph/image pages and static vector
geometry are reused. The backend is optional/exported as `VoxelUI::vulkan`; core
and software-only packages retain no Vulkan dependency.

The host retains instance/device/queues, submission, swapchain and completion
fences. Per-slot coherent vertex/index/staging buffers grow and are reused.
Atlas pages use immutable CPU snapshots; a changed page uploads in the normal
host command buffer before rendering. Replacement buffers/images, descriptor
updates and cache retirement occur after the slot fence completes. UI uses a
color-only rendering scope after world rendering. No queue/device idle is added
for normal UI interaction; existing swapchain/shutdown synchronization remains.

The native observer counts buffers, images, views, memory, sampler, descriptors,
atlas pages, staging and pipeline/layout objects. A 400-transition menu/pause and
resource-reload workload stabilized: one atlas page, one native pipeline, with
identical midpoint/end live and created counts. **All counted first-party Vulkan
objects returned to zero at shutdown**, including the native backend. API and
synchronization validation emitted no warnings/errors. Compiler caches are bounded
and released with the owning host; none outlives the device.

Bounds and semantics are explicit in
[VoxelUI's backend contract](../../VoxelUI/backends/vulkan/README.md): 2048-square
maximum atlas page/individual image, bounded glyph identities, 32 cached vector
documents and bounded tessellation complexity. Oversized/malformed input produces
a command-indexed diagnostic, not silent omission or full-frame fallback. Rotated
clips preserve the v1 software reference's axis-aligned bounds. Opaque SRGB colors
are compared; translucent SRGB composition is linear-light and can differ from
software byte-space blending. Transparent offscreen RGB is premultiplied. These
are documented limits, not unrestricted browser/SVG conformance.

## Measured responsiveness

Fresh Release before/after runs use the current optimized software bridge and
native renderer, respectively, on the same RTX 3050/Ryzen 2700 system. Each VSync
mode has 20 warmup + 240 samples per scenario: 10,560 retained samples total.
Validation was disabled for timing. The actual viewport was 947×1019 for both.
All builds/other GPU probes finished before the serial performance runs. One
interrupted native VSync-on run was discarded and completely rerun.

Whole-frame milliseconds (median / p95):

| Scenario | VSync off bridge | VSync off native | VSync on bridge | VSync on native |
|---|---:|---:|---:|---:|
| Hover | 76.586 / 78.672 | 0.193 / 0.442 | 73.388 / 75.784 | 13.319 / 13.384 |
| Text editing | 76.481 / 77.838 | 0.202 / 0.215 | 74.420 / 75.924 | 13.319 / 13.385 |
| Rapid typing | 76.328 / 78.049 | 0.214 / 0.425 | 73.848 / 75.146 | 13.318 / 13.410 |
| Pause/resume | 20.621 / 30.872 | 1.790 / 2.422 | 18.054 / 28.194 | 13.319 / 13.419 |

Native VSync-off hover p99/worst: 1.041/1.261 ms; text: 0.955/0.975 ms.
Idle median regressed slightly, 0.109→0.165 ms with VSync off; no uniform improvement
claim. Native warm hover/text/rapid typing had **zero atlas uploads**, and all
native measured scenarios had zero software-render time and queue-idle calls.
VSync-on frames are presentation-paced; 13.32 ms is not CPU UI processing time.
GPU timestamps describe a previous-submission pipeline interval, not isolated
shader execution or input-to-photon latency.

[Full quantiles and per-phase metrics](voxelui-native-results/latency.json),
[hardware/methodology](voxelui-native-results/environment.json), and compressed raw
samples/checksums are committed. These are new runs, not the historical 32.70/
48.76 ms measurements. The health row remains a deterministic host fixture;
click latency measures text focus. Resize rows are requests on a tiled compositor,
not completed-resize timing. Actual resize to 800×600 and 1120×720, minimize,
restore and shutdown were verified separately in an isolated KWin Wayland virtual
compositor. Clocks were unpinned; this is one run per mode, not a significance study.

## Current validation and stale-test repair

| Executed check | Result |
|---|---|
| VoxelUI GCC Release core/geometry/GPU | Pass |
| VoxelUI GCC Debug core/geometry | Pass |
| Independent installed Debug and Release consumers, exact 0.2.1, including native target link | 2/2 each pass |
| Clean committed VoxelUI archive, fresh native build | 144 GPU readbacks pass, zero API/sync validation warnings/errors |
| Current game host/input/discovery tests | 24/24 pass |
| Inventory snapshot and existing transactional inventory cases | 3/3 pass |
| Actual native UI/world/reload graphics test | Pass |
| Actual local/save retry/mod/remote session lifecycle | Pass |
| Real resize/minimize/restore on isolated KWin | Pass with API/sync validation |
| Native lifecycle, 400 transitions/reloads | Stable allocations, zero residual objects, validation clean |
| Native main executable, 30-frame survival run | Pass with validation including shutdown |
| Clang ASan/UBSan core/geometry and host tests | Pass; host 7/7 |
| Clang ASan/UBSan native GPU and 400-transition application | Rendering/lifetime/validation pass, process fails external DBus LSan finding below |
| Current legacy configuration | Client and server build |
| Current dedicated server Release cases | 5/5 pass |
| Current headless Clang TSan server process/malformed stress | 3/3 pass |

The old session test could not reach its gameplay body because compositor minimize
was its first prerequisite. Minimize/restore assertions now remain in a separate
mandatory `Client.Sessions.WindowLifecycle` graphics test. The session test itself
runs local and multiplayer behavior independently. A second stale assumption was
fixed: settings clicks now use the actual framebuffer size, not requested window
dimensions. No lifecycle assertions were deleted to claim a pass. Hyprland's
unsupported/unacknowledged requests remain a platform limitation; the separate
case passes on KWin. No system display configuration was modified.

Focused checks were selected for affected code rather than rerunning every old
suite. Earlier reports remain historical. GitHub Actions and Windows were not run;
sustained human gameplay/manual native IME are not claimed. The new optional target
is tested in Linux Debug/Release package CI; hardware CI uses a maintained runner.
[Retained logs and checksums](voxelui-native-results/logs/index.json) include the
sanitizer failures, not only successful tests.

## Leak investigation outcome

No first-party native UI leak was confirmed. LSan still reports 1,258 bytes/3
DBus allocations in the standalone Vulkan process and 1,270 bytes/3 in the native
SDL/application lifecycle. The context-only control, which never constructs a UI
renderer, has the same standalone report. A debugger breakpoint on DBus registration
identifies NVIDIA `libnvidia-glcore.so.615.78.08` as the caller; symbolized DBus traces
are retained. This is heap ownership, not measured VRAM.

An isolated experiment explicitly calling DBus shutdown after Vulkan teardown did
not release the leaked allocations. That experiment was not added to production.
DBus documents global shutdown as unsafe while other library users remain;
[upstream DBus lifecycle documentation](https://dbus.freedesktop.org/doc/api/html/group__DBusMemory.html)
also cautions against library-owned global shutdown. No blanket suppressions,
leak-check disabling, forced foreign-object unrefs, driver replacement or system
changes were introduced. The NVIDIA/DBus defect is **unresolved**, not falsely
reported as fixed. The actionable upstream reproduction is the native graphics
test's `--context-only` mode with leak detection enabled.

## Inventory, hotbar and crafting

The new `game/ui/inventory_view.hpp` provides owning typed slot/icon/recipe snapshots
and bounded existing `InventoryAction` intent validation. Tests prove snapshots
follow authoritative changes without mutating/duplicating inventory state, preserve
resource revision keys, and reject out-of-range dispatch. Multiplayer still sends
the existing session transactions and accepts authoritative server snapshots.

The host item-atlas cache should key icons by item identifier plus resource revision,
using the current tile/block presentation metadata and completion-safe retirement.
Actual inventory/hotbar/crafting screens still use their existing shared host atlas
and controls. Their declarative migration is **not implemented**; legacy UI is kept.
The feature-parity checklist and typed action design in `voxelui-linux-v1.md` remain
applicable. Migrating those screens, borrowing host atlas regions without repacking,
and sustained manual UI/IME testing are subsequent independently testable work.

## Build/run from code/

```sh
cmake -S VoxelCraft -B VoxelCraft/build/ui-native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI -DVC_ENABLE_GPU_TESTS=ON
cmake --build VoxelCraft/build/ui-native --parallel 3
VoxelCraft/build/ui-native/bin/voxelcraft --no-mods --no-settings

# Explicit software reference, never an automatic native fallback:
cmake -S VoxelCraft -B VoxelCraft/build/ui-reference -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI -DVC_VOXELUI_NATIVE=OFF

# Optional installed backend:
cmake -S VoxelUI -B VoxelUI/out/native-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXELUI_VULKAN=ON -DVOXELUI_PREVIEW_SDL=OFF
cmake --build VoxelUI/out/native-release --parallel 3
cmake --install VoxelUI/out/native-release --prefix "$PWD/VoxelUI/stage-native"
cmake -S VoxelCraft -B VoxelCraft/build/native-installed -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR:STRING= -DCMAKE_PREFIX_PATH="$PWD/VoxelUI/stage-native"
```

Local execution used `VC_USE_SYSTEM_DEPS=ON` and existing pinned dependency source
caches recorded in build logs; no packages were installed. For hardware tests,
configure the validation manifest as in `voxelui-linux-v1.md`, then run:

```sh
ctest --test-dir VoxelUI/out/native-gpu -R vulkan --output-on-failure
ctest --test-dir VoxelCraft/build/ui-native -R '^(Client.VoxelUI|Client.Sessions)$' --output-on-failure
VoxelCraft/build/ui-native/bin/vc_voxelui_lifecycle native 400 --require-validation
# Run on a compositor supporting minimize and requested window sizes:
ctest --test-dir VoxelCraft/build/ui-native -R '^Client.Sessions.WindowLifecycle$' --output-on-failure
# Or isolate KWin using installed tools, without modifying desktop configuration:
python3 VoxelCraft/tests/run_window_lifecycle.py VoxelCraft/build/ui-native
```

Actual captures: [main menu](voxelui-native-results/captures/main-menu.png),
[pause](voxelui-native-results/captures/pause.png),
[native HUD over survival world](voxelui-native-results/captures/world-hud.png).
Menu/pause use the automated fixture; its pause capture uses a test camera against
sky. The world HUD capture uses the actual local-session camera. No generated art
or software-only images are presented as native application screenshots.
