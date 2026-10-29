# World format — version 1

Implemented: 32³ chunks with contiguous 16-bit runtime block IDs, signed 64-bit
world/chunk coordinates and configurable inclusive height bounds (default -512..511).
Array order is `x + 32 * (z + 32 * y)`. Negative coordinates use floor division:
world X=-1 is chunk X=-1/local X=31. Partial boundary chunks are supported.
Missing chunks are distinguishable from resident air and block walking/placement.

## Snapshot layout and commit

```text
world-directory/
  .session.lock
  snapshots/
    12/
      world.vcw
      r.-1.0.0.vcr
      r.0.0.0.vcr
    13/
      world.vcw
      r.-1.0.0.vcr
      r.0.0.0.vcr
```

The main thread holds an OS-owned exclusive session lock. The empty lock file is
retained; its existence does not indicate an active lock. The OS releases the lock
on process termination. Platform-specific locking stays in `engine/platform`.

A save writes a complete snapshot into `snapshots/.writing-N`, closes and syncs all files,
then renames the directory to the fresh revision `N`. Readers use the highest
canonical unsigned integer directory name and ignore staging directories. Thus an
interrupted save never exposes a half-written committed revision. The two most
recent revisions are retained; older cleanup is best-effort. This is atomic
visibility for ordinary local filesystems, not a guarantee of power-loss durability
or unusual network filesystem semantics. v0.8.1 adds file fsync and staging/parent
directory barriers on POSIX. Windows uses FlushFileBuffers and write-through
publication to a fresh name; no portable directory flush equivalent is claimed.
Any synchronization error is reported as a failed save. The preceding revision
remains available; a failure after publication can leave a valid new revision,
which is safe to retain and retry. No format bytes or version numbers change.

A corrupt latest revision fails explicitly. To recover: close all clients, back
up the entire world directory, then move the damaged highest revision outside
`snapshots`. The preceding revision will become current. Never delete the lock
file while another process uses the world. No automatic rollback is performed.

## Binary encoding

All integers are explicitly little-endian; signed coordinates are encoded as
two's-complement bit patterns. No C++ struct layouts are written. Each metadata
and region file ends in an unsigned 64-bit FNV-1a checksum of all preceding bytes
(offset basis 14695981039346656037, prime 1099511628211). This detects accidental
corruption and is not an authentication mechanism. Readers reject truncation,
trailing data, unsupported versions and invalid structure even with a valid checksum.

`world.vcw` payload, in order:

| Field | Encoding |
| --- | --- |
| Magic | 8 bytes, `VCWORLD1` |
| Format version | u32, currently 1 |
| Terrain generation version | u32, currently 1..3 |
| Seed | u64 |
| Minimum/maximum inclusive Y | two i64 |
| Total chunks / region files | two u32 |
| Region coordinates | X/Y/Z i64 for each region |
| Game payload length / payload | u32 length, then bytes, maximum 524,288 |
| Checksum | u64 |

Regions contain 4×4×4 chunks. Region coordinates use floor division by four on
chunk coordinates, including negative values. Filename is derived only from those
integers: `r.X.Y.Z.vcr`; manifests cannot supply arbitrary file paths.

Each region payload:

| Field | Encoding |
| --- | --- |
| Magic / format version | `VCREGION` (8 bytes), u32 version 1 |
| Region coordinate | three i64 |
| Palette count | u32, 1..65,536 |
| Namespaced palette entries | u16 byte length plus identifier bytes, each ≤256 |
| Chunk count | u32, 1..64 |
| Each chunk coordinate / run count | three i64, then u32 |
| Each run | u16 palette index, u16 nonzero length |
| Checksum | u64 |

Runs fill exactly 32,768 cells in the array order above. Full uniform chunks use
one run; generated columns compress well. Runtime numeric IDs never cross the
save boundary. Unknown names become magenta solid cubes using their original
identifiers; resaving preserves them. Restoring content with the same name resolves
to the restored definition on a subsequent load. Block states beyond a type ID
are not supported by version 1.

Current implementation limits: 8,192 chunks per snapshot, 32 MiB per region file,
1 MiB manifest, and 512 KiB game payload. These are validated before large reads;
world memory still scales with resident chunks. Empty worlds are valid.

