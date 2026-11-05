#pragma once
#include "client/screen_manager.hpp"
#include "client/ui_frame_clock.hpp"
#include "engine/resources/catalog.hpp"
#include "engine/platform/input.hpp"
#include "game/ui/sandbox_ui.hpp"
#include <voxelui/runtime.hpp>
#include <optional>

namespace vc::client {
enum class MenuAction { None, Play, Connect, Quit, Back, Resume, Settings, Save, Leave };
// UI-thread host adapter. Actions queue intent; application/session changes occur
// after dispatch. ResourceCatalog outlives this object and its provider callbacks.
class DeclarativeUi {
public:
    DeclarativeUi(ResourceCatalog&,const std::filesystem::path& fonts,std::string address,std::string name);
    void update(ApplicationScreen,std::string_view message,int width,int height,float scale);
    void process(const Input&);
    // Uses value snapshots and queued commands, never callbacks into a session.
    bool processGameplay(SandboxUiController&,const Input&,int width,int height);
    bool updateGameplay(UiScreen,bool survival,unsigned health,bool remote,int width,int height,float scale);
    void reload();
    MenuAction takeAction();
    std::string address() const;
    std::string name() const;
    bool editing() const;
    voxelui::DrawList draw();
private:
    ResourceCatalog& catalog_;
    voxelui::Runtime runtime_;
    UiFrameClock frameClock_;
    std::optional<ApplicationScreen> current_;
    MenuAction pending_{};
    bool builtinOnly_{};
    std::string loadedResource_;
    void show(std::string_view,const voxelui::FrameInfo&,bool focus);
};
}
