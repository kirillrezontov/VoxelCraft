# Building

Milestone 1 splits CPU generation, lighting and meshing from voxel storage and
checks project module links at configure time. See the
[dependency graph](module-dependency-graph.md) and
[executed verification](modularization-progress.md). `VC_CORE_MESHER=greedy`
(default) or `culled` selects the actual client meshing policy at compile time;
see [extension points](core-extension-points.md). The dedicated server has no
meshing/lighting/UI link dependency. With `VC_BUILD_CLIENT=OFF` and
`BUILD_TESTING=OFF`, unused client CPU targets are excluded from the default build.

## Dependency policy

CMake FetchContent pins SDL to `release-3.2.10`, GLM to `1.0.1`, and GoogleTest to
`v1.16.0`. EnTT uses exact package 3.14.0 or FetchContent tag `v3.14.0`
(commit `4a2d1a8541228a90e02a873dba6a980506c42c03`). Sources and build products stay under `build/`. SDL is linked statically
in this mode; Vulkan is linked to the system loader. Git tags select exact release
versions; updates are explicit changes to CMakeLists.txt. The Vulkan SDK/driver and
platform development libraries remain host prerequisites. EnTT is used only by dynamic actor simulation; ImGui is not a dependency. Lua 5.4.9 is fetched from its upstream tarball,
verified with SHA-256, and compiled as C++ for callback RAII error handling.
nlohmann/json is pinned to v3.12.0 for mod manifests and definitions.
PNG tiles use stb_image from commit
`013ac3beddff3dbffafd5177e7972067cd2b5083`, fetched as a single header and checked
against SHA-256 `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`.
Its [upstream header](https://github.com/nothings/stb/blob/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h)
includes its MIT/public-domain license. Only PNG support is compiled, with a
custom bounded allocator. No system PNG package is needed.

`VC_USE_SYSTEM_DEPS=ON` opts into installed CMake packages: SDL3 >=3.2, GLM >=1.0,
GoogleTest >=1.14 and nlohmann/json exactly 3.12.0. This convenient offline path is not version-reproducible; record
the host versions when reporting results. EnTT still fetches if its exact package
is absent; use an installed 3.14.0 package or `FETCHCONTENT_SOURCE_DIR_ENTT` for an
offline source checkout. `BUILD_TESTING=OFF` omits GoogleTest.
`VC_BUILD_CLIENT=OFF` omits SDL and Vulkan entirely for CPU-only builds.
The `server` preset builds the dedicated server and CPU tests with that setting;
`VC_BUILD_SERVER=OFF` omits the standalone executable when only a client is wanted.

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

cmake --preset server
cmake --build --preset server --parallel 4
ctest --preset server
./build/server/bin/voxelcraft-server --no-save --ticks 90
```

For isolated CPU sanitizers use separate build trees:

```sh
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --parallel 4
ctest --preset asan-ubsan
cmake --preset tsan
cmake --build --preset tsan --parallel 4
ctest --preset tsan
```

`VC_SANITIZER=address-undefined` instruments ASan/UBSan; `thread` instruments TSan.
The TSan test preset selects terrain/mesh/snapshot ownership and shutdown tests.
The options cannot combine incompatible runtimes. Presets select Clang and omit
SDL/Vulkan. A runner must permit the sanitizer's address-space layout and leak
inspection; failure to initialize a runtime is a verification blocker, not a pass.

When Python 3 is available, CTest also exercises the real server process and,
in the graphics suite, two actual client executables. Python is optional and is
not linked to the game. For a longer eight-player transport/edit/reconnect soak:

```sh
cmake --preset server -DVC_BUILD_SOAK_TOOLS=ON
cmake --build --preset server --parallel 4
./build/server/bin/vc_network_soak 600
```

The tool uses an isolated temporary world and ephemeral loopback port, drives
eight real TCP sessions at 60 Hz, edits lamps, churns names, saves and verifies
profile restoration. Its duration accepts 10..14400 seconds; it is outside the
ordinary test suite.

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
runtime remains a driver prerequisite. CPack includes CMake-detected runtime DLLs
and MSVC redistributable libraries. `/utf-8` plus an embedded UTF-8 process manifest
preserves Unicode command-line paths on Windows 10 1903+. Local Linux results do
not establish Windows build/runtime success.

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

For CTest integration, use `cmake --preset graphics`,
`cmake --build --preset graphics --parallel 4`, and `ctest --preset graphics`.
This keeps the display-dependent suite in a separate build. Alternatively, use
a separate build with `-DVC_ENABLE_GPU_TESTS=ON` and run
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


Scripted input playtests need stable desktop focus. An isolated compositor can
avoid interference from active desktop use. The 2026-10-29 continuation used the
installed KWin virtual backend on a private D-Bus/Wayland session for seven
interactive tests. The initial automatic minimize/restore smoke required X11. The timed continuation
fixed restore by also requesting SDL window activation, and all eight strict
tests now pass on the isolated Wayland display. Activation remains subject to
desktop policy; the game continues to use the same Vulkan renderer. See the
[continuation verification](development-session-2026-10-29.md).


## v0.8.1 release archives

Target CMake 3.25+, GCC 13+/Clang 18+ with a C++23 standard library, or Visual
Studio 2022 MSVC x64. Locally exercised versions are GCC 16.2.1 and Clang 22.1.8;
MSVC and CI's older Linux toolchains remain acceptance checks until CI executes.
Debug, Release and RelWithDebInfo presets share the same target graph. No Vulkan
or SDL dependency is discovered when `VC_BUILD_CLIENT=OFF`.

```sh
cmake --preset release -DVC_BUILD_SOAK_TOOLS=ON
cmake --build --preset release --parallel 4
ctest --preset release
cpack --config build/release/CPackConfig.cmake -C Release
python3 tests/package_smoke.py build/release/packages/*.tar.gz
# On a graphics host, also check the extracted client:
python3 tests/package_smoke.py build/release/packages/*.tar.gz --gpu
```

MSVC uses `cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64`,
`cmake --build build/windows --config Release --parallel 4`,
`ctest --test-dir build/windows -C Release --output-on-failure`, then
`cpack --config build/windows/CPackConfig.cmake -C Release`.
Pass the two generated ZIP paths to `python tests/package_smoke.py`.
CI builds and checks both component archives without requesting a graphics device.
The optional manually dispatched Vulkan workflow requires a maintained runner
labeled `self-hosted`, `linux`, `vulkan` with a display and validation layer.

Archives live in `build/<preset>/packages/` and contain exactly one executable,
required detected redistributables, notices, launch instructions, documentation,
and optional demo assets. Client shaders are compiled and installed automatically.
Core textures/content live in the executable. Client and server both work outside
the source working directory; the package test also exercises Unicode world paths.
No compiler, SDK, build caches, GoogleTest, object files or development libraries
are packaged. C++ runtime libraries are bundled on Linux; glibc, desktop libraries
and the Vulkan driver/loader remain OS prerequisites. Linux compatibility is
bounded by the build host's glibc; CI's Ubuntu 24.04 packages should be verified
on that baseline before distribution. System-dependency builds must supply their
matching license notices; the release configuration uses pinned sources.

A fresh checkout needs network access to upstream pinned sources. Offline builds
can explicitly reuse existing source checkouts via CMake's
`FETCHCONTENT_SOURCE_DIR_GLM`, `_ENTT`, `_NLOHMANN_JSON`, `_LUA`, `_STB_IMAGE`,
`_SDL3`, and `_GOOGLETEST` cache variables. Each points to its source directory,
not another build tree. This preserves independent compiler/configuration outputs.
Do not silently substitute unpinned installed libraries. The stabilization session
used these overrides after a fresh GitHub clone stalled; see its report.

`vc_mesh_benchmark` prints paired reference/greedy counts and CPU times for a fixed
seed and 150 chunks, then compares synchronous and owned background scheduling.
It excludes generation, lighting and GPU time and includes 1 ms polling sleeps in
background wall time. Normal client title/log diagnostics expose frame/FPS,
update/draw/upload CPU times (draw includes fence/present waits), draw calls,
triangles, loaded/visible chunks, pending jobs, generation/mesh timings, cached edit
bytes, save duration and connected network byte counters. Logging is periodic,
not per-frame. No GPU timer-query measurements are claimed.

## v0.8.2 fonts, console and packages

The pinned stb_truetype header is vendored with its upstream license. CMake embeds
bundled DejaVu Sans/Mono font bytes once; it needs no FreeType development package,
OS font installation or runtime download. MSVC keeps `/utf-8` and the existing
UTF-8 process manifest. Windows stdin uses bounded native console events/pipe
reads; POSIX stdin uses zero-timeout polling. Headless code links neither SDL nor
Vulkan. New UI/parser tests also run without a display.

`cmake --build --preset release`, `ctest --preset release`, then
`cpack --config build/release/CPackConfig.cmake -C Release` creates separate 0.8.2-rc1
client/server archives. Run `python3 tests/package_smoke.py --gpu
build/release/packages/*.tar.gz` on a Vulkan desktop to validate extracted archives
from an unrelated directory. Client archives include `assets/fonts`, shader SPIR-V,
DejaVu/stb notices and UI/command docs; fonts remain embedded for reliable lookup.
`cmake --build --preset graphics` and `ctest --preset graphics` include the
Unicode/console/command test with resize requests for 1280×720, 1920×1080 and
2560×1440. Compositors may constrain actual sizes; the test reports both.
CPU layout/font checks validate those exact viewports independently.
See [the implementation record](v0.8.2-implementation.md) for executed checks;
Windows CI configuration is maintained but local Windows execution is unverified.
