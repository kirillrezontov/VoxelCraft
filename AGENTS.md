# VoxelCraft — Codex Instructions

## Project
VoxelCraft is a Minecraft-inspired, moddable voxel sandbox
game written in C++23 using Vulkan 1.3.

## Priorities
1. Working, playable software.
2. Cross-platform compatibility.
3. Clean modular architecture.
4. Performance.
5. Modding support.

## Platforms
- Development environment: Linux.
- Primary release target: Windows.
- Secondary release target: Linux.
- Use SDL3 for windowing and input.
- Use one shared Vulkan renderer for both platforms.
- Avoid platform-specific code outside the platform layer.

## Architecture
- Separate rendering, world storage, world generation,
  gameplay simulation, and platform code.
- Gameplay must not depend on Vulkan.
- Static blocks must not be ECS entities.
- Use compact chunk-based block storage.
- Use namespaced block identifiers.
- Avoid unnecessary abstractions and premature optimization.

## World
- Chunk size: 32x32x32.
- Configurable finite world height.
- Default vertical bounds: -512 to 511.
- Horizontally expandable world.
- Support negative world coordinates correctly.
- Prepare architecture for composite cells and multiblock objects.

## Development Rules
- Always inspect existing code before modifying it.
- Keep the project buildable.
- Implement real functionality, not placeholder implementations.
- Build and test after substantial changes.
- Fix compilation errors before proceeding.
- Do not silently substitute Vulkan with OpenGL.
- Do not implement future features prematurely.
- Write portable C++23 compatible with GCC, Clang, and MSVC.
- Do not install system packages without permission.

## Documentation
- Keep README.md updated.
- Keep ROADMAP.md updated.
- Document significant architectural decisions.
- Clearly distinguish completed features from planned ones.

## Completion
At the end of each task:
1. Summarize implemented functionality.
2. Report build and test results.
3. List known issues.
4. Identify the next development step.
