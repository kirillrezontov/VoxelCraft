# Milestone 1 implementation and verification

Work began on 2026-11-01 from `main` at `82d7f28`, which merges the recovered
baseline. Local `recovery/reproducible-baseline` exists at `5ab11af`; all four
recovery commits (`dc7480f`, `4feda33`, `53a157c`, `460a4f3`) are ancestors of
the starting commit. The only content added between recovery and that merge is
the two user-owned task notes. They are already tracked by `5ab11af` in this
workspace; this milestone leaves their bytes and history unchanged.

Both repositories initially had clean status. Another active Codex session then
created/checked out `feature/modular-core-m1` in the original worktree, so this
implementation uses **`feature/modular-core-foundation` in an isolated worktree**.
The existing `feature/modular-core` branch is untouched. No branch was reset,
no commit was merged into main, and nothing was pushed. VoxelUI remains at
`2e1cb61` on `master`, unchanged and clean. No VoxelUI dependency is introduced.

## Delivered implementation

* Engine mechanisms now build independently: `vc_core`, `vc_voxel`, `vc_generation`,
  `vc_streaming`, `vc_lighting`, `vc_meshing`, `vc_physics`, and CPU `vc_render_types`.
  Existing platform/files/persistence/network/gameplay/simulation targets remain.
* `ContentServices` in `vc_game_content` owns content/mod/item/recipe/mining state
  in local client, remote client and server. Palette restoration and rebuild order
  are explicit; stable registry object references replace duplicated ownership.
* Content fingerprinting moved out of packet codecs. Protocol no longer links Lua
  or modding. World storage no longer links generation, meshing or render algorithms.
  Vulkan and font implementation links are private to rendering. Configure-time
  checks reject cycles and forbidden engine/protocol/server dependencies.
* `BasicTerrainWorker<ChunkGenerator>` shares the real bounded worker algorithm
  with an independent layered-terrain implementation. An engine-only executable
  actually generates and meshes alternate terrain; the default game keeps its
  existing deterministic terrain versions and streaming/spawn semantics.
* Ownership and destruction rules are documented in [modular architecture](modular-architecture.md).
  The [Milestone 2 handoff](next-milestone-client-sessions.md) names actual APIs,
  capture/lifetime hazards, local/remote responsibilities and the next test gate.

Implementation commits:

1. `5ec7396` — CPU target extraction, content contracts, static worker policy,
   actual example and regressions.
2. `3e12bcc` — project-root-relative CMake source expressions (same resolved sources;
   avoids the tracking helper interpreting included-file inputs relative to `cmake/`).
3. Subsequent documentation/evidence commits contain this report, architecture,
   handoff, README/ROADMAP/build updates and machine-readable results.

## Executed checks

These are **current-run results**, distinct from the historical recovery report.
Local raw logs/JUnit/captures are retained under `/tmp/vc-foundation-1qjjhnqr`.
The initial baseline is an independent detached worktree of `82d7f28`, not the old
incomplete main tree. Dependency sources for the baseline/candidate/sanitizer builds
are explicit FetchContent overrides to existing pristine pinned source checkouts;
compiler outputs and caches are fresh. Git dependency revisions were inspected,
and their source checkouts were clean. The final clean build uses the default
FetchContent path and downloads dependencies afresh, with no overrides.

