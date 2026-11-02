# VoxelUI host integration — 2026-11-02

VoxelUI now drives the optional application main menu, editable connection form,
loading/saving status and recoverable error screen. XML, CSS and the SVG mark live
in `assets/resources/voxelcraft/ui`. Actions queue Play/Connect/Back/Quit intents;
the application executes them after UI dispatch. Gameplay inventory, HUD, console,
pause and settings retain their existing implementation for this migration stage.

## Build

```sh
cmake -S VoxelCraft -B VoxelCraft/build/ui-integration -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR=../VoxelUI -DVC_ENABLE_GPU_TESTS=ON
cmake --build VoxelCraft/build/ui-integration --parallel 4
ctest --test-dir VoxelCraft/build/ui-integration -LE graphics --output-on-failure
VoxelCraft/build/ui-integration/bin/voxelcraft
```

Use `VC_USE_SYSTEM_DEPS=ON` when the documented system dependencies are installed.
The default dependency build remains pinned. VoxelUI requires libxml2, FreeType
and PNG; no dependencies are downloaded by its CMake project. The bridge is opt-in
so existing client builds and server-only builds do not acquire those dependencies.
The server-only path never discovers VoxelUI, SDL or Vulkan.

An installed VoxelUI **0.2.1** works with `VC_VOXELUI_SOURCE_DIR=` and
`CMAKE_PREFIX_PATH` pointing at its install prefix. Only the published C++ headers
and `VoxelUI::voxelui`/`VoxelUI::software` targets are consumed. There is no implicit
sibling discovery, moving-branch fetch, source copy, new graphics context or game
dependency in VoxelUI. The earlier integration referenced a Vulkan header/target
and revision absent from the available VoxelUI repository; that dependency is removed.

## Rendering and lifetime

`VulkanRenderer::setUi(DrawList)` retains an owning snapshot. Equality compares
command values and owning immutable resource identities, so repeated equivalent
snapshots avoid both rasterization and uploads. The host reference bridge renders
a **changed** snapshot to a straight-alpha RGBA image and composites one textured
quad using the existing device, shader, graphics queue and HUD pipeline. It is a
software-rasterized surface presented through Vulkan, not a native GPU DrawList
backend, glyph atlas or reusable SVG mesh implementation.

All image/mesh creation, descriptor replacement and retirement occur after the
existing frame fence. Uploads use the existing synchronous texture upload path;
changed large surfaces can stall and should not be used for a high-frequency HUD.
Unchanged frames retain the texture and issue one quad. `clearUi` immediately stops
using the snapshot and defers GPU destruction until that fence, or device-idle
shutdown. Minimized frames submit nothing. Resize uses the ordinary swapchain path.
A session entering gameplay clears status chrome before its first world frame.

`UiRenderMetrics` reports surface preparation time, uploaded RGBA bytes, quad draws
and logical texture bytes. GPU timestamps are **unavailable**, reported explicitly;
zero tessellations does not imply GPU SVG geometry. The reference rasterizer combines
UI layers in byte-valued color space; final host sRGB texture composition blends in
linear attachment space. Opaque UI matches the reference closely; translucent UI
against arbitrary world colors has no pixel-exact gamma-space equivalence promise.

## Input, resources and ownership

`vc_declarative_ui` translates the existing host input into VoxelUI events,
including UTF-8 text/preedit, focus loss, clipboard, pointer release and keyboard
navigation. Window input already supplies physical pixels; DPI is 1 in this adapter
and UI scale is applied once. Coalesced host input is not a raw SDL event stream;
full native IME candidate placement remains outside this stage.

ResourceCatalog outlives the runtime provider callback. Pack activation clears
resolver caches and reloads XML/CSS/SVG; rejected pack resources retry through the
builtin provider. Valid focus and edited address/name survive reload and screen
replacement. No LayoutNode pointers are kept by the host. Runtime/model operations
stay on the application thread. Native SDK 1.0 does not expose UI callbacks; future
native UI extensions must unregister and quiesce before unloading executable code.

## Verification and next task

Public host tests cover actual menu editing/actions, invalid XML/CSS pack recovery,
widget unregistration and snapshots surviving runtime destruction. The real graphics
playtest covers UI-only/world composition, screen replacement, resource reload,
resize, minimize/restore and shutdown. Application/session and existing graphics
regressions remain enabled. Exact final results are in `integration-results.json`.

Next: define a native GPU DrawList adapter with conformance images, bounded glyph
and image caches, and frame-completion retirement before migrating dynamic gameplay
overlays. Do not interpret this reference bridge as completing that optimization.
