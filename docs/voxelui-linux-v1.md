# Linux VoxelUI continuation

This is a buildable performance and lifetime foundation, **not completion of the
native Vulkan milestone**. The application still rasterizes DrawLists with the
software backend. No native command coverage, SVG GPU tessellation, GPU glyph
atlas or native/software GPU conformance pass is claimed.

| Stage | Checkpoint |
|---|---|
| 0 | Source discovery and Debug/Release installed-package gate passes |
| 1 | Instrumented and measured; click/resize/animation-clock limits are explicit in the baseline |
| 2 | Repeated menu work, ordered input and global upload waits fixed; CPU rasterization remains |
| 3 | Native DrawList renderer, GPU SVG/text caches and conformance suite not implemented |
| 4 | Bridge object lifetime verified; external graphics heap reports and native atlas audit remain |
| 5 | Existing dependencies, atlas boundary and typed snapshot/actions designed; migration deferred |
| 6 | Local Linux results below; CI execution, sustained manual play and minimize/restore are not claimed complete |

## Source provenance and build repair

Initial clean checkouts:

| Repository | Initial branch | Initial commit | Development branch |
|---|---|---|---|
| VoxelCraft | main | 643b8e0060c3 | perf/linux-voxelui-v1 |
| VoxelUI | master | 8e6b3e3dc311 | fix/linux-package-configurations |

The requested `.agents/skills` directory and seven named project skills were
absent, including after searching local skill directories. Both repository
AGENTS.md files were read. Windows and module ABI changes were deferred.

The exact parent-directory CMake command reproduced the source failure. An
untyped `-DVC_VOXELUI_SOURCE_DIR=../VoxelUI` was converted by `CACHE PATH` relative
to `/home/kirillr/code` into `/home/kirillr/VoxelUI`. All required files actually
exist under `/home/kirillr/code/VoxelUI`. Storing the argument as `CACHE STRING`
preserves it for resolution relative to VoxelCraft's source directory. Invalid
paths still fail, with original spelling, absolute path and the exact missing
file. The test exercises parent/source launch directories, an absolute path,
a missing directory, and each missing public header.

VoxelUI's unconfigured Ninja package generated `VoxelUITargets-noconfig.cmake`.
Its install script guarded that file with `CMAKE_INSTALL_CONFIG_NAME MATCHES
"^()$"`. Installing with `--config Debug` skipped the configuration file while
installing the generic export and archives. The independent consumer reproduced
both reported `IMPORTED_LOCATION` failures. CI now explicitly sets the build type
and matches build, test, install and consumer configurations. Separate Linux
Debug and Release jobs check imported targets and their actual library paths.

## Implementation

- Menu preparation runs once in the ordinary frame; DrawList generation and
  comparison run after input. Loading/session transitions can still prepare
  their newly selected screen in the same frame. `buildDrawList()` retains its
  zero-delta flush so input styling and bindings are visible immediately.
- `DeclarativeUi::show()` advances the runtime once. The former first update,
  which advanced the departing screen and animations twice, is removed.
- An owning ordered UI stream accompanies the original gameplay snapshot.
  Repeated backspaces, text/key ordering, pointer release positions and IME
  ordering survive a poll. Clipboard paste in this stream uses the runtime's
  clipboard callback at the ordered key event. Legacy snapshot consumers retain
  their existing clipboard and gameplay behavior.
- Screen actions stop delivery to the departing UI. Focus loss cancels capture.
  Overflow at 1,024 events or 64 KiB of event text cancels UI capture and is
  explicitly flagged. CPU tests cover repeated edits, ownership and overflow.
- Changed UI surfaces reuse an image, sampler, descriptor bindings, quad and
  persistently mapped staging buffer at a fixed viewport. Transfers are recorded
  before rendering in the normal frame command buffer. A completed host frame
  fence protects staging writes, descriptor changes and retirement. Pending
  uploads survive out-of-date acquisition. The existing single-frame design
  remains; resize/shutdown device waits remain intentional.
