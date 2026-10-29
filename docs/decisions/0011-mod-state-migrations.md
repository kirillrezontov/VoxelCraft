# ADR 0011: Explicit mod state schemas and atomic migration

Status: accepted for v0.7 development.

Pack release versions and persistent state schemas serve different purposes.
An optional manifest `state_version` defaults to 1 and increases only when saved
state needs conversion. Same-schema pack version changes retain their existing
diagnostic and do not require a migration.

Player payload v7 stores each mod's schema; payload v6 records default to 1.
Missing mods retain that schema with their names/versions/state. Older payloads
remain readable. A saved schema newer than the installed pack fails startup,
rather than attempting an implicit downgrade.

Schema upgrades require `on_migrate`, called once with source/target versions and
schemas. It can read/write only its staged string state, using the same limits
and protected execution as ordinary hooks. Authors handle skipped schema versions
within that function. Fresh Lua VMs and copied pack state make the entire restore
transaction atomic, including Lua globals and diagnostics. Failure leaves the
previous runtime intact and the client never writes a migrated snapshot.

This does not yet migrate block identifiers, inventory types, actors, generation
or external assets. Those need explicit content migration designs and tests.
