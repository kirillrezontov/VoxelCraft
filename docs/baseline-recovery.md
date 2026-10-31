# Reproducible baseline recovery

Recovery started from `692aef6066641393fd3ce17c2890b94177544ab6` (v0.8.2) on 2026-10-31. The branch is `recovery/reproducible-baseline`; `main` and its prior commits were preserved. This change recovers repository inputs; it does not modularize the engine or integrate VoxelUI.

## Root cause and provenance

The earlier skill report recorded 40 missing-input edges across 33 distinct paths, rather than 33 missing algorithms. Every one of those paths existed in the initial working directory as an untracked file. None existed in any of the four reachable original commits, including the remote-tracking branch. The repository was not shallow; local branch/ref/reflog, stash, worktree and `git fsck --full --unreachable --no-reflogs` inspection found no additional recoverable Git snapshot. The 33 paths were not ignored.

Tracked consumers already referenced absent files: v0.8.1 (`9764073`) added CMake references to file synchronization and packaging; v0.8.2 (`692aef6`) added text/UI/command consumers. The evidence establishes that new files were omitted while changes to existing files were committed. This is consistent with staging tracked files only, but the original staging command is not recorded and cannot be established. Existing local builds succeeded using those untracked originals and existing dependency/build directories; they did not prove a complete committed checkout.

All recovered implementations are the pre-existing local originals, preserved byte for byte. **No implementation had to be invented or reconstructed.** Initial untracked files were archived outside the repository before staging. [The provenance manifest](baseline-recovery.json) records all 54 recovered paths, SHA-256 hashes at recovery, their recovery commits, and the exact original 33-path subset. These hashes are historical provenance, not a requirement to freeze future source edits.

## Exact original 33 paths

All paths below are relative to VoxelCraft. Origin for every row: initial local untracked original; absent from the original reachable Git trees.

| Path | Responsibility |
| --- | --- |
| `assets/fonts/DejaVuSans.ttf` | Embedded proportional Latin/Cyrillic font. |
| `assets/fonts/DejaVuSansMono.ttf` | Embedded monospace console font. |
| `cmake/Packaging.cmake` | Client/server CPack components, runtime libraries, licenses and resources. |
| `cmake/font_data.cpp.in` | Generated translation unit embedding both font byte arrays. |
| `engine/core/content_presentation.cpp` | Content metadata/category validation. |
| `engine/core/content_presentation.hpp` | Content presentation values and category API. |
| `engine/core/utf8.cpp` | Bounded UTF-8 decoding, validation, case conversion and safe truncation. |
| `engine/core/utf8.hpp` | UTF-8 scalar and byte-boundary interfaces. |
| `engine/core/work_metrics.hpp` | Value snapshots for worker CPU timing/completion counters. |
| `engine/platform/console_input.cpp` | Bounded nonblocking POSIX/Windows server stdin handling. |
| `engine/platform/console_input.hpp` | Console input and fragmented-line buffer ownership. |
| `engine/platform/file_sync.cpp` | Durable file/directory synchronization and atomic publication. |
| `engine/platform/file_sync.hpp` | Persistence synchronization/publication interfaces. |
| `engine/ui/font.cpp` | Bundled-font rasterization, bounded glyph atlas and clipped text meshes. |
| `engine/ui/font.hpp` | Font atlas ownership, metrics and glyph mesh API. |
| `engine/ui/text_edit.cpp` | Bounded UTF-8 editing, cursor/selection and clipboard insertion. |
| `engine/ui/text_edit.hpp` | Text input values and editor state interface. |
| `engine/ui/widgets.cpp` | CPU widget focus, interaction and scrolling. |
| `engine/ui/widgets.hpp` | Theme, rectangles and widget state/interfaces. |
| `game/commands/commands.cpp` | Command registry, permission checks, completion and real gameplay callbacks. |
| `game/commands/commands.hpp` | Typed command definitions, results, permissions and trusted service context. |
| `game/commands/parser.cpp` | Bounded command tokenization and finite numeric/relative coordinates. |
| `game/commands/permissions.cpp` | Validated, atomic persistence of authenticated profile roles. |
| `game/commands/permissions.hpp` | Permission store ownership and access API. |
| `game/network/identity.cpp` | Bounded credential-file parsing and token validation/comparison. |
| `game/network/identity.hpp` | Profile credential map and identity functions. |
| `game/ui/console.cpp` | In-game console editing/history/completion/output. |
| `game/ui/console.hpp` | Console state and command/chat interfaces. |
| `game/ui/sandbox_views.cpp` | Legacy HUD, inventory, crafting, settings, pause and console geometry. |
| `tests/command_tests.cpp` | Parser, commands, permissions, console input and inventory regressions. |
| `tests/console_playtest.cpp` | Real SDL/Vulkan console, Unicode, commands and resize playtest. |
| `tests/greedy_mesh_tests.cpp` | Greedy/reference surface equivalence, winding, seams and coordinate limits. |
| `tests/ui_core_tests.cpp` | Headless UTF-8/editor/font/widget/UI behavior and geometry regressions. |

