#pragma once
#include "client/options.hpp"
#include "game/settings.hpp"

namespace vc::client {
struct Preferences { GameSettings settings;std::filesystem::path path; };
Preferences loadPreferences(const Options& options);
}
