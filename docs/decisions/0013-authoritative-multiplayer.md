# ADR 0013: Shared headless simulation and bounded multiplayer transport

Status: accepted for v0.8 development.

Keep native socket and console-signal details in `engine/platform`. A CPU
framing/address module handles bounded partial I/O; the game protocol explicitly
encodes values in little-endian form. Never transmit C++ layouts, runtime object
handles or client-owned authoritative positions. The dedicated executable links
the CPU services without SDL/Vulkan. The existing local client remains playable.

Host owns world storage, generation, controllers, survival, creatures, day and
scripts. Clients request movement/actions, recipes and slot transactions. Only
Host validates reach, bodies, timing, tool wear, quantities and damage. Named
profiles persist independently, including inventories and health. A world-wide
mode selects survival/creative behavior. Private inventory snapshots are sent
only to their owner. This initially trusts LAN names; authentication/encryption
and administration are separate work.

Up to eight streaming centers share the terrain worker. Chunk data revisions
are process-local and differ from mesh-neighbor dirtiness. TCP carries compressed
chunk snapshots/unloads and 20 Hz player/actor state, bounded by frame/buffer/
command limits. A slow or malformed peer is removed without ending simulation.
The connected Vulkan client renders a mirror with local look and smoothed
authoritative positions; prediction remains planned.

Player payload v8 introduced named creative profiles. V9 extends each profile
with health and survival stacks; v1..v8 remain readable. Game payload capacity
increases to 512 KiB so all 32 worst-case profiles fit alongside actors/mods,
while keeping the 1 MiB manifest bound. Region format remains version 1.
Both offline and network saves preserve the other mode's stored profiles.

Test with real loopback sockets, independent clients, malformed/content/name
rejection, survival progression and save/reopen; add hardware SDL/Vulkan captures
and sanitizer checks. Windows and human LAN verification remain explicit gaps.
