# 0009 — Bounded EnTT actors and reusable Vulkan model instances

Status: implemented development foundation, 2026-10-28.

EnTT 3.14.0 manages dynamic creatures and dropped items inside the CPU LivingWorld
service. Static blocks remain compact chunk arrays. Registry handles never cross
the save boundary: each actor has a monotonic stable u64 ID and a namespaced kind.
The public simulation API exposes values, not EnTT ownership or Vulkan resources.

The first rules are deliberately bounded: at most 32 creatures and 32 drops,
with automatic population below 16 creatures. Grazers wander and flee after hits;
stalkers chase/attack nearby survival players at night and wander by day. Bodies
use swept voxel AABBs, gravity, swimming rise and one-block obstacle jumps.
There is no pathfinding, creature-to-creature collision, breeding or ecosystem.
Far creatures despawn beyond 128 horizontal blocks. Drops are retained and frozen
outside resident terrain, including through saves; they do not expire. At the drop
cap, a killing blow is refused so its reward cannot be discarded. Full inventories
leave loot in the world. Held attacks use a cooldown, voxel-occluded ray targets,
tool damage and durability. Raw grazer meat heals three health when consumed.

Player payload v5 appends bounded, validated actor records and reads v1..v4.
Unknown creature types fail explicitly; unknown dropped item keys are retained as
missing content. Actor records and terrain share the same snapshot commit.
Simulation timer/random sequence is not saved, so AI behavior after load need not
repeat bit for bit. Creature health/motion and dropped stacks are persisted.

The renderer accepts generic local opaque models and value-based instance data.
Colored cuboid models are built by game content. Vertex sway amplitudes plus an
instance phase animate limbs; yaw rotates positions and normals. The Vulkan
renderer batches visible instances by model using instance-rate vertex input.
Terrain and HUD use identity instance zero. Instance data updates only after the
single frame fence, and capacity grows without per-frame terrain mesh uploads.
Model bounds include animation and conservatively cover yaw rotations. GPU
ownership/culling remain separate from EnTT and gameplay.

CPU tests cover AI/collision/combat/cooldowns/loot/full inventory/caps/food/save
migration and missing items. A strict SDL/Vulkan test follows and attacks a fleeing
grazer, checks tool wear and loot, collects/eats it, saves/reloads actor identity,
and exercises model culling and instance-buffer growth/removal. Human playtesting
and Windows execution remain pending.
