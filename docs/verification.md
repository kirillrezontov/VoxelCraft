# v0.1 verification record

This is the completed v0.1 baseline record. Current development and test results
are in [v0.2 verification](verification-v02.md); version 0.2.0 is a development build.

Executed on 2026-10-28 in the Linux development environment, including the texture
atlas and validation follow-up. These are local results, not claims about
unexecuted GitHub Actions jobs or Windows behavior.

## Host and test tools

- Arch Linux, x86_64, kernel 7.2.11-arch1-1.
- CMake 4.4.4, Ninja, GCC 16.2.1, Clang 23.1.3.
- System SDL3 3.4.18, GLM 1.0.3, GoogleTest 1.18.0.
- Vulkan headers/loader 1.4.363; `glslc` and `spirv-val`.
- NVIDIA GeForce RTX 3050, driver 615.78.08, device API 1.4.351; application API 1.3.
- No system packages or global settings were changed.

The system still has no Khronos validation layer installed. A project-local layer
was downloaded from the official [Khronos nightly release](https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/tag/nightly)
and extracted under `build/tools/validation`. Its library and manifest are build
artifacts, not application dependencies or redistributed project assets.

Exact test-tool identity:

- Nightly date: 2026-10-27; manifest API version 1.4.366.
- Source commit: `b2b44c0d378a0f4b574d939080a7c5ea9c8f363d`.
- Archive: `Validation_Layers_Linux_64.tar.gz` (10,315,777 bytes).
- SHA-256: `680415f7b9db64fb9dfc77a56ebe9d5029d71dc420d88a23281cefa3162c017a`.

The archive digest was checked against the release API metadata before extraction.
Nightly is a moving release, so the commit and digest identify this specific run.

## Builds and tests actually executed

| Configuration | Dependency source | Result |
| --- | --- | --- |
| GCC Debug, `build/debug` | System packages | Full client compiled; 19/19 CPU and CLI CTest tests pass |
| Clang Debug, `build/clang` | System packages | Full client compiled; 19/19 CPU and CLI CTest tests pass |
| GCC Release, `build/pinned` | FetchContent: SDL 3.2.10, GLM 1.0.1, GoogleTest 1.16.0 | Full client compiled; 19/19 CPU/CLI tests plus 1/1 opt-in graphics test pass |
| Clang Debug, `build/sanitize`, client disabled | System packages; address/undefined/leak sanitizers | 17/17 CPU tests pass; no sanitizer findings |
| Textured vertex/fragment SPIR-V | `spirv-val --target-env vulkan1.3` | Both modules pass |
| Windows/MSVC | CI configuration only | Not compiled or run |

GCC and Clang project builds produced no compiler warnings. Pinned GLM 1.0.1
emits upstream CMake policy deprecation notices with CMake 4.4.4; builds succeed.
The earlier colored foundation also passed a Ninja Multi-Config Debug build and
15 tests; that generator was not rerun for the texture follow-up.

An explicit `--target voxelcraft` was previously built after moving its staged
assets away; both shader files were recreated and matched compiler outputs.
The asset build path remains shared by the textured client and IDE target.

The 17 core GoogleTest cases cover coordinates, chunk indexing and bounds,
registry failures, camera motion/projection, terrain determinism and seams,
meshing/winding/neighbor culling, RGBA image validation, atlas layout and UV
bounds, edge/corner gutters, per-face grass materials and fringe orientation.
Two additional CTest cases verify help and invalid bound rejection without a
window or GPU initialization.

LeakSanitizer cannot inspect processes inside the execution sandbox. The sanitizer
build/tests ran outside it with leak detection enabled. No ptrace settings were
changed.

## Graphics verification and synchronization correction

Initial colored geometry and terrain were rendered successfully without a layer.
The follow-up run enabled synchronization validation and found a WRITE_AFTER_READ
hazard in swapchain acquisition: the first layout transition used TOP_OF_PIPE as
its source stage while the acquire semaphore waited at COLOR_ATTACHMENT_OUTPUT.
The transition could therefore run before the presentation read was finished.

The barrier now uses COLOR_ATTACHMENT_OUTPUT for its source and destination,
chaining it to the acquire wait. Strict validation reports remain alive through
GPU teardown, and reported errors make the process fail instead of leaving a
successful-looking smoke test. `--require-validation` also rejects missing layers;
that rejection was executed and returned exit code 1 as expected.

After correction and atlas integration, the Debug client completed 120 frames
with `--smoke --require-validation`, including core and synchronization validation,
with **0 errors and 0 warnings, including shutdown**. The textured capture was
visually inspected.

The pinned-dependency Release client also passed the new CTest integration:

```sh
cmake -S . -B build/pinned -DVC_ENABLE_GPU_TESTS=ON
cmake --build build/pinned --parallel 4
VK_LAYER_PATH="$PWD/build/tools/validation" \
  ctest --test-dir build/pinned -L graphics --output-on-failure -V
```

CTest sets `VK_LAYER_VALIDATE_SYNC=1` and invokes `--smoke --require-validation`.
It executed 120 frames in approximately 2.85 seconds and reported **0 errors and
0 warnings through shutdown**. This verifies the Release build explicitly enables
the layer when requested. The default GPU-test option remains OFF; only this
separate test build has it enabled.

The sequence covers staged atlas upload, descriptor sampling, indexed terrain
drawing, scripted camera movement, requested resizes, mouse capture changes,
minimize/restore, swapchain recreation, image readback and clean shutdown.
The atlas/descriptors survive swapchain recreation. Default seed 12345 gives
147 loaded chunks, 97 uploaded meshes and 321,000 triangles. The compositor
adjusted requested window sizes, including actual extents 954×511, 947×505 and
1904×1019 in the Release test.

One textured Release startup measured generation ~27 ms, meshing ~130 ms and
mesh upload ~42 ms. These are individual observations, not performance targets.
The smoke sequence includes minimization and is not a steady-state FPS benchmark.
The Release screenshot is preserved as [voxelcraft-v01.png](voxelcraft-v01.png).
Earlier Release testing from `/tmp` also verified asset lookup independently of
the shell working directory.

## Windows preparation and remaining limits

This checkout has no Git remote and no Windows SDK/toolchain environment, so a
Windows CI run cannot be initiated here. The workflow's pinned LunarG SDK URL was
checked and returned HTTP 200. This does not verify Windows compilation/runtime.

Manual sustained keyboard/mouse playtesting, other GPU vendors and Windows remain
unverified. Current textures use nearest filtering without mipmaps, so distant
surfaces can shimmer. The world remains fixed, single-threaded and opaque;
streaming, culling, collision, block editing, persistence, mods and networking
are not implemented. Project licensing/release packaging remain open.

Next: execute Windows CI when a remote/runner is available, manually playtest
controls, and repeat graphics validation on other drivers. Later interaction work
belongs to v0.2; it was not started during this v0.1 follow-up.