## Additional input closure

The static initial audit could not descend into missing source files or resolve every platform, packaging and runtime input. Recovery therefore includes the following 21 additional original files, rather than stopping at the initial 33:

- `assets/fonts/LICENSE-DejaVu.txt`
- `third_party/stb/README.md`
- `third_party/stb/stb_truetype.h`
- `.github/workflows/vulkan-smoke.yml`
- `cmake/windows-utf8.manifest`
- `docs/decisions/0014-stabilization.md`
- `docs/release-v0.8.1-test-plan.md`
- `docs/v0.8.1-release-notes.md`
- `docs/v0.8.1-stabilization.md`
- `docs/v0.8.2-commands.md`
- `docs/v0.8.2-implementation.md`
- `docs/v0.8.2-release-notes.md`
- `docs/v0.8.2-ui-design.md`
- `docs/voxelcraft-v082-console.png`
- `docs/voxelcraft-v082-inventory.png`
- `packaging/README.txt`
- `packaging/licenses/GCC-Runtime-Library-Exception.txt`
- `packaging/licenses/GPL-3.0.txt`
- `packaging/licenses/THIRD-PARTY.txt`
- `tests/network_stress.py`
- `tests/package_smoke.py`

This includes the Windows UTF-8 process manifest, font/vendor licenses, the pinned stb_truetype header, Python server/package tests, CPack-installed documentation and linked release captures. The optional Vulkan workflow was already documented by the tracked build guide. Two original `.codex/tasks/*.md` notes remain untracked and unchanged; they are not build, test or package inputs.

The font files match `/usr/share/fonts/TTF/DejaVuSans.ttf` and `DejaVuSansMono.ttf` byte for byte in the recovery environment. The preserved stb header matches its local provenance README hash `a34d8d536ce7c11b9163ab2d524721c1f4df1452cce6595c4f11d3048384f925` and declared upstream commit `013ac3beddff3dbffafd5177e7972067cd2b5083`. Original license bytes, including their existing whitespace, are preserved. Two `git diff --check` warnings are confined to the original DejaVu and GCC exception notices; recovered implementation/test code passes that check.

## Compatibility and trust boundaries

The recovery preserves the complete CMake target graph, test registrations, shader build, protocol 4, player-payload version 10 with prior-version readers, world/region format 1 and mod API 1. It does not change gameplay, authoritative networking, save schemas, Lua hooks or resource limits. Compatibility evidence is the existing gameplay/inventory, protocol/server/restart/identity, storage/snapshot/save-state and mod rollback/migration/resource tests, rather than replacement stubs.

Security review focused on the recovered boundaries: strict UTF-8 and command/numeric limits, command role checks, bounded credential files, atomic role/save writes, bounded stdin and trusted bundled fonts. The original Lua budgets, transactional script effects, PNG limits, server authority and private-credential handling remain unchanged. The vendored font rasterizer is used for bundled fonts, not claimed as a hostile-font sandbox. Tests use temporary worlds and ephemeral loopback servers; no real saved games or external native modules are loaded.

## Verification

Source for the executed clean builds is an independent clone of recovery commit `53a157c4b66a0317cfc5efd637fbe3b237f3f0ba`, containing all 54 recovered originals. Build directories and evidence are under `/tmp/vc-baseline-recovery-9n1wafcl`, outside both repositories. The first full configuration uses the default pinned FetchContent path and downloads dependencies afresh, with no source overrides or old VoxelCraft cache.


| Check | Actual result |
| --- | --- |
| GCC 16.2.1 full client/server Debug configure/build | Passed, default pinned dependencies freshly downloaded, all 543 Ninja steps |
| CPU/CLI CTest | **211/211 passed**, no skipped tests |
| SDL/Vulkan CTest | **12/12 passed**, private KWin Wayland session, strict core/synchronization validation |
| Clang 22.1.8 ASan/UBSan headless configure/build | Passed, separate 155-step build; leak detection and halt-on-error enabled |
| ASan/UBSan CTest | **192/192 passed**, no skipped tests |
| Clang 22.1.8 TSan headless configure/build | Passed, separate 155-step build |
| Existing worker/snapshot TSan test selection | **11/11 passed**, no skipped tests |
| CPack client and server TGZ archives | Both generated; both extracted-archive smoke checks passed |
| Eight-player network verification tool | 10 seconds passed; 4,656 commands, 39 placements/removals, eight profiles saved/reopened; no timed rejoin cycle occurred |
| Reference/greedy and synchronous/background meshing tool | Passed geometry equivalence; timings are not promoted to performance results |
| Compiler dependency/source audit | 177 project inputs, all Git-tracked; no path to the original working directory |
| Original recovery bytes | All 54 files match initial SHA-256 hashes and the independent committed checkout |
| Shared required-file audit | Zero findings for VoxelCraft after recovery |

