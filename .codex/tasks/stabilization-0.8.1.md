# VoxelCraft v0.8.1 — Stability, Performance and Cross-Platform Release

## ROLE

You are the lead C++23 engine developer, Vulkan graphics engineer, multiplayer networking engineer, QA engineer, and release engineer responsible for stabilizing the existing VoxelCraft project.

Your objective is to transform the current VoxelCraft v0.8 Multiplayer Foundation into **VoxelCraft v0.8.1 — Stable Multiplayer Preview**.

This is an implementation task, not merely an architectural review.

You must inspect the existing code, identify real problems, implement necessary fixes, compile the project, execute tests, and prepare usable Linux and Windows release configurations.

Do not stop after producing recommendations or a development plan.

Work autonomously through the requested tasks, within the permissions and tools available to you.

Preserve the existing game's functionality and architecture wherever practical.

---

# 1. PROJECT CONTEXT

VoxelCraft is a Minecraft-inspired voxel sandbox developed using:

- C++23
- Vulkan 1.3
- SDL3
- CMake
- Linux as the development platform
- Windows as the primary player-facing platform

The existing codebase reportedly includes:

- Vulkan rendering.
- 32×32×32 voxel chunks.
- Large-world coordinate support.
- Procedural terrain generation.
- Biomes, caves, trees, ores and structures.
- Block placement and destruction.
- Player physics and collisions.
- Inventory and crafting.
- Health, combat and survival mechanics.
- Basic mobs and AI.
- World persistence.
- Region storage and recovery mechanisms.
- JSON-defined mod content.
- Lua mod scripting.
- Dedicated multiplayer server.
- TCP networking.
- Client/server chunk synchronization.
- Automated unit and integration tests.

These features were identified in a previous source audit. **Do not assume they are all correct, complete or currently functional. Verify the actual repository state.**

Read these files first, where available:

- `AGENTS.md`
- `SPEC.md`
- `ROADMAP.md`
- `README.md`
- `CMakeLists.txt`
- Existing architecture documentation
- Existing build and CI workflows

Inspect all relevant source code before making significant architectural changes.

The repository is the source of truth.

---

# 2. MAIN GOAL

Deliver a stable v0.8.1 release candidate that supports:

1. Single-player gameplay on Linux.
2. Single-player gameplay on Windows.
3. Dedicated server hosting.
4. Windows clients connecting to Linux servers.
5. Linux clients connecting to Windows or Linux servers.
6. Reliable world saving and loading.
7. Stable inventory and survival gameplay.
8. Reliable multiplayer interactions.
9. Predictable chunk loading and unloading.
10. Reasonable performance without severe recurring frame-time spikes.
11. Clean startup and shutdown.
12. Reproducible development and release builds.

The target is a usable multiplayer preview, not a feature-complete commercial release.

Do not introduce unnecessary gameplay features.

Prioritize correctness, reliability, performance and usability.

---

# 3. NON-NEGOTIABLE ARCHITECTURAL RULES

Preserve these principles:

- C++23.
- Vulkan as the only required rendering backend.
- SDL3 for windows and input.
- CMake for building.
- Shared Linux/Windows code.
- Gameplay independent of Vulkan.
- Dedicated server independent of graphical rendering.
- Chunk-oriented block storage.
- No ECS entity per static block.
- Stable namespaced content identifiers.
- Deterministic world generation.
- Configurable finite world height.
- Correct negative coordinate handling.
- Future support for composite blocks.
- Future support for multiblock structures.
- Mod-friendly content architecture.

Do not replace functioning core systems merely because an alternative seems more elegant.

Avoid unnecessary abstractions.

Prefer focused, measurable improvements.

---

# 4. WORKFLOW AND EXECUTION STRATEGY

Organize the work into clear phases.

Before modifying major subsystems:

1. Inspect their implementation.
2. Identify reproducible problems.
3. Establish a baseline where possible.
4. Determine dependencies between fixes.
5. Implement targeted changes.
6. Add regression tests.
7. Build and verify.
8. Update documentation.

Create a working development checklist in `docs/v0.8.1-stabilization.md`.

Record:

- Problems discovered.
- Severity.
- Affected files.
- Planned fixes.
- Implemented fixes.
- Tests performed.
- Remaining risks.

