#include "engine/platform/input.hpp"
#include <SDL3/SDL_mouse.h>
#include "engine/core/utf8.hpp"
#include <cmath>
#include <algorithm>

namespace vc {
void InputState::resetActions() {
    keys_.fill(false);leftHeld_=rightHeld_=leftEditArmed_=false;wheelRemainder_=0;
    const bool quit=pending_.quit,resized=pending_.resized,discard=pending_.quitWithoutSaving;
    pending_={};pending_.quit=quit;pending_.resized=resized;pending_.quitWithoutSaving=discard;
}
void InputState::setCaptured(bool captured) {
    if(pointerMode_&&captured) return;
    if (captured_ == captured) return;
    captured_ = captured;
    resetActions();
}
void InputState::setPointerMode(bool enabled) {
    if(pointerMode_==enabled) return;
    pointerMode_=enabled;setCaptured(false);
    resetActions();
}
void InputState::setTextInput(bool enabled) {
    if(textInput_==enabled) return;
    textInput_=enabled;resetActions();if(enabled) setCaptured(false);
}
void InputState::process(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: pending_.quit = true; break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESTORED: pending_.resized = true; break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MINIMIZED: setCaptured(false);resetActions(); break;
    case SDL_EVENT_TEXT_INPUT:
        if(textInput_&&event.text.text) {
            const std::string_view text=event.text.text;
            if(text.size()<=4096&&utf8::valid(text)&&pending_.text.text.size()+text.size()<=4096) pending_.text.text+=text;
        }
        break;
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_KEY_DOWN: {
        const auto key = event.key.scancode;
        if (key < 0 || key >= SDL_SCANCODE_COUNT) break;
        const auto index = static_cast<std::size_t>(key);
        if (event.type == SDL_EVENT_KEY_UP) { keys_[index] = false; break; }
        if(key==SDL_SCANCODE_Q&&!event.key.repeat&&(event.key.mod&SDL_KMOD_CTRL)&&(event.key.mod&SDL_KMOD_SHIFT)) {
            pending_.quit=pending_.quitWithoutSaving=true;break;
        }
        const bool control=(event.key.mod&SDL_KMOD_CTRL)!=0;
        if(textInput_) {
            auto& text=pending_.text;text.shift=(event.key.mod&SDL_KMOD_SHIFT)!=0;
            if(key==SDL_SCANCODE_ESCAPE) pending_.closeUi=text.escape=true;
            if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER) text.enter=true;
            if(key==SDL_SCANCODE_BACKSPACE) text.backspace=true;
            if(key==SDL_SCANCODE_DELETE) text.erase=true;
            if(key==SDL_SCANCODE_LEFT) text.left=true;
            if(key==SDL_SCANCODE_RIGHT) text.right=true;
            if(key==SDL_SCANCODE_HOME) text.home=true;
            if(key==SDL_SCANCODE_END) text.end=true;
            if(key==SDL_SCANCODE_UP) text.up=true;
            if(key==SDL_SCANCODE_DOWN) text.down=true;
            if(key==SDL_SCANCODE_TAB) text.tab=true;
            if(control&&key==SDL_SCANCODE_A) text.selectAll=true;
            if(control&&key==SDL_SCANCODE_C) text.copy=true;
            if(control&&key==SDL_SCANCODE_X) text.cut=true;
            if(control&&key==SDL_SCANCODE_V) text.paste=true;
            break;
        }
        if(!event.key.repeat&&(key==SDL_SCANCODE_SLASH||key==SDL_SCANCODE_T||key==SDL_SCANCODE_GRAVE)) {
            const bool capturedBefore=captured_;setTextInput(true);pending_.consoleCapturedBefore=capturedBefore;pending_.console=key==SDL_SCANCODE_SLASH?1:key==SDL_SCANCODE_T?2:3;break;
        }
        if (key == SDL_SCANCODE_ESCAPE) {
            if(!event.key.repeat) { if(pointerMode_) pending_.closeUi=true;else { setCaptured(false);pending_.pause=true; } }
            break;
        }
        if(pointerMode_&&(key==SDL_SCANCODE_TAB||key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_UP||key==SDL_SCANCODE_DOWN||key==SDL_SCANCODE_LEFT||key==SDL_SCANCODE_RIGHT)) {
            pending_.text.tab=key==SDL_SCANCODE_TAB;pending_.text.enter=key==SDL_SCANCODE_RETURN;
            pending_.text.up=key==SDL_SCANCODE_UP;pending_.text.down=key==SDL_SCANCODE_DOWN;
            pending_.text.left=key==SDL_SCANCODE_LEFT;pending_.text.right=key==SDL_SCANCODE_RIGHT;pending_.text.shift=(event.key.mod&SDL_KMOD_SHIFT)!=0;break;
        }
        if ((!captured_&&!pointerMode_&&key!=SDL_SCANCODE_E&&key!=SDL_SCANCODE_C&&key!=SDL_SCANCODE_R&&key!=SDL_SCANCODE_F5&&key!=SDL_SCANCODE_F10) || event.key.repeat || keys_[index]) break;
        keys_[index] = true;
        if (key == SDL_SCANCODE_SPACE&&captured_) pending_.jump = true;
        if (key == SDL_SCANCODE_F&&captured_) pending_.toggleFlight = true;
        if (key == SDL_SCANCODE_E) pending_.inventory=true;
        if (key == SDL_SCANCODE_F10) pending_.settings=true;
        if (key == SDL_SCANCODE_C) pending_.crafting=true;
        if (key == SDL_SCANCODE_R) pending_.respawn=true;
        if (key == SDL_SCANCODE_F5) pending_.save = true;
        if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_9) pending_.material = key - SDL_SCANCODE_1 + 1;
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT) { pending_.primaryRelease=leftHeld_;leftHeld_ = false;leftEditArmed_=false; }
        if (event.button.button == SDL_BUTTON_RIGHT) rightHeld_ = false;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if(std::isfinite(event.button.x)&&std::isfinite(event.button.y)) { cursorX_=event.button.x;cursorY_=event.button.y; }
        if (event.button.button == SDL_BUTTON_LEFT && !leftHeld_) {
            if(pointerMode_) { pending_.primaryClick=true;pending_.cursorX=cursorX_;pending_.cursorY=cursorY_; }
            else if (!captured_) setCaptured(true);
            else { pending_.destroy = true;leftEditArmed_=true; }
            leftHeld_ = true;
        }
        if (event.button.button == SDL_BUTTON_RIGHT && !rightHeld_) {
            if(pointerMode_) { pending_.secondaryClick=true;pending_.cursorX=cursorX_;pending_.cursorY=cursorY_; }
            else if (captured_) pending_.place = true;
            rightHeld_ = true;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if(std::isfinite(event.motion.x)&&std::isfinite(event.motion.y)) { cursorX_=event.motion.x;cursorY_=event.motion.y; }
        if (captured_ && std::isfinite(event.motion.xrel) && std::isfinite(event.motion.yrel)) {
            pending_.mouseX += event.motion.xrel; pending_.mouseY += event.motion.yrel;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        if((captured_||pointerMode_)&&std::isfinite(event.wheel.y)) {
            const float direction=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1.0f:1.0f;
            wheelRemainder_+=std::clamp(event.wheel.y*direction,-100.0f,100.0f);
            const auto steps=static_cast<int>(wheelRemainder_);
            wheelRemainder_-=static_cast<float>(steps);
            pending_.scroll=std::clamp(pending_.scroll+steps,-100,100);
        }
        break;
    default: break;
    }
}
Input InputState::consume() {
    Input result = pending_; pending_ = {};
    result.captured = captured_;result.primaryHeld=leftHeld_;result.shift=keys_[SDL_SCANCODE_LSHIFT]||keys_[SDL_SCANCODE_RSHIFT];
    result.destroyHeld=captured_&&leftHeld_&&leftEditArmed_;
    // Hit-test at the button event, even if motion follows in the same poll.
    if(!result.primaryClick&&!result.secondaryClick) { result.cursorX=cursorX_;result.cursorY=cursorY_; }
    if (captured_) {
        result.forward = static_cast<float>(keys_[SDL_SCANCODE_W]) - static_cast<float>(keys_[SDL_SCANCODE_S]);
        result.right = static_cast<float>(keys_[SDL_SCANCODE_D]) - static_cast<float>(keys_[SDL_SCANCODE_A]);
        result.up = static_cast<float>(keys_[SDL_SCANCODE_SPACE]) - static_cast<float>(keys_[SDL_SCANCODE_LSHIFT]);
    }
    return result;
}
}