- Other texture uploads use the submission fence instead of queue idle. There
  are no remaining `vkQueueWaitIdle` calls in the renderer. This does not remove
  the full-surface software rasterization cost.
- Optional phase profiling, GPU timestamps and live/peak/created Vulkan object
  counters make further work measurable. See [latency](voxelui-latency-baseline.md)
  and [memory](voxelui-memory-audit.md) for results and limitations.

## Reproduce from code/

The default dependency path remains pinned by the existing CMake inputs. Use
`-DVC_USE_SYSTEM_DEPS=ON` when intentionally selecting installed SDL3/GLM/GTest.
The measurements used installed SDL 3.4.18. No VoxelUI checkout is fetched.

```sh
cmake -S VoxelCraft -B VoxelCraft/build/ui-integration -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI -DVC_ENABLE_GPU_TESTS=ON
cmake --build VoxelCraft/build/ui-integration --parallel 3
ctest --test-dir VoxelCraft/build/ui-integration -LE graphics --output-on-failure

cmake -S VoxelCraft -B VoxelCraft/build/ui-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI -DVC_ENABLE_GPU_TESTS=ON
cmake --build VoxelCraft/build/ui-release --parallel 3
VoxelCraft/build/ui-release/bin/voxelcraft --no-mods --no-settings

# Installed package alternative; use a NEW build directory and explicit prefix.
cmake -S VoxelCraft -B VoxelCraft/build/ui-package -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR:STRING= -DCMAKE_PREFIX_PATH=/absolute/voxelui-prefix

# Separate legacy and headless builds retain their original test suites.
cmake -S VoxelCraft -B VoxelCraft/build/legacy-v1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVC_ENABLE_VOXELUI=OFF
cmake --build VoxelCraft/build/legacy-v1 --parallel 3
ctest --test-dir VoxelCraft/build/legacy-v1 --output-on-failure
cmake -S VoxelCraft -B VoxelCraft/build/server-v1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DVC_BUILD_CLIENT=OFF
cmake --build VoxelCraft/build/server-v1 --parallel 3
ctest --test-dir VoxelCraft/build/server-v1 --output-on-failure

# Requires a display, hardware Vulkan and a discoverable Khronos validation layer.
ctest --test-dir VoxelCraft/build/ui-release -L graphics --output-on-failure
VoxelCraft/build/ui-release/bin/vc_voxelui_lifecycle bridge 400 --require-validation
```

VoxelUI's README contains the independently installed Debug/Release consumer
loop. `--require-validation` now explicitly enables synchronization validation
through `VK_EXT_validation_features`; it fails if that facility is unavailable.
See the memory audit for this host's local validation-library setup.

If a trusted Khronos `.so` is already present but has no discoverable manifest,
the optional helper creates one only under the chosen build directory:

```sh
python3 VoxelCraft/tests/local_validation_manifest.py \
  "$HOME/.local/share/Steam/steamrt64/libVkLayer_khronos_validation.so" \
  VoxelCraft/build/local-validation
export VK_LAYER_PATH="$PWD/VoxelCraft/build/local-validation"
```

## Executed Linux verification

