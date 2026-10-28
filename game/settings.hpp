#pragma once
#include <filesystem>
#include <optional>

namespace vc {
struct GameSettings {
    double fieldOfView{75},mouseSensitivity{0.0025};
    bool invertY{};
    void validate() const;
    bool operator==(const GameSettings&) const=default;
};
struct LoadedSettings { GameSettings settings;bool recoveredBackup{}; };
std::optional<LoadedSettings> loadSettings(const std::filesystem::path& path);
void saveSettings(const std::filesystem::path& path,const GameSettings& settings);
}
