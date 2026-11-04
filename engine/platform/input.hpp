#pragma once
#include <SDL3/SDL_events.h>
#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include "engine/ui/text_edit.hpp"

namespace vc {
struct UiInputEvent {
    enum class Type { pointerMove,pointerDown,pointerUp,wheel,keyDown,textInput,composition,focusLost };
    Type type{};
    float x{},y{},delta{};
    SDL_Scancode key{SDL_SCANCODE_UNKNOWN};
    bool control{},shift{};
    std::string text{};
};
struct Input {
    std::uint64_t pollNanoseconds{};
    // Owning, ordered UI stream alongside the unchanged gameplay snapshot.
    std::vector<UiInputEvent> uiEvents;
    bool orderedUiEvents{},uiEventsOverflow{};
    bool quit{}, resized{}, captured{}, focusLost{};
    bool quitWithoutSaving{};
    bool destroy{}, place{}, jump{}, toggleFlight{}, save{};
    bool inventory{}, closeUi{}, primaryClick{}, secondaryClick{};
    bool settings{};
    bool crafting{},respawn{},destroyHeld{};
    int material{};
    int scroll{};
    float forward{}, right{}, up{}, mouseX{}, mouseY{};
    float cursorX{}, cursorY{};
    int console{}; // 1 slash, 2 chat, 3 expanded; text itself comes from SDL.
    bool pause{},primaryRelease{},primaryHeld{},shift{},consoleCapturedBefore{};
    ui::TextInput text;
};

// Event state is independent of window creation and can be tested without a display.
class InputState {
public:
    void process(const SDL_Event& event);
    void setCaptured(bool captured);
    void setPointerMode(bool enabled);
    void setTextInput(bool enabled);
    bool textInput() const { return textInput_; }
    bool captured() const { return captured_; }
    Input consume();
    void reset() { resetActions(); }
private:
    void resetActions();
    bool captured_{}, leftHeld_{}, rightHeld_{};
    bool pointerMode_{},textInput_{};
    bool leftEditArmed_{};
    float cursorX_{},cursorY_{},wheelRemainder_{};
    std::array<bool, SDL_SCANCODE_COUNT> keys_{};
    Input pending_;
    std::size_t uiEventBytes_{};
    void appendUi(UiInputEvent);
};
}
