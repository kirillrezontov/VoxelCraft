# Milestone 1 implementation and verification

The previous environment blockers were closed by the 2026-11-01 unrestricted
rerun: **222/222 CPU/CLI, 12/12 graphics, 203/203 ASan/UBSan with leaks enabled,
and both extracted packages passed**. See [the gate recheck](modular-core-gate-recheck.json).
The original record below preserves the earlier run and its limits.

Recorded 2026-11-01. **The implementation is committed; the full milestone gate
remains open because this runner blocks networking, graphical playtests and
LeakSanitizer process inspection. Milestone 2 has not started.** This is not a
production-readiness claim. Machine-readable outcomes and the exact failing test
names are in [results](modularization-results.json).

## Starting state and source preservation

Started on clean `main` at `82d7f28570d3340b0633786b7725bd175cbd768d`, which merges
recovery. The expected `dc7480f`, `4feda33`, `53a157c`, `460a4f3` are ancestors;
`recovery/reproducible-baseline` exists at the later `5ab11af`. That later commit
also tracks the two originally untracked `.codex/tasks` notes. Their bytes are
unchanged. Existing `feature/modular-core` also points at `5ab11af`; a separate
`feature/modular-core-m1` was created from the verified merge rather than resetting
it. Main, recovery and the old feature branch were not modified.

All 54 recovered originals are tracked and still match the recovery manifest's
SHA-256 values, including the original missing 33-path subset. The historical
recovery results are not counted as this run's tests. VoxelUI stays clean on
`master` at `2e1cb61121d907e215facd89fc84b8a7b2e44798` with no source/history changes.

Implementation commit: `4f9c1b37a12dc808a037d63da3e42a3cdb3c93d9`.
The subsequent documentation/evidence commit adds no C++/CMake behavior changes.

## Changes and preserved behavior

The previous voxel library combined storage/rays/collision with generation,
streaming, lighting and meshing. Extracted `vc_generation`, `vc_lighting` and
`vc_meshing` now express those ownership/build boundaries. Existing `vc_storage`
gains the concrete `WorldPersistence` owner used by local play and server.
The renderer exposes CPU data dependencies and keeps graphics implementation
links private. Unused client CPU targets are excluded from server-only default
builds. CMake rejects project link cycles and forbidden core/server dependencies.
`vc_client_modules` selects a real compile-time meshing policy in both client
loops; the old aliases/default greedy algorithm remain available.

See [architecture](modular-architecture.md), [graph](module-dependency-graph.md),
[extension contract/tests](core-extension-points.md) and
[grounded session handoff](next-milestone-client-sessions.md).

| Behavior | Existing/new boundary and evidence | Current limit |
| --- | --- | --- |
| Terrain/storage/rays/collision | Original algorithms; generation fixtures, bounds/negative-coordinate tests pass | No new generation policy |
| Meshing/streaming/lighting | Original algorithms; geometry equivalence, retained edits, worker snapshots/tickets and both selected policies pass | GPU appearance not reverified |
| Player/inventory/crafting/survival/creatures | Existing gameplay/simulation transactions and CPU tests pass | No new sustained human playtest |
| Saves/reload | Existing validation/atomic format; service tests pass; an exact baseline binary writes a world/player payload, the new service edits/saves it, and the old reader reopens it | Real saves untouched; fixture in disposable directory |
| Networking/authority/profiles | Original protocol 4 and host logic retained; pure protocol/command tests pass | 21 socket-dependent tests fail with error 1 in both baseline and candidate |
| Existing data/Lua mods | Existing content/VM/rollback/migration/security CPU tests pass | No native SDK/loader implemented |
| Legacy screens/console/settings/HUD | Existing CPU editor/font/widget/UI/command/settings tests pass; same live UI remains active | All 12 SDL/Vulkan playtests blocked by this runner |
| Shutdown/ownership | Persistence joins before lock release; existing and new worker tests; TSan 16/16 | GPU shutdown validation unavailable |

No formats, content IDs, protocol, gameplay rules, UI layouts or shaders changed.
No VoxelUI dependency or new native execution/resource parsing surface was added.

## Executed verification

Fresh baseline source: independent local clone at `82d7f28`.
Fresh candidate source: shared clean-checkout helper's detached clone at `4f9c1b3`.
No untracked working files are copied to that clone. Evidence is retained locally
under `/tmp/vc-m1-44pkvig5`; isolated siblings are under
`/tmp/voxel-clean-checkout-x8fd6ecj`. These temporary artifacts may not persist.

GCC 16.2.1, Clang 22.1.8, CMake 4.4.3, Ninja, Linux x86-64, Ryzen 7 5800H.
Network restriction required explicit cached dependency-source overrides; fresh
outputs were built independently for every configuration. Git release revisions,
Lua archive SHA-256 and stb_image header SHA-256 were checked against pins.
No package installation, system-dependency substitution or fresh dependency
download is claimed.

