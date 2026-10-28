# Building

## Dependency policy

CMake FetchContent pins SDL to `release-3.2.10`, GLM to `1.0.1`, and GoogleTest to
`v1.16.0`. EnTT uses exact package 3.14.0 or FetchContent tag `v3.14.0`
(commit `4a2d1a8541228a90e02a873dba6a980506c42c03`). Sources and build products stay under `build/`. SDL is linked statically
in this mode; Vulkan is linked to the system loader. Git tags select exact release
versions; updates are explicit changes to CMakeLists.txt. The Vulkan SDK/driver and
platform development libraries remain host prerequisites. EnTT is used only by dynamic actor simulation; ImGui and Lua are not yet dependencies.

`VC_USE_SYSTEM_DEPS=ON` opts into installed CMake packages: SDL3 >=3.2, GLM >=1.0,
GoogleTest >=1.14. This convenient offline path is not version-reproducible; record
the host versions when reporting results. EnTT still fetches if its exact package
is absent; use an installed 3.14.0 package or `FETCHCONTENT_SOURCE_DIR_ENTT` for an
offline source checkout. `BUILD_TESTING=OFF` omits GoogleTest.
`VC_BUILD_CLIENT=OFF` omits SDL and Vulkan entirely for CPU-only builds.

## Linux prerequisites

Install packages only with the machine owner's permission. On Debian/Ubuntu a
typical prerequisite set is `build-essential cmake ninja-build git libvulkan-dev
glslc libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev
libxss-dev libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev`.
SDL's CMake checks describe which desktop backends were detected. At least one
usable X11 or Wayland backend is required. Install Khronos validation layers for
Debug API validation. Distribution SDK versions must support Vulkan 1.3.

```sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug
./build/debug/bin/voxelcraft

cmake --preset release
cmake --build --preset release --parallel 4
ctest --preset release
```

Clang and a headless test-only build:

```sh
cmake -S . -B build/clang -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DVC_USE_SYSTEM_DEPS=ON
cmake --build build/clang --parallel 4
ctest --test-dir build/clang --output-on-failure

cmake -S . -B build/core -G Ninja -DVC_BUILD_CLIENT=OFF
cmake --build build/core --parallel 4
ctest --test-dir build/core --output-on-failure
```

For sanitizers, configure Clang with `-DCMAKE_CXX_FLAGS="-fsanitize=address,undefined
-fno-omit-frame-pointer"` and `-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined`.
LeakSanitizer may require running outside a restricted process sandbox.

## Windows

Use the MSVC x64 commands in the README. FetchContent builds SDL and GoogleTest
from the pinned versions; GoogleTest uses the shared MSVC CRT to match the client.
Vulkan's SDK path is discovered through `VULKAN_SDK`. A compatible graphics driver
is needed at runtime; the SDK alone does not provide one. The Windows workflow
uses LunarG SDK 1.3.296.0 and its documented Qt unattended installer command with
`copy_only=1`, avoiding registry changes on the ephemeral runner. See
[LunarG's installation instructions](https://vulkan.lunarg.com/doc/view/1.3.296.0/windows/getting_started.html).

Installed shared dependencies on Windows have their CMake-declared runtime DLLs
copied beside the executable. Fetched SDL is static. Driver-provided Vulkan
runtime and MSVC runtime availability remain runtime prerequisites. Distributable
release packaging is not part of v0.1.

## Assets and graphics testing

`glslc --target-env=vulkan1.3` compiles shaders. The default build always synchronizes
SPIR-V into `bin/assets/shaders` (or `bin/Release/assets/shaders` with MSVC).
Both the default build and an explicit `--target voxelcraft` synchronize shaders,
including shader-only changes. Move the entire executable
directory together; shader lookup uses SDL_GetBasePath rather than the shell CWD.

```sh
./build/debug/bin/voxelcraft --smoke --screenshot /tmp/voxelcraft.ppm
```

PPM readback requires surface `TRANSFER_SRC` capability. A display-less machine
can still build the full client and run CTest; the graphics smoke test is opt-in
and is not mixed into GPU-independent unit tests. CI currently compiles the client
and runs CPU/CLI tests, with no GPU runtime verification.

To require validation instead of skipping missing layers (also works in Release):

```sh
VK_LAYER_VALIDATE_SYNC=1 ./build/debug/bin/voxelcraft --smoke --require-validation
```

For CTest integration, use a separate build with `-DVC_ENABLE_GPU_TESTS=ON` and run
`ctest --test-dir build/graphics -L graphics --output-on-failure` after building.
`Client.GpuSmoke` requires a desktop, a compatible GPU, the Khronos validation
layer and screenshot readback. It enables synchronization validation, has a
60-second timeout, and fails on any reported validation errors, including shutdown.
In v0.2 it also checks jump/landing, removal/restoration across a chunk seam,
and adding/removing an isolated mesh. Default seed/bounds exercise all phases;
unusual bounds may have no empty chunk for the isolated-mesh phase or insufficient
terrain depth for the scripted destruction/restoration. Use default bounds for
the full interactive smoke sequence.
Default builds leave this option OFF. See [verification](verification.md) for the
checks actually executed here.

The layer can be installed in a project-local directory without modifying the
system. Put its library and JSON manifest together and set `VK_LAYER_PATH` to that
directory for the test process. This session used the official
[Khronos nightly binaries](https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/tag/nightly)
under `build/tools/validation`, verified against the published SHA-256. The
downloaded layer is a test tool, not a new application dependency. Nightly is a
moving release; the exact commit and digest used are recorded in verification.md.