| Check | Result |
|---|---|
| VoxelUI standalone GCC Debug / Release | Both build; 67 named cases pass in each; headless preview runs |
| VoxelUI independent installed Debug / Release consumers | Both pass exact 0.2.1 lookup, imported-location checks and executable contract |
| VoxelUI Clang ASan/UBSan from clean source archive | All 67 cases pass with leak detection |
| VoxelCraft GCC Debug / clean-source Release, VoxelUI ON | Build and 260 CPU/CLI cases pass in each |
| VoxelCraft Clang ASan/UBSan, VoxelUI ON | 260 CPU/CLI cases pass with leak detection |
| Final ordered-input edge guards | 16 input cases pass in GCC Release and Clang ASan/UBSan |
| VoxelCraft installed-package host | Configures; all 6 host cases pass |
| Adapter against published VoxelUI 0.2.1 headers | Compiles without the optional new profiling header |
| Legacy configuration, VoxelUI OFF | Builds; 253 CPU/CLI cases pass; Network, Survival, Inventory and GPU smoke graphics cases pass |
| Clean headless Clang TSan | Builds server; 16 worker/session cases and 3 server/CLI/process cases pass |
| Headless server dependency inspection | No SDL or Vulkan dynamic linkage |
| Enabled full graphics suite | First run 10/15; 3 gameplay failures pass on isolated retry; 2 minimize-related failures remain |
| Clean-source Wayland bridge stress | 400 transitions/reloads, sync validation enabled, no warnings/errors; stable counts; all counted resources zero after shutdown |
| X11/XWayland bridge probe | 30 transitions, sync validation enabled; passes and releases counted resources |
| Actual application with profiling/query pool and validation | Menus/typing/world/pause/resize requests and shutdown pass; zero validation warnings/errors |
| Leak-enabled graphics controls | External heap reports remain; see the audit. Not marked green |
| Native backend conformance / atlas retirement | Not implemented or tested |
| GitHub Actions and Windows | CI configuration updated; remote CI and Windows not executed |

Clean-source verification used `git archive` copies of both repositories under
`/tmp/voxelui-linux-v1-u1G64I`, with committed renderer/probe/input updates copied
from Git before rebuilding. No untracked build input was needed. Installed
SDL3/GLM/GTest and existing pinned dependency source caches were selected with
`VC_USE_SYSTEM_DEPS=ON` and `FETCHCONTENT_SOURCE_DIR_ENTT`, `..._LUA` and
`..._STB_IMAGE`. The EnTT checkout is clean at `v3.14.0`. This is distinct from
claiming the network-fetch/vcpkg CI job was run locally.

The two remaining graphics failures are `Client.VoxelUI` (minimize not
acknowledged) and `Client.Sessions` (its minimize wait times out before the
session scenarios). No assertion or graphics label was disabled to turn them
green. Separate local and remote gameplay graphics checks passed, but this
complete session/minimize/restore script remains unverified on this compositor.
Repeating those two checks through X11/XWayland also fails at the same minimize
stage; the successful X11 bridge probe does not close that gap.
Sustained manual play and native renderer validation are also pending.

The headless TSan configuration is reproducible with:

```sh
cmake -S VoxelCraft -B VoxelCraft/build/ui-tsan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ \
  -DVC_BUILD_CLIENT=OFF -DVC_SANITIZER=thread
cmake --build VoxelCraft/build/ui-tsan --parallel 3
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir VoxelCraft/build/ui-tsan \
  --output-on-failure -R '^(GameSession|CoreModules|TerrainWorker|MeshWorker|BackgroundMeshing|SnapshotWriter)\.'
SDL_VIDEO_DRIVER=x11 VoxelCraft/build/ui-release/bin/vc_voxelui_lifecycle \
  bridge 30 --require-validation
```

Implementation commits in VoxelCraft: `b85d3ef` (discovery/CI), `53a7e8a`
(instrumentation/workloads), `beadb81` (ordered input/upload lifetime/counters),
`deee8e8` (validation feature chain), `58e7a96` (reproducible probes/controls),
`a1c98d2` (bounded wheel and malformed release handling), plus the final report
and artifact commit. VoxelUI: `bcf91a9` (package configurations), `a18bdab`
(phase observers/tests), `799b9f2` (documentation). All are local development
commits; neither published reference branch was reset.

Raw verification logs, including failed graphics and LSan runs, are preserved
with checksums in [the audit log index](voxelui-audit-logs/index.json). Normal
Linux CI now has VoxelUI-enabled GCC/Clang jobs; the manual GPU workflow also
checks out the pinned UI source and enables its graphics tests. These workflow
edits were reviewed locally, not executed on GitHub Actions.

## Executable next increment: native DrawList backend

