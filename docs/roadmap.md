# Development roadmap

The maintained task checklist and long-term milestone definitions are in the
root [ROADMAP.md](../ROADMAP.md). This page links that single source of truth to
avoid diverging checklists.

v0.1 implementation is complete: the Linux viewer is built, unit-tested and
hardware-validated, and Windows CMake/CI preparation is present. Windows execution
is still unverified. The v0.2 interactive implementation provides a playable slice
of selection, block edits, chunk mesh updates, walking collision and jumping.
Broader manual control/collision playtesting remains pending. The v0.3 foundation
adds persistence, regional snapshots, player state, culling, bounded streaming,
creative inventory/hotbar and settings. The active v0.4 slice now adds timed mining,
item stacks, backpack/crafting, tools/durability, fall health/death/respawn and
versioned trees/ore generation.

The v0.5 foundation adds versioned biomes/caves/rivers/ruins, static water and
swimming, day/night, directional sun, sky visibility and emissive lamp lighting.

The v0.6 foundation adds EnTT creatures/drops, basic animal/enemy AI, combat,
loot/food, saved actors and reusable animated model instances.

Immediate follow-up: mod content/Lua foundation, plus manual playtesting and
Windows compilation when a runner is available. Later versions remain planned. See the original
[v0.1 verification](verification.md) and current [v0.2 verification](verification-v02.md).
The [timed session log](development-session-2026-10-28.md) records new work and checks.
