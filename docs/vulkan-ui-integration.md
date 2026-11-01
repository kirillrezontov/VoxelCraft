# VoxelUI Vulkan host adapter — Milestone 4

The optional backend lives in independent VoxelUI, version 0.2.1.
Compatible implementation revision: `cb3dd6ff19b7d3ebe01ab01f8b0d6e82c7596948`. VoxelCraft's
adapter passes its existing physical device, logical device, graphics queue family,
attachment formats, command buffer and actual fence completion. It creates no
second Vulkan context. The legacy high-level UI remains active in this milestone.
XML game screens, original SVG visual design and feature-parity replacement follow
in the ordered next milestones.

## Dependency contract

```sh
# Explicit local sibling; source is compiled by its own CMake project.
cmake -S VoxelCraft -B build/client-ui -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DVC_ENABLE_VOXELUI=ON -DVC_VOXELUI_SOURCE_DIR=../VoxelUI \
    -DVC_ENABLE_GPU_TESTS=ON
cmake --build build/client-ui --parallel 4

# Standalone installed dependency; exact package version, no implicit clone.
cmake -S VoxelUI -B build/ui -DVOXELUI_VULKAN=ON -DVOXELUI_PREVIEW_SDL=OFF
cmake --build build/ui --parallel 4
cmake --install build/ui --prefix ui-install
cmake -S VoxelCraft -B build/installed-ui -DVC_ENABLE_VOXELUI=ON \
    -DVC_VOXELUI_SOURCE_DIR= -DCMAKE_PREFIX_PATH="$PWD/ui-install"
```

`cmake/VoxelUI.cmake` accepts only an explicit checkout or installed VoxelUI 0.2.1
with the Vulkan component. It never fetches a moving branch or copies generic
source into VoxelCraft. The server-only path never includes that file or discovers
VoxelUI/SDL/Vulkan. Default client builds keep the adapter opt-in until the UI
migration gate. `vc_renderer` privately links `VoxelUI::vulkan`; gameplay and
network/persistence public contracts remain graphics independent.

## Concrete renderer interface and lifetime

With the option enabled, `VulkanRenderer::setUi(voxelui::DrawList)` moves an owning
snapshot into renderer state; `clearUi` removes it and requests cache retirement.
`uiMetrics()` returns plain CPU/GPU timing/counter values without exposing Vulkan.
The constructor's original Window/device/atlas ownership remains unchanged.

The backend is created lazily on the first DrawList. VoxelCraft still has one frame
in flight and a single frame fence. After waiting that fence, the adapter releases
the previous UI serial. `prepare` records bounded staging uploads outside dynamic
rendering. `render` executes after world/legacy overlays inside the host's existing
rendering pass, with matching depth/color/sample declarations. Screen replacement
can release CPU layouts immediately because DrawList payloads/cache entries own
immutable values and submitted frame plans retain GPU buffers/images/descriptors.

Swapchain recreation already waits device idle; it releases UI serials before
changing the target format. Minimized frames perform no UI prepare/submit.
Shutdown waits host GPU completion and destroys the UI backend before the device.
`clearSession` also clears UI snapshots. Atlas/world/session lifetimes remain those
of the verified application/session split. Native mods cannot register Vulkan
objects or unload UI callbacks through SDK 1.0.

The backend uses legacy Vulkan barriers on the graphics queue. It requires Vulkan
1.3 dynamic rendering, with no new synchronization2 or shader-demote feature.
It renders rectangles, rounded borders, images, positioned glyph atlases,
concave/holed/self-crossing even-odd vectors, supported gradients, opacity,
clips/transforms and UI/DPI scaling. See VoxelUI's `docs/vulkan-backend.md` for
budgets, frame-completion contract, color-space policy and supported SVG limits.

## Verification

`Client.VoxelUI` builds a real XML/text/SVG DrawList, captures UI-only rendering,
composes it with a real chunk mesh on the same device, replaces screens repeatedly,
resizes, requires minimize/restore acknowledgement, reloads a resource-backed
screen/font, verifies warmed cache counters and checks validation after shutdown.
The fixture actionless button demonstrates drawing only; interactive game-screen
behavior belongs to Milestone 6 and is not claimed here.

The independent VoxelUI conformance suite compares UNORM GPU captures with the
software renderer at DPI 1/1.5/2. Both English/Russian text use the existing text
engine. It also exercises two submitted slots, LRU pressure, cache clearing before
completion, double-prepare/slot rejection, canceled unsubmitted work, GPU/CPU
budget rollback, target changes and shutdown with synchronization validation.

Normal RADV ASan/LSan execution exposed a 128-byte allocation after driver unload.
A host-only probe that never creates the UI backend reproduces it; loader mappings
place the allocation at offset 0x35b99e in the loaded `libvulkan_radeon.so` image.
Keeping RADV loaded through `LD_PRELOAD` allows leak-enabled GPU ASan/UBSan checks
to complete. Record the normal finding and the driver-retained run separately;
this is an environment-specific test workaround, not an application dependency or
a suppression of project allocation leaks. No driver package was modified.

The final committed-source build, installed-package consumer, sanitizer and graphics
results are recorded in `vulkan-ui-results.json` after the milestone gate.
