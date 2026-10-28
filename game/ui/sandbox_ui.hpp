#pragma once
#include "game/gameplay/inventory.hpp"
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/texture.hpp"
#include "game/settings.hpp"
#include "game/gameplay/survival.hpp"
#include <string_view>
#include <optional>

namespace vc {
struct UiRect {
    float x{},y{},width{},height{};
    bool contains(float px,float py) const { return px>=x&&py>=y&&px<x+width&&py<y+height; }
};
struct SandboxUiLayout {
    static constexpr std::size_t pageCapacity=32;
    float scale{};
    UiRect panel;
    float barX{},barY{};
    SandboxUiLayout(int width,int height);
    UiRect hotbarSlot(std::size_t index) const;
    UiRect catalogCell(std::size_t index) const;
    UiRect settingsButton(std::size_t index) const;
    UiRect backpackCell(std::size_t index) const;
    UiRect recipeButton(std::size_t index) const;
};
struct HudGeometry { Mesh colored,textured,foreground; };
struct SurvivalUiContext { SurvivalState* state;const ItemRegistry* items;const std::vector<Recipe>* recipes; };
HudGeometry sandboxHud(const CreativeInventory& inventory,const voxel::BlockRegistry& registry,
    const AtlasLayout& atlas,int width,int height,bool inventoryOpen,std::size_t page=0,
    std::string_view status={},bool settingsOpen=false,const GameSettings& settings={},
    const SurvivalUiContext* survival=nullptr,bool craftingOpen=false);

struct SandboxUiInput {
    bool toggle{},close{},primary{},secondary{};
    int slot{},scroll{};
    float x{},y{};
    bool settingsToggle{};
    bool craftingToggle{};
};
// Menu interaction is shared by the client and SDL playtest. No SDL/Vulkan types.
class SandboxUiController {
public:
    explicit SandboxUiController(CreativeInventory& inventory,GameSettings settings={},SurvivalUiContext* survival=nullptr)
        :inventory_(inventory),settings_(settings),survival_(survival) { settings_.validate(); }
    // True when capture should change; the caller captures iff !open().
    bool process(const SandboxUiInput& input,int width,int height);
    bool open() const { return open_||(survival_&&!survival_->state->alive()); }
    std::size_t page() const { return page_; }
    const GameSettings& settings() const { return settings_; }
    bool settingsOpen() const { return open_&&settingsOpen_; }
    bool craftingOpen() const { return open_&&craftingOpen_; }
    void closeMenus() { open_=false;dirty_=true; }
    bool takeSettingsChanged();
    void status(std::string message,double now);
    std::optional<HudGeometry> rebuild(const voxel::BlockRegistry& registry,const AtlasLayout& atlas,
        int width,int height,double now);
private:
    CreativeInventory& inventory_;
    bool open_{},dirty_{true};
    GameSettings settings_;
    bool settingsOpen_{},settingsChanged_{};
    bool craftingOpen_{};
    SurvivalUiContext* survival_{};
    std::uint64_t itemRevision_{};
    unsigned health_{20};
    std::size_t page_{};
    int width_{},height_{};
    std::string status_;
    double statusUntil_{};
};
}