| Check | Result in this run |
| --- | --- |
| Baseline GCC full Debug client/server/graphics targets | Passed, 543 Ninja steps |
| Baseline CPU/CLI | 190/211 passed; 21 failed at sandbox-denied TCP/socket operations |
| Candidate working-source full GCC / targeted tests | Build passed; 20/20 ownership/module/worker checks passed |
| Candidate full `culled` client | Build passed; CLI 6/6 passed; separate host consumers verify greedy/culled geometry |
| Clean committed GCC full client/server/graphics targets | Passed, 553 Ninja steps |
| Independent clean server-only configure/default build, tests OFF | Passed, 104 steps; no SDL/Vulkan discovery, no UI/lighting/meshing in link command |
| Clean committed CPU/CLI | 201/222 passed; all original outcomes match baseline; all 11 new tests pass |
| Clean Clang ASan/UBSan headless build | Passed, 165 steps |
| ASan/UBSan full suite, leaks disabled | 182/203 passed; same 21 socket failures; no sanitizer memory/UB findings observed |
| ASan/UBSan available subset, leaks disabled | 182/182 passed; the 21 known socket-dependent tests explicitly excluded |
| LeakSanitizer enabled probe | Five new tests pass assertions, then LSan fatally fails process inspection; blocked, not a leak-check pass |
| Clean Clang TSan headless build and selected tests | Build passed, 165 steps; **16/16 passed**, including all five new policy/persistence cases |
| SDL/Vulkan suite | 12 attempted, 12 failed on environment setup; no successful GPU execution/validation or new screenshots |
| Private graphics session / direct baseline smoke | D-Bus bind denied (`Operation not permitted`); direct SDL smoke reports `No available video device` |
| CPack clean client/server TGZ | Both generated |
| Extracted client package smoke | Passed assets/fonts/shaders/licenses/runtime/help checks; GPU option not requested |
| Extracted server package smoke | Assets/runtime/help checks reached server startup, then TCP listen denied; runtime save/restart blocked |
| Compiler dependency audit | 185 project inputs, all tracked; no original-workspace project sources outside explicit dependency cache |
| Tracking/helper audit | Zero required-input findings in both repositories and detached committed checkout |
| Windows/MSVC, GPU timings, heap profile, long soak/fuzz/human gameplay | Not executed |

### Reproduce the build commands

From the sibling workspace, the following parameterizes the actual source/cache
locations without installing absolute development paths in project configuration.
Use new directories for an independent rerun. The recorded run used the temporary
directories above. Dependency sources must match the pins recorded in results.

```bash
workspace="$PWD"
run=/tmp/vc-m1-44pkvig5
clean=/tmp/voxel-clean-checkout-x8fd6ecj/VoxelCraft
deps="$workspace/VoxelCraft/build/debug/_deps"
dep_args=()
for name in glm entt nlohmann_json lua stb_image googletest sdl3; do
    dep_args+=("-DFETCHCONTENT_SOURCE_DIR_${name^^}=$deps/$name-src")
done

# Baseline clone at 82d7f28; no source changes.
cmake -S "$run/baseline" -B "$run/baseline-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DVC_ENABLE_GPU_TESTS=ON -DVC_BUILD_SOAK_TOOLS=ON "${dep_args[@]}"
cmake --build "$run/baseline-build" --parallel 4
ctest --test-dir "$run/baseline-build" -LE graphics --output-on-failure --no-tests=error

# This helper creates detached committed siblings, audits them, and prints paths.
python3 .agents/skills/migration-testing/scripts/clean_checkout.py --workspace . --repo VoxelCraft
cmake -S "$clean" -B "$run/clean-client" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DVC_ENABLE_GPU_TESTS=ON -DVC_BUILD_SOAK_TOOLS=ON "${dep_args[@]}"
cmake --build "$run/clean-client" --parallel 4
ctest --test-dir "$run/clean-client" -LE graphics --output-on-failure --no-tests=error
ctest --test-dir "$run/clean-client" -L graphics --output-on-failure --no-tests=error

cmake -S "$clean" -B "$run/clean-server" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DVC_BUILD_CLIENT=OFF -DBUILD_TESTING=OFF "${dep_args[@]}"
cmake --build "$run/clean-server" --parallel 4

cmake -S "$clean" -B "$run/clean-asan" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=clang++ -DVC_BUILD_CLIENT=OFF \
    -DVC_SANITIZER=address-undefined "${dep_args[@]}"
cmake --build "$run/clean-asan" --parallel 3
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    ctest --test-dir "$run/clean-asan" --output-on-failure --no-tests=error
# Available subset uses -E with the 21 exact failure names in results.json,
# joined as ^(name1|name2|...)$ with literal dots escaped (CTest POSIX regex).
# The full run above retains the environment failures; it is not a green run.

cmake -S "$clean" -B "$run/clean-tsan" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=clang++ -DVC_BUILD_CLIENT=OFF -DVC_SANITIZER=thread "${dep_args[@]}"
cmake --build "$run/clean-tsan" --parallel 3
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir "$run/clean-tsan" \
    -R '^(TerrainWorker|MeshWorker|BackgroundMeshing|SnapshotWriter|WorldPersistence|CoreModules)\.' \
    --output-on-failure --no-tests=error

cpack --config "$run/clean-client/CPackConfig.cmake" -C Debug
python3 "$clean/tests/package_smoke.py" "$run/clean-client"/packages/*.tar.gz
python3 .agents/skills/migration-testing/scripts/audit_tracking.py --workspace .
git -C VoxelCraft diff --check
git -C VoxelUI diff --check
```

