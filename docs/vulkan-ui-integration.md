# VoxelUI host integration — 2026-11-02

VoxelUI now drives the optional application main menu, editable connection form,
loading/saving status and recoverable error screen. XML, CSS and the SVG mark live
in `assets/resources/voxelcraft/ui`. Actions queue Play/Connect/Back/Quit intents;
the application executes them after UI dispatch. The mode/health HUD and pause
menu now also use XML/CSS/SVG in both local and multiplayer sessions. Resume,
Settings, Save and Return to main menu queue commands into the existing session
state machine. Hotbar icons, inventory, crafting, settings, death and console
views retain their existing renderer and behavior during this migration stage.

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
changed large surfaces can stall. The current HUD displays discrete mode/health
changes and avoids per-frame counters; a native adapter is needed before adding
frequently changing HUD elements. Stable snapshots produce no surface uploads.
Unchanged frames retain the texture and issue one quad. `clearUi` immediately stops
using the snapshot and defers GPU destruction until that fence, or device-idle
shutdown. Minimized frames submit nothing. Resize uses the ordinary swapchain path.
A session entering gameplay replaces status chrome with its HUD before presenting
the world. Inventory/settings/console/death clear the declarative surface; returning
to gameplay rebuilds the appropriate view. A frame with no prepared gameplay view
retires the previous status snapshot.

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

`SessionEnvironment` borrows the application-owned adapter; sessions are destroyed
before it. No UI callback captures a world, controller, network peer or coroutine.
`SandboxUiController` remains the authority for overlay state: single player pauses
simulation, multiplayer isolates input while the server keeps running. Legacy pause
hit regions and health drawing are disabled only when the new host chrome is active.
Pause input goes to one widget system; transitions cancel held mining/movement.
Callbacks only queue intent, which is consumed after dispatch. Invalid screens or
coalesced Escape/toggle events discard stale pause actions. Resource packs may
override `ui/hud.xml`, `ui/pause.xml` and `ui/gameplay.css` with the usual fallback.

## Verification and next task

Public host tests cover actual menu editing/actions, invalid XML/CSS pack recovery,
widget unregistration, changing health pixels, stable HUD snapshots, all pause
commands, focus-loss capture cancellation and snapshots surviving runtime destruction.
The real graphics
playtest covers UI-only/world composition, screen replacement, resource reload,
resize, minimize/restore and shutdown. Application/session and existing graphics
regressions remain enabled. The application playtest exercises XML pause navigation,
persisted settings/return navigation and multiplayer pause/disconnect without changing
renderer ownership. Exact results and platform limitations are in
[integration-results.json](integration-results.json).

Linux checks for implementation `44cdd47`:

| Configuration | Result |
|---|---|
| GCC Debug / Release CPU and CLI | 256 / 256 in each |
| Clang ASan + UBSan, leak detection enabled, CPU and CLI | 256 / 256 |
| Installed VoxelUI package, independent host cases | 5 / 5; client `--help` passed |
| VoxelUI disabled, fresh build | 251 / 251 CPU and CLI; session graphics passed |
| Full Debug Vulkan regression gate | 15 / 15 before final coalesced-input guard |
| Final UI + session graphics, Debug / Release | 2 / 2 in each after that guard; zero validation errors/warnings |

The host pixel oracle compares reused and fresh runtimes at 640×480 and 1920×1080
with UI scale 1.75. Actual Vulkan captures show the
[creative HUD](screenshots/voxelui/creative-hud.png),
[survival HUD](screenshots/voxelui/survival-hud.png) and
[pause menu](screenshots/voxelui/pause.png). Capture cameras isolate the UI; these
are screen evidence, not performance or world-visual acceptance baselines.

Reproduce CPU checks using the build command above, `CMAKE_BUILD_TYPE=Release`
for the optimized build, or `CMAKE_CXX_COMPILER=clang++`,
`CMAKE_C_COMPILER=clang`, `VC_SANITIZER=address-undefined` for sanitizer builds:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/ui-asan -LE graphics --output-on-failure
ctest --test-dir build/ui-integration -L graphics --output-on-failure
```

Graphics tests require a Vulkan-capable display/window manager that acknowledges
minimize/restore, plus Khronos validation with synchronization checks enabled. This
run used project-local Xvfb/Openbox with the hardware NVIDIA ICD. The CPU sanitizer
gate is clean. Earlier leak-enabled **graphics** runs failed LSan; a legacy SDL-only
control also reports leaks, but full attribution remains unresolved. No suppression
or claim of a clean GPU leak check is made. Windows work is deferred; the supplied
historical 213/219 Windows CTest log is recorded separately in the JSON.

The earlier 60-frame Release bridge sample measured 32.70 ms per unchanged
world/UI frame and 48.76 ms per changed frame; the last changed surface preparation
was 15.85 ms and uploaded 2,457,600 bytes. These are complete fixture-frame means,
not GPU timestamps, a controlled host before/after comparison, or current gameplay
frame-rate guarantees. Idle gameplay now has an explicit zero-upload assertion.
VoxelUI's controlled runtime before/after measurements are in the separate
repository's `docs/performance-before-after.md`; fully visible label emission and
regional list insertion/removal remain significant costs.

Next: define a native GPU DrawList adapter with conformance images, bounded glyph
and image caches, and frame-completion retirement; then migrate inventory/crafting
with host-owned atlas resources, and settings/console. Do not interpret this
reference bridge as completing GPU optimization or the full UI migration.
