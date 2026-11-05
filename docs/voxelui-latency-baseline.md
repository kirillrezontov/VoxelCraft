> Historical checkpoint. See [native v1 implementation and current validation](voxelui-native-v1.md) for the November 6 continuation.

# VoxelUI latency baseline and upload changes

These are new measurements from November 5, 2026. The native renderer milestone is unfinished. The UI still rasterizes a full RGBA surface on the CPU; this is the dominant remaining cost.

The complete distributions, per-phase call counts and environment are in [the JSON report](voxelui-latency-baseline.json). Compressed raw JSONL samples and SHA-256 values are referenced there. [Environment/method metadata](voxelui-latency-environment.json) is an explicit input to the summarizer.

## Whole-frame measurements

GCC 16.2.1 Release (`-O3 -DNDEBUG`, C++23), Ryzen 7 2700, 16 GiB RAM, RTX 3050 / NVIDIA 615.78.08, SDL 3.4.18, Vulkan 1.3 application, Wayland/Hyprland. The compositor supplied **947 × 1064** pixels for the requested 960 × 700 window. Each row has 20 warmup frames followed by 240 measured frames. Performance runs disabled validation and ran serially after builds and other GPU probes finished.

Before: VoxelCraft `53a7e8a`. After: application code `deee8e8`, tested from committed source archives. Both used VoxelUI code `a18bdab`. The later probe/helper/documentation commits do not change the measured application path.

Milliseconds; each group is median / p95 / p99 / worst. These are poll + application-call wall times, not input-to-photon latency or isolated GPU execution time.

### VSync OFF

| Scenario | Before (ms) | After (ms) |
|---|---:|---:|
| idle_main_menu | 0.113 / 0.205 / 0.490 / 0.784 | 0.114 / 0.206 / 0.509 / 0.822 |
| pointer_movement | 0.113 / 0.126 / 0.346 / 0.468 | 0.122 / 0.147 / 0.444 / 0.723 |
| button_hover | 89.588 / 93.500 / 94.815 / 96.438 | 81.460 / 86.161 / 87.582 / 89.033 |
| pointer_click_text_focus | 0.144 / 3.983 / 5.840 / 6.737 | 0.784 / 4.280 / 4.958 / 7.028 |
| text_editing | 89.891 / 94.074 / 94.996 / 95.670 | 81.527 / 86.407 / 87.565 / 87.770 |
| rapid_typing | 90.378 / 94.334 / 95.727 / 96.993 | 81.806 / 86.051 / 87.251 / 87.550 |
| screen_replacement_reload | 91.285 / 95.262 / 96.967 / 97.372 | 81.791 / 86.291 / 87.310 / 88.250 |
| ui_over_active_world | 0.320 / 4.521 / 6.242 / 7.042 | 0.320 / 2.172 / 4.852 / 9.230 |
| pause_resume | 30.314 / 47.155 / 53.362 / 54.569 | 23.958 / 35.128 / 41.377 / 43.966 |
| resize | 0.308 / 4.407 / 6.341 / 6.955 | 0.308 / 4.495 / 5.912 / 6.471 |
| health_updates_host_fixture | 8.909 / 12.090 / 13.031 / 13.320 | 6.138 / 6.345 / 6.538 / 6.585 |

### VSync ON

| Scenario | Before (ms) | After (ms) |
|---|---:|---:|
| idle_main_menu | 13.323 / 13.337 / 13.363 / 13.627 | 13.322 / 13.361 / 13.380 / 13.654 |
| pointer_movement | 13.322 / 13.356 / 13.391 / 13.420 | 13.323 / 13.338 / 13.353 / 13.360 |
| button_hover | 89.317 / 92.526 / 93.644 / 94.343 | 83.734 / 87.730 / 88.829 / 89.098 |
| pointer_click_text_focus | 13.314 / 13.370 / 13.408 / 14.398 | 13.323 / 13.341 / 13.373 / 13.470 |
| text_editing | 89.385 / 93.071 / 95.625 / 96.131 | 83.465 / 88.451 / 89.062 / 89.197 |
| rapid_typing | 89.253 / 92.923 / 93.684 / 98.063 | 83.595 / 88.056 / 89.067 / 89.651 |
| screen_replacement_reload | 90.530 / 94.212 / 95.918 / 97.691 | 83.745 / 87.594 / 88.335 / 88.904 |
| ui_over_active_world | 13.323 / 13.338 / 13.343 / 13.374 | 13.323 / 13.334 / 13.348 / 13.380 |
| pause_resume | 32.336 / 47.102 / 48.114 / 50.162 | 28.226 / 43.389 / 46.419 / 48.102 |
| resize | 13.323 / 13.342 / 13.382 / 13.433 | 13.323 / 13.335 / 13.347 / 13.375 |
| health_updates_host_fixture | 13.278 / 16.560 / 19.718 / 20.151 | 13.318 / 13.489 / 15.182 / 15.671 |

## Findings

- VSync-off hover median improves from 89.588 to 81.460 ms (about 9%); text editing from 89.891 to 81.527 ms. Pause/resume improves from 30.314 to 23.958 ms. The deterministic health-host fixture improves from 8.909 to 6.138 ms.
- Every changed hover/text/rapid-typing/reload frame previously called queue idle. The corresponding after rows contain **zero queue-idle calls**, as do all other measured after rows. Warm hover/text/rapid-typing/reload frames also contain zero Vulkan buffer/image allocation time. Pause still rebuilds some legacy HUD geometry.
- Unchanged menu frames emit/compare one DrawList rather than two. The runtime preparation scope runs once rather than twice; zero-delta style/animation flush calls fall from six to two. Ordered edits are validated separately by host/input regression tests.
- CPU rasterization still costs roughly 77 ms in changed full-menu frames. Allocation and synchronization improvements do not meet a 60 FPS changed-menu target. The native GPU backend remains the next major deliverable.
- Idle is essentially unchanged (0.113 vs 0.114 ms with VSync off). Some pointer/focus microcases and GPU timestamp intervals regress. No uniform speedup or statistical-significance claim is made.
- VSync-on idle remains about 13.32 ms; acquisition accounts for about 13.1 ms of that example frame. It must not be attributed to bindings/layout. Presentation, acquisition, and fence waits are measured separately.

