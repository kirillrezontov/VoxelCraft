# ADR 0001: v0.1 foundation

Status: accepted for v0.1; texture deferral superseded by ADR 0002.

Use one Vulkan 1.3 renderer and SDL3 window/input/surface integration on Linux and
Windows. Vulkan dynamic rendering removes render-pass/framebuffer objects while
keeping the minimum device contract explicit. There is no alternate graphics
backend. SDL owns platform differences; gameplay does not consume SDL events.

Use 32³ chunks with compact IDs, signed 64-bit coordinates and configurable
finite vertical bounds (-512..511 by default). A fixed, modest surface-region
viewer is permitted by SPEC §28H and gives a working foundation before streaming.
Palette storage, composite cells and multiblock records remain future work.

Use a registry with stable namespaced identifiers and compact runtime IDs. This
keeps content separate from storage and prepares for future mods without building
a Lua runtime or serialization prematurely.

Keep CPU meshes free of Vulkan ownership. Use per-chunk local vertices and
camera-relative offsets, direct host-visible uploads, one frame in flight, and
one indexed draw per nonempty chunk. Start with vertex colors and constant face
shading; defer the optional texture atlas until the foundation is validated on
Windows and with validation layers. This is a deliberate scope choice under
SPEC §7.2/§28C, not a claim that texture support is finished.

Use target-based CMake, release-pinned FetchContent dependencies, CTest and
GoogleTest. Provide an explicit installed-dependency mode for offline development
and a CPU-only mode for testing without graphics libraries. GCC, Clang and MSVC
share the same portable C++23 source. Avoid optional ECS/UI/scripting dependencies
and empty source modules. Diagnostics in logs/title satisfy SPEC §28I without
adding ImGui before it is needed.
