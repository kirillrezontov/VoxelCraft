> Historical checkpoint. See [native v1 implementation and current validation](voxelui-native-v1.md) for the November 6 continuation.

# Native primitive rendering and test freshness

This continuation starts at VoxelCraft `d419794` and VoxelUI `2b8ecd5`.
VoxelUI commit `b006f61` adds an actual optional Vulkan primitive renderer.
**The application still uses the software bridge. The full milestone is not complete.**
No new application latency improvement is claimed; prior measurements remain
historical and the roughly 77 ms changed-menu rasterization bottleneck remains.

## Implemented

- Host-context Vulkan 1.3 dynamic-rendering pipeline and embedded compiled shaders;
  no backend instance/device/swapchain, queue submission or global waits.
- Persistent coherent mapped vertex/index buffers per host completion fence,
  geometric capacity growth, allocation statistics and rejection of unfinished
  frame slots. Replaced buffers retire only at a completed slot. Host must finish
  submissions before destruction; backend does not own host fences.
- Square and rounded rectangles/borders, ordered alpha blending, affine transforms,
  reflected geometry, DPI scale and nested reference-compatible rectangular clips.
  Rounded coverage uses inverse local coordinates in the fragment shader.
- Hardware offscreen reference comparisons with strict API/synchronization
  validation, including buffer growth, stable reuse, empty updates and fence checks.
- VoxelCraft animation uses elapsed steady-clock time instead of fixed 1/60 per
  update. First/reset update advances zero; long resume intervals are capped at
  100 ms. Existing zero-delta DrawList flush behavior is retained.
- Both VoxelCraft CI workflows now use published integration source
  `2b8ecd52b44ca1ce0d8e2fb26091f53277312ef8` instead of original `8e6b3e3`.
  This is deliberate reproducible dependency pinning, not a moving branch. The
  unpublished native commit is not made a remote checkout prerequisite. VoxelUI's
  new compile and manual hardware workflows test their own checked-out revision.

Images, text/glyph atlas, SVG tessellation/cache, sampled-image descriptors,
native application adapter and native resource-reload stress remain unimplemented.
Those commands produce explicit errors; the adapter does not silently drop them.
Rotated clips retain the reference's axis-aligned bounding boxes. Exact rotated
clip geometry and sRGB target conformance remain follow-ups. Tested format is
R8G8B8A8_UNORM over opaque destination alpha. Transparent offscreen composition
produces premultiplied RGB and is not interchangeable with SoftwareRenderer's
straight-alpha image. Native code is an experimental unexported optional target.

## Focused validation actually executed

Local hardware: NVIDIA RTX 3050, GCC 16.2.1, Clang 23.1.4, same Linux system and
local Khronos layer configuration as the previous reports. Release uses -O3.

| Check | Result |
|---|---|
| GCC Release core and geometry suites | Pass |
| Clean committed-source archive, fresh Release GPU build | Pass at `b006f61`, all shaders compiled from tracked source |
| Native GPU reference suite | 72 comparisons pass, zero API/sync validation warnings/errors including teardown |
| Clang Debug ASan/UBSan core and geometry | Pass with leak detection enabled |
| Clang ASan/UBSan GPU suite | Rendering/validation pass; process fails LSan, 1,258 bytes / 3 DBus allocations |
| Clang context-only GPU control | Same 1,258 bytes / 3 DBus allocations without constructing Renderer |
| Current VoxelCraft Release executable | Builds with VoxelUI enabled |
| Current survival application, 20 frames | Pass, zero Vulkan warnings/errors including shutdown; software UI bridge |
| Current VoxelCraft host suite | 7/7 pass, including deterministic animation timing |
| Headless settings preview | Rendered and visually inspected, including Cyrillic |

The sanitizer traces point to `dbus_message_new_empty_header` and message-loader
buffers with external driver frames. Identical context-only findings support an
external initialization/lifetime issue; they do not establish an upstream fix or
measure VRAM. No first-party backend leak site was found in these traces. Nothing
was suppressed, and the GPU sanitizer process is recorded as failed.

The native suite spans three viewport sizes and three scales, eight updates each.
It allows at most two UNORM levels of color rounding and no blanket coverage
exception. New rounded tests initially exposed an optional shader-demote capability
and interpolated boundary error; shaders now avoid that optional capability and
use inverse fragment coordinates. Final validation is clean. The hardware suite
fails on missing hardware/layers instead of claiming a software fallback pass.

[GPU readback](native-vulkan-primitives/gpu-readback.png) is a **native primitive
fixture**, not an application menu. Existing actual UI screenshots remain in the
previous milestone report. A [fresh survival capture](native-vulkan-primitives/game-bridge-current.png)
shows the rebuilt application using its existing bridge, not the new backend. Full raw logs, including LSan failures, accompany this
report in `native-vulkan-primitives/`.

## Which tests still need running

Do not rerun the entire old matrix after every primitive change. Build the target
before CTest; CTest alone does not rebuild stale executables. Distinguish historical
reports from evidence for the current revision.

| Change / gate | Minimum useful checks |
|---|---|
| Geometry/shader/buffer code | Geometry suite + hardware readback with sync validation; ASan when ownership/bounds change |
| Input, clock or host screen routing | Current host suite; relevant input tests only if routing changed |
| Build/discovery/package exports | Discovery + exact-version independent Debug/Release consumers when those files change |
| Native text/images/SVG | Extend the same readback fixture with those commands and cache reuse/reload cases |
| Native adapter becomes selectable | Actual menus/pause/HUD over world, transitions and completion-safe teardown; rerun Release latency VSync on/off |
| Full release gate | Current legacy/headless builds, multiplayer/session regression, relevant TSan, real resize/minimize/restore, full graphics validation |

No unrelated persistence, protocol, server or legacy gameplay changes occurred in
this slice, so those large historical suites were not rerun. Their old results are
not represented as fresh passes. Existing minimize/restore failures remain open;
no assertions or tests were deleted to hide them. Package exports did not change,
so repeating the already-verified installation matrix was unnecessary here.
GitHub workflows have not been executed remotely. Windows remains deferred.

## Reproduction from code/

```sh
cmake -S VoxelUI -B VoxelUI/out/native-gpu -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXELUI_PREVIEW_SDL=OFF -DVOXELUI_VULKAN=ON
cmake --build VoxelUI/out/native-gpu --parallel 3
# Set VK_LAYER_PATH if required by the local validation installation.
ctest --test-dir VoxelUI/out/native-gpu --output-on-failure

cmake -S VoxelUI -B VoxelUI/out/native-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  -DVOXELUI_PREVIEW_SDL=OFF -DVOXELUI_VULKAN=ON -DVOXELUI_SANITIZERS=ON
cmake --build VoxelUI/out/native-asan --parallel 3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir VoxelUI/out/native-asan --output-on-failure
# Isolate driver initialization from UI renderer ownership:
VoxelUI/out/native-asan/backends/vulkan/voxelui-vulkan-gpu-tests --context-only

cmake --build VoxelCraft/build/ui-release --target voxelcraft vc_voxelui_host_tests --parallel 3
ctest --test-dir VoxelCraft/build/ui-release -R '^VoxelUiHost\.' --output-on-failure
```

For a fresh VoxelCraft build directory use the source discovery command in
`voxelui-linux-v1.md`. `VOXELUI_GPU_CAPTURE=/path/image.ppm` captures actual native
readback from the primitive test. The next implementation is bounded sampled-image
and glyph atlases, cached SVG tessellation, then game integration. Inventory,
hotbar and crafting still follow that gate using the documented authoritative
inventory actions; no replacement inventory state was added.