Working-source candidate commands used the same full-build flags with
`-S "$workspace/VoxelCraft" -B "$run/candidate-build"`. The alternative build
reconfigured that directory with `-DVC_CORE_MESHER=culled` and built `voxelcraft`.
The five-test LSan probe used `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and
`vc_tests --gtest_filter='WorldPersistence.*:CoreModules.*'`.

### CPU performance evidence

Both revisions used fresh GCC RelWithDebInfo, no sanitizers, the same dependency
cache, seed 42/generator 3/radius 2 and 150 chunks. One warmup per binary preceded
three alternating independent runs after builds completed. Configuration:
`cmake -S <source> -B <release-build> -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
-DVC_BUILD_CLIENT=OFF -DBUILD_TESTING=OFF -DVC_BUILD_SOAK_TOOLS=ON "${dep_args[@]}"`,
then `cmake --build <release-build> --target vc_mesh_benchmark --parallel 2` and
`<release-build>/bin/vc_mesh_benchmark`. No custom profiler/instrumentation was
added; perf/heaptrack/RenderDoc command-line tools were not available on PATH.

| Workload, total across 150 chunks | Baseline median CPU ms | Candidate median CPU ms | Baseline/candidate median wall ms |
| --- | ---: | ---: | ---: |
| Unit faces | 85.860 | 80.327 | 88.218 / 82.036 |
| Greedy faces | 61.431 | 58.105 | 61.454 / 58.128 |
| Synchronous queue, budget 2 | 60.750 | 57.086 | 60.759 / 57.095 |
| Background queue, snapshot budget 2 | 8.662 | 8.544 | 94.651 / 94.783 |

Geometry matches in every run: 150 chunks, reference 639,600 vertices/319,800
triangles; greedy/direct/background 185,836 vertices/92,918 triangles. Chunk
payload is still 65,536 bytes and Vertex is 84 bytes on this compiler. Raw runs
and per-sample ranges are [tracked](performance-runs/modular-core/) and in results.
No regression is apparent in these short matched measurements. Timing variation,
normal CPU frequency scheduling, three samples and background polling sleeps
prevent a speedup or broad frame-time guarantee. Background wall includes 1 ms
sleeps; these are CPU measurements, not GPU durations or allocation counts.
Static policy calls add no per-voxel dispatch; the scheduling containers and mesh
algorithms are unchanged. GPU, frame-time tails and heap usage remain unmeasured.

## Files and remaining gate

Implementation files: `CMakeLists.txt`, `client/{core_modules.hpp,main.cpp,
network_client.cpp}`, `cmake/ModuleBoundaries.cmake`,
`engine/voxel/{mesher.hpp,mesh_queue.cpp,mesh_queue.hpp,mesh_queue_impl.hpp,
mesh_worker.cpp,mesh_worker.hpp,mesh_worker_impl.hpp,persistence.cpp,persistence.hpp}`,
`server/host.cpp`, `tests/{core_module_tests.cpp,core_selection.cpp,persistence_tests.cpp}`,
`tests/cmake/{check_boundary.cmake,module_boundaries/CMakeLists.txt}`.
Documentation updates are README/ROADMAP, architecture/building guides, the five
milestone documents, machine-readable results and six raw benchmark logs.

All required new inputs are committed together with their consumers. No required
sources/resources remain untracked or ignored; the shared skills are outside both
Git histories. No user notes, saves, repository permissions or remote refs changed.

To close Milestone 1, rerun the full CPU/CLI suite with socket permission, all
12 strict SDL/Vulkan playtests on an accessible display/device/layer, extracted
server save/restart smoke, and ASan/UBSan with leak inspection permitted. Then
start the ordered [Milestone 2 session extraction](next-milestone-client-sessions.md).
Windows/MSVC execution remains a cross-platform limitation. Native Mod SDK,
VoxelUI Vulkan backend, SVG visual redesign and legacy UI migration are future
milestones and were not implemented or claimed here.
