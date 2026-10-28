# ADR 0005: versioned regional snapshots for v0.3

Status: accepted for the persistent-sandbox development slice.

Persist real edited chunks and player state before implementing eviction. Store
namespaced identifier palettes and RLE cell runs in 4³-chunk regions, not numeric
runtime IDs or C++ memory layouts. Explicit little-endian encoding, version fields,
checksums and bounded readers make corruption and compatibility failures visible.
Unknown content retains its name with solid magenta fallback geometry.

Use a complete immutable snapshot per save for the current finite resident world.
Write all regions and a manifest containing the opaque versioned game payload,
close the files, then rename the staging directory to a fresh revision number.
This commits world edits and player state together without relying on replacement
rename behavior differing between Windows and Linux. Retain the previous revision;
never silently roll back a damaged newest revision. No fsync durability claim.

Hold an exclusive OS-owned session lock, isolated in `engine/platform`, to avoid
concurrent writers. POSIX flock and Windows exclusive file handles release on
process termination. The empty lock file remains; deleting it would permit inode
replacement races on POSIX. The CPU-only storage build needs neither SDL nor Vulkan.

Normal play saves under SDL's user-data path. Explicit paths select other worlds;
bounded/smoke runs are temporary unless persistence is explicitly requested.
F5, 60-second autosave and exit saving are implemented synchronously. Region
journaling, background saves and memory-bounded live streaming are later concrete
improvements, not simulated by discarding edits or regenerating over saved chunks.
