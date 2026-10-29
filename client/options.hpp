#pragma once
#include "engine/voxel/coordinates.hpp"
#include <filesystem>
#include <optional>
#include <string>

namespace vc::client {
struct Options {
    std::uint64_t seed{12345};
    int radius{3};
    bool radiusSpecified{},nameSpecified{};
    std::string connectHost,playerName{"Player"};
    std::uint16_t connectPort{25565};
    vc::voxel::WorldBounds bounds;
    int width{1280},height{720};
    bool fullscreen{},vsync{true};
    unsigned frameLimit{};
    bool smoke{}, help{}, requireValidation{}, fly{}, noSave{}, seedSpecified{}, minSpecified{}, maxSpecified{},noSettings{};
    std::optional<double> fieldOfView,mouseSensitivity;
    std::optional<bool> invertY;
    std::optional<bool> survival;
    std::optional<std::uint32_t> generatorVersion;
    std::optional<double> timeOfDay;
    bool freezeTime{},noMods{};
    std::filesystem::path screenshot, worldDirectory,settingsFile,modsDirectory,identityFile;
};
Options parseOptions(int argc,char** argv);
}
