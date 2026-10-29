# ADR 0012: Bounded PNG tiles in one shared content atlas

Status: accepted for v0.7 development.

Support real external block textures without introducing a renderer-specific mod
API. ContentAtlas owns CPU tile data and name resolution, preserving built-in
indices and appending mod tiles. Pack textures load before blocks resolve face
materials. The final atlas packs up to 4096 tiles in rows of at most 32, with
edge/corner gutters. Terrain, transparent streams and HUD icons use that exact
layout; Vulkan receives only the final RGBA image.

Use a commit/SHA-256-pinned stb_image header compiled for PNG only. Exact 16×16
dimensions, 128 KiB input and a 1 MiB decoder allocation budget keep asset work
bounded, including malicious inflate data. Filenames are relative UTF-8 paths
contained in their pack, rejecting traversal/escaping symlinks. This remains
trusted local content loading, not an OS security sandbox.

No hot atlas updates, animated textures, texture arrays, external models or
non-cube material mapping are introduced. Stable names persist through block
identifiers; numeric tiles and packed UVs are rebuilt at startup. Missing packs
continue to use the existing solid magenta block fallback.
