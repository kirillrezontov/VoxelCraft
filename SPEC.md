# VOXELCRAFT — MASTER DEVELOPMENT SPECIFICATION

## ROLE AND RESPONSIBILITIES

You are a senior C++ engine architect, Vulkan graphics programmer, gameplay systems engineer, and technical lead.

Your task is to design and implement the initial foundation of a long-term open-ended voxel sandbox game project, provisionally named **VoxelCraft**.

The project should eventually become a fully playable, highly moddable Minecraft-inspired sandbox game.

This is a real software engineering project, not a tutorial, code demonstration, or throwaway prototype.

You are expected to:

1. Design a practical and maintainable architecture.
2. Implement working code.
3. Establish a reproducible build system.
4. Compile and test the project in the available Linux environment.
5. Ensure that Windows remains a first-class target.
6. Document architectural decisions.
7. Prepare the project for future development.
8. Prioritize a playable vertical slice over unnecessary engine complexity.

Do not attempt to implement the entire final game in one iteration.

Instead, build a solid foundation that can be extended over many future development stages.

The initial result must be a genuine executable application with a Vulkan renderer, camera controls, and a basic voxel world.

Do not substitute OpenGL, software rendering, or an unrelated game engine for Vulkan.

If a required dependency or graphics capability is unavailable, preserve the intended architecture, implement and verify everything possible, and clearly report the environmental limitation.

Do not silently replace requested technologies.

---

# 1. PROJECT VISION

VoxelCraft is a Minecraft-inspired sandbox game with:

- Procedural voxel terrain.
- A large persistent world.
- Block placement and destruction.
- Exploration and survival gameplay.
- Crafting and inventory systems.
- Modifiable game content.
- Support for complex block shapes.
- Support for multiple objects inside one voxel cell.
- Support for objects spanning multiple cells.
- Modern graphics implemented using Vulkan.
- Linux and Windows compatibility.
- Future multiplayer support.
- A long-term extensible engine architecture.

The project must balance three priorities:

**1. Playability**

The game should become interactive and enjoyable as early as possible.

**2. Performance**

The engine must be designed for large voxel worlds and efficient rendering.

**3. Modifiability**

Gameplay content should be extensible without rewriting low-level engine systems.

Do not prioritize speculative engine abstractions over these three goals.

---

# 2. DEVELOPMENT PHILOSOPHY

Follow these principles throughout development.

## 2.1 Game-first architecture

We are not building a general-purpose engine capable of running every conceivable type of game.

We are building a specialized engine for voxel sandbox games.

The engine should be reusable where practical, but gameplay requirements take priority.

## 2.2 Incremental development

Every major stage should produce a runnable, testable result.

Avoid large periods of development during which the application cannot be executed.

## 2.3 Separation of responsibilities

The following systems must remain logically independent:

- Platform abstraction.
- Rendering.
- Voxel storage.
- World generation.
- Gameplay simulation.
- Content registration.
- Mod scripting.
- Networking.

The renderer must not directly control gameplay state.

World generation must not depend on Vulkan.

Gameplay systems must not directly invoke Vulkan commands.

## 2.4 Avoid premature abstraction

Do not implement complex generic infrastructure without a concrete requirement.

Examples of systems that should NOT be fully implemented immediately:

- Universal rendering backends.
- A complete custom memory allocator.
- A complex rendering graph.
- A fully featured editor.
- A distributed multiplayer architecture.
- GPU-driven chunk rendering.
- Arbitrary native plugin loading.
- A complete physics engine.

Design clean interfaces where necessary, but implement the simplest solution that genuinely satisfies current requirements.

## 2.5 Engineering quality

Prefer:

- RAII.
- Strong ownership semantics.
- Explicit resource lifetimes.
- const-correctness.
- Clear API contracts.
- Deterministic behavior where possible.
- Meaningful error messages.
- Automated tests.
- Minimal global state.
- Clean shutdown.
- Small, comprehensible modules.

Avoid:

- Hidden ownership.
- Unnecessary singletons.
- Circular dependencies.
- Giant manager classes.
- Platform-specific assumptions in core gameplay.
- Undefined behavior.
- Raw owning pointers.
- Unchecked Vulkan results.
- Fake implementations that only appear functional.

---

# 3. TECHNOLOGY STACK

Target the following technologies.

## Language

C++23.

Use portable C++23 features supported by the selected toolchains.

If a specific feature is not consistently supported by GCC, Clang, and MSVC, use a portable alternative and document the reason.

## Build system

CMake.

Use modern target-based CMake configuration.

Avoid global compiler flags when target-specific configuration is possible.

## Graphics API

Vulkan 1.3.

Use the Vulkan SDK and Vulkan headers.

Prefer conventional, well-understood Vulkan programming practices over experimental abstraction layers.

## Windowing and input

SDL3.

Use SDL3 for:

- Window creation.
- Keyboard input.
- Mouse input.
- Event processing.
- Vulkan surface integration.

Do not implement separate Win32 and X11 windowing systems.

## Mathematics

GLM.

Use consistent coordinate conventions throughout the renderer.

Document the chosen:

- World coordinate system.
- Camera coordinate system.
- Handedness.
- Clip-space conventions.
- Matrix multiplication conventions.

