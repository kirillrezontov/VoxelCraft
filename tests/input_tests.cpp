#include "engine/platform/input.hpp"
#include <SDL3/SDL_mouse.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
SDL_Event key(SDL_EventType type, SDL_Scancode scan, bool repeat = false) {
    SDL_Event event{}; event.type = type; event.key.scancode = scan; event.key.repeat = repeat; return event;
}
SDL_Event button(SDL_EventType type, Uint8 which) {
    SDL_Event event{}; event.type = type; event.button.button = which; return event;
}
}
TEST(Input, CaptureClickDoesNotEditAndHeldButtonDoesNotRepeat) {
    vc::InputState state;
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT));
    auto input = state.consume(); EXPECT_TRUE(input.captured); EXPECT_FALSE(input.destroy);
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT));
    EXPECT_FALSE(state.consume().destroy);
    state.process(button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT));
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT));
    EXPECT_TRUE(state.consume().destroy); EXPECT_FALSE(state.consume().destroy);
    state.process(button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_RIGHT));
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_RIGHT));
    EXPECT_TRUE(state.consume().place); EXPECT_FALSE(state.consume().place);
}
TEST(Input, HeldMovementPersistsAndOpposingKeysCancel) {
    vc::InputState state; state.setCaptured(true);
    state.process(key(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_W));
    state.process(key(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_D));
    state.process(key(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE));
    auto input = state.consume(); EXPECT_EQ(input.forward,1); EXPECT_EQ(input.right,1); EXPECT_EQ(input.up,1); EXPECT_TRUE(input.jump);
    input = state.consume(); EXPECT_EQ(input.forward,1); EXPECT_FALSE(input.jump);
    state.process(key(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_S));
    state.process(key(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_A));
    state.process(key(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LSHIFT));
    input = state.consume(); EXPECT_EQ(input.forward,0); EXPECT_EQ(input.right,0); EXPECT_EQ(input.up,0);
    state.process(key(SDL_EVENT_KEY_UP, SDL_SCANCODE_W));
    EXPECT_EQ(state.consume().forward,-1);
}
TEST(Input, RepeatedKeysDoNotToggleFlightJumpOrSelectAgain) {
    vc::InputState state; state.setCaptured(true);
    for (const auto scan : {SDL_SCANCODE_F,SDL_SCANCODE_SPACE,SDL_SCANCODE_3}) state.process(key(SDL_EVENT_KEY_DOWN,scan));
    auto input=state.consume(); EXPECT_TRUE(input.toggleFlight); EXPECT_TRUE(input.jump); EXPECT_EQ(input.material,3);
    for (const auto scan : {SDL_SCANCODE_F,SDL_SCANCODE_SPACE,SDL_SCANCODE_3}) state.process(key(SDL_EVENT_KEY_DOWN,scan,true));
    input=state.consume(); EXPECT_FALSE(input.toggleFlight); EXPECT_FALSE(input.jump); EXPECT_EQ(input.material,0);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_F)); EXPECT_FALSE(state.consume().toggleFlight);
    state.process(key(SDL_EVENT_KEY_UP,SDL_SCANCODE_F)); state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_F));
    EXPECT_TRUE(state.consume().toggleFlight);
}
TEST(Input, FocusLossCancelsPendingActionsAndHeldKeys) {
    vc::InputState state; state.setCaptured(true);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_W)); state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_SPACE));
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_F)); state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    SDL_Event event{}; event.type=SDL_EVENT_WINDOW_FOCUS_LOST; state.process(event);
    auto input=state.consume(); EXPECT_FALSE(input.captured); EXPECT_EQ(input.forward,0); EXPECT_FALSE(input.destroy); EXPECT_FALSE(input.jump); EXPECT_FALSE(input.toggleFlight);
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    input=state.consume(); EXPECT_TRUE(input.captured); EXPECT_FALSE(input.destroy); EXPECT_EQ(input.forward,0);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_W,true)); EXPECT_EQ(state.consume().forward,0);
    state.process(key(SDL_EVENT_KEY_UP,SDL_SCANCODE_W)); state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_W)); EXPECT_EQ(state.consume().forward,1);
}
TEST(Input, EscapeRecaptureAndUncapturedActions) {
    vc::InputState state;
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_F)); state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_2));
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT));
    auto input=state.consume(); EXPECT_FALSE(input.toggleFlight); EXPECT_EQ(input.material,0); EXPECT_FALSE(input.place);
    state.setCaptured(true); state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_ESCAPE));
    state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    input=state.consume(); EXPECT_TRUE(input.captured); EXPECT_FALSE(input.destroy);
}
TEST(Input, MouseMotionIsAccumulatedOnceAndIgnoredWithoutCapture) {
    vc::InputState state; SDL_Event event{}; event.type=SDL_EVENT_MOUSE_MOTION; event.motion.xrel=4; event.motion.yrel=-3;
    state.process(event); EXPECT_EQ(state.consume().mouseX,0);
    state.setCaptured(true); state.process(event); state.process(event);
    const auto input=state.consume(); EXPECT_EQ(input.mouseX,8); EXPECT_EQ(input.mouseY,-6); EXPECT_EQ(state.consume().mouseX,0);
    event.motion.xrel=std::numeric_limits<float>::quiet_NaN(); state.process(event); EXPECT_EQ(state.consume().mouseX,0);
}
TEST(Input, QuitAndResizeSurviveCaptureLoss) {
    vc::InputState state; state.setCaptured(true); SDL_Event event{};
    event.type=SDL_EVENT_QUIT; state.process(event); event.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED; state.process(event);
    event.type=SDL_EVENT_WINDOW_MINIMIZED; state.process(event);
    const auto input=state.consume(); EXPECT_TRUE(input.quit); EXPECT_TRUE(input.resized); EXPECT_FALSE(input.captured);
}
TEST(Input, InventoryPointerModeDoesNotCaptureOrEdit) {
    vc::InputState state;state.setCaptured(true);state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_E));
    EXPECT_TRUE(state.consume().inventory);state.setPointerMode(true);
    auto event=button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT);event.button.x=12;event.button.y=34;
    state.process(event);auto input=state.consume();
    EXPECT_TRUE(input.primaryClick);EXPECT_FALSE(input.destroy);EXPECT_FALSE(input.captured);
    EXPECT_EQ(input.cursorX,12);EXPECT_EQ(input.cursorY,34);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_W));EXPECT_EQ(state.consume().forward,0);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_9));EXPECT_EQ(state.consume().material,9);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_F5));EXPECT_TRUE(state.consume().save);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_ESCAPE));EXPECT_TRUE(state.consume().closeUi);
    state.setPointerMode(false);state.setCaptured(true);EXPECT_EQ(state.consume().forward,0);
}
TEST(Input, MenuClicksKeepButtonPositionWhenLaterMotionArrives) {
    for(const auto which:{SDL_BUTTON_LEFT,SDL_BUTTON_RIGHT}) {
        vc::InputState state;state.setPointerMode(true);
        auto click=button(SDL_EVENT_MOUSE_BUTTON_DOWN,static_cast<Uint8>(which));click.button.x=12;click.button.y=34;
        state.process(click);
        SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=200;motion.motion.y=300;state.process(motion);
        const auto input=state.consume();
        EXPECT_TRUE(which==SDL_BUTTON_LEFT?input.primaryClick:input.secondaryClick);
        EXPECT_EQ(input.cursorX,12);EXPECT_EQ(input.cursorY,34);
        const auto next=state.consume();EXPECT_EQ(next.cursorX,200);EXPECT_EQ(next.cursorY,300);
    }
}
TEST(Input, FractionalAndFlippedWheelAndFocusCancellation) {
    vc::InputState state;state.setCaptured(true);SDL_Event event{};event.type=SDL_EVENT_MOUSE_WHEEL;event.wheel.y=0.5f;
    state.process(event);EXPECT_EQ(state.consume().scroll,0);state.process(event);EXPECT_EQ(state.consume().scroll,1);
    event.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;event.wheel.y=2;state.process(event);EXPECT_EQ(state.consume().scroll,-2);
    state.setPointerMode(true);state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    event={};event.type=SDL_EVENT_WINDOW_FOCUS_LOST;state.process(event);EXPECT_FALSE(state.consume().primaryClick);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_F5));EXPECT_TRUE(state.consume().save);
}
TEST(Input, HeldMiningRequiresAnEditingPressAfterCaptureAndStopsOnFocusLoss) {
    vc::InputState state;state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    EXPECT_FALSE(state.consume().destroyHeld);EXPECT_FALSE(state.consume().destroyHeld);
    state.process(button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT));state.process(button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT));
    auto input=state.consume();EXPECT_TRUE(input.destroy);EXPECT_TRUE(input.destroyHeld);
    input=state.consume();EXPECT_FALSE(input.destroy);EXPECT_TRUE(input.destroyHeld);
    SDL_Event event{};event.type=SDL_EVENT_WINDOW_FOCUS_LOST;state.process(event);EXPECT_FALSE(state.consume().destroyHeld);
}
TEST(Input, ExplicitDiscardExitWorksWithoutCaptureAndSurvivesFocusLoss) {
    vc::InputState state;auto event=key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_Q);event.key.mod=SDL_KMOD_CTRL|SDL_KMOD_SHIFT;
    state.process(event);event={};event.type=SDL_EVENT_WINDOW_FOCUS_LOST;state.process(event);
    const auto input=state.consume();EXPECT_TRUE(input.quit);EXPECT_TRUE(input.quitWithoutSaving);
    EXPECT_FALSE(state.consume().quitWithoutSaving);
}
TEST(Input, ConsoleUsesUnicodeTextEventsAndIsolatesAllGameplayKeys) {
    vc::InputState state;state.setCaptured(true);state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_W));
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_SLASH));auto input=state.consume();EXPECT_EQ(input.console,1);EXPECT_FALSE(input.captured);EXPECT_EQ(input.forward,0);
    SDL_Event text{};text.type=SDL_EVENT_TEXT_INPUT;text.text.text="Привет /give";state.process(text);
    for(const auto scan:{SDL_SCANCODE_W,SDL_SCANCODE_E,SDL_SCANCODE_C,SDL_SCANCODE_F,SDL_SCANCODE_SPACE,SDL_SCANCODE_1}) state.process(key(SDL_EVENT_KEY_DOWN,scan));
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_BACKSPACE));input=state.consume();EXPECT_EQ(input.text.text,"Привет /give");EXPECT_TRUE(input.text.backspace);
    EXPECT_FALSE(input.inventory);EXPECT_FALSE(input.crafting);EXPECT_FALSE(input.toggleFlight);EXPECT_FALSE(input.jump);EXPECT_EQ(input.material,0);EXPECT_EQ(input.forward,0);
    auto copy=key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_C);copy.key.mod=SDL_KMOD_CTRL;state.process(copy);EXPECT_TRUE(state.consume().text.copy);
    state.process(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_ESCAPE));EXPECT_TRUE(state.consume().closeUi);
    state.setTextInput(false);state.setCaptured(true);EXPECT_EQ(state.consume().forward,0);
}
TEST(Input, DeclarativeCompositionCommitAndFocusLossDoNotLeaveStalePreedit) {
    vc::InputState state;state.setPointerMode(true);state.setTextInput(true);
    SDL_Event event{};event.type=SDL_EVENT_TEXT_EDITING;event.edit.text="При";state.process(event);
    auto in=state.consume();EXPECT_TRUE(in.text.compositionChanged);EXPECT_EQ(in.text.composition,"При");
    event.edit.text="Привет";state.process(event);event={};event.type=SDL_EVENT_TEXT_INPUT;event.text.text="Привет";state.process(event);
    in=state.consume();EXPECT_EQ(in.text.text,"Привет");EXPECT_TRUE(in.text.compositionChanged);EXPECT_TRUE(in.text.composition.empty());
    event={};event.type=SDL_EVENT_WINDOW_FOCUS_LOST;state.process(event);
    in=state.consume();EXPECT_TRUE(in.focusLost);EXPECT_TRUE(in.text.composition.empty());EXPECT_FALSE(in.primaryHeld);
    EXPECT_FALSE(state.consume().focusLost);
}
