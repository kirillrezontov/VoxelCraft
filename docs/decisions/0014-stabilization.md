# ADR 0014 — v0.8.1 compatibility and stabilization boundaries

Status: accepted for release candidate implementation; platform acceptance pending.

The v0.8 foundation already has bounded terrain/mesh workers and an owned snapshot
writer. Reuse them rather than introduce a generic scheduler. Main-thread world,
registry, lighting and player state are copied before worker ownership; mesh
tickets reject obsolete results and completed jobs retain capacity until consumed.
Snapshot shutdown explicitly awaits the latest save and preserves retry on failure.

Keep metadata/region format 1, player payloads 1..9, terrain generators 1..3 and
mod API 1. Durability improvements belong in the platform layer: file barriers,
staging-directory sync, publication to a fresh revision, and parent sync on POSIX;
FlushFileBuffers plus write-through publication on Windows. Never silently roll
back a corrupt newest snapshot. The previous revision is retained for explicit
operator recovery, tested in an isolated directory. Filesystem/drive semantics
still limit power-loss guarantees; no transactional database guarantee is implied.

Names alone cannot protect disconnected profiles. Use externally provisioned,
per-name random tokens, bounded credential files, case-insensitive lookup and
validation before any profile mutation. This avoids changing persistent payloads
or giving one shared password access to every saved name. Upgrade wire protocol
to 3 and clearly reject old clients. Preserve unauthenticated local development;
require explicit --trusted-lan for external unauthenticated binds. Plain TCP is
supported only on trusted LANs or secured private tunnels. This does not provide
TLS, server authentication, accounts or public internet security.

Merge only ordinary opaque cube faces with equal block and illumination keys.
Allocate slice masks only where visible faces exist. Keep the culled unit-face
reference mode for geometry/UV tests and paired benchmarks. Optional per-vertex
atlas bounds allow repeated local UVs on merged quads, preserving pixel density,
material edges and grass orientation. Non-opaque/transparent cubes keep their
specialized path. Palette compression remains deferred: each resident chunk uses
65,536 bytes of block payload, retained edits already use bounded-per-chunk RLE/raw
fallback, and a format/storage rewrite is not justified by this stabilization run.
The existing 8,192 saved-chunk limit remains explicit; lazy paging is future work.

With one frame in flight, the submitted frame fence covers every terrain/model/HUD
buffer reference. Wait for this fence before replacement, without waiting for
presentation of unrelated swapchain images. Continue device-wide waits at swapchain
recreation/shutdown, and retain per-image presentation semaphores. Do not redesign
renderer frames/resources without measured need. GPU allocation/upload remains on
the main thread and full-world lighting/snapshot capture can still cause stalls.

CPack components produce separate client/server archives with redistributables,
shaders, demo content, documentation and third-party notices. Base textures/content
are executable-generated. Do not bundle the Vulkan driver/loader as a renderer
substitute. Linux glibc baseline is the build host; CI targets Ubuntu 24.04.
Windows embeds a UTF-8 active-code-page manifest for Unicode CLI paths on Windows
10 1903+. Windows CI/runtime results and human LAN acceptance must be recorded
before stable; Linux compilation alone cannot satisfy those criteria.