| Check | Result |
| --- | --- |
| All 54 recovered paths tracked, SHA-256 equal to recovery provenance at starting baseline | Passed, zero missing inputs |
| Baseline GCC full Debug client/server/graphics-tools build | Passed |
| Baseline CPU/CLI CTest | **211/211 passed** |
| Baseline SDL/Vulkan CTest | **12/12 passed** |
| Refactored GCC full Debug client/server/tools build | Passed |
| Refactored CPU/CLI CTest | **216/216 passed** |
| Refactored SDL/Vulkan CTest | **12/12 passed** |
| Clang ASan/UBSan headless build and full suite, leak detection + halt enabled | **197/197 passed** |
| Clang TSan worker/snapshot/core-extension build and selection | **14/14 passed** |
| Client OFF/testing OFF, build only `voxelcraft_server`, execute 30 ticks | Passed; no SDL/Vulkan discovery or UI/mesher compilation |
| Existing baseline survival world with example Lua/data mods, copied and reopened by candidate | Passed; revision 1 loaded, revision 2 saved; original preserved |
| Dependency-check fixtures | Acyclic accepted; cycle and forbidden transitive dependency rejected |
| Committed-source clean full build / CPU tests / input audit | In progress; final result recorded before milestone completion |
| Extracted client/server packages | Not executed yet |
| Windows/MSVC, sustained human gameplay, long soak/fuzz campaign | Not executed |

Environment: Linux x86_64, AMD Ryzen 7 5800H, GCC 16.2.1, Clang 22.1.8,
CMake 4.4.3, Ninja 1.13.2. Graphics ran on AMD Radeon Graphics (RADV RENOIR)
with the installed Khronos layer, strict synchronization validation, and a private
KWin virtual Wayland display. Tests cover console/Unicode/resize, local and real
remote executables, survival/death/crafting, creatures/environment, streaming,
inventory, persistence, SDL edits and renderer lifecycle. Successful explicit
renderer reports show zero validation errors/warnings through shutdown. These
are scripted interactions, not claims of sustained manual play or Windows success.

## Exact reproduction commands

From a clean committed VoxelCraft checkout, use a new build directory:

```sh
cmake -S . -B build/m1 -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DVC_ENABLE_GPU_TESTS=ON -DVC_BUILD_SOAK_TOOLS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/m1 --parallel 3
ctest --test-dir build/m1 -LE graphics --output-on-failure --no-tests=error
# Requires a usable isolated or desktop display, GPU and Khronos layer:
ctest --test-dir build/m1 -L graphics --output-on-failure --no-tests=error

cmake -S . -B build/m1-server -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DVC_BUILD_CLIENT=OFF -DBUILD_TESTING=OFF
cmake --build build/m1-server --target voxelcraft_server --parallel 3
build/m1-server/bin/voxelcraft-server --no-save --port 0 --radius 1 \
  --ticks 30 --no-mods --no-creatures --no-console

cmake --preset asan-ubsan
cmake --build --preset asan-ubsan --parallel 3
ctest --preset asan-ubsan --no-tests=error
cmake --preset tsan
cmake --build --preset tsan --parallel 3
ctest --preset tsan --no-tests=error

cmake -S . -B build/m1-perf -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DVC_BUILD_CLIENT=OFF -DBUILD_TESTING=OFF -DVC_BUILD_SOAK_TOOLS=ON
cmake --build build/m1-perf --target vc_mesh_benchmark --parallel 2
build/m1-perf/bin/vc_mesh_benchmark
```

Current-run sanitizer trees use the same preset variables in out-of-tree `asan`
and `tsan` directories. The TSan selection is
`^(CoreModule|TerrainWorker|MeshWorker|BackgroundMeshing|SnapshotWriter)\.`.
ASan options: `detect_leaks=1:halt_on_error=1`; UBSan:
`halt_on_error=1:print_stacktrace=1`; TSan: `halt_on_error=1`.

For tracking, from the parent workspace:

```sh
python3 .agents/skills/migration-testing/scripts/audit_tracking.py --workspace .
git -C VoxelCraft status --short
git -C VoxelCraft ls-files --others --exclude-standard
git -C VoxelUI status --short
git -C VoxelUI ls-files --others --exclude-standard
```

For an isolated worktree, supply a disposable audit workspace containing links
named `VoxelCraft` and `VoxelUI` to the worktree and unchanged sibling. This uses
the same helper without altering it or mixing histories. Review explicit font,
shader, CMake and new-source requirements plus compiler dependency paths; the
static helper cannot resolve every generated/variable-based input.

