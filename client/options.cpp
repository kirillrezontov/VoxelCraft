#include "client/options.hpp"
#include "engine/core/paths.hpp"
#include "engine/network/address.hpp"
#include "engine/voxel/storage.hpp"
#include "game/player/profile.hpp"
#include "game/settings.hpp"
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace vc::client {

template<class T> T number(std::string_view value) {
    T result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) throw std::invalid_argument("Invalid number: " + std::string(value));
    return result;
}
Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        const auto value = [&]() -> std::string_view {
            if (++i >= argc) throw std::invalid_argument("Missing value for " + std::string(flag));
            return argv[i];
        };
        if (flag == "--seed") { options.seed = number<std::uint64_t>(value()); options.seedSpecified=true; }
        else if (flag == "--radius") { options.radius = number<int>(value());options.radiusSpecified=true; }
        else if (flag == "--min-y") { options.bounds.minY = number<std::int64_t>(value());options.minSpecified=true; }
        else if (flag == "--max-y") { options.bounds.maxY = number<std::int64_t>(value());options.maxSpecified=true; }
        else if (flag == "--world-dir") {
            options.worldDirectory=vc::utf8Path(value());
            if(options.worldDirectory.empty()) throw std::invalid_argument("--world-dir must not be empty");
        }
        else if (flag == "--no-save") options.noSave=true;
        else if (flag == "--connect") { const auto address=net::parseEndpoint(value());options.connectHost=address.host;options.connectPort=address.port; }
        else if (flag == "--name") { options.playerName=value();options.nameSpecified=true; }
        else if (flag == "--mods-dir") {
            options.modsDirectory=vc::utf8Path(value());
            if(options.modsDirectory.empty()) throw std::invalid_argument("--mods-dir must not be empty");
        }
        else if (flag == "--no-mods") options.noMods=true;
        else if (flag == "--survival") options.survival=true;
        else if (flag == "--creative") options.survival=false;
        else if (flag == "--generator-version") options.generatorVersion=number<std::uint32_t>(value());
        else if (flag == "--time") options.timeOfDay=number<double>(value());
        else if (flag == "--freeze-time") options.freezeTime=true;
        else if (flag == "--settings-file") {
            options.settingsFile=vc::utf8Path(value());
            if(options.settingsFile.empty()) throw std::invalid_argument("--settings-file must not be empty");
        }
        else if (flag == "--no-settings") options.noSettings=true;
        else if (flag == "--fov") options.fieldOfView=number<double>(value());
        else if (flag == "--sensitivity") options.mouseSensitivity=number<double>(value());
        else if (flag == "--invert-y") options.invertY=true;
        else if (flag == "--normal-y") options.invertY=false;
        else if (flag == "--frames") {
            options.frameLimit = number<unsigned>(value());
            if (options.frameLimit == 0) throw std::invalid_argument("--frames must be positive");
        }
        else if (flag == "--screenshot") {
            const auto path = value();
            options.screenshot = vc::utf8Path(path);
        }
        else if (flag == "--smoke") options.smoke = true;
        else if (flag == "--fly") options.fly = true;
        else if (flag == "--require-validation") options.requireValidation = true;
        else if (flag == "--help") options.help = true;
        else throw std::invalid_argument("Unknown option: " + std::string(flag));
    }
    if(options.smoke&&!options.generatorVersion) options.generatorVersion=2; // Stable interactive regression scene.
    if(options.minSpecified&&options.maxSpecified) options.bounds.validate();
    if (options.radius < 1 || options.radius > 6) throw std::invalid_argument("--radius must be between 1 and 6 chunks");
    if (options.bounds.minY < -1048576 || options.bounds.maxY > 1048575) throw std::invalid_argument("Viewer height bounds must lie within -1048576..1048575");
    if (options.smoke && !options.frameLimit) options.frameLimit = 120;
    if (options.smoke && options.frameLimit < 120) throw std::invalid_argument("--smoke requires at least 120 frames");
    if (!options.worldDirectory.empty() && (options.noSave || options.smoke))
        throw std::invalid_argument("--world-dir cannot be combined with --no-save or --smoke");
    if(options.noSettings&&!options.settingsFile.empty()) throw std::invalid_argument("--no-settings conflicts with --settings-file");
    if(options.noMods&&!options.modsDirectory.empty()) throw std::invalid_argument("--no-mods conflicts with --mods-dir");
    if(options.smoke&&!options.modsDirectory.empty()) throw std::invalid_argument("--smoke cannot use --mods-dir");
    if(options.survival.value_or(false)&&(options.fly||options.smoke)) throw std::invalid_argument("--survival cannot be combined with --fly or --smoke");
    if(options.generatorVersion&&(*options.generatorVersion<1||*options.generatorVersion>vc::voxel::terrainGenerationVersion))
        throw std::invalid_argument("Unsupported --generator-version");
    if(options.timeOfDay&&(!std::isfinite(*options.timeOfDay)||*options.timeOfDay<0||*options.timeOfDay>1))
        throw std::invalid_argument("--time must be inside 0..1");
    if(!validPlayerName(options.playerName)) throw std::invalid_argument("--name needs 1..24 letters, digits, underscores or hyphens");
    if(!options.connectHost.empty()) {
        if(options.smoke||options.noSave||!options.worldDirectory.empty()||options.seedSpecified||options.minSpecified||options.maxSpecified||
            options.generatorVersion||options.timeOfDay||options.freezeTime||options.radiusSpecified)
            throw std::invalid_argument("Connected worlds use server world/seed/time/radius/save configuration");
        if(options.survival) throw std::invalid_argument("Connected worlds use the server game mode");
    } else if(options.nameSpecified) throw std::invalid_argument("--name requires --connect");
    vc::GameSettings check;
    if(options.fieldOfView) check.fieldOfView=*options.fieldOfView;
    if(options.mouseSensitivity) check.mouseSensitivity=*options.mouseSensitivity;
    check.validate();
    return options;
}

}