## Entity component system

EnTT is the preferred ECS library for future entity management.

Do not force every block or chunk into the ECS.

Voxel terrain and dynamic entities have different storage and performance requirements.

## UI

Dear ImGui for development diagnostics and debugging tools.

A production gameplay UI can be implemented later.

## Scripting

Lua is the planned primary scripting language for game modifications.

The final Lua runtime and mod API do not need to be fully implemented in the initial milestone.

## Dependencies

Choose a reproducible strategy using vcpkg or CMake FetchContent.

Document the selected approach.

Avoid unpinned dependency versions.

## Testing

Use a lightweight C++ testing framework where appropriate.

Prefer Catch2 or GoogleTest.

Set up CTest integration.

---

# 4. TARGET PLATFORMS

The primary development platform is Linux.

The primary player-facing release platform is Windows.

Both Linux and Windows must be supported.

## 4.1 Platform goals

The majority of source code should be shared between platforms.

Do not create separate Linux and Windows versions of the engine.

The application should have:

- One gameplay implementation.
- One voxel storage implementation.
- One Vulkan renderer implementation.
- One asset format.
- One world format.
- One modding API.

Platform-specific implementation details should be isolated.

## 4.2 Compiler support

Target:

- GCC on Linux.
- Clang on Linux.
- MSVC on Windows.

## 4.3 Windowing

Use SDL3.

SDL3 should abstract the primary differences between Windows, X11, and Wayland.

## 4.4 Vulkan

Use the same Vulkan renderer on Linux and Windows.

Handle platform-specific Vulkan surface creation through SDL3.

## 4.5 Filesystem

Use std::filesystem and centralized path management.

Do not assume:

- A particular path separator.
- Case-insensitive filenames.
- A specific current working directory.
- A Unix home directory.
- A Windows drive letter.

Assets must be locatable independently of the terminal's working directory.

User configuration, logs, and save files must be stored in appropriate user-writable locations.

## 4.6 Distribution

The long-term build pipeline must support:

- Linux executable packages.
- Windows executable packages.
- Bundled assets.
- Required runtime dependencies.
- Reproducible release archives.

End users should not need to install a C++ compiler or Vulkan SDK.

They may still require a compatible Vulkan graphics driver.

---

# 5. HIGH-LEVEL ARCHITECTURE

Use approximately the following modular structure.

The exact organization may be adjusted when justified.

VoxelCraft/
|
|-- CMakeLists.txt
|-- CMakePresets.json
|-- README.md
|-- LICENSE
|-- .gitignore
|
|-- cmake/
|
|-- docs/
|   |-- architecture.md
|   |-- roadmap.md
|   |-- building.md
|   |-- world-format.md
|   |-- modding-design.md
|   |-- decisions/
|
|-- engine/
|   |
|   |-- core/
|   |   |-- logging/
|   |   |-- application/
|   |   |-- filesystem/
|   |   |-- jobs/
|   |   |-- utilities/
|   |
|   |-- platform/
|   |   |-- window/
|   |   |-- input/
|   |
|   |-- renderer/
|   |   |-- include/
|   |   |-- src/
|   |   |-- vulkan/
|   |
|   |-- voxel/
|   |   |-- blocks/
|   |   |-- chunks/
|   |   |-- geometry/
|   |   |-- generation/
|   |   |-- storage/
|   |   |-- lighting/
|   |
|   |-- ecs/
|   |
|   |-- scripting/
|   |
|   |-- physics/
|
|-- game/
|   |-- player/
|   |-- gameplay/
|   |-- content/
|   |-- simulation/
|
|-- client/
|
|-- server/
|
|-- assets/
|   |-- shaders/
|   |-- textures/
|   |-- models/
|   |-- config/
|
|-- mods/
|
|-- tests/
|
|-- tools/
|
|-- .github/
|   |-- workflows/

Do not generate empty placeholder source files for systems that do not exist yet.

Future modules may initially be represented by documentation rather than incomplete implementations.

The initial codebase should remain reasonably small.

---

# 6. PLATFORM LAYER

Implement a platform layer using SDL3.

Responsibilities:

- Application window.
- Input events.
- Mouse capture.
- Window resizing.
- Application close events.
- High-resolution timing.
- Vulkan surface integration.

The windowing system must not contain gameplay logic.

The input system should expose input state through a clean interface.

The application should support:

- WASD movement.
- Mouse look.
- Escape to release mouse capture.
- Reacquiring mouse capture.
- Correct window resize handling.

Avoid tightly coupling the camera controller to SDL event structures.

---

# 7. VULKAN RENDERER

Implement a minimal but real Vulkan rendering backend.

## 7.1 Required initial functionality

Implement:

1. Vulkan instance creation.
2. Validation layers in development builds, when available.
3. SDL3 Vulkan surface creation.
4. Physical device selection.
5. Logical device creation.
6. Queue family selection.
7. Swapchain creation.
8. Swapchain recreation.
9. Graphics pipeline creation.
10. Shader module loading.
11. Vertex and index buffers.
12. Uniform or equivalent camera data.
13. Depth buffering.
14. Frame synchronization.
15. Command recording.
16. Triangle or mesh drawing.
17. Window resize handling.
18. Correct resource destruction.