## Behavior preservation matrix

| Existing behavior | Changed boundary | Executed evidence |
| --- | --- | --- |
| Terrain v1–3, negatives, residency/retained edits | Separate generation/streaming/world targets; default algorithm unchanged | Terrain/resource/expansion/streaming/worker tests, Client.Streaming/GpuSmoke |
| Meshing, light, voxel collision and movement | Independent CPU targets; same implementations | Greedy/reference/lighting/interaction tests, SDL/environment/smoke playtests |
| Saves, snapshots, profiles, missing content | WorldStore unchanged; content rebuild after palette resolution | Storage/player/save/snapshot/server restart tests; baseline-produced save reopened |
| Inventory, crafting, survival, creatures | Content ownership consolidated; gameplay/simulation implementations unchanged | Inventory/survival/creature tests and graphics survival/network/creature tests |
| Network protocol, identity, roles, commands | Fingerprint composition separated from codecs; wire bytes unchanged | Protocol/fingerprint/server/auth/command/stress suites and real network executables |
| Lua/data/PNG packs and migration rollback | Existing runtime remains private to modding | Modding/material/resource tests, modded survival/reload and network tests |
| Legacy inventory/settings/pause/console/HUD | No replacement or deletion | CPU UI/font/text/editor tests and Console/Inventory graphics tests |

## Performance findings

The existing seed-42, 150-chunk benchmark ran with GCC RelWithDebInfo, no
sanitizers: three exploratory/warmup pairs, then seven measured pairs alternating
before/after order. Geometry counts agree in every run: reference 639,600 vertices /
319,800 triangles; greedy/synchronous/background 185,836 vertices / 92,918 triangles.
Chunk cell payload remains 65,536 bytes; Vertex remains 84 bytes.

| Workload | Baseline median main CPU, ms | Candidate median main CPU, ms |
| --- | --- | --- |
| Reference unit faces | 73.980 | 76.197 |
| Greedy opaque faces | 55.249 | 55.170 |
| Synchronous budget 2 | 54.663 | 54.691 |
| Background snapshot budget 2 | 7.851 | 7.822 |

Raw samples/ranges are in [machine-readable results](modular-core-results.json).
Ranges overlap; shared-machine clocks/load and small samples preclude a speedup
or regression guarantee. The default mesher/generator computations are unchanged;
worker dispatch remains a direct template call, with one added position check per
completed chunk. No new per-voxel allocation or dispatch was introduced. This is
CPU meshing/snapshot timing, not frame/GPU/generation timing or a full allocation
profile. Background wall time includes 1 ms polling sleeps. Further controlled
profiling belongs to later performance work, not an unmeasured optimization claim.

## Files and remaining limits

Code changes are in CMake/engine module checks, core resource IDs, terrain-worker
headers/implementation, content services/fingerprinting, the three composition
sites, explicit mod includes in three protocol/network tests, and the new engine
example/content/worker regressions. No renderer implementation, shader, storage
format, network encoding, gameplay algorithm, legacy UI source or VoxelUI file is
replaced. Architecture/build/README/ROADMAP and this evidence/handoff are updated.
The complete path list is available from `git diff --name-only 82d7f28..HEAD`.

Known limits: the public include root is still the repository rather than an SDK;
link guards do not analyze arbitrary conditional expressions/external targets;
spawn validation still couples gameplay to generation/streaming; the static seam
does not replace a whole saved-world generator; application and session loops
remain combined. Windows/MSVC and sustained manual gameplay remain unverified.

Next: extract local session ownership from `client/main.cpp`, then the remote loop
from `client/network_client.cpp`, under a persistent application/window/renderer
and a small ScreenManager. Preserve save/error/input behavior and test repeated
enter/leave/reconnect/cancel cycles in one process. The detailed sequence and risks
are in the handoff. **Native Mod SDK, VoxelUI Vulkan backend, original SVG redesign
and legacy UI migration/removal are future milestones, not delivered features.**
