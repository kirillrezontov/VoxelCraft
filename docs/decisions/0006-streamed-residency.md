# ADR 0006: bounded streamed residency with retained edited chunks

Status: accepted for v0.3 development.

Stream the player's horizontal radius and vertical neighborhood while retaining
the generator's surface band for above-ground views. The CPU worker remains
bounded; the main thread integrates at most two generated/cache chunks per frame.
Request order is near-first. Discard stale results after movement/teleportation,
and always prefer a retained edited chunk over a generated result.

World owns modified flags and dirty mesh origins. Removing a chunk invalidates
its GPU resource and resident face neighbors. A separate near-first CPU mesh queue
builds two chunks/frame (four for an edit), and removal messages bypass that CPU
budget. Vulkan resources remain exclusively on the main thread.

Before eviction, cache edited or legacy-saved chunks as runtime-ID RLE, falling
back to a raw cell array when RLE would be larger. Runtime IDs are process-local
and stable during play; persistence still encodes namespaced palettes. Untouched
generated chunks need not be retained. Saves merge the compact retained cache
with current resident edits into a transactional snapshot. Older full snapshots
are treated as retained content so migration cannot discard previous edits.

This bounds resident terrain, not all possible world-edit memory. The compact
edit cache grows with edited chunks and snapshots currently cap at 8,192 saved
chunks. Loading still reads all saved chunks before compressing distant ones.
Future lazy disk paging/incremental commits can remove those limits without
discarding current data or changing the renderer/gameplay boundary.

Meshing, snapshots and GPU replacement are synchronous but budgeted where possible.
Do not claim fully asynchronous rendering or constant total memory. Strict hardware
tests compare final GPU counts against a complete CPU remesh after horizontal and
vertical travel, eviction, edits, save/reload and revisit.
