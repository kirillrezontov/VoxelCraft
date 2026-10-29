#pragma once
#include "engine/renderer/mesh.hpp"
#include <array>

namespace vc {
// Grazer, stalker and dropped-item models, matching ActorView model indices.
std::array<Mesh,3> builtinCreatureModels();
Mesh playerAvatarModel();
}
