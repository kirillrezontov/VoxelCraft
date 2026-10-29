# Multiplayer development foundation

The same CPU world, player collision, day cycle, creatures, survival and Lua
services run in a headless server. The Vulkan client sends input intent and
inventory requests; the server owns positions, mining time, damage, recipes,
item quantities and terrain edits. Local single-player remains available.

## Start a server and join

From the repository root after building the default targets:

```sh
./build/debug/bin/voxelcraft-server --world-dir worlds/friends --bind 0.0.0.0 --auth-file /private/identities.txt
./build/debug/bin/voxelcraft --connect 127.0.0.1 --name Alice --identity-file /private/alice.token
```

Use the server's LAN address on another computer, for example
`--connect 192.168.1.10:25565 --name Bob`. Both computers need identical game and
mod content. Names contain 1..24 ASCII letters, digits, underscores or hyphens.
Case-insensitive names select saved profiles; simultaneous duplicate names are
rejected. IPv6 endpoints with a port use brackets: `--connect '[::1]:25565'`.

The default bind address is `127.0.0.1`, port 25565. `--bind 0.0.0.0` enables IPv4
LAN connections. Configure the host's firewall as needed. The server supports
eight connected players and 32 saved profiles. Names identify local/LAN profiles;
there is no account service, encryption or internet hosting
service. Profile-specific credentials can bind each persistent name to its owner.
An external bind requires credentials or explicit `--trusted-lan`. Loopback without
credentials is intended for local development; `--trusted-lan` permits profile name
reuse by anyone on that trusted network and must not be used with untrusted peers.

Create a survival world with:

```sh
./build/debug/bin/voxelcraft-server --world-dir worlds/survival --survival
./build/debug/bin/voxelcraft --connect 127.0.0.1 --name Alice
```

The saved world mode resumes automatically. Server `--creative` or
`--survival` selects the default for legacy/new profiles; explicit per-profile
command modes persist independently while retaining counted inventories. Creative flight is disabled
in survival. Mining, crafting, tool durability, backpack transfers, food, creature
combat/loot, fall damage, death and R respawn use server authority. Each player
has separate health and inventory; death keeps items. Inventories are sent only
to their owner. Connected clients use the server's mode, seed, bounds, time,
generator and streaming radius; local flags that conflict with those are rejected.

## Content and scripts

Pass the same pack directory to both processes:

```sh
./build/debug/bin/voxelcraft-server --world-dir worlds/modded --mods-dir examples/mods
./build/debug/bin/voxelcraft --connect 127.0.0.1 --name Alice --mods-dir examples/mods
```

A handshake checks protocol and a deterministic content fingerprint including
block/item/recipe/harvest definitions, pack versions/schemas and atlas pixels.
Files are not downloaded automatically. Saved missing content retains its names
and magenta fallback rendering. Scripts execute on the server; connected clients
only register matching content and render the resulting state.

Load/tick hooks run once per server world/tick. Tick hooks use the first connected
player as their inventory/player context, or the server anchor when empty. Action
hooks use the acting player's context. All connected player bodies protect solid
script/block placement. Actor simulation uses nearby living survival players for
chasing/damage/loot and keeps independent attacker cooldowns.

## Saving and shutdown

The server defaults to `worlds/server` relative to its working directory.
`--world-dir` selects another directory, `--no-save` keeps a temporary world.
F5 requests a server snapshot, throttled to one request per five seconds.
Autosave runs every 60 simulation seconds. A snapshot atomically contains edits,
all saved profiles, actors, world time and mod state. Ctrl-C/SIGTERM saves and
stops; an unbounded server remains running if that save fails so it can retry.
The OS session lock prevents the offline client and server opening one world
for writing simultaneously.

An offline player's pose/items seed the first named profile. Later names start
with a clear spawn and empty survival inventory. Named profiles remain retained
when opening that world offline; offline play uses its independent base player.

`--ticks N` performs a bounded 60 Hz run, temporary unless `--world-dir` is
explicit. `--port 0` asks the OS for an available port, printed in the log.
`--no-creatures`, `--radius 1..6`, `--seed`, `--min-y`, `--max-y`, and
`--generator-version 1..3` support reproducible worlds. Run `--help` for options.
On Windows use `voxelcraft-server.exe` and `voxelcraft.exe` in the configuration's
binary directory. Windows compilation/runtime are not verified in this session.

## Synchronization and current limits