Prefer Vulkan 1.3 dynamic rendering where practical.

Document the exact device features and extensions required.

## 7.2 Initial rendering capabilities

The renderer should be able to:

- Render textured or vertex-colored voxel meshes.
- Use perspective projection.
- Render multiple chunks.
- Update camera matrices.
- Handle depth testing.
- Clear the frame.
- Present frames correctly.

A texture atlas is desirable for the initial milestone if reasonably achievable.

If texture atlas support adds substantial risk to producing a working initial build, first complete a reliable vertex-colored voxel renderer, then add textures immediately afterward.

## 7.3 Renderer abstraction

The rendering system must not know about gameplay rules.

Gameplay code must not use Vulkan handles directly.

A possible conceptual design:

RenderDevice
RenderResourceManager
RenderScene
MeshHandle
MaterialHandle
VulkanRenderer

Do not build a massive generic rendering abstraction.

A simple Vulkan-oriented rendering interface is enough.

## 7.4 Resource lifetime

GPU resource lifetime management must be correct.

Pay special attention to:

- Swapchain recreation.
- Frames in flight.
- Buffer destruction.
- Device shutdown.
- Synchronization.
- Resizing.
- Exception safety.

Use RAII wherever practical.

## 7.5 Debugging

Include meaningful Vulkan error reporting.

Enable validation layers in debug builds when supported.

Make graphics initialization failures understandable.

Do not silently fall back to a different graphics API.

---

# 8. WORLD COORDINATES

VoxelCraft uses a conventional cubic world grid.

Each standard block occupies one world-space cell.

Use integer coordinates for block positions.

## 8.1 Horizontal world expansion

The world should be horizontally expandable without a predetermined gameplay border.

Use 64-bit coordinates where appropriate for persistent world positions.

Avoid storing all distant world positions solely as single-precision floats.

## 8.2 Vertical world limits

The world must have configurable finite vertical boundaries.

Suggested initial defaults:

minimum Y = -512
maximum Y = 511

These are inclusive block-coordinate bounds.

The exact values must be configurable for new worlds.

Do not hardcode these values throughout the engine.

## 8.3 Coordinate conversions

Implement carefully tested conversion functions for:

- World block position to chunk position.
- World block position to local block position.
- Chunk position to world origin.
- Negative coordinate handling.

Pay particular attention to mathematical floor division and floor modulus.

Standard C++ signed division is not sufficient for correct negative-coordinate chunk mapping.

Example:

A block at X = -1 must map to the chunk immediately left of chunk zero, with the correct local coordinate.

## 8.4 Floating-point precision

Design the renderer to support camera-relative rendering.

Do not require large 64-bit world coordinates to be directly represented as absolute 32-bit GPU floats.

---

# 9. CHUNK SYSTEM

Use three-dimensional chunks.

Suggested chunk size:

32 × 32 × 32 blocks.

Represent this using a compile-time constant.

Do not allow arbitrary runtime changes to the size of chunks in an existing world.

## 9.1 Initial chunk functionality

Implement:

- Chunk coordinate representation.
- Block storage.
- Block access.
- Chunk creation.
- Chunk initialization.
- Chunk mesh generation.
- Chunk visibility management.
- Basic nearby chunk generation.

The game should load chunks around the player.

## 9.2 Chunk memory

Do not represent every block as a heap-allocated object.

Prefer compact contiguous storage.

For the first milestone, a simple fixed-size block ID array is acceptable.

However, keep the implementation structured so that palette-based storage can replace the initial representation without rewriting all game systems.

## 9.3 Future palette compression

Eventually support:

- Uniform chunks.
- Local block palettes.
- Bit-packed indices.
- Efficient air representations.
- Serialization-friendly layouts.

Avoid premature compression before profiling and correctness testing.

## 9.4 Chunk states

The future chunk lifecycle may include:

Unloaded
Generating
Generated
Meshing
Ready
Unloading

Do not implement an excessively complicated state machine in the initial version.

However, chunk ownership and transitions must be clear.

## 9.5 Chunk boundaries

Mesh generation must eventually consider adjacent chunks.

Internal faces between neighboring solid blocks should not be rendered.

Chunk boundary changes should trigger appropriate mesh invalidation.

---

# 10. BLOCK REGISTRY

Block definitions must be logically separate from chunk storage.

Blocks should be registered using stable namespaced identifiers.

Examples:

voxelcraft:air
voxelcraft:stone
voxelcraft:dirt
voxelcraft:grass
voxelcraft:wood

Runtime IDs may be compact integers.

Do not assume that runtime numeric IDs remain stable between game launches or mod configurations.

Persistent world data must be capable of resolving stable content identifiers.

A conceptual block definition:

struct BlockDefinition {
    std::string identifier;
    bool solid;
    bool transparent;
    float hardness;
};

Adapt the actual type design as needed.

The registry should support:

- Registering definitions.
- Looking up definitions.
- Resolving runtime IDs.
- Validating duplicate identifiers.
- Handling unknown identifiers safely.

In the future, block definitions may also include:

- Rendering properties.
- Material references.
- Collision geometry.
- Occupancy geometry.
- Lighting properties.
- Sound references.
- Script callbacks.
- Block state definitions.

Do not implement all of these immediately.

---

# 11. BLOCK STATES

Block types and block states are distinct concepts.

Examples of block states:

- Orientation.
- Open or closed.
- Powered or unpowered.
- Growth stage.
- Waterlogged state.
- Visual variation.

Avoid designing a system where every orientation of a block must be registered as a completely unrelated block type.

For the initial milestone, a minimal block state representation is acceptable.

Document how it will evolve.

The future system must support compact runtime state storage and persistent state serialization.

---

# 12. COMPOSITE BLOCKS

VoxelCraft must eventually support multiple smaller blocks occupying one standard world cell.

This is a major planned feature.

Do not fully implement it during the first rendering milestone.

However, document and preserve an architectural path for implementing it.

## 12.1 Cell subdivision

Use a conceptual subdivision resolution of:

16 × 16 × 16 units per standard block cell.

Examples of possible components:

- Half blocks.
- Quarter blocks.
- Small decorative blocks.
- Thin panels.
- Small lamps.
- Combined materials.

## 12.2 Storage strategy

Normal world cells must remain compact.

Do not allocate 4096 subcells for every block in the world.

Instead, use a hybrid representation:

- Ordinary cells use compact block state storage.
- Composite cells reference separately stored component data.

## 12.3 Placement rules

Components must have occupancy information.

The engine must validate:

- Cell boundaries.
- Spatial overlap.
- Placement constraints.
- Neighbor interactions.

## 12.4 Rendering

Composite cell geometry should eventually be merged into chunk meshes where practical.

Avoid one GPU draw call per sub-block.

## 12.5 Future implementation

The architecture should allow composite cells without requiring every existing game system to understand their internal implementation.

Use a centralized placement and spatial occupancy subsystem.

---

# 13. MULTIBLOCK OBJECTS

VoxelCraft must eventually support objects occupying more than one world cell.

Examples:

- Doors.
- Beds.
- Large containers.
- Machines.
- Portals.
- Decorative structures.
- User-defined constructions.

## 13.1 Design principles

A multiblock object should have:

- Stable instance identity.
- A world-space origin.
- Orientation.
- Defined occupied cells.
- Shared logical state.
- Consistent lifecycle behavior.

## 13.2 Cross-chunk objects

Multiblock instances may cross chunk boundaries.

The future implementation must account for:

- Chunk loading.
- Chunk unloading.
- Persistence.
- Placement validation.
- Consistent removal.
- Neighbor updates.

## 13.3 Atomic placement

Placement and destruction operations must not leave partially created objects.

This is particularly important for multiplayer.

Do not implement full multiblock persistence during the initial milestone unless all earlier foundations are already complete.

---

# 14. SEPARATE RENDER, COLLISION, AND OCCUPANCY SHAPES

A block or world object may have three different spatial representations.

## Render shape

The geometry visible to the player.

## Collision shape

The geometry used for physical interactions.

## Occupancy shape

The spatial region reserved for placement rules.

These representations must not be assumed to be identical.

Example:

A door can rotate its visible mesh and collision geometry while continuing to reserve the same placement cells.

The initial game may use unit cubes for all three purposes.

The architecture should permit future differentiation.

---

# 15. TERRAIN GENERATION

Implement a deterministic procedural terrain generator.

The initial generator does not need to be highly sophisticated.

A simple coherent noise-based heightmap is sufficient.

## Initial terrain requirements

- Configurable world seed.
- Deterministic output.
- Grass surface.
- Dirt beneath grass.
- Stone underground.
- Air above terrain.
- Smoothly varying elevation.
- Terrain generation across chunk boundaries.

The same world seed and coordinates should produce the same generated terrain, given the same generator version and configuration.

## Future terrain features

- Multiple biomes.
- Mountains.
- Oceans.
- Rivers.
- Cave systems.
- Underground biomes.
- Ore generation.
- Structures.
- Trees.
- Vegetation.
- Climate simulation.
- Mod-provided world generators.

Keep world generation separate from rendering.

---

# 16. CHUNK MESH GENERATION

Implement a basic chunk mesher.

For initial development:

- Generate visible faces.
- Eliminate internal faces between opaque solid blocks.
- Generate valid vertex and index buffers.
- Correctly handle all six cube faces.
- Use consistent face winding.
- Support depth testing.
- Support basic material or block color assignment.

The output should be independent of Vulkan resource management.

Conceptually:

Chunk data → CPU mesh data → Renderer upload.

This separation is mandatory.

## Future optimization

The meshing architecture must eventually support:

- Greedy Meshing.
- Different block render layers.
- Transparent block handling.
- Non-cubic geometry.
- Composite blocks.
- Neighbor-aware face visibility.
- Background mesh generation.
- Partial mesh updates.

Do not introduce a highly complex meshing architecture until basic rendering works.

---

# 17. PLAYER CAMERA

Implement a usable first-person camera.

Required controls:

W — forward
A — left
S — backward
D — right

Mouse — look around.

Space — move upward in free-fly mode.

Left Shift or another documented control — move downward.

Escape — release mouse capture.