Continue from the committed baseline and repeat the existing latency/lifecycle
executables after each increment. Do not replace the software reference.

1. Add optional `VoxelUI/backends/vulkan` with host-supplied physical/device/queue,
   render format, command buffer and completion token. The host retains instance,
   swapchain, frame acquisition/submission and presentation ownership. Keep the
   core's dependency graph unchanged.
2. Implement persistent vertex/index storage, ordered adjacent batch runs,
   straight-alpha/color-space handling, rectangles/borders/rounded rectangles,
   image cache, transform stack and nested clip geometry. Rotated clips need
   actual clipping, not only an axis-aligned scissor. Unsupported commands must
   produce diagnostics rather than disappear.
3. Tessellate `VectorDocument::shapes` using their flattened contours, holes and
   even-odd semantics; arbitrary polygon fans are invalid. Cache using retained
   immutable geometry identity plus scale/tolerance. Include strokes, endpoint
   gradients, shape clips and embedded images from the existing public profile.
4. Pack `PositionedGlyph::bitmap` masks into atlas pages, retaining their owning
   identity and page descriptors through completion. Add bounded growth/eviction
   and retirement before enabling resource reload. Caret/selection/composition
   visuals already arrive as ordered DrawList commands.
5. Add readback comparisons against `SoftwareRenderer` at several viewports and
   scales for all eight command variants. Define edge tolerances and separate
   interior-color checks. Test reload, hundreds of transitions, translucent UI
   over world rendering and explicit shutdown counters under sync validation.
6. Select this backend in the existing application adapter only after menus,
   pause and HUD pass those tests. Then remove the full-screen bridge from that
   selected path and repeat the committed raw benchmark methodology.

No native backend target or atlas API is declared implemented in this continuation.

## Inventory/hotbar/crafting audit for the following increment

`SandboxUiController`, `sandboxHud`, `SurvivalUiContext`, `CreativeInventory`,
`SurvivalInventory` and `InventoryAction` are the current integration boundaries.
The survival store has 36 slots and a 9-slot hotbar. Local transactions use the
existing controller/context. Remote UI sends `session.inventory(action)`;
`client/network_client.cpp` restores authoritative item stacks, selected slot and
health from the server. These remain the only authoritative state paths.

Proposed typed snapshots: `ItemSlotView { slot, itemName, count, iconKey,
selected }`, `InventoryView { revision, slots, catalog, query, category,
dragSource }`, `RecipeView { recipeId, ingredients, output, craftable }` and
`HotbarView { slots, selectedSlot }`. They copy presentation values from existing
stores, never mutate inventory through a UI data binding. Typed actions should
map bounded slot/recipe identifiers to the existing Swap, TransferOne,
TransferHalf, TransferAll, QuickTransfer, MoveToBackpack and Craft commands.

The host must own item icon rendering and atlas page lifetime. Bind a stable
icon key plus resource revision and UV region to the future backend's borrowed
image registration; pack reload changes the revision and retires pages after
completion. Do not add Vulkan handles to XML or to VoxelUI core, create one image
per item, or independently recreate the game inventory inside a ViewModel.
The current `DrawImage` has no source UV subrectangle, so atlas integration needs
a versioned, renderer-neutral image-region reference and backend registration;
it cannot be declared complete using the existing full-image command alone.

Parity gates: creative category/search/pagination and hotbar assignment;
survival quantities, drag/swap and all transfer modes; crafting ingredient/output
and no-space behavior; number/wheel selection, death/respawn retention and save
round trip; multiplayer acceptance/rejection and stale/reordered authoritative
updates; resource-pack/native-mod icons and screen/focus transitions. Keep the
legacy inventory until every behavior is covered. No further UI migration was
implemented before the native renderer gate.

A subsequent [native geometry continuation](voxelui-native-geometry-continuation.md)
adds optional bounded CPU rectangle mesh preparation and reference tests in
VoxelUI. Vulkan recording and application-native rendering remain pending.
