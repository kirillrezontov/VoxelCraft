# VoxelUI Linux memory and Vulkan lifetime audit

The bridge's tracked first-party Vulkan objects stabilize during repeated screen
changes and all reach zero at explicit shutdown. LeakSanitizer still reports
small external DBus/NVIDIA heap allocations in graphics processes. Neither
observation is a measurement of leaked VRAM. A native backend/atlas audit remains
pending because that backend is not implemented.

## Method and controls

GCC 16.2.1 Release hardware validation and Clang 23.1.4 Debug ASan/UBSan were run
on Linux x86_64, Ryzen 7 2700, RTX 3050, NVIDIA 615.78.08, SDL 3.4.18 and
Wayland/Hyprland. CPU ASan/UBSan testing ran with leak detection and halt-on-error
enabled. The installed LLVM symbolizer resolved DBus sources and first-party
Debug executable locations. Some NVIDIA tail frames remain module offsets or
unloaded-module addresses; these are not represented as resolved first-party
stacks.

`vc_voxelui_lifecycle` has separate `sdl`, `software`, `vulkan` and `bridge`
controls. SDL-only creates a plain SDL window with no Vulkan flag, instance or
device. Vulkan-only does not instantiate `DeclarativeUi`. Software-only loads
real menus/pause XML and rasterizes without a window/device. Bridge mode renders
400 alternating menu/pause screens and reloads resources every 50 iterations.
The application control runs an actual bounded local session. There is no
`native` lifecycle mode pretending to work.

The first probe revision used a Vulkan-capable SDL window as its SDL control
(no instance/device) and constructed an unused runtime in its Vulkan control.
Those controls were corrected and rerun; their earlier reports are not used as
pure SDL/no-VoxelUI evidence.

| Leak-enabled process | Workload | Exit/result |
|---|---|---|
| Plain SDL-only | 30 polls, no Vulkan window flag | 0; no LSan report |
| VoxelUI software-only | 100 menu/pause renders and reloads | 0; no LSan report |
| Vulkan without VoxelUI runtime | 100 submitted frames | 1; 1,270 bytes in 3 DBus/driver allocations |
| VoxelUI bridge | 400 transitions/reloads | 1; 1,270 bytes in the same 3 allocation classes |
| Full temporary game session | 30 actual gameplay frames | 1; 1,354 bytes in 3 DBus/driver allocations |
| Native VoxelUI GPU backend | — | Not implemented; not tested |

The earlier Vulkan-capable SDL-window control reported 1,330 bytes without any
host-created Vulkan instance/device. The corrected plain-SDL control is clean.
The symbolized allocation sites are `dbus_message_new_empty_header` and related
DBus message-loader allocations, reached through NVIDIA library frames. The
inference is that driver/DBus initialization owns these allocations; precise
upstream cleanup remains unresolved. No first-party UI allocation site appears
in the reports, and increasing bridge transitions did not add allocation
classes. The byte totals vary slightly between process setups, so they are not
used as a subtraction-based UI leak estimate.

## First-party GPU ownership

The counters track successful creation and destruction of host-owned buffers,
images, views, memory allocations, samplers, descriptor pools/sets and staging
buffers. They survive renderer shutdown for inspection, include exception
cleanup, and distinguish staging buffers from other buffers. Swapchain images
are borrowed and excluded; their owned views are counted. These are object
counts, not allocated-byte or driver residency counters. Atlas pages are absent;
the existing legacy host font texture is not a VoxelUI glyph atlas.

The clean tracked-source Release run under API and synchronization validation
reported identical counters at the midpoint and end of 400 transitions:

| Object | Warm live | Final live before shutdown | Peak | Live after shutdown |
|---|---:|---:|---:|---:|
| VkBuffer | 12 | 12 | 14 | 0 |
| VkImage | 4 | 4 | 5 | 0 |
| VkImageView | 7 | 7 | 8 | 0 |
| VkDeviceMemory | 16 | 16 | 19 | 0 |
| VkSampler | 3 | 3 | 4 | 0 |
| Descriptor pool | 1 | 1 | 1 | 0 |
| Descriptor set | 2 | 2 | 2 | 0 |
| Staging buffer (subset of VkBuffer) | 1 | 1 | 1 | 0 |

