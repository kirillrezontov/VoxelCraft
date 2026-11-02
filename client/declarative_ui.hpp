#pragma once
#include "client/screen_manager.hpp"
#include "engine/resources/catalog.hpp"
#include "engine/platform/input.hpp"
#include <voxelui/runtime.hpp>
#include <optional>

namespace vc::client {
enum class MenuAction { None, Play, Connect, Quit, Back };
// UI-thread host adapter. Actions queue intent; application/session changes occur
// after dispatch. ResourceCatalog outlives this object and its provider callbacks.
class DeclarativeUi {
public:
    DeclarativeUi(ResourceCatalog&,const std::filesystem::path& fonts,std::string address,std::string name);
    void update(ApplicationScreen,std::string_view message,int width,int height,float scale);
    void process(const Input&);
    void reload();
    MenuAction takeAction();
    std::string address() const;
    std::string name() const;
    bool editing() const;
    voxelui::DrawList draw();
private:
    ResourceCatalog& catalog_;
    voxelui::Runtime runtime_;
    std::optional<ApplicationScreen> current_;
    MenuAction pending_{};
    bool builtinOnly_{};
};
}
