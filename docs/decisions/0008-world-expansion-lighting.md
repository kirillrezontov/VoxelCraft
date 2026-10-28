# 0008 — Versioned world expansion and derived voxel lighting

Status: implemented development foundation, 2026-10-28.

Generator v3 adds coherent climate biomes, river valleys/static water, mountains,
snow, intersecting 3D cave fields and small deterministic stone ruins. Generation
samples global coordinates so chunk boundaries agree. Versions 1 and 2 remain
unchanged for existing saves. The expanded initial surface band spans Y=-32..159;
normal radius-three residency is 294 chunks, plus player-height coverage as needed.

World lighting is derived, not serialized. Highest opaque cells determine direct
vertical sky visibility per column. Emissive blocks propagate a bounded level-15
flood through nonopaque resident cells. Chunk changes recompute affected sources
and invalidate affected meshes. This provides caves, roofs and warm craftable lamps
without claiming full sky-light diffusion, angled sun shadows or GI.

A CPU day cycle owns time and atmospheric colors; the renderer receives generic
sun, ambient, sky and fog parameters. World vertices carry normals, opacity and
sky/block light. Player payload v4 persists time and reads versions 1..3.

Opaque and translucent mesh indices are separate. Translucent chunks draw far to
near with depth testing, alpha blending and no depth writes. This is coarse chunk
ordering, not exact per-face order. Water is a static voxel, with swimming,
sinking/surface escape, underwater fog and fall cushioning. There is no fluid flow,
breath/drowning simulation, refraction or wave geometry.

Verification includes CPU generation/version/seam/day-cycle/lighting/swimming tests,
strict Vulkan day/night/lamp/transparent-only lifecycle tests and actual client
captures of rivers, desert, snow and a ruin. Human balance/playtesting and Windows
execution remain pending.
