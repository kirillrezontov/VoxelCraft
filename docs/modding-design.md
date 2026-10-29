# Modding foundation (v0.7 development)

The client discovers declarative packs and controlled Lua hooks. Content loads
before saved block/item names resolve. Gameplay and scripting do not depend on
SDL, Vulkan or EnTT. This is an initial API, not a stable extension ABI.

Try the included pack from the repository root:

```sh
./build/debug/bin/voxelcraft --mods-dir examples/mods --world-dir build/demo-world
```

Open E to select `demo:glowstone` in creative, or craft it in survival using one
dirt and one coal. C opens crafting; scroll to page two for the added recipe.
The pack counts world visits and glowstone placements in saved state.

Normal unbounded play discovers packs in `mods/` under the SDL user-data directory.
`--mods-dir PATH` selects a root containing pack subdirectories; `--no-mods` loads
none. Bounded runs ignore default mods, but allow an explicit root. Smoke tests
always use built-in content and reject `--mods-dir`. A nonexistent root is empty.
Directories without `mod.json` are ignored. Invalid packs stop startup with an
error; there is no silent partial registration.

## Pack layout and content

Each subdirectory has a required `mod.json`, optional `content.json` and optional
`init.lua`. The [demo manifest](../examples/mods/demo/mod.json) is a minimal example:

```json
{"id":"demo","version":"1.0.0","api":1,"dependencies":{}}
```

An optional `state_version` integer (1..65535, default 1) identifies the pack's
persistent state schema independently of its release version.

IDs start with a lowercase letter, contain lowercase letters/digits/underscores,
and have at most 48 characters. `voxelcraft` is reserved. Versions contain three
numeric components, each 0..65535, with at most 24 characters total. Dependencies
map mod IDs to **exact** versions. Loading is deterministic: dependencies first,
then lexicographic order for independent packs. Missing or mismatched dependencies,
cycles, duplicate IDs, unsupported APIs and unknown JSON fields fail explicitly.

Content is an object with optional arrays `textures`, `blocks`, `items` and
`recipes`. All textures load before blocks; all blocks before items and recipes.
Definitions must use their pack's namespace. Runtime IDs remain compact and are never saved.
See [demo content](../examples/mods/demo/content.json) for a working recipe/block.

- Blocks: required `id`; optional RGB `color` (three 0..1 values), `solid`,
  `opaque`, `fluid`, `emission` (0..15), `opacity` (0..1, excluding zero), and
  either `texture` or six `face_textures`. Transparent opacity requires
  `opaque:false`. Face order is +X, -X, +Y, -Y, +Z, -Z.
- Texture names resolve declared namespaced PNG assets or built-in atlas tiles: `white`, `stone`, `dirt`,
  `grass_top`, `grass_side`, `log`, `log_top`, `planks`, `leaves`, `cobblestone`,
  `coal_ore`, `iron_ore`, `sand`, `sandstone`, `snow`, `water`, `lamp`.
- Each registered block automatically receives a same-name placeable item.
  Additional `items` must use distinct IDs. Fields are `id`, optional `block`,
  `stack_size` (1..64), `durability` (0..65535; nonzero requires stack size 1),
  `tool` (`none` or `pickaxe`), `tier` (0..16), and positive `mining_speed`.
- Recipes require a printable ASCII `name` (1..24 characters), `ingredients`
  (1..4 item-name/count entries, each count 1..64), `output` item name and `count`
  (1..output stack size). Recipes use the existing transactional inventory rules.
  All recipes appear in the paged crafting menu.
- Blocks may include `mining`: `seconds` (0.05..3600), `tool` (`none` or
  `pickaxe`), `tier` (0..16; nonzero requires a tool), `drop` (item name or null),
  and `drop_count` (1..64, requires a drop). Omitted fields use normal mod block
  defaults: one second and one same-name block item. Inadequate tools break
  slowly without yielding drops; full inventories preserve blocks and tools.

Textures use `{ "id":"demo:glowstone", "file":"textures/glowstone.png" }`.
PNG tiles must be exactly 16×16 pixels. Relative UTF-8 paths use `/`, stay inside
the pack, and cannot contain `..`, absolute roots, control characters, Windows
reserved punctuation or escaping symlinks. The demo includes a custom cyan tile.
The atlas appends tiles after the fixed built-in indices, packs into rows of up to
32 tiles, and duplicates edge/corner gutters. Terrain, transparent meshes and HUD
icons share the same final layout. PNG color channels convert to RGBA8; shader
block opacity still determines whether a block uses the blended terrain stream.