The initial camera can use free-fly controls.

Player physics, gravity, jumping, and collision are planned for later milestones.

The camera should feel responsive.

Use frame-time-aware movement.

Avoid frame-rate-dependent camera speed.

Clamp camera pitch to prevent invalid orientation behavior.

---

# 18. PLAYER INTERACTION

The initial release should establish the foundation for world interaction.

If practical after the core renderer is complete, implement:

- Ray casting into the voxel grid.
- Highlighting the targeted block.
- Destroying blocks.
- Placing blocks.
- Triggering chunk mesh updates.

Use grid traversal, preferably a well-tested 3D DDA algorithm.

Do not use expensive brute-force intersection tests against every block.

If this functionality cannot be completed reliably in the first iteration, document it as the next milestone.

Do not sacrifice rendering correctness to force it into the initial build.

---

# 19. GAMEPLAY SYSTEMS

The eventual game should contain:

- Player movement.
- Health.
- Hunger or another survival mechanic.
- Inventory.
- Item stacks.
- Tools.
- Mining.
- Crafting.
- Block placement.
- Building.
- Enemies and animals.
- Resource progression.
- Exploration rewards.
- World structures.
- Environmental interactions.

These are future milestones.

Do not implement incomplete versions merely to claim feature coverage.

---

# 20. PHYSICS

For the initial milestone, physics can be omitted or minimal.

Later, implement:

- Player collision against voxels.
- Gravity.
- Jumping.
- Ground detection.
- Basic entity movement.

Jolt Physics is a possible future dependency for dynamic rigid bodies and more sophisticated simulation.

Voxel terrain collision should remain efficient.

Avoid creating a separate rigid body for every static voxel.

---

# 21. ENTITY SYSTEM

Use EnTT for future dynamic entities.

Examples:

- Players.
- Creatures.
- Dropped items.
- Projectiles.
- Movable world objects.

Do not represent every static voxel as an ECS entity.

Static voxel storage should remain chunk-oriented.

ECS entities and voxel objects may interact through explicit APIs.

---

# 22. MODDING ARCHITECTURE

Modifiability is a core long-term goal.

Design for three levels of extension.

## Level 1 — Data-driven content

Mods should eventually register content using structured files.

Examples:

- Blocks.
- Items.
- Materials.
- Recipes.
- Models.
- Textures.
- Sounds.
- Biomes.

Use namespaced resource identifiers.

## Level 2 — Lua scripting

Mods should be able to provide logic through a Lua API.

Potential hooks:

- On block placement.
- On block destruction.
- On entity update.
- On player interaction.
- On world generation.
- On crafting.
- On scheduled events.

Provide controlled APIs rather than exposing internal C++ object ownership directly.

## Level 3 — Native C++ plugins

Native extensions may be supported in a later development phase.

Prefer a versioned C ABI for compatibility.

Native plugins are trusted code and must not be treated as sandboxed scripts.

## Mod compatibility

The future mod system should support:

- Mod identifiers.
- Mod versions.
- Dependencies.
- Load ordering where required.
- Asset overrides.
- Namespaced content.
- Error reporting.
- Compatibility diagnostics.

## Important initial requirement

Do not implement a full Lua mod runtime during the first milestone.

However, make the initial block registry and resource organization compatible with a future mod-driven content model.

---

# 23. MULTIPLAYER ARCHITECTURE

Multiplayer is a future requirement.

The game should eventually support a dedicated server.

The dedicated server must not depend on:

- Vulkan.
- SDL window creation.
- GPU rendering resources.
- Client-side UI.

The simulation layer must remain independent of the renderer.

A future architecture:

Client
  |
Network protocol
  |
Server simulation
  |
World storage and gameplay

Single-player mode should eventually reuse the same gameplay simulation logic.

The server must be authoritative for world state.

Network synchronization should eventually include:

- Player positions.
- Block changes.
- Chunk data.
- Entities.
- Inventory.
- Interactions.
- Mod compatibility.

Do not implement a complete network stack during the first milestone.

Preserve the separation needed to implement it later.

---

# 24. WORLD PERSISTENCE

World persistence will be introduced after initial generation and interaction.

The future storage system should support:

- World metadata.
- World seed.
- World generation version.
- Chunk data.
- Modified blocks.
- Player data.
- Entities.
- Multiblock objects.
- Composite cells.
- Mod-defined content.
- Format versioning.

Persistent content must not rely exclusively on unstable runtime block IDs.

Design toward regional storage and chunk compression.

Avoid directly serializing raw C++ object memory layouts.

Document how unknown or missing mod content should eventually be handled.

---

# 25. MULTITHREADING

The long-term engine should support background work.

Examples:

- Terrain generation.
- Chunk meshing.
- World serialization.
- Resource loading.
- Asset processing.

Eventually implement a bounded job system.

Do not spawn an uncontrolled number of threads.

Do not allow background threads to manipulate Vulkan resources without a deliberate synchronization and ownership model.

The main thread should remain responsive.

However, do not overengineer multithreading during the first milestone.

A correct single-threaded prototype with clear work boundaries is preferable to an unstable multithreaded system.

---

# 26. PERFORMANCE TARGETS

The engine must be designed for future scalability.

