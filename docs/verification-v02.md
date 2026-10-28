# v0.2 development verification

The v0.1 implementation is complete under SPEC §35; its baseline evidence is
retained in [verification.md](verification.md). This record covers the initial
v0.2 interactive slice, version 0.2.0. It is a development build, not a claim that
v0.2 release stabilization or Windows testing is finished.

## Executed builds and checks

All checks below ran on the same Linux/RTX 3050 host and toolchain documented in
the v0.1 record. No system packages were installed.

| Build/check | Result |
| --- | --- |
| GCC Debug, installed dependencies, `build/debug` | Full client builds; 33/33 CPU/CLI CTest tests pass |
| Clang Debug, installed dependencies, `build/clang` | Full client builds; 33/33 CPU/CLI CTest tests pass |
| GCC Release, pinned dependencies, `build/pinned` | Full client builds; 33/33 CPU/CLI tests and 1/1 graphics test pass |
| Clang address/undefined/leak sanitizers, client disabled, `build/sanitize` | 31/31 CPU tests pass; no sanitizer findings |
| Updated vertex/fragment shaders, `spirv-val --target-env vulkan1.3` | Both pass |
| Windows compilation/runtime | Prepared, not executed |

The project builds with normal warnings enabled. Clang initially reported signed
array-index conversions in DDA; these were corrected by using GLM's integer-indexed
vectors. The final incremental Clang build emitted no warnings. Pinned GLM's
existing upstream CMake deprecation notices remain unchanged.

The additional tests cover six ray directions and normals, reach rejection,
negative seams, extreme direction magnitudes, starting inside a cell, integer
planes, diagonal corner ties, invalid inputs, dirty-neighbor sets, boundary-face
regeneration, placement/destruction, player overlap and resident/height limits.
Collision tests cover high-speed sweeps through walls, sliding, floor/ceiling,
negative coordinates, unavailable chunks, independent solidity, gravity,
grounded jumping, landing, frame-rate independence and unsafe flight exit.
The normal test suite still requires no display or graphics initialization.

## Hardware graphics tests

The Debug interactive client completed 120 frames with strict core and
synchronization validation, including two actual game edits at a negative chunk
seam. Each rebuilt three affected meshes. The selected block outline and crosshair
were captured and visually inspected. Result: **0 validation errors and 0 warnings,
including resource destruction**.

The extended Release CTest then passed with:

```sh
cmake --build build/pinned --parallel 4
VK_LAYER_PATH="$PWD/build/tools/validation" \
  ctest --test-dir build/pinned -L graphics --output-on-failure -V
```

The build has `VC_ENABLE_GPU_TESTS=ON`; CTest sets `VK_LAYER_VALIDATE_SYNC=1` and
invokes `--smoke --require-validation`. The checksum-verified local Khronos layer
is the same one identified in the v0.1 record.

The final Release sequence completed in approximately 2.86 seconds and checked:

- A grounded jump raises the player at least one unit and lands before the edit phase.
- DDA selects a surface cell at negative X, with Z on a chunk boundary.
- Destruction and placement restore that same cell and its original material.
- Each seam edit replaces three chunk meshes while other GPU meshes remain present.
- Creating one isolated block in a previously empty chunk adds one GPU mesh.
- Removing that isolated block removes the GPU mesh and restores the original count.
- Selection/crosshair rendering, resize, minimize/restore, capture changes and readback.
- Clean shutdown with **0 validation errors and 0 warnings**.

The default world contained 147 loaded chunks and 97 terrain meshes, with 321,000
terrain triangles. The temporary isolated chunk made 98 meshes and returned to
97; the log showed two three-chunk rebuilds and two one-chunk rebuilds. Terrain
triangle diagnostics exclude the selection/HUD geometry.

The Release [selection screenshot](voxelcraft-v02.png) shows a gold outline and
white crosshair after the seam edit is restored. The smoke test drives game input
and editing directly; it does not synthesize physical mouse/keyboard events.
Sustained manual input testing is not claimed.

## Remaining work and limits

v0.2 is active, with its initial interaction slice implemented. Manual walking,
jumping, flight and building playtests, control/collision polish and Windows CI
execution remain outstanding. The checkout has no Git remote/Windows SDK; no
Windows compilation or runtime success is claimed.

Edits are in memory and disappear on exit. World residency is fixed; missing
chunks are collision barriers. There is no automatic stepping, crouching,
held-click repetition, inventory, save system, streaming or multiplayer. Mesh
updates synchronously remesh whole affected chunks and wait for GPU idle, so edits
may hitch. Mipmaps remain absent and distant atlas sampling can shimmer.
These limitations are recorded in the active [roadmap](../ROADMAP.md).
