#pragma once
#include "engine/renderer/texture.hpp"
#include <filesystem>

namespace vc {
// Only 16x16 PNG tiles, at most 128 KiB compressed and 1 MiB decoder allocation.
ImageRgba loadPngTile(const std::filesystem::path& path);
}
