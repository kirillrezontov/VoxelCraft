# ADR 0007: optional survival using the shared sandbox simulation

Status: accepted for v0.4 development.

Keep creative play and add survival as an explicit saved mode. Both use the same
World, player collision, raycasting, placement, dirty meshes, streaming and Vulkan
renderer. Do not represent static resources as entities or give gameplay Vulkan
handles. Use a separate namespaced item registry; block items reference block
definitions while tools/sticks/coal do not need world blocks.

Use nine hotbar and 27 backpack stacks. Quantities, durability and names are
serialized explicitly in player payload v3, retaining v1/v2 creative migration.
Inventory adds/transfers and crafting operate transactionally so missing material,
full output space or failed placement never consume data. Unknown item identity
and durability are preserved as missing content.

Mining accumulates work while the same captured edit/tool/target remains active.
The recapture click is not armed. Rules define duration, drops and required tool
tiers. Check output capacity before removing a block; then apply tool wear. Use a
renderer-owned generic progress bar with persistent meshes and push constants,
rather than rebuilding HUD buffers for every progress update.

The player reports resolved landing speed, and survival applies fall damage.
Unavailable streamed contacts freeze vertical motion instead of becoming real
grounding/impact events. Keep inventory on death in this foundation; R searches
near the origin for a clear supported spawn, respecting edited cells. No dropped
item entities, hunger, creatures, combat or smelting are implied.

New-world generator v2 adds seeded trees and coal/iron deposits in global
coordinates. Keep v1 unchanged for existing saves; never switch an existing world's
generator automatically. Built-in blocks remain ordinary compact cells. Further
terrain changes require new generator versions, not silent changes to old worlds.
