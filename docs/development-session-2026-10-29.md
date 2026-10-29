# Development continuation — 2026-10-29

## Recovered context

The repository arrived at commit `b6a08bc` with a clean working tree. The previous
[session log](development-session-2026-10-28.md) ended at v0.6, while CMake already
identified version 0.7.0 and `game/modding/` contained an unfinished Lua runtime.
That runtime was not wired into the client, player saves, crafting pages or tests.
README/ROADMAP still described it as future work. No other-PC conversation or
uncommitted source was available; this continuation follows the repository evidence.

Inspected the project instructions, specification, architecture/decisions, save
format, build/CI, renderer/world/gameplay boundaries, client loop, mod runtime and
existing CPU/SDL tests before editing. No commits were created or existing changes
reset. All changes are left in the working tree for review.

## Implemented

- Completed v0.7 client integration: default user-data mod discovery,
  `--mods-dir`/`--no-mods`, content registration before snapshot name resolution,
  item/recipe installation before inventory restore, load/tick hooks and
  post-success placement/break/craft notifications. Bounded runs ignore default
  mods; strict smoke runs use built-in content.
- Added player payload v6 with validated mod IDs/versions/string state; v1..v5
  remain readable. Missing mod records survive resaves; changed/missing versions
  produce diagnostics. Existing missing block/item handling remains in use.
- Paged the survival crafting list and exported successful crafting notifications
  from the UI controller without coupling it to scripting/platform/renderer code.
- Fixed the transferred Lua C++ wrapper's build failure: enable internal Lua
  configuration and include `lprefix.h` before guarded public headers. Kept C
  linkage for public APIs and C++ exception unwinding for callback RAII.
- Removed the unnecessary EnTT simulation dependency from the mod runtime and
  added its direct collision header dependency.
- Fixed non-string Lua errors (`error({})`) to produce diagnostics safely.
  Preserved staged rollback, per-script disabling and bounded execution.
- Added the runnable `examples/mods/demo` glowstone pack, content/API guide and
  ADR0010. Updated README, both roadmap pages, architecture and world format.
- Fixed a real menu input bug found during graphics tests: motion after a button
  event in the same poll overwrote its click coordinates. Click positions now
  remain tied to the button event while later frames retain current pointer
  position. A CPU regression test failed before the fix and passes afterward.

## Verification

Dependencies were fetched into `build/` at the existing pinned versions. No system
packages were installed. GCC 16.2.1 and Clang 22.1.8 compiled the full SDL3/Vulkan
client. Clang used AddressSanitizer/UndefinedBehaviorSanitizer and default Linux
leak detection. Project code compiled without warnings after fixes; dependency
configuration emits GLM CMake deprecation notices and the Clang configuration's
GCC-built SDL C source emits a const-qualifier warning.

- GCC Debug: **115/115 CPU/CLI tests passed**.
- Clang Debug ASan/UBSan/LSan: **115/115 CPU/CLI tests passed**, no findings.
- GCC isolated Wayland run: **122/122 tests passed**, including **7/7 strict
  interactive graphics tests** (38.82 seconds total; graphics 38.39 seconds).
- X11 `Client.GpuSmoke`: **1/1 passed** in 4.40 seconds, 120 rendered frames,
  seam edits/jump/mesh lifecycle checked, **zero validation errors/warnings**.
  Together the two display runs passed all **8/8 graphics tests**.

New CPU coverage checks deterministic dependencies, invalid/cyclic/mismatched
packs, transactional registration, declarative content/tools/recipes, paged
crafting, all five Lua events, staged reads/commits, error rollback and disabling,
VM/memory budgets, restricted libraries, coordinate/placement restrictions,
mod save validation, old payload migration and missing content/state restoration.

The SDL survival test now also crafts glowstone on page two using actual wheel
and click events, places/consumes it, verifies its Lua placement counter and
reloads that counter. The executable persistence test opens the example world
twice, disables the pack for a save, restores it, and checks visits 1 → 2 → 3.
The inspected [crafting capture](voxelcraft-v07-crafting.png) shows the added recipe
and page label with a crafted glowstone stack in the hotbar.

Initial runs on the live desktop exposed focus/resize interference: scripted
walking/mining stopped when desktop focus changed. Their individual reruns passed.
Final graphics verification uses the already installed KWin virtual backend on a
private D-Bus session and Wayland socket, with configuration/cache under `build/`.
The game still reports **AMD Radeon Graphics (RADV RENOIR)** and Vulkan validation;
no game renderer or driver fallback was introduced. The virtual Wayland compositor cannot automatically unminimize the smoke window
with the existing SDL 3.2.10 restore call; that run times out, so the independent
minimize/restore smoke runs on the live X11 display instead. This automated
Wayland restore limitation remains known. Final logs: `build/debug/tests-isolated.log`, `build/debug/smoke-x11.log`, and
`build/clang-sanitize/tests-final.log`. The temporary compositor was stopped and
the Debug cache returned to `VC_ENABLE_GPU_TESTS=OFF`, so ordinary CTest runs
remain display-independent; graphics binaries/captures remain under `build/debug/`.

## Current stopping point and next step

The previously unfinished v0.7 foundation is connected and documented. Stabilize
it with sustained human modded/survival playtesting and Windows CI execution,
then define mod-state migrations/API compatibility before extending content APIs.
The historical five-hour session window is no longer active.

Known limits: Windows compilation/runtime and sustained human playtesting remain
unverified. Lua instruction budgets do not preempt native library calls and are
not an OS security boundary. External textures, harvest declarations, schema
migrations, hot reload and native plugins remain planned. Existing synchronous
saving/remeshing, no mipmaps, static water, limited lighting, AI/pathfinding and
power-loss durability limits remain. Live-display scripted input tests require
stable focus or an isolated display.
