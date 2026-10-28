#pragma once
#include <iostream>
#include <string_view>

namespace vc {
inline void log(std::string_view message) { std::clog << "[VoxelCraft] " << message << '\n'; }
}
