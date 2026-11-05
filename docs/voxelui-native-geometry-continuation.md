# Native geometry continuation — November 5, 2026

The next buildable slice is implemented in VoxelUI commit `2b8ecd5` on
`feat/native-vulkan-geometry`. It is CPU preparation for a future native backend,
**not an application-native Vulkan renderer**. VoxelCraft still uses the existing
software surface bridge. No new responsiveness improvement is claimed.

Initial checkouts were VoxelCraft `perf/linux-voxelui-v1` at `c59cf2f4c121` and
VoxelUI `master` at `d72eaa5d3629`. The former had three untracked artifact paths
(`docs/performance-runs/vulkan/`, `docs/screenshots/vulkan/`,
`docs/vulkan-ui-results.json`); all were preserved. VoxelUI was clean. No branches
were reset and no commits were pushed.

## Build gate rechecked

The prior discovery fix is present: `CACHE STRING` preserves `../VoxelUI` until
resolution against the VoxelCraft source directory. The old `CACHE PATH` had
converted it relative to the parent launch directory to `/home/kirillr/VoxelUI`.
The actual sibling checkout and all three required files exist. The requested
parent-directory configure now prints `/home/kirillr/code/VoxelUI` and succeeds.
The discovery regression script passes invalid/missing-file diagnostic cases.

The package CI fix is also already present: build type, install configuration
and consumer configuration match for both Linux Debug and Release. Fresh local
build/install/consumer runs pass. Generated exports contain
`IMPORTED_LOCATION_DEBUG` / `IMPORTED_LOCATION_RELEASE` for both archives;
independent consumers check those locations and exact package version 0.2.1.
No further source-discovery or packaging edits were needed.

## Implemented boundary and coverage

`VoxelUI/backends/vulkan` adds an optional private static geometry target,
`VOXELUI_VULKAN_GEOMETRY=ON`. Dependencies are:

`DrawList -> CPU geometry compiler -> owning indexed mesh -> future host Vulkan upload/recording`

Core has no Vulkan dependency; the experiment is not exported as a supported
Vulkan package target. Square rectangles, disjoint borders, straight-alpha
vertex colors, affine transform stacks, nested rectangular clips, and DPI scale
produce ordered triangles. Only convex clipped rectangles use triangle fans.
Rounded rectangles, images, glyphs and SVG explicitly fail with a command index.
All failures discard partial output. Stack and mesh limits and nonfinite-input
checks bound processing of DrawLists derived from untrusted UI resources.

The current reference uses axis-aligned bounds for rotated clips. This slice
matches that v1 behavior, not exact rotated clip geometry. No GPU buffers,
shader/pipeline, atlas, cache or retirement mechanism was introduced. The result
owns its data; future GPU uploads and destination resources must outlive submitted
commands under the host fence. Host instance/device/swapchain ownership is unchanged.

## Executed verification

| Check | Result in this continuation |
|---|---|
| VoxelUI standalone Debug / Release | Core suite passes in both |
| Independently installed Debug / Release consumers | Exact 0.2.1 and both imported targets pass |
| Enabled VoxelCraft Debug build | Pass |
| VoxelCraft non-graphics suite | 260/260 pass |
| Native geometry Debug / Release | Pass; 33 software comparisons plus error/budget cases |
| Clang ASan/UBSan with leak detection | Core and geometry tests pass |
| Clean cloned committed VoxelUI Release | Fresh build; both CTest executables pass |
| VoxelCraft `Client.GpuSmoke` | Pass with required validation and sync-validation environment |
| Headless VoxelUI settings preview | Rendered and visually inspected; Latin/Cyrillic controls readable |
| Tracking audit / diff whitespace checks | Zero findings after committing required new inputs |

Software comparisons cover three viewports and five scales. Axis-aligned
fixtures require exact pixels. Transformed mismatches are allowed only within
0.001 framebuffer pixel of a triangle edge; this captures forward/inverse
floating-point boundary classification, not arbitrary image error. These are CPU
triangle tests, not native GPU conformance.

The [new game capture](native-geometry-validation/bridge-gpu-smoke.png) was
visually inspected: creative HUD and hotbar are visible over the smoke scene's
grass surface. It is from the existing bridge, not the new geometry compiler.
The separate settings preview was written to `/tmp/voxelui-continue-settings.png`.

Logs are retained under [native-geometry-validation](native-geometry-validation/).
Clean clones and build logs remain at `/tmp/voxel-clean-checkout-7gv7yf7x`.
No fresh TSan, legacy build, dedicated-server build, full graphics suite,
multiplayer playtest, native GPU conformance or lifecycle stress run is claimed.
Those earlier results remain recorded in the prior report and were not relabeled
as new runs. No first-party GPU lifetime changes occurred; external graphics
heap findings in the prior memory audit remain unresolved.

## Performance and remaining work

The prior committed benchmark reports VSync-off hover median 89.588 -> 81.460 ms
and text editing 89.891 -> 81.527 ms after bridge improvements. Those are previous
measurements, not this continuation's before/after results. Changed full-menu
software rasterization remains roughly 77 ms in that baseline. This private,
unused geometry target cannot improve game latency yet.

Next: host-context Vulkan solid-color pipeline and persistent vertex/index
buffers consuming this mesh, with GPU readback comparison and fence-safe reuse.
Then rounded primitives, cached SVG contours/holes/strokes/gradients, image
sampling and bounded glyph atlases, resource retirement, and native game selection.
The existing menus must work before removing full-screen software uploads.
Inventory/hotbar/crafting migration remains deferred; use the authoritative
inventory/action and host-owned item-atlas design in `voxelui-linux-v1.md`.

## Reproduction from code/

```sh
cmake -S VoxelCraft -B VoxelCraft/build/ui-integration -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DVC_ENABLE_VOXELUI=ON \
  -DVC_VOXELUI_SOURCE_DIR=../VoxelUI -DVC_ENABLE_GPU_TESTS=ON
cmake --build VoxelCraft/build/ui-integration --parallel 3
ctest --test-dir VoxelCraft/build/ui-integration -LE graphics --output-on-failure
ctest --test-dir VoxelCraft/build/ui-integration -R '^Client.GpuSmoke$' --output-on-failure
VoxelCraft/build/ui-integration/bin/voxelcraft --no-mods --no-settings

cmake -S VoxelUI -B VoxelUI/out/native-geometry -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DVOXELUI_PREVIEW_SDL=OFF \
  -DVOXELUI_VULKAN_GEOMETRY=ON
cmake --build VoxelUI/out/native-geometry --parallel 3
ctest --test-dir VoxelUI/out/native-geometry --output-on-failure --no-tests=error
```

The installed consumer reproduction remains in VoxelUI's README; this
continuation used separate `out/continue-debug` and `out/continue-release`
builds, each with its own `prefix` and independent `consumer` directory.