Creation totals also stopped increasing between the midpoint and end: 16
buffers, 7 images, 16 views, 23 memory allocations, 4 samplers, 1 pool, 2 sets and
3 staging buffers, including initial swapchain configuration. The counters do
not cover every Vulkan object class; existing pipelines, query pool, command
pool, semaphores and fences retain their explicit host teardown code.

The frame fence must complete before replacing an image/descriptor, changing
staging contents or retiring UI buffers. Transfer commands and drawing share the
host's frame submission. The old shader-read image layout transitions to
transfer destination and back before sampling. Out-of-date acquisition preserves
the pending transfer. Clearing UI retires resources on the next completed host
frame; minimized/shutdown paths retain them until the device shutdown wait.

No first-party leak was confirmed in the executed controls. Avoidable UI GPU
allocation churn and global queue waits were removed, but they were not labeled
as leaks. CPU raster storage can retain its largest surface until UI/renderer
teardown; handle counts do not quantify that CPU capacity.

## Validation and limitations

The system initially exposed no Khronos layer. An existing, trusted Steam runtime
library was found at `$HOME/.local/share/Steam/steamrt64/libVkLayer_khronos_validation.so`.
`tests/local_validation_manifest.py` reads its exported layer/extension versions
and writes a build-local manifest. The library reports API 1.3.285; no driver or
system package was installed or replaced.

`--require-validation` now explicitly chains synchronization validation into
instance creation using [VkValidationFeaturesEXT](https://docs.vulkan.org/refpages/latest/refpages/source/VkValidationFeaturesEXT.html).
The feature chain is cleared before creating the persistent debug messenger.
An initial run caught that messenger-chain mistake; the fix was committed and
the clean-source tests below were rerun. Validation counters alone did not
capture that early loader diagnostic, so the logs were inspected for VUID and
SYNC-HAZARD messages as well.

The clean 400-transition probe and the actual application profiling run with
GPU timestamps both complete with validation enabled and zero warnings/errors.
An X11 (XWayland) 30-transition probe also passes API/synchronization validation
and retires all counted objects. Its swapchain has one additional view; counts
are consequently different from the Wayland table above.
The full graphics suite initially passed 10/15; three gameplay failures passed
when rerun alone after competing graphics probes stopped. `Client.VoxelUI` and
`Client.Sessions` still fail because minimize is not acknowledged by this
compositor. These tests were not weakened or marked passed. The same two tests
also fail at minimize through X11/XWayland. Minimize/restore validation and the
complete application-session script remain incomplete here.

## Reproduction

Compressed symbolized reports and validation/test output are retained in
[the audit log index](voxelui-audit-logs/index.json), including failure reports.

From `code/`, configure the enabled graphics build as in
[the implementation report](voxelui-linux-v1.md). For sanitizers:

```sh
cmake -S VoxelCraft -B VoxelCraft/build/ui-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  -DVC_ENABLE_VOXELUI=ON -DVC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI \
  -DVC_ENABLE_GPU_TESTS=ON -DVC_SANITIZER=address-undefined
cmake --build VoxelCraft/build/ui-asan --parallel 3
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir VoxelCraft/build/ui-asan -LE graphics --output-on-failure

# Run each process separately, retaining stdout/stderr and its exit status.
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  VoxelCraft/build/ui-asan/bin/vc_voxelui_lifecycle sdl 30
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  VoxelCraft/build/ui-asan/bin/vc_voxelui_lifecycle software 100
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  VoxelCraft/build/ui-asan/bin/vc_voxelui_lifecycle vulkan 100
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  VoxelCraft/build/ui-asan/bin/vc_voxelui_lifecycle bridge 400
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  VoxelCraft/build/ui-asan/bin/voxelcraft --frames 30 --radius 1 \
    --generator-version 1 --no-settings --no-mods --no-save --no-vsync
```

No suppressions or leak-detection disabling were introduced. Further work must
add atlas-page/descriptor retirement tests to the native backend, cover complete
local/remote session repetition under ASan, and rerun minimize/restore on a
compositor that acknowledges those operations. The current graphics LSan
failures remain visible rather than being relabeled as passes.