Long-term objectives include:

- Large visible worlds.
- Thousands of loaded chunks where hardware permits.
- Efficient chunk streaming.
- Low memory overhead.
- Limited draw-call counts.
- Smooth player movement.
- Minimal frame-time spikes.
- Efficient background generation.

Do not claim performance targets without benchmarks.

The eventual goal is smooth gameplay at common desktop resolutions on reasonably modern Vulkan-capable hardware.

Performance should be measured through:

- Frame time.
- CPU time.
- GPU time.
- Number of rendered triangles.
- Number of visible chunks.
- Number of draw calls.
- Chunk meshing time.
- Terrain generation time.
- Memory consumption.

Add basic profiling counters when appropriate.

---

# 27. FUTURE RENDERING ROADMAP

The renderer should eventually support the following capabilities.

## Stage A — Basic rendering

- Textured cubes.
- Chunk meshes.
- Perspective camera.
- Depth testing.
- Basic transparency handling.

## Stage B — Rendering efficiency

- Greedy Meshing.
- Frustum Culling.
- Chunk mesh caching.
- Optimized buffer uploads.
- Reduced draw calls.

## Stage C — Lighting and atmosphere

- Voxel light propagation.
- Sunlight.
- Block-emitted light.
- Shadows.
- Fog.
- Sky rendering.
- Day/night cycle.

## Stage D — Advanced visuals

- PBR materials.
- Water rendering.
- Volumetric effects.
- Improved shadows.
- Atmospheric scattering.
- Advanced particle effects.

## Stage E — Large-world rendering

- Distant terrain LOD.
- Occlusion Culling.
- GPU-driven rendering.
- Multi-draw indirect rendering.

## Stage F — Optional advanced effects

- Vulkan Ray Tracing.
- Advanced global illumination.
- Higher quality reflections.

Do not make advanced graphics mandatory for the game to run.

Allow graphics quality presets in the future.

---

# 28. INITIAL DEVELOPMENT MILESTONE

The first major deliverable is:

**VoxelCraft 0.1 — Playable World Viewer Foundation**

The goal is a reliable executable application that displays a procedurally generated voxel world.

This milestone is the highest priority.

## Required deliverables

### A. Repository structure

Create a clean, understandable repository.

Provide:

- Root CMakeLists.txt.
- CMake presets where useful.
- Dependency configuration.
- README.
- Build instructions.
- Architecture documentation.
- Roadmap.
- Tests.
- Git ignore rules.

### B. Application

Create a working application with:

- SDL3 window.
- Event loop.
- Frame timing.
- Input processing.
- Clean initialization.
- Clean shutdown.

### C. Vulkan renderer

Implement a real Vulkan renderer capable of drawing voxel meshes.

The minimum application must display actual geometry, not only a blank window.

### D. Camera

Implement first-person free-fly controls.

### E. Basic voxel data model

Implement:

- Block identifiers.
- Block registry.
- Chunk coordinates.
- Chunk storage.
- World-to-chunk conversion.
- Chunk-to-world conversion.

### F. Procedural terrain

Implement a deterministic simple terrain generator.

### G. Chunk meshing

Generate visible cube faces and render them through Vulkan.

### H. Basic chunk loading

Generate and display a modest area around the camera.

A small fixed region is acceptable initially, provided it is documented and the architecture is suitable for later streaming.

### I. Debug information

Provide useful diagnostics such as:

- FPS.
- Frame time.
- Camera position.
- Loaded chunk count.
- Rendered chunk count.

A simple logging interface is sufficient if an overlay cannot be implemented reliably within the first milestone.

### J. Automated testing

Test important non-graphics algorithms.

### K. Windows build preparation

Add Windows-compatible CMake configuration and GitHub Actions workflow.

Do not claim a successful Windows build unless it has actually been compiled.

---

# 29. OPTIONAL FIRST-MILESTONE FEATURES

Implement these only after all required functionality works.

Priority order:

1. Texture atlas.
2. Block ray casting.
3. Block destruction.
4. Block placement.
5. Chunk mesh regeneration.
6. Basic frustum culling.
7. Background terrain generation.
8. Persistent world storage.

If time or environmental constraints prevent these, leave them as documented future tasks.

---

# 30. DEVELOPMENT ROADMAP

Use the following long-term roadmap.

## VERSION 0.1 — Voxel world foundation

Goal:

A working Vulkan application displaying procedural terrain.

Features:

- SDL3 window.
- Vulkan renderer.
- Camera.
- Chunk storage.
- Block registry.
- Basic procedural terrain.
- Basic chunk meshing.
- Debug information.

## VERSION 0.2 — Interactive world

Goal:

Allow the player to modify terrain.

Features:

- Voxel ray casting.
- Block selection.
- Block destruction.
- Block placement.
- Chunk mesh updates.
- Basic collision detection.
- Basic player movement.

## VERSION 0.3 — Persistent sandbox

Goal:

Create a useful building sandbox.

Features:

- World saving.
- World loading.
- Region storage.
- Basic inventory.
- Hotbar.
- Block selection.
- World configuration.
- Basic settings.

## VERSION 0.4 — Survival foundation

Goal:

Introduce gameplay progression.

