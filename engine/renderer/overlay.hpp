#pragma once
#include "engine/renderer/mesh.hpp"

namespace vc {
Mesh selectionOutline();
Mesh crosshairMesh();
Mesh progressBarMesh(glm::vec3 color);
}
