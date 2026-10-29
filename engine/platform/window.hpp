#pragma once
#include <SDL3/SDL.h>
#include "engine/platform/input.hpp"
#include <vulkan/vulkan.h>
#include <filesystem>
#include <string_view>
#include <utility>
#include <vector>

namespace vc {
class Window {
public:
    Window(int width=1280,int height=720,bool fullscreen=false);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Input poll();
    SDL_WindowID id() const;
    void capture(bool enabled);
    void pointerMode(bool enabled);
    void textInput(bool enabled);
    void clipboard(std::string_view text);
    void title(std::string_view text);
    std::pair<int, int> pixelSize() const;
    std::vector<const char*> instanceExtensions() const;
    VkSurfaceKHR createSurface(VkInstance instance) const;
    std::filesystem::path assetPath() const;
    static std::filesystem::path userDataPath();
    void resize(int width, int height);
    void minimize();
    void restore();
    static double seconds();
    static void idle();
private:
    SDL_Window* window_{};
    InputState input_;
};
}