Features:

- Items.
- Resources.
- Mining tools.
- Crafting.
- Health.
- Survival rules.
- Trees.
- Ores.
- Basic progression.

## VERSION 0.5 — World expansion

Goal:

Make exploration interesting.

Features:

- Multiple biomes.
- Caves.
- Underground generation.
- Rivers.
- Mountains.
- Trees and vegetation.
- Structures.
- Lighting improvements.
- Improved terrain algorithms.

## VERSION 0.6 — Living world

Goal:

Introduce dynamic gameplay.

Features:

- ECS-based entities.
- Animals.
- Enemies.
- Entity AI.
- Combat.
- Dropped items.
- Environmental interactions.

## VERSION 0.7 — Modding foundation

Goal:

Enable meaningful user-created content.

Features:

- Declarative content loading.
- Lua runtime.
- Event system.
- Mod loader.
- Mod dependencies.
- Content registration.
- Scripted gameplay behavior.
- Basic modding documentation.

## VERSION 0.8 — Multiplayer

Goal:

Allow players to play together.

Features:

- Dedicated server.
- Client/server communication.
- Server-authoritative world state.
- Chunk synchronization.
- Player synchronization.
- Block interaction synchronization.
- Multiplayer persistence.

## VERSION 0.9 — Advanced voxel features

Goal:

Expand construction possibilities.

Features:

- Composite block cells.
- Sub-block placement.
- Multiblock structures.
- Advanced block states.
- Custom collision shapes.
- Non-cubic rendering.
- Improved placement system.

## VERSION 0.10 — Advanced rendering

Goal:

Improve visual quality and performance.

Features:

- Advanced lighting.
- Shadows.
- Water.
- PBR materials.
- Atmospheric effects.
- Distant terrain LOD.
- Occlusion Culling.
- GPU-driven optimizations where justified.

## VERSION 1.0 — Playable release

Goal:

Deliver a polished, stable sandbox game.

Features:

- Stable Windows and Linux builds.
- Reliable save formats.
- Consistent gameplay.
- Usable modding API.
- Multiplayer stability.
- Optimized world streaming.
- Graphics settings.
- Audio.
- Main menu.
- Input settings.
- Packaging.
- User documentation.

This roadmap is a planning framework, not a rigid schedule.

Features may be moved between milestones when dependencies or gameplay priorities justify the change.

---

# 31. TESTING REQUIREMENTS

Implement tests for critical non-graphics functionality.

Especially:

- Negative coordinate conversion.
- Chunk boundaries.
- Block indexing.
- Registry identifier validation.
- Terrain determinism.
- Mesh face generation.
- Empty chunks.
- Fully solid chunks.
- Boundary conditions.
- World height limits.

Avoid tests that depend on timing assumptions or a specific GPU.

Where possible, make CPU-side voxel systems testable without initializing Vulkan.

---

# 32. BUILD AND CI REQUIREMENTS

Create a reliable development workflow.

## Linux

Support CMake configuration and building using a supported GCC or Clang toolchain.

Provide documented commands.

## Windows

Prepare for MSVC builds.

Avoid compiler-specific code unless isolated and justified.

## GitHub Actions

Provide an initial CI workflow with:

- Linux build.
- Windows build.
- Unit tests.

If Windows testing or dependency installation requires additional configuration, document it.

Do not misrepresent untested CI workflows as verified.

## Development configurations

Support:

- Debug.
- Release.
- RelWithDebInfo.

Enable useful warnings.

Do not disable warnings globally merely to achieve a successful build.

---

# 33. DOCUMENTATION

Write useful project documentation.

## README.md

Explain:

- What VoxelCraft is.
- Current functionality.
- Required dependencies.
- Linux build instructions.
- Windows build instructions.
- How to run the game.
- Controls.
- Known limitations.
- Next development milestones.

## docs/architecture.md

Explain:

- Module responsibilities.
- Dependencies.
- Major data flows.
- Resource ownership.
- Renderer boundaries.
- World storage architecture.

## docs/roadmap.md

Include the long-term roadmap.

Mark:

- Completed.
- In progress.
- Planned.

## docs/world-format.md

Describe:

- Chunk coordinates.
- World bounds.
- Block identifiers.
- Planned persistence.
- Planned composite cells.
- Planned multiblock storage.

## docs/modding-design.md

Describe the future modding model.

Include:

- Namespaced identifiers.
- Content definitions.
- Lua API goals.
- Event hooks.
- Native plugin considerations.

Clearly distinguish implemented functionality from planned functionality.

## Architecture Decision Records

Create short ADR documents for major choices such as:

- Vulkan as the only initial renderer.
- SDL3 as the platform layer.
- 32³ chunks.
- Finite world height.
- Renderer/gameplay separation.
- Mod-friendly block registration.
- CMake-based cross-platform builds.

---

# 34. IMPLEMENTATION ORDER

Follow this implementation order.

## Step 1 — Inspect the environment

Determine:

- Operating system.
- Compiler availability.
- CMake version.
- Vulkan headers and loader availability.
- SDL3 availability.
- Available GPU devices.
- Build tool availability.

Do not assume that all dependencies are installed.

Do not modify global system configuration unnecessarily.