Use the existing roadmap where appropriate.

Do not automatically mark a feature as stable merely because its code compiles.

If Git commits are available and permitted, organize changes into logical commits.

Do not delete unrelated user changes.

If the current Git working tree contains uncommitted changes, preserve them.

---

# PHASE 1 — FULL BASELINE AUDIT

## 5. BUILD SYSTEM AUDIT

Inspect the complete CMake configuration.

Verify:

- Correct target dependencies.
- Debug configuration.
- Release configuration.
- RelWithDebInfo configuration.
- Correct C++23 settings.
- Compiler warning configuration.
- Vulkan dependency discovery.
- SDL3 dependency resolution.
- Shader compilation.
- Asset packaging.
- Cross-platform paths.
- Test target configuration.
- Dedicated server build independence.

Fix confirmed problems.

Do not require developers to manually copy shader binaries into arbitrary directories.

Ensure generated shaders are available to the executable.

The application should not rely on being launched from the repository root.

Document supported compilers and versions.

## 6. DEPENDENCY MANAGEMENT

Inspect dependency configuration and pinning.

Ensure that a fresh developer checkout can configure and build the project following documented instructions.

Avoid silently relying on globally installed development libraries.

Prefer reproducible dependency versions.

If the system lacks packages or network access, provide clear setup instructions rather than fabricating successful build results.

Ensure that end users do not require the Vulkan SDK or a C++ compiler.

---

# PHASE 2 — WINDOWS AND LINUX COMPATIBILITY

## 7. WINDOWS BUILD SUPPORT

Windows is a first-class release target.

Verify compatibility with MSVC.

Review:

- Compiler-specific extensions.
- Filesystem APIs.
- Integer conversions.
- Character encoding.
- Dynamic library handling.
- Platform macros.
- Compiler warnings.
- Dependency linkage.
- Runtime DLL distribution.
- Asset discovery.
- Shader loading.

Fix confirmed incompatibilities.

Ensure Windows builds are produced through the existing CMake architecture.

Do not create a separate Windows-specific gameplay implementation.

## 8. GITHUB ACTIONS

Implement or improve CI pipelines for:

### Linux

- Configure.
- Build.
- Execute CPU-side tests.
- Package client.
- Package dedicated server.

### Windows

- Configure with MSVC.
- Build.
- Execute compatible tests.
- Package client.
- Package dedicated server where supported.

Avoid relying on a real GPU in ordinary hosted CI.

Graphics validation should have a separate optional or appropriately skipped test path.

Do not report successful Windows runtime testing based only on compilation.

## 9. PORTABLE RELEASE PACKAGING

Create release packaging using CMake/CPack or an existing suitable system.

Expected artifacts:

- Windows client archive.
- Windows dedicated server archive.
- Linux client archive.
- Linux dedicated server archive.

Include required runtime dependencies and assets where licensing permits.

Include:

- Compiled shaders.
- Required textures.
- Base game content.
- Default configuration.
- Relevant licenses.
- Basic launch instructions.

Do not package build caches, object files, internal test data or unnecessary development dependencies.

Release artifacts should be runnable from their extracted directories.

---

# PHASE 3 — VULKAN STABILITY

## 10. VULKAN RESOURCE MANAGEMENT

Audit:

- Vulkan instance creation.
- Logical device initialization.
- Physical device selection.
- Queue family selection.
- Swapchain creation.
- Swapchain recreation.
- Image views.
- Depth attachments.
- Render passes or dynamic rendering.
- Graphics pipelines.
- Command buffers.
- Descriptor sets.
- Vertex buffers.
- Index buffers.
- Staging buffers.
- GPU memory ownership.
- Shutdown sequence.

Fix:

- Resource leaks.
- Incorrect destruction order.
- Dangling handles.
- Invalid resource reuse.
- Unsafe resource ownership.
- Device initialization error handling.

Use RAII consistently.

## 11. FRAME SYNCHRONIZATION

Inspect:

- Frames in flight.
- Fences.
- Semaphores.
- Swapchain image synchronization.
- Command buffer reuse.
- GPU upload synchronization.
- Dynamic mesh replacement.
- Resource destruction after GPU work.

Ensure resources are not destroyed while still referenced by pending GPU commands.

