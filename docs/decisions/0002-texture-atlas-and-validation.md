# ADR 0002: small texture atlas and strict graphics verification

Status: accepted for v0.1; supersedes the texture deferral in ADR 0001.

SPEC §7.2 and §29 prioritize a texture atlas after a reliable colored voxel
renderer. The Linux foundation works, so add a small atlas while Windows CI is
unavailable locally. Do not begin block interaction or collision in this step.

Use five deterministic C++-generated 16² tiles, padded by one replicated texel.
This delivers real sampled textures without an image decoding library, external
art requirements or a content file format. The engine accepts generic RGBA image
data; built-in tile assignments and pixels stay in game/content. CPU meshing
generates UVs and remains independent of Vulkan. Grass has distinct top, side
and bottom materials. Keep vertex tinting through a white fallback tile.

Upload once through a coherent staging buffer into a device-local sRGB image.
Use a combined image/sampler descriptor and nearest filtering. Keep the atlas
alive across swapchain recreation. Defer mipmaps and anisotropy; distant texture
aliasing is an explicit limitation rather than a new advanced graphics system.

Use a checksum-verified project-local Khronos layer instead of installing system
packages. Synchronization validation found an acquire/transition dependency bug
that visual testing missed. Correct the transition's source stage to chain to the
COLOR_ATTACHMENT_OUTPUT acquire wait. Add `--require-validation`, count reports
through shutdown, and offer a display/GPU-dependent CTest behind an OFF-by-default
option. Keep the normal CPU tests runnable without SDL or Vulkan initialization.
