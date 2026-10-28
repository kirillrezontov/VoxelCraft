#pragma once
#include <SDL3/SDL_events.h>
#include <array>

namespace vc {
struct Input {
    bool quit{}, resized{}, captured{};
    bool quitWithoutSaving{};
    bool destroy{}, place{}, jump{}, toggleFlight{}, save{};
    bool inventory{}, closeUi{}, primaryClick{}, secondaryClick{};
    bool settings{};
    bool crafting{},respawn{},destroyHeld{};
    int material{};
    int scroll{};
    float forward{}, right{}, up{}, mouseX{}, mouseY{};
    float cursorX{}, cursorY{};
};

// Event state is independent of window creation and can be tested without a display.
class InputState {
public:
    void process(const SDL_Event& event);
    void setCaptured(bool captured);
    void setPointerMode(bool enabled);
    bool captured() const { return captured_; }
    Input consume();
private:
    void resetActions();
    bool captured_{}, leftHeld_{}, rightHeld_{};
    bool pointerMode_{};
    bool leftEditArmed_{};
    float cursorX_{},cursorY_{},wheelRemainder_{};
    std::array<bool, SDL_SCANCODE_COUNT> keys_{};
    Input pending_;
};
}