Avoid unnecessary device-wide waits during ordinary gameplay.

Use appropriate synchronization mechanisms.

## 12. SWAPCHAIN AND WINDOW HANDLING

Verify:

- Window resizing.
- Window minimization.
- Window restoration.
- Swapchain invalidation.
- Surface out-of-date errors.
- Suboptimal swapchain results.
- Fullscreen/windowed transitions where supported.
- Alt-tab behavior.
- Application shutdown during resize.

Do not recreate the swapchain unnecessarily every frame.

Ensure minimized windows do not cause endless busy-looping or invalid swapchain use.

## 13. VULKAN VALIDATION

Ensure validation layers can be enabled in development builds.

Improve diagnostic logging.

Add an optional Vulkan smoke-test workflow.

Validation errors must not be ignored.

If no usable Vulkan device is available in the current environment, report that limitation and continue with non-GPU testing.

Never claim graphics were visually tested if they were not.

---

# PHASE 4 — PERFORMANCE AND CHUNK STREAMING

## 14. PROFILING INFRASTRUCTURE

Before introducing major optimizations, implement or improve lightweight performance diagnostics.

Track:

- Frame time.
- FPS.
- CPU update time.
- CPU rendering submission time.
- Visible chunks.
- Loaded chunks.
- Pending generation jobs.
- Pending meshing jobs.
- Triangle count.
- Draw-call count.
- Chunk generation time.
- Chunk meshing time.
- Chunk upload time.
- World save time.
- Network throughput where practical.

Provide a development debug overlay or accessible logging output.

Use these metrics to identify bottlenecks.

Do not invent measured performance improvements.

## 15. CHUNK LOADING AND UNLOADING

Audit chunk lifecycle management.

Verify:

- Correct loading radius.
- Proper chunk prioritization.
- Player movement across chunk boundaries.
- Negative coordinate behavior.
- World height limits.
- Chunk eviction.
- Mesh invalidation.
- Cross-chunk visibility updates.
- Safe cancellation or rejection of obsolete work.
- Correct shutdown while background work is active.

Prevent uncontrolled growth in memory usage.

Ensure chunks are not accidentally generated multiple times due to duplicate scheduling.

## 16. BACKGROUND WORK

Inspect chunk generation, meshing, lighting and serialization.

Identify operations causing main-thread stalls.

Move expensive CPU-side operations to bounded background workers where justified.

Requirements:

- No uncontrolled thread creation.
- No data races.
- No stale mesh uploads.
- No dangling references.
- No unsafe world mutations.
- No Vulkan calls from inappropriate threads.
- Predictable shutdown.

Use generation counters, immutable snapshots, ownership transfer or equivalent mechanisms as appropriate.

Avoid unnecessarily replacing the existing threading system.

## 17. GREEDY MESHING

Investigate implementing Greedy Meshing for ordinary opaque full-cube terrain.

The implementation must:

- Merge adjacent compatible visible faces.
- Preserve correct winding.
- Preserve texture orientation.
- Preserve material boundaries.
- Preserve face visibility behavior.
- Handle chunk boundaries.
- Support mesh regeneration.
- Avoid merging incompatible materials or lighting states.

Important:

Do not break transparent blocks, plants, liquids or non-cubic geometry.

If needed, keep specialized geometry paths separate.

Add tests comparing expected mesh geometry and visible surfaces.

Benchmark vertex counts and meshing time before and after optimization.

Do not report percentage improvements without measured results.

## 18. CHUNK STORAGE

Audit memory consumption.

The current implementation reportedly stores block IDs in fixed-size arrays.

Evaluate palette-compressed storage.

However, do not rewrite the chunk storage format solely for theoretical memory savings.

Implement changes only when they are sufficiently tested and do not jeopardize world persistence compatibility.

If full palette compression is too risky for v0.8.1, document it for a later version.

Memory correctness and save compatibility take priority.

---

# PHASE 5 — WORLD PERSISTENCE AND DATA SAFETY

## 19. SAVE SYSTEM AUDIT

Inspect:

- World metadata.
- Chunk serialization.
- Region files.
- Player data.
- Inventory persistence.
- Mod-defined content.
- Entity persistence.
- Format versioning.
- Checksums.
- Recovery mechanisms.

