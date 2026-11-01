#pragma once
#include "client/game_session.hpp"
#include "client/screen_manager.hpp"
#include "client/preferences.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include <coroutine>
#include <utility>

namespace vc::client {
struct SessionEnvironment {
    Window& window;
    VulkanRenderer& renderer;
    Preferences& preferences;
    ScreenManager& screens;
    Input input;
    bool leaveRequested{}, quitRequested{}, presented{};
    struct NextFrame {
        SessionEnvironment& host;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<>) const noexcept {}
        Input await_resume() { return std::exchange(host.input, {}); }
    };
    NextFrame nextFrame() { return {*this}; }
    bool draw(const Camera& camera,const std::filesystem::path& screenshot={}) {
        presented=true;return renderer.draw(camera,screenshot);
    }
    void applySettings(const GameSettings& settings) {
        settings.validate();preferences.settings=settings;
        if(!preferences.path.empty()) saveSettings(preferences.path,settings);
    }
};
GameSession localSession(SessionEnvironment& host,Options options);
GameSession remoteSession(SessionEnvironment& host,Options options);
}
