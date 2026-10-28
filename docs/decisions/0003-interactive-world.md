# ADR 0003: initial v0.2 interactive world

Status: accepted for v0.2 development, following completed v0.1 implementation.

Implement a playable interaction slice before introducing persistence, inventories
or streaming. Keep fixed world residency and the existing Vulkan/SDL foundation.
Windows verification remains explicitly pending; prepared CI satisfies v0.1's
Windows build-preparation deliverable and does not imply a successful Windows run.

Use CPU grid DDA for selection, with a six-block client reach. Use separate game
functions for placement/destruction so storage, targeting, physics and rendering
can be tested without SDL/Vulkan initialization. Placement rejects occupied,
unavailable, out-of-height and player-overlapping cells. Existing content IDs
remain namespaced and edits store compact runtime IDs in the same chunk arrays.

World owns dirty chunk tracking, including face neighbors at boundary edits. The
client remeshes and the renderer updates resources by integer mesh origin. Empty
meshes remove resources, and all replacements wait for GPU idle before freeing
old buffers. This is synchronous and may hitch, but avoids prematurely building
a streaming/job/resource-retirement framework.

Use a fixed-step AABB player controller and swept axis collision instead of a
physics library or one rigid body per block. Separate opacity from solidity.
Keep the v0.1 free-fly controller behind F/--fly. Refuse switching into walking
inside solids. The normal spawn is on the generated surface; unknown chunk data
acts as a walking boundary. One-block stairs require jumping; movement polish
and manual playtesting are follow-up v0.2 work.

A small CPU-generated selection outline and screen-space crosshair share the
existing pipeline with a vertex-color flag. No new UI framework, inventory or
future milestone dependency is introduced. Extend strict hardware smoke testing
to cover real world edits, neighbor mesh replacement, player jump/landing and
GPU mesh add/remove, alongside the existing resize and teardown validation.