If dependency installation requires elevated permissions, prefer a documented project-local solution where feasible.

## Step 2 — Create the repository foundation

Implement:

- Directory organization.
- CMake configuration.
- Dependency handling.
- Logging.
- Application entry point.
- SDL3 window.

Verify compilation.

## Step 3 — Implement Vulkan initialization

Create a working Vulkan context.

Verify that the application can initialize Vulkan.

Handle errors gracefully.

## Step 4 — Render basic geometry

Render a triangle or cube.

Verify:

- Pipeline creation.
- Command recording.
- Presentation.
- Depth testing.
- Camera matrices.

## Step 5 — Implement voxel data

Create block definitions, chunks, and coordinate conversion.

Write unit tests.

## Step 6 — Implement terrain generation

Generate deterministic voxel terrain.

Validate chunk boundaries.

## Step 7 — Implement chunk meshing

Convert chunk contents into CPU meshes.

Write basic meshing tests.

## Step 8 — Connect meshes to the renderer

Display procedural voxel terrain.

Implement camera movement.

## Step 9 — Stabilize the application

Fix:

- Validation errors.
- Resize issues.
- Shutdown problems.
- Resource leaks.
- Build warnings where practical.

## Step 10 — Documentation and reporting

Update the README and roadmap.

Document what was actually implemented.

Report remaining limitations.

---

# 35. DEFINITION OF DONE

The initial milestone is considered complete only when:

1. The project configures through CMake.
2. The project builds successfully on the available Linux toolchain.
3. The executable initializes SDL3.
4. The executable initializes Vulkan.
5. The application displays visible voxel terrain.
6. The camera can move through the world.
7. Terrain is divided into chunks.
8. Terrain generation is deterministic.
9. Core voxel coordinate operations are tested.
10. The application shuts down cleanly.
11. Source code is organized into meaningful modules.
12. Build instructions are documented.
13. Windows build support is prepared.
14. Future architectural requirements are documented.
15. The implementation status is reported accurately.

If a graphics environment is unavailable, do not falsely mark rendering as verified.

Report the distinction between:

- Compiled.
- Unit-tested.
- Vulkan initialization tested.
- Visually tested.
- Windows compiled.
- Windows runtime tested.

---

# 36. IMPORTANT LIMITATIONS

Do not:

- Replace Vulkan with OpenGL.
- Introduce Unreal Engine or Unity.
- Implement every planned feature immediately.
- Create an unnecessarily generic engine.
- Store each terrain block as an ECS entity.
- Allocate every individual block on the heap.
- Couple world generation to Vulkan.
- Couple game simulation to SDL3.
- Hardcode Linux-specific behavior.
- Assume numeric block IDs are permanent.
- Implement an enormous multithreading framework prematurely.
- Claim features are implemented when they are not.
- Hide build errors.
- Leave essential functionality as TODO-only stubs.
- Generate thousands of lines of unused framework code.

Implement the smallest complete system that genuinely satisfies the first milestone.

---

# 37. WORKING AUTONOMOUSLY

You are working in the project repository.

You are authorized to create and modify project source files, build scripts, documentation, and tests within the working directory.

You should execute available build and test commands.

Proceed through implementation tasks without asking for confirmation after each routine step.

Make reasonable engineering decisions when requirements are underspecified.

Document major decisions.

Do not install system-wide dependencies with elevated permissions without explicit authorization.

Do not delete unrelated files or overwrite existing work unnecessarily.

If the repository already contains code, inspect it before making changes and preserve unrelated modifications.

If a task is too large for one development session:

1. Complete the current coherent milestone as far as possible.
2. Keep the project buildable.
3. Record unfinished work.
4. Update the roadmap.
5. Explain the exact next steps.

Do not leave the repository in a deliberately broken intermediate state.

---

# 38. FINAL REPORT

After implementation, provide a detailed development report.

Include:

### Implemented features

Describe actual working functionality.

### Repository structure

Explain the most important directories and modules.

### Build instructions

Provide the commands needed to build and run the project.

### Tests

Describe the tests that were executed and their results.

### Rendering verification

State whether Vulkan rendering was actually tested on a GPU.

### Windows compatibility

State whether Windows was compiled, CI-configured, or only architecturally prepared.

Do not confuse these levels of verification.

### Architectural decisions

Explain any important deviations from this specification.

### Known issues

List unresolved technical problems.

### Next milestone

Recommend the next concrete development stage.

---

# 39. FINAL PRIORITY

Remember that VoxelCraft is fundamentally a game project.

The long-term goal is a Minecraft-inspired sandbox that people can actually enjoy playing.

The architecture should be robust, but it must not prevent rapid gameplay development.

A working voxel world with correct rendering, clean code, and a clear expansion path is more valuable than a huge theoretical engine containing dozens of incomplete systems.

**Your immediate objective is to build VoxelCraft 0.1: a working, cross-platform-oriented C++23/Vulkan voxel world foundation that can be compiled and run on Linux, with Windows support designed in from the beginning.**

Begin by inspecting the environment and repository.

Then implement the foundation, build it, test it, fix errors, and document the result.

Do not stop after producing a design document or implementation plan.

Actually write the code.