The game-owned payload starts with `VCPLAYER`, then u32 version 10, five IEEE-754
binary64 values (camera XYZ, yaw, pitch), u32 flight flag (0/1), a u32 selected slot
(0..8), and nine length-prefixed hotbar identifiers (empty string means empty slot).
Version 3 then stores u32 survival-mode flag, u32 health (0..20), and 36 item stacks.
Each stack contains a length-prefixed namespaced item key, u16 count and u16
remaining durability; empty keys require zero count/durability. Definitions validate
stack limits and durability on restore. Unknown item names and durability are
retained as missing-content items. Item runtime IDs are never serialized.
Version 4 appends binary64 simulation seconds in [0,1200), defaulting to noon
(300 seconds) for older saves. Lighting is derived and never serialized.
Version 5 appends a u32 actor count and actor records. Each record stores a u64
stable ID, namespaced kind string, six binary64 position/velocity coordinates,
binary64 yaw/age/animation phase, u32 health, and a namespaced item string plus
u16 count/durability for dropped items. Creatures require empty item fields.
Current caps are 32 creatures and 32 drops. IDs must be nonzero and unique;
invalid motion/health/types/counts fail explicitly. Unknown item keys are retained,
while unknown actor kinds fail. Older payloads start with no actors. AI cooldowns,
random state and spawn timers are not serialized. See
[ADR0009](decisions/0009-living-world.md).
Version 6 appends a u32 mod count (maximum 32), then for each mod two strings
(ID and version), a u32 state-entry count (maximum 64), and string key/value pairs.
Duplicate IDs/keys, invalid versions, oversized keys/values and aggregate state
over 8 KiB fail explicitly. Missing mods retain these records on resave; version
changes are diagnosed. Older payloads have no mod records. See
[modding](modding-design.md) and [ADR0010](decisions/0010-modding-foundation.md).
Version 7 adds a u32 state-schema version (1..65535) after each mod's ID/version
and before its state-entry count. V6 records use schema 1. Schema upgrades run
transactional state-only Lua migrations; downgrades fail explicitly. See
[ADR0011](decisions/0011-mod-state-migrations.md).
Version 8 appends a u32 named-profile count (maximum 32). Each profile contains
a name string, five binary64 pose values, u32 flight flag, u32 selected hotbar
slot and nine identifier strings. Names use 1..24 ASCII letters/digits/underscore/
hyphen and are case-insensitively unique. V8 profiles start with full health and
empty survival inventories.
Version 9 appends to each profile u32 health (0..20), followed by 36 saved item
stacks using the same string/u16/u16 layout as the base player. World mode remains
the base player's survival flag. Offline saves retain named profiles and server
saves retain the offline base player. Profile velocity and transient mining/AI
state are not persisted. The game-data bound is now 512 KiB to fit all profiles;
metadata/region format stays version 1 and previous snapshots remain readable.
See [ADR0013](decisions/0013-authoritative-multiplayer.md).
Version 10 appends u32 per-profile mode (0 inherits world default, 1 creative,
2 survival), then a u32 spawn-present flag and three binary64 feet coordinates
when present. After all profiles it appends the base player's spawn flag/coordinates.
Namespaced hotbar strings now also resolve ordinary/tool items; unresolved names
remain missing-item assignments. Region/metadata schemas and item identifiers are
unchanged. Versions 1..9 load with no custom spawn and inherited profile mode.
Old versions cannot read newly written player payload 10; preserve a backup before
downgrading. The settings payload separately advances from 1 to 2 to append UI scale.

Version 1..9 player payloads remain readable: v1 migrates its selected material into
slot zero, and v1/v2 migrate to creative mode with full health and empty survival
stacks. Camera inputs are validated for finiteness and the
supported ±1e15 physics range. Velocity is not persisted; walking resumes under
gravity. An obstructed/unavailable walking position resumes in flight for recovery.

## Generation and future evolution

Generator version 1 uses explicit seed hashing and coherent interpolated value
noise at three scales. It independently samples global positions, then fills
grass at the surface, three dirt layers below, stone deeper and air above.
Saved chunks preserve their exact cells; missing chunks use the recorded seed and
generation version. Floating interpolation is deterministic within a build;
bit-identical generation across all compilers/FP modes is not yet guaranteed.

Generator version 2 preserves those terrain heights and layers, then adds seeded
global-grid trees and underground coal/iron veins. It uses complete-tree headroom
checks for partial height bounds and global coordinates across chunk seams. Old
worlds retain version 1; new worlds default to version 3. Explicit new-world
generator selection is available in the client. A saved world's generator cannot
be changed through CLI flags.

Generator version 3 adds climate biomes, mountains/snow, river valleys/static
water, 3D cave fields, biome tree density and deterministic small cobblestone ruins
with lamp rewards. Versions 1 and 2 keep their existing samples.

The current client saves modified chunks and legacy retained chunks, including
edited chunks outside residency. Untouched generated terrain is regenerated from
the recorded seed/version; exact saved chunk data is never regenerated over edits.
Storage APIs still accept full worlds, so older full snapshots remain readable.
Snapshot capture runs on the main thread; serialization and durable writes use
one bounded background worker with an owned world/registry/game-data snapshot.
Shutdown completes required writes or reports failure, retaining live edits for
retry. Incremental/region journaling and lazy on-disk paging remain planned. The compact in-memory edit cache
grows with edited chunks; the 8,192-chunk snapshot cap applies to retained edits.
Palette compression may later replace runtime chunk arrays behind Chunk::get/set.
Future format versions must explicitly describe block states, composite cells
and multiblocks; they are not implicitly serialized today.

Composite cells remain planned as sparse component data with conceptual 16³
occupancy units per standard cell. Multiblocks will need stable instance IDs,
orientation, cross-chunk references and atomic placement/removal. Render, collision
and occupancy shapes remain separate architectural concerns.
