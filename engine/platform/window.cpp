#include "engine/platform/window.hpp"
#include "engine/core/paths.hpp"
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <string>
#include <memory>

namespace vc {
namespace {
void check(bool result) { if (!result) throw std::runtime_error(SDL_GetError()); }
}
Window::Window(int width,int height,bool fullscreen) {
    if(width<320||height<240||width>16384||height>16384) throw std::invalid_argument("Invalid window size");
    check(SDL_Init(SDL_INIT_VIDEO));
    window_ = SDL_CreateWindow("VoxelCraft " VC_VERSION_STRING, width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (fullscreen?SDL_WINDOW_FULLSCREEN:0));
    if (!window_) { const std::string error = SDL_GetError(); SDL_Quit(); throw std::runtime_error(error); }
}
Window::~Window() { SDL_DestroyWindow(window_); SDL_Quit(); }
void Window::capture(bool enabled) {
    input_.setCaptured(enabled);
    check(SDL_SetWindowRelativeMouseMode(window_, input_.captured()));
}
void Window::textInput(bool enabled) {
    input_.setTextInput(enabled);
    if(SDL_TextInputActive(window_)!=enabled)
        check(enabled?SDL_StartTextInput(window_):SDL_StopTextInput(window_));
    check(SDL_SetWindowRelativeMouseMode(window_,input_.captured()));
}
void Window::clipboard(std::string_view text) { check(SDL_SetClipboardText(std::string(text).c_str())); }
void Window::pointerMode(bool enabled) {
    input_.setPointerMode(enabled);
    check(SDL_SetWindowRelativeMouseMode(window_,input_.captured()));
}
SDL_WindowID Window::id() const { return SDL_GetWindowID(window_); }
Input Window::poll() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        input_.process(event);
        if (input_.captured() != SDL_GetWindowRelativeMouseMode(window_))
            check(SDL_SetWindowRelativeMouseMode(window_, input_.captured()));
    }
    auto input=input_.consume();
    if(input.text.paste) {
        const std::unique_ptr<char,decltype(&SDL_free)> pasted(SDL_GetClipboardText(),SDL_free);
        if(pasted) {
            const auto size=std::char_traits<char>::length(pasted.get());
            if(size<=4096) input.text.text+=pasted.get();
        }
    }
    int logicalWidth{},logicalHeight{};check(SDL_GetWindowSize(window_,&logicalWidth,&logicalHeight));
    const auto [pixelWidth,pixelHeight]=pixelSize();
    if(logicalWidth>0&&logicalHeight>0&&pixelWidth>0&&pixelHeight>0) {
        input.cursorX*=static_cast<float>(pixelWidth)/static_cast<float>(logicalWidth);
        input.cursorY*=static_cast<float>(pixelHeight)/static_cast<float>(logicalHeight);
    }
    return input;
}
void Window::title(std::string_view text) { check(SDL_SetWindowTitle(window_, std::string(text).c_str())); }
std::pair<int, int> Window::pixelSize() const {
    if (SDL_GetWindowFlags(window_) & SDL_WINDOW_MINIMIZED) return {0, 0};
    int w{}, h{}; check(SDL_GetWindowSizeInPixels(window_, &w, &h)); return {w, h};
}
std::vector<const char*> Window::instanceExtensions() const {
    Uint32 count{};
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!extensions) throw std::runtime_error(SDL_GetError());
    return {extensions, extensions + count};
}
VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface{};
    check(SDL_Vulkan_CreateSurface(window_, instance, nullptr, &surface));
    return surface;
}
std::filesystem::path Window::assetPath() const {
    const char* base = SDL_GetBasePath();
    if (!base) throw std::runtime_error(SDL_GetError());
    return utf8Path(base) / "assets";
}
std::filesystem::path Window::userDataPath() {
    const std::unique_ptr<char,decltype(&SDL_free)> path(SDL_GetPrefPath("VoxelCraft","VoxelCraft"),SDL_free);
    if(!path) throw std::runtime_error(SDL_GetError());
    return utf8Path(path.get());
}
void Window::resize(int w, int h) { check(SDL_SetWindowSize(window_, w, h)); }
void Window::minimize() { check(SDL_MinimizeWindow(window_)); }
void Window::restore() {
    check(SDL_RestoreWindow(window_));
    // Wayland restore only unmaximizes; activation also requests unminimization.
    // The compositor retains final control of focus and window visibility.
    check(SDL_RaiseWindow(window_));
}
double Window::seconds() { return static_cast<double>(SDL_GetTicksNS()) / 1.0e9; }
void Window::idle() { SDL_Delay(16); }
}