Ensure existing worlds remain compatible.

Do not introduce breaking save-format changes without explicit versioning and migration support.

## 20. SAFE WORLD WRITES

Review the save strategy.

Where practical, use:

1. Serialization to a temporary file.
2. Data validation.
3. Appropriate synchronization to disk.
4. Atomic replacement where supported.
5. Recovery metadata or previous revisions.

Consider platform-specific filesystem semantics.

Ensure that unsuccessful writes do not destroy the last valid world state.

Avoid treating successful buffered writes as guaranteed durable storage.

## 21. ASYNCHRONOUS SAVING

Identify whether world saves can cause frame-time spikes.

Move serialization and compression to background work where safe and beneficial.

Use consistent snapshots of world data.

Ensure game shutdown waits for required save operations to finish or reports failure clearly.

Never silently discard modified world data.

## 22. PERSISTENCE TESTS

Add tests for:

- Save/load round trips.
- Modified block persistence.
- Inventory persistence.
- Player position persistence.
- Corrupted region detection.
- Interrupted write recovery.
- Unknown content identifiers.
- Mod-dependent content.
- Large and negative coordinates.
- World height boundaries.
- Loading old supported save versions.

Use temporary test directories.

Do not modify actual user worlds during automated tests.

---

# PHASE 6 — MULTIPLAYER STABILITY

## 23. NETWORK PROTOCOL AUDIT

Inspect the complete TCP protocol.

Verify:

- Message framing.
- Partial reads.
- Partial writes.
- Packet size limits.
- Invalid packet handling.
- Unexpected disconnection.
- Timeout handling.
- Serialization bounds.
- Protocol version checking.
- Error propagation.
- Shutdown behavior.

Never assume that one TCP read corresponds to one complete message.

Prevent malformed packets from causing uncontrolled allocations or crashes.

## 24. SERVER AUTHORITY

The server must remain authoritative for:

- World block changes.
- Inventory state.
- Item consumption.
- Mining actions.
- Crafting.
- Health.
- Combat.
- Entity state.
- Player position validation where applicable.

Audit client requests.

Prevent obvious exploits such as:

- Breaking blocks outside valid reach.
- Placing blocks in prohibited positions.
- Duplicating items.
- Crafting without resources.
- Invalid inventory indices.
- Invalid block IDs.
- Repeating one-time actions incorrectly.
- Invalid world coordinates.
- Unauthorized manipulation of other players' data.

Apply defensive validation at external input boundaries.

Do not blindly trust client-provided state.

## 25. PLAYER IDENTITY

Inspect how multiplayer profiles are identified.

If players can claim another player's persistent profile simply by supplying the same username, fix the issue before considering the server suitable for untrusted users.

For v0.8.1, prioritize a practical solution.

Possible options:

- Configurable private-server authentication.
- Random server-issued player identity tokens.
- Password-protected private servers.
- Proper session binding to persistent player IDs.

Design the solution carefully and document its threat model.

Do not describe an unauthenticated plain TCP protocol as secure for public internet hosting.

If secure transport or a robust identity system cannot be fully implemented within this release, restrict the supported deployment model to trusted LAN or appropriately secured private networks.

Do not expose credentials in logs.

## 26. CONNECTION RELIABILITY

Implement or improve:

- Graceful disconnect.
- Server shutdown notification.
- Client connection timeout.
- Clear connection error messages.
- Reconnection behavior where feasible.
- Protocol version mismatch reporting.
- Server-full responses.
- Duplicate connection handling.
- Partial session cleanup.

A client disconnect must not leave invalid world or player state.

## 27. MULTIPLAYER SYNCHRONIZATION

Audit:

- Player position synchronization.
- Chunk streaming.
- Block updates.
- Inventory updates.
- Entity state.
- Item pickups.
- Damage events.
- Crafting operations.
- Save consistency.

Look for race conditions and event ordering problems.

Ensure other players observe a consistent world.

## 28. CLIENT-SIDE SMOOTHING

Improve movement and rendering smoothness where necessary.

Possible techniques:

- Interpolation for remote players.
- Buffered network snapshots.
- Client-side prediction.
- Server reconciliation.

Do not implement an excessively complicated prediction system unless current architecture supports it cleanly.