## Instrumentation contract

`VC_UI_PROFILE=/absolute/file.jsonl` enables buffered per-frame collection in normal play. Without it (or explicit test enablement), scopes do not read clocks. Internal runtime phases require this branch of VoxelUI with `profiling.hpp`; older installed 0.2.1 packages still render but lack those internal observations.

| Field | What it measures |
|---|---|
| poll | SDL polling, snapshot creation, clipboard bridge and framebuffer coordinate conversion |
| dispatch / events | Host UI dispatch / core routed event handling, including repeated ordered events |
| runtime | Host screen update, including load/reload if necessary |
| bindings / rebuild / style / layout / animation | Core phase scopes; load/full rebuild includes work not fully subdivided into every phase |
| draw_list | Owning commands and the runtime zero-delta flush |
| comparison | Snapshot comparison and invalidation in setUi |
| software | Full-surface reference rasterization |
| allocation / upload | Vulkan buffer/image allocation / mapped CPU copies |
| synchronization | Frame/upload/readback fence waits and exceptional device/queue waits |
| acquire / present | CPU calls into swapchain acquisition / queue presentation |
| recording / submit | Frame command recording / CPU queue submission |
| gpu_ms | Previous submission’s timestamp interval adjacent to UI drawing, or null when unavailable |
| whole_frame | Poll duration plus application frame-call duration |

Scopes are inclusive and may nest; do not add all phase durations. DrawList generation can flush binding/style/layout changes. Initial synchronous texture-upload command recording in the before build was outside the frame-recording scope; whole-frame timing includes it.

GPU samples use two BOTTOM_OF_PIPE timestamps around the UI commands and are read only after the existing frame fence, without a query wait. They can reflect pipeline/queue overlap and clock state, and must not be interpreted as isolated UI shader cost. See the [Khronos timestamp sample](https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html) for those limitations. A separate profiling-enabled actual application run passed API and synchronization validation with zero errors/warnings.

## Workloads and limits

The fixture uses actual ClientApplication menus and a temporary local survival session (seed 42, radius 1, generator 1). SDL events are injected before polling; physical keyboard/mouse input is filtered out of the benchmark window. Pointer targets come from the same XML/CSS layout evaluated at the actual framebuffer size. Startup/generation precede the warmed active-world row. Screen replacement reloads the actual application resource provider.

`pointer_click_text_focus` clicks the text field; pause/resume activates real pause controls with Enter. There is no separate measured pointer-button-action row. Health changes use the real host adapter/renderer with deterministic snapshots, outside gameplay damage simulation, and are labeled `host_fixture`. They use a 960 × 700 logical surface in the existing window.

Resize requests did not change the tiled viewport on this compositor, so that row measures requests rather than successful swapchain resizes. Minimize/restore graphics checks remain failed here. Actual resizing at other viewports is covered by CPU geometry/pixel tests, not claimed as a complete measured GPU resize result. Runtime animation delta still defaults to 1/60 of a logical second; wall-clock animation pacing remains follow-up work.

Only one before/after run per mode was collected. CPU/GPU clocks and ordinary desktop activity were not pinned. Quantiles use median and nearest-rank p95/p99; 240 samples provide limited tail precision. The historical 32.70/48.76 ms fixture means and 2,457,600-byte upload are not these measurements and were not used as the before column.

## Reproduce

Build Release with VoxelUI and GPU test targets enabled using [the build commands](voxelui-linux-v1.md). Run modes sequentially, with other builds/graphics probes stopped. From VoxelCraft/:

```sh
python3 tests/run_voxelui_latency.py build/ui-release/bin/vc_voxelui_latency build/latency-after --frames 240
# For a before/after comparison, build 53a7e8a in a separate tracked-source archive,
# then use its binary with the same runner and separate output directory.
python3 tests/summarize_voxelui_latency.py docs/voxelui-latency-baseline.json \
  --before build/latency-before --after build/latency-after \
  --metadata docs/voxelui-latency-environment.json
# Update the explicit environment metadata when changing host/build settings.
VC_UI_PROFILE=/tmp/voxelcraft-ui.jsonl build/ui-release/bin/voxelcraft --no-mods --no-settings
```

## Actual captures

PNG files are lossless conversions of Vulkan readback PPMs, without visual edits. [Main menu](voxelui-linux-captures/main-menu.png) and [pause](voxelui-linux-captures/pause.png) come from the application workload. Its capture helper uses a test camera, hence the sky behind pause. [World + HUD](voxelui-linux-captures/world-hud.png) uses the actual local-session camera from a separate bounded `--screenshot` run with validation enabled. The before/after main-menu readbacks compare with zero differing pixels (`magick compare -metric AE`). These are bridge screenshots, not native-backend conformance.

```sh
build/ui-release/bin/voxelcraft --frames 20 --radius 1 --seed 42 \
  --generator-version 1 --survival --no-settings --no-mods --no-save \
  --no-vsync --require-validation --screenshot /tmp/actual-world-hud.ppm
```
