#pragma once
#include "engine/resources/catalog.hpp"
#include "engine/ui/widgets.hpp"

namespace vc {
// Semantic validation is host-owned; malformed overrides fall back to builtin.
ui::Theme loadResourceTheme(ResourceCatalog& resources,std::vector<std::string>& diagnostics);
}