Prioritize correctness.

Any prediction must reconcile with server authority.

## 29. NETWORK RESOURCE CONTROL

Implement appropriate limits for:

- Incoming packet size.
- Connection count.
- Per-client request rate.
- Pending outgoing data.
- Chunk transfer backlog.
- World interaction frequency.
- Memory allocation caused by untrusted data.

Use bounded queues and reasonable timeouts.

Prevent one slow or malformed client from exhausting server resources.

---

# PHASE 7 — GAMEPLAY RELIABILITY

## 30. PLAYER CONTROLLER

Audit:

- Movement.
- Gravity.
- Collision.
- Ground detection.
- Jumping.
- Sprinting if implemented.
- Flying.
- World boundary handling.
- Movement across chunk boundaries.
- Recovery from invalid spawn locations.

Fix clipping, falling through terrain and other reproducible issues.

Ensure collision behavior is reasonably stable under variable frame rates.

## 31. INVENTORY AND CRAFTING

Verify:

- Stack limits.
- Item movement.
- Inventory serialization.
- Crafting resource consumption.
- Crafting output.
- Full inventory behavior.
- Item pickup.
- Item dropping.
- Multiplayer inventory synchronization.

Add regression tests for duplication and item loss scenarios.

## 32. SURVIVAL SYSTEMS

Audit:

- Health.
- Damage.
- Death.
- Respawning.
- Mining.
- Tool durability.
- Combat.
- Entity interactions.

Ensure the systems behave consistently in single-player and multiplayer.

Do not introduce major new progression mechanics for this patch.

## 33. ENTITY AND AI RELIABILITY

Audit:

- Entity spawning.
- Entity despawning.
- Entity update scheduling.
- AI state transitions.
- Entity persistence.
- Entity synchronization.
- Collision and damage logic.

Prevent unbounded entity growth.

Avoid expensive entity updates outside relevant simulation areas where possible.

---

# PHASE 8 — MODDING AND CONTENT STABILITY

## 34. MOD LOADING

Audit the JSON and Lua mod systems.

Verify:

- Mod discovery.
- Dependency ordering.
- Version constraints where supported.
- Duplicate content identifiers.
- Missing dependencies.
- Invalid JSON.
- Invalid Lua scripts.
- Missing assets.
- Script error reporting.
- Mod load failure behavior.

A broken mod should not unnecessarily corrupt a world or cause undefined behavior.

## 35. LUA RUNTIME SAFETY

Inspect Lua exposure.

Prefer controlled bindings rather than raw access to internal C++ ownership.

For mods treated as untrusted, restrict or clearly document dangerous Lua standard libraries.

Consider:

- Filesystem access.
- OS command execution.
- Dynamic library loading.
- Unbounded memory allocation.
- Excessive script execution.

Do not claim full sandbox security unless it has actually been established and tested.

## 36. MOD COMPATIBILITY

Ensure base content remains stable.

Do not casually rename content identifiers or break existing mod APIs.

If an API change is necessary, document compatibility implications.

Test loading at least one representative mod.

---

# PHASE 9 — USER EXPERIENCE

## 37. CONFIGURATION

Ensure practical configuration for:

- Resolution.
- Fullscreen/windowed mode, if supported.
- Mouse sensitivity.
- Render distance.
- VSync.
- Audio settings where implemented.
- Server address.
- Player name.
- World location.

Use platform-appropriate configuration paths.

Avoid requiring source code changes for ordinary gameplay settings.

## 38. BASIC CONNECTION UX

A full main menu is not mandatory.

However, the game should be reasonably usable without modifying source files.

Provide at least one documented, practical way to:

- Launch single-player.
- Host a server.
- Connect to a server.
- Specify a player identity.
- Choose a world.
- Exit cleanly.

If adding a minimal connection screen is low-risk and useful, implement it.

Otherwise, provide clean command-line arguments and clear error messages.

## 39. LOGGING

Improve structured diagnostics.

Include relevant context for:

- Startup failures.
- Vulkan failures.
- Asset failures.
- Network disconnects.
- Save errors.
- Mod loading errors.
- Chunk generation errors.

Avoid excessive per-frame logging.

Do not log secrets or authentication tokens.

---

