#include "client/application.hpp"
#include "engine/core/log.hpp"
#include <SDL3/SDL_main.h>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        const auto options = vc::client::parseOptions(argc, argv);
        if (options.help) {
            std::cout << "VoxelCraft " VC_VERSION_STRING "\n--seed N --radius 1..6 --min-y N --max-y N --frames N --screenshot file.ppm --smoke --require-validation --fly\n"
                "--width N --height N --fullscreen / --windowed --vsync / --no-vsync\n"
                "--world-dir PATH (create/load a saved world) --no-save (temporary world)\n"
                "--connect HOST[:PORT] --name PLAYER --identity-file PATH (multiplayer; server controls world and game mode)\n"
                "--native-mods PATH (explicit trusted DLL/SO loading; process privileges)\n"
                "--resource-pack PATH (repeatable declarative resource providers)\n"
                "--mods-dir PATH (load mod packs) --no-mods (retain saved mod data without loading packs)\n"
                "--survival (quantities, crafting, mining and health) --creative (infinite blocks and flight)\n"
                "--generator-version 1..3 (new worlds only; saved worlds retain their version)\n"
                "--time 0..1 (0 sunrise, .25 noon, .5 sunset, .75 midnight) --freeze-time\n"
                "--settings-file PATH --no-settings --fov 50..110 --sensitivity 0.0005..0.01 --invert-y --normal-y\n"
                "WASD/mouse: move/look. Space: jump. F: creative flight. F5: save. F10: settings. Left/right: mine/place. 1-9/wheel: hotbar. E: inventory. C: survival crafting. R: respawn after death. Escape: release mouse/close menu.\n"
                "Normal play saves to the user data directory; bounded runs are temporary unless --world-dir is specified.\n";
            return 0;
        }
        vc::client::ClientApplication application(options);
        const int result=application.run();
        vc::log("Clean shutdown");
        return result;
    } catch (const std::exception& e) {
        vc::log(e.what());
        return 1;
    }
}
