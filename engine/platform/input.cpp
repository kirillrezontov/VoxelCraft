#include "engine/platform/input.hpp"
#include <SDL3/SDL_mouse.h>
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
void InputState::process(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: pending_.quit = true; break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_RESTORED: pending_.resized = true; break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MINIMIZED: setCaptured(false);resetActions(); break;
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_KEY_DOWN: {
        const auto key = event.key.scancode;
        if (key < 0 || key >= SDL_SCANCODE_COUNT) break;
        const auto index = static_cast<std::size_t>(key);
        if (event.type == SDL_EVENT_KEY_UP) { keys_[index] = false; break; }
        if(key==SDL_SCANCODE_Q&&!event.key.repeat&&(event.key.mod&SDL_KMOD_CTRL)&&(event.key.mod&SDL_KMOD_SHIFT)) {
            pending_.quit=pending_.quitWithoutSaving=true;break;
        }
        if (key == SDL_SCANCODE_ESCAPE) {
            if(pointerMode_&&!event.key.repeat) pending_.closeUi=true;
            else setCaptured(false);
            break;
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
        if (event.button.button == SDL_BUTTON_LEFT) { leftHeld_ = false;leftEditArmed_=false; }
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
    result.captured = captured_;
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