Limits: 32 packs, 128 texture/block/item/recipe definitions per pack, 4096 total
atlas tiles including built-ins, 128 total recipes including built-ins, 128 KiB
per JSON/Lua/PNG file, and 24 JSON nesting levels. PNG decoding has a 1 MiB
allocation budget per call, rejecting invalid dimensions and excessive inflate
data. Native PNG library calls are not wall-clock preempted. External models,
custom shapes, native extensions, hot reload and generation hooks remain planned.

## Lua API 1

`init.lua` returns a table with any of these functions:
`on_load`, `on_tick`, `on_place`, `on_break`, `on_craft`.
No other hook names are accepted. Each function receives an event table with
`x`, `y`, `z`, `id` and `dt`. Placement/break hooks report the changed block cell
and identifier **after** the player action succeeds; crafting reports the output
item and the player's current cell. These are notifications, not cancellable
pre-action hooks. Script edits do not recursively dispatch hooks.

`on_load` runs after world/player/inventory restoration and once per world open.
`on_tick` runs each client update with `dt` clamped to 0..0.1 seconds; menus do not
pause it. Load/tick positions default to zero and their IDs are empty.

The global `voxelcraft` table exposes:

| API | Result |
| --- | --- |
| `api_version` | Integer 1 |
| `get_block(x,y,z)` | Stable block name, or nil for unavailable/out-of-height data |
| `set_block(x,y,z,id)` | Boolean; stages an edit in resident data, within eight blocks of the player's body center; solid placement cannot overlap the player |
| `give_item(id,count)` | Boolean; stages 1..64 items for a living survival player if all fit |
| `message(text)` | Stages a HUD/log message, at most four per hook and 120 bytes each |
| `get_state(key)` | String value or nil for this pack's persistent state |
| `set_state(key,value)` | Boolean; stages a string value or deletes it with nil |
| `player_position()` | Three numbers: feet X, Y, Z |
| `is_survival()` | Boolean |

World APIs are available only inside hooks. Coordinates are integers within
±1e15. Reads observe earlier staged edits in the same hook. At most 64 edits
are staged per hook. State uses nonempty keys up to 64 bytes, values up to 256
bytes, at most 64 entries and 1 KiB encoded state per script; saved state across
all packs is capped at 8 KiB. Entries include four bytes of length overhead.

Hooks stage world edits, inventory, persistent state and messages. A Lua/API
error or exceeded budget discards that hook's changes, disables its script for
the session, logs/displays a diagnostic, and lets other scripts continue. The
player action triggering a hook is already committed. Lua globals are temporary;
only `set_state` data persists. Missing mods retain saved state and content names;
reinstalling a pack restores them. Same-schema version changes produce diagnostics.

## State migrations

Increase the manifest's `state_version` when its state format changes and return
an `on_migrate` function alongside ordinary hooks. Its event contains
`from_version`, `to_version`, `from_state_version` and `to_state_version`.
Only `get_state`/`set_state` are available during migration; world, inventory,
message and player APIs are rejected. Handle all supported older schemas in
one function, for example:

```lua
on_migrate = function(event)
    local v = voxelcraft
    if event.from_state_version < 2 then
        assert(v.set_state("visits_total", v.get_state("visits") or "0"))
        assert(v.set_state("visits", nil))
    end
end
```

All pack migrations and fresh Lua sessions commit together before `on_load`.
Failure, a missing required migration, or an installed schema older than the
saved one rejects startup and preserves the existing world snapshot/runtime.
Successful migrations persist on the next normal save and do not repeat on
reopen. Missing mods keep their schema for reinstallation. V6 payloads use schema
1; v1..v5 contain no mod state. This migrates string state only; content-key and
world/inventory conversions and strict mod-set pinning remain planned.

Lua 5.4.9 is hash-pinned and compiled as C++ so errors unwind callback RAII objects.
Each script has a 4 MiB Lua allocator limit and 100,000 VM instructions per
initialization/hook. Base, table, string, math and UTF-8 libraries are available;
file/process/package/debug access, dynamic loading, coroutines, protected calls
and metatable mutation/access are removed. Instruction counts bound VM execution,
not wall-clock time inside library calls. Limits are safeguards, not an OS security
boundary; use trusted local packs.


## Compatibility policy

API 1 is the development contract for this slice. Existing API-1 manifests,
content and hooks remain readable; additions use optional fields/hooks with
backward-compatible defaults. A breaking interpretation change must use a new
API number and be explicitly supported or rejected, never silently reinterpreted.
Unknown fields and API numbers fail early, so packs requiring a newer client
produce a clear load error. Pack versions describe releases; `state_version`
controls string-state conversion; player payload versions describe the save
encoding. None implies binary compatibility for native plugins, which are not
implemented. Real-world modded playtesting is still needed before a stable API
release promise.