# PHASE 10 — AUTOMATED TESTING

## 40. UNIT TESTS

Run all existing tests.

Add targeted tests for confirmed defects.

High-priority areas:

- Chunk coordinate math.
- World bounds.
- Meshing.
- Terrain determinism.
- Serialization.
- Inventory.
- Crafting.
- Damage calculation.
- Networking.
- Mod loading.

## 41. INTEGRATION TESTS

Implement or improve scenarios that:

1. Start a dedicated server.
2. Connect a test client.
3. Authenticate or identify the client using the supported mechanism.
4. Enter the world.
5. Receive chunk data.
6. Modify a block.
7. Confirm synchronized world state.
8. Disconnect.
9. Restart the server.
10. Verify persistence.

Add a two-client test where practical.

Ensure tests have timeouts and reliable cleanup.

## 42. STRESS AND LONG-RUN TESTING

Add reproducible stress-test tooling for:

- Repeated connection/disconnection.
- Rapid block edits.
- Chunk loading/unloading.
- Repeated world save cycles.
- Multiple active clients.
- Malformed network input.

Run bounded tests within available resources.

Provide longer-running commands for manual verification.

Do not claim a multi-hour soak test unless it actually ran.

## 43. SANITIZERS

Where supported, provide sanitizer configurations for:

- AddressSanitizer.
- UndefinedBehaviorSanitizer.
- ThreadSanitizer in suitable isolated tests.

Do not combine incompatible sanitizer configurations.

Use sanitizer findings to fix real defects.

---

# PHASE 11 — RELEASE VERIFICATION

## 44. ACCEPTANCE CRITERIA

The release is ready only when all mandatory conditions are satisfied or explicitly listed as release blockers.

### Build

- Linux client builds successfully.
- Linux server builds successfully.
- Windows client builds successfully.
- Windows server builds successfully where targeted.
- Release archives contain required dependencies and assets.

### Rendering

- Vulkan initializes on supported hardware.
- Terrain renders correctly.
- Window resizing works.
- Application shutdown is clean.
- No known recurring Vulkan validation errors in tested scenarios.

### World

- Chunks load and unload reliably.
- Block modifications persist.
- Worlds survive normal restarts.
- Recovery behavior is tested.
- No known reproducible critical data corruption.

### Gameplay

- Player movement works.
- Collision works.
- Inventory works.
- Crafting works.
- Combat works.
- Survival systems work.
- Core interactions work in multiplayer.

### Multiplayer

- Server starts without a GPU.
- Clients connect successfully.
- Two clients can observe each other.
- Block changes synchronize.
- Inventory and relevant gameplay actions remain server-authoritative.
- Disconnects are handled.
- Repeated connections do not cause known resource leaks.
- Persistent player identity is protected according to the documented deployment model.

### Testing

- Existing tests pass.
- New regression tests pass.
- CI builds pass.
- Release blockers are documented.

## 45. MANUAL TEST MATRIX

Create `docs/release-v0.8.1-test-plan.md`.

Include test scenarios for:

- Linux client + Linux server.
- Windows client + Linux server.
- Windows client + Windows server.
- Single-player Linux.
- Single-player Windows.
- Two-player LAN session.
- World save/reload.
- Abrupt client disconnect.
- Server restart.
- Window resize.
- Fullscreen transitions if available.
- Different render distances.
- Mod load failures.
- Invalid network packets.

Provide expected results and a place to record actual outcomes.

Do not mark tests completed unless they were actually executed.

---

# PHASE 12 — DOCUMENTATION AND RELEASE

## 46. UPDATE DOCUMENTATION

Update:

- `README.md`
- `ROADMAP.md`
- `docs/architecture.md`, if present
- Build instructions
- Multiplayer hosting instructions
- Release test plan
- Stabilization report

Create:

`docs/v0.8.1-release-notes.md`

Include:

- Fixed bugs.
- Performance improvements.
- Networking changes.
- Save compatibility.
- Security limitations.
- Known issues.
- Supported platforms.
- Testing status.

## 47. UPDATE VERSIONING

Introduce or update the version identifier:

`0.8.1`

Ensure it is consistently represented in the executable and relevant documentation.

Do not create a Git release or push changes unless explicitly authorized.