The host ticks at 60 Hz and sends player/actor snapshots at 20 Hz. Each client
receives near-first palette/RLE chunks with data revisions and explicit unloads;
the server streams the union of player regions. The connected camera smooths
authoritative motion, while look input responds locally. There is no local
movement prediction yet, so latency is perceptible on slower connections.

Portable TCP is nonblocking after connection, with a bounded connect timeout
(OS hostname resolution is synchronous). Frames are limited to 4 MiB, per-peer
buffering to 8 MiB, queued frames to 256, and command rate to 120/s with a 240
burst. Malformed or slow peers disconnect without stopping the host. The network
block palette is limited to 8,192 names. The limits bound transport memory but
do not provide an internet-facing security guarantee.

Terrain generation, mesh construction and snapshot I/O use bounded workers;
lighting and snapshot capture can still hitch. Player bodies do not physically collide and player-versus-player damage
is not implemented. Save/profile administration, chat, prediction, interpolation
of creature animation between snapshots, bandwidth tuning, long-duration network
soaks and human LAN playtesting remain follow-up work.

CPU tests run two real loopback sessions for shared edits, persistence, invalid
peers and survival transactions. Strict SDL/Vulkan tests cover visible avatars,
custom textures, input intent and gameplay. See the
[timed session record](development-session-five-hour-2026-10-29.md) for the latest
executed build/test counts and limits.


## Private-server identity (v0.8.1)

Use protocol-3 clients and servers together. To protect existing profiles, create a
private server file with one `NAME TOKEN` pair per line; names are case-insensitive.
Use a distinct random 256-bit token for each player, encoded as 64 lowercase hex
digits. Generate with `python3 -c 'import secrets; print(secrets.token_hex(32))'`
or an equivalent cryptographic generator. Each player's `--identity-file` contains
only their token. Send it privately and restrict credential file permissions.
Credentials are read once at startup and never included in logs, Welcome packets,
world snapshots, content fingerprints or mod state. They survive world upgrades
independently; back up the server credential file separately. Restart with a
changed token to revoke access. Authentication failure cannot create a profile or
change its saved pose/items. Existing unlisted names are retained but cannot join
until configured. Simultaneous duplicate names remain rejected.

This is a practical private-server identity mechanism, **not secure public-internet
hosting**. TCP transmits credentials without encryption and does not authenticate
the server; use a trusted LAN or authenticated encrypted private tunnel. A network
observer or impersonated endpoint can steal a token. `--trusted-lan` explicitly
permits the old unauthenticated model and should only be used among trusted peers.
Local loopback development also permits it; another local user can select a name.
The credential file is not a general account database; there is no online account,
password recovery or live reload. Do not expose it with the world or mod assets.

Idle input stops movement after two simulation seconds and disconnects after 30;
clients send heartbeats even while menus are open. TCP connection timeout is
bounded, welcome timeout is 30 wall seconds, and an established client fails after
15 wall seconds without any server packet. Hostname resolution still uses the OS
synchronous resolver. Protocol mismatches produce a clear rejection. Clean server
shutdown saves first, then attempts one bounded notification flush; a stalled peer
cannot hold shutdown open. Reconnect by relaunching the client with the same name
and identity file. Client mirroring is capped at 2,048 resident chunks, above the
maximum ordinary streaming region; there is no unbounded remote allocation.

Run `python3 tests/network_stress.py build/release/bin/voxelcraft-server --iterations 1000`
for fragmented invalid-session stress, and `build/release/bin/vc_network_soak 1800`
for eight authenticated clients, edits, reconnects, save cycles and restart checks.
Only bounded runs recorded in the stabilization report have actually executed.

## v0.8.2 commands and operators

Protocol 4 explicitly rejects protocol 3 at handshake. Commands, replies, modes,
authoritative creative hotbars and pose revisions use bounded snapshots. Camera
reconciliation snaps on a teleport revision, including short teleports. Each
profile may have its own survival/creative mode; counted items and tool durability
are private and preserved. Custom respawn points are saved and checked again at
respawn, with safe fallback if terrain later obstructs them.

Use `--auth-file PATH --admin Alice` for the initial authenticated operator. Roles
persist in `<world>/operators.vcp`, or `--permissions-file PATH`. Console `op`/`deop`
modify roles; anonymous names never inherit them. `--no-console` disables stdin;
EOF does not stop the host. `/` in the graphical client and stdin use the same
registry. See the [complete command/permission reference](v0.8.2-commands.md).
