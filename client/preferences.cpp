#include "client/preferences.hpp"
#include "engine/platform/window.hpp"
#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"

namespace vc::client {
Preferences loadPreferences(const Options& options) {
    Preferences result;auto& settings=result.settings;
    auto& settingsFile=result.path;
    if(!options.noSettings&&((!options.frameLimit&&!options.smoke)||!options.settingsFile.empty())) {
        settingsFile=options.settingsFile.empty()?vc::Window::userDataPath()/"settings.vcs":options.settingsFile;
        try {
            if(const auto loaded=vc::loadSettings(settingsFile)) {
                settings=loaded->settings;
                vc::log(std::string(loaded->recoveredBackup?"Recovered settings backup: ":"Loaded settings: ")+vc::pathText(settingsFile));
            }
        } catch(const std::exception& error) { vc::log(std::string("Settings load failed; using defaults: ")+error.what()); }
    }
    if(options.fieldOfView) settings.fieldOfView=*options.fieldOfView;
    if(options.mouseSensitivity) settings.mouseSensitivity=*options.mouseSensitivity;
    if(options.uiScale) settings.uiScale=*options.uiScale;
    if(options.invertY) settings.invertY=*options.invertY;
    settings.validate();
    return result;
}
}