Do not mark the version as stable while mandatory acceptance criteria remain unverified.

A release candidate designation is acceptable until final validation.

---

# 48. PRIORITY ORDER

If the full scope cannot be completed in one execution, follow this order:

**P0 — Release blockers**

1. Build failures.
2. Crashes and undefined behavior.
3. World corruption or data loss.
4. Broken multiplayer synchronization.
5. Unauthorized persistent player identity reuse.
6. Serious server input validation bugs.
7. Vulkan synchronization and resource lifetime bugs.
8. Windows portability problems.

**P1 — High-priority stabilization**

1. Chunk loading reliability.
2. Save-related frame stalls.
3. Bounded background work.
4. Network disconnect reliability.
5. Gameplay consistency.
6. Regression tests.
7. Release packaging.
8. Performance instrumentation.

**P2 — Performance improvements**

1. Greedy Meshing.
2. Chunk streaming optimizations.
3. Reduced GPU upload overhead.
4. Memory optimizations based on profiling.
5. Client-side smoothing.

**P3 — Optional polish**

1. Better settings interface.
2. Improved debug overlay.
3. Additional graphics configuration.
4. Minor visual and UX improvements.

Finish critical correctness work before major optimization.

Do not let optional improvements block important stability fixes.

---

# 49. OUT OF SCOPE FOR v0.8.1

Do not implement these unless necessary to fix a release-blocking defect:

- Composite voxel cells.
- Multiblock object system.
- Complete arbitrary block geometry.
- New major survival mechanics.
- New biomes.
- New creatures.
- New dimensions.
- Complex new crafting systems.
- Ray tracing.
- Full PBR rendering.
- Complete rendering engine redesign.
- Native C++ mod loader.
- Large-scale public server infrastructure.
- Full anti-cheat system.
- Comprehensive account management.

These features belong to later milestones.

The objective is to stabilize the existing game, not expand feature scope indefinitely.

---

# 50. FINAL REPORT FORMAT

When implementation ends, provide a detailed report.

## A. Summary

Describe whether v0.8.1 is ready for release, a release candidate, or still blocked.

## B. Implemented fixes

List concrete fixes with affected files.

## C. Performance

Include before/after measurements when available.

Do not invent benchmarks.

## D. Multiplayer

Describe implemented changes and remaining limitations.

## E. Build verification

Provide exact commands and results.

Distinguish:

- Linux compiled.
- Linux runtime tested.
- Windows compiled.
- Windows runtime tested.
- Vulkan validation tested.

## F. Test results

Report test counts and failures based on actual execution.

## G. Security

Describe validated trust boundaries and remaining risks.

## H. Compatibility

Explain save format and mod API compatibility.

## I. Release artifacts

List produced packages and their paths.

## J. Remaining issues

Classify each unresolved issue as P0, P1, P2 or P3.

## K. Next steps

Provide actionable follow-up tasks.

---

# 51. AUTONOMOUS EXECUTION INSTRUCTIONS

Start by inspecting the current Git working tree and repository.

Read the project instructions and relevant documentation.

Then:

1. Establish the baseline build and test status.
2. Create the stabilization checklist.
3. Identify critical defects.
4. Implement fixes incrementally.
5. Build and test after meaningful changes.
6. Continue through the highest-priority unfinished tasks.
7. Document results.
8. Keep the project buildable.
9. Preserve unrelated changes and existing user data.
10. Clearly report anything that cannot be verified.

Do not ask for routine confirmation after every modification.

Do not stop simply because the initial code review is complete.

Do not introduce placeholder implementations and count them as completed features.

Do not silently skip difficult tasks.

If the environment prevents verification, explain precisely what is missing and provide the necessary commands or CI configuration.

If the available execution context cannot complete every phase, preserve the completed work and document the exact remaining tasks in the repository.

Never claim that a check passed unless it actually passed.

---

# 52. FINAL OBJECTIVE

**Deliver VoxelCraft v0.8.1 as a reliable, cross-platform Minecraft-inspired multiplayer sandbox foundation.**

By the end of this milestone, the project should be significantly closer to being a game that friends can launch on Windows and Linux, connect to a shared server, explore, build, survive, and save without critical instability.

Prioritize reliability over additional features.

Begin the implementation now.
