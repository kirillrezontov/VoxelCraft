#pragma once
#include <string>
#include <utility>

namespace vc::client {
enum class ApplicationScreen { MainMenu, Loading, Gameplay, Saving, Error };
// Application routes only. Gameplay overlay state stays in SandboxUiController.
class ScreenManager {
public:
    ApplicationScreen current() const { return current_; }
    const std::string& message() const { return message_; }
    void show(ApplicationScreen screen,std::string message={}) { current_=screen;message_=std::move(message); }
private:
    ApplicationScreen current_{ApplicationScreen::MainMenu};
    std::string message_;
};
}