The machine-readable [verification results](baseline-recovery-results.json) preserve tool versions, exact test counts, environment, dependency revisions and the distinction between executed and skipped work. Raw configure/build/CTest logs, JUnit XML, package archives and graphics captures are retained in the temporary evidence directory named above. They are local evidence, not new runtime dependencies.

The sanitizer builds reuse only pristine third-party source directories freshly downloaded by the clean Debug configuration; they compile independently and do not use original VoxelCraft source/build caches. Their headless configuration is the repository's existing sanitizer strategy, not a change to the baseline target graph. TSan uses the existing preset's worker/snapshot filter. The full Debug suite additionally covers SDL/client behavior.

Hardware verification used RADV RENOIR / Mesa 26.2.2 and the installed Khronos 1.4.357 validation layer. The virtual compositor has its own D-Bus, runtime/config/data/cache directories and does not replace the desktop compositor. GPU tests cover console/Unicode/resize, network clients/survival, creatures/environment, survival/crafting/mod content, streaming, inventory, persistence, SDL edits and smoke/shutdown. These scripted tests are not a claim of sustained human gameplay.

No project compiler errors or sanitizer errors were observed. Nonblocking diagnostics were one pinned SDL `SDL_string.c` discarded-const warning with GCC 16 and upstream GLM CMake deprecation notices. Windows/MSVC execution, sustained human playtesting, long soaks and fuzzing were not performed. There are **no remaining Linux source-completeness/build/test blockers** from this recovery; Windows verification and the project's existing release-acceptance work remain separate.

## Reviewable commits and preservation

1. `dc7480f9c5` — original bundled fonts/licenses, pinned rasterizer and font-data template (6 files).
2. `4feda33eb7` — original platform/core/UI/command/identity implementations and regressions (29 files).
3. `53a157c4b6` — original packaging/platform inputs, process tests, release docs/captures and optional Vulkan workflow (19 files).
4. The subsequent evidence commit adds this report, JSON provenance/results and README/ROADMAP links only; no implementation or CMake changes.

All 54 recovered files are tracked in the recovery branch. The original `main` history stays at `692aef6`. The only initial files whose contents change are the explicitly documented README/ROADMAP additions. The two `.codex/tasks` notes, all existing implementation/resource bytes and VoxelUI's HEAD/status are preserved. Nothing is reset, overwritten, deleted or pushed.

## Reproduce independently

From the recovery branch in VoxelCraft, clone the committed source into a new directory; no parent-workspace skills or VoxelUI checkout are required for the build:

```sh
git clone --no-hardlinks --branch recovery/reproducible-baseline /path/to/VoxelCraft /tmp/vc-clean-source
cmake -S /tmp/vc-clean-source -B /tmp/vc-clean-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DVC_ENABLE_GPU_TESTS=ON -DVC_BUILD_SOAK_TOOLS=ON
cmake --build /tmp/vc-clean-build --parallel 4
ctest --test-dir /tmp/vc-clean-build -LE graphics --output-on-failure --no-tests=error
# With an available isolated/desktop display, Vulkan device and validation layer:
ctest --test-dir /tmp/vc-clean-build -L graphics --output-on-failure --no-tests=error
cpack --config /tmp/vc-clean-build/CPackConfig.cmake -C Debug
python3 /tmp/vc-clean-source/tests/package_smoke.py /tmp/vc-clean-build/packages/*.tar.gz
```

The initial build needs the documented host development tools and network access to pinned upstream dependencies. Font bytes, vendor header, shader sources, game code, tests and packaging inputs now come from Git; SPIR-V, embedded-font translation units and dependency build outputs are generated. An entirely network-free first build or bit-identical output across different compilers/hosts is not claimed. See [building](building.md) for headless and sanitizer presets; run each preset from the clean source root with a new build directory.

Before subsequent modularization, preserve this baseline and work in an incremental branch. VoxelUI integration and a native mod loader remain outside this recovery.
