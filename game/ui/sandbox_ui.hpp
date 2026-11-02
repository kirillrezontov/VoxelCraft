#pragma once
#include "game/gameplay/inventory.hpp"
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/ui/widgets.hpp"
#include "game/settings.hpp"
#include "game/gameplay/survival.hpp"
#include "game/ui/console.hpp"
#include <functional>
#include <optional>
#include <utility>

namespace vc {
struct UiRect {
    float x{},y{},width{},height{};
    bool contains(float px,float py) const { return px>=x&&py>=y&&px<x+width&&py<y+height; }
    operator ui::Rect() const { return {x,y,width,height}; }
};
struct SandboxUiLayout {
    static constexpr std::size_t pageCapacity=32,recipePageCapacity=5;
    float scale{};
    UiRect panel;
    float barX{},barY{};
    SandboxUiLayout(int width,int height,double userScale=1);
    UiRect hotbarSlot(std::size_t index) const;
    UiRect catalogCell(std::size_t index) const;
    UiRect settingsButton(std::size_t index) const;
    UiRect backpackCell(std::size_t index) const;
    UiRect recipeButton(std::size_t index) const;
    UiRect search() const;
    UiRect category(std::size_t index) const;
    UiRect pauseButton(std::size_t index) const;
};
struct HudGeometry { Mesh colored,textured,foreground; };
struct SurvivalUiContext {
    SurvivalState* state;
    const ItemRegistry* items;
    const std::vector<Recipe>* recipes;
    std::function<void(InventoryAction)> request{};
};
enum class UiScreen { Gameplay,Inventory,Crafting,Settings,Pause,Console };
enum class PauseAction { Resume, Settings, Save, Leave };
struct SandboxUiView {
    UiScreen screen{UiScreen::Gameplay};
    bool externalChrome{};
    const std::vector<ItemId>* catalog{};
    const std::vector<std::size_t>* recipes{};
    std::string query,tooltip,hovered,focused;
    ItemCategory category{ItemCategory::All};
    const CommandConsole* console{};
    const ui::TextEdit* searchEditor{};
    bool searchFocused{};
    const std::deque<ConsoleMessage>* notifications{};
    std::optional<std::size_t> dragSource;
    float mouseX{},mouseY{};
    double now{};
};
HudGeometry sandboxHud(const CreativeInventory& inventory,const voxel::BlockRegistry& registry,
    const AtlasLayout& atlas,int width,int height,bool inventoryOpen,std::size_t page=0,
    std::string_view status={},bool settingsOpen=false,const GameSettings& settings={},
    const SurvivalUiContext* survival=nullptr,bool craftingOpen=false,const SandboxUiView* view=nullptr,const ui::Theme& theme={});
struct SandboxUiInput {
    bool toggle{},close{},primary{},secondary{};
    int slot{},scroll{};
    float x{},y{};
    bool settingsToggle{},craftingToggle{};
    int console{};
    bool pause{},primaryRelease{},primaryHeld{},shift{};
    ui::TextInput text{};
};
class SandboxUiController {
public:
    explicit SandboxUiController(CreativeInventory& inventory,GameSettings settings={},SurvivalUiContext* survival=nullptr);
    bool process(const SandboxUiInput& input,int width,int height);
    bool open() const { return screen_!=UiScreen::Gameplay||(survival_&&!survival_->state->alive()); }
    std::size_t page() const { return page_; }
    const GameSettings& settings() const { return settings_; }
    bool settingsOpen() const { return screen_==UiScreen::Settings; }
    bool craftingOpen() const { return screen_==UiScreen::Crafting; }
    bool paused() const { return screen_==UiScreen::Pause||screen_==UiScreen::Settings; }
    bool wantsTextInput() const { return screen_==UiScreen::Console||searchFocused_; }
    UiScreen screen() const { return screen_; }
    void closeMenus();
    // Host UI may replace pause controls and gameplay health without owning the
    // simulation/menu state. Commands are accepted only while Pause is active.
    void pauseAction(PauseAction);
    void externalChrome(bool enabled) { if(externalChrome_!=enabled) { externalChrome_=enabled;dirty_=true; } }
    void setSurvivalContext(SurvivalUiContext* context);
    void setCompletionContext(commands::CompletionContext context);
    bool takeSettingsChanged();
    bool takeSaveRequested();
    bool takeLeaveRequested() { return std::exchange(leaveRequested_,false); }
    std::vector<std::string> takeCraftedItems();
    std::vector<std::string> takeCommands() { return console_.takeCommands(); }
    std::string takeClipboard();
    void postCommandResult(const commands::Result& result,double now);
    void status(std::string message,double now,NotificationSeverity severity=NotificationSeverity::Info);
    void setTheme(ui::Theme theme) { theme_=std::move(theme);dirty_=true; }
    const ui::WidgetTree& widgets() const { return widgets_; }
    const std::vector<ItemId>& filteredItems() const { return catalog_; }
    const CommandConsole& console() const { return console_; }
    std::optional<HudGeometry> rebuild(const voxel::BlockRegistry& registry,const AtlasLayout& atlas,int width,int height,double now);
private:
    void refreshCatalog();
    void layoutWidgets(int width,int height);
    void activate(std::string_view id,const SandboxUiInput& input);
    InventoryActionResult transact(InventoryAction action);
    CreativeInventory& inventory_;
    UiScreen screen_{},previousScreen_{},settingsReturn_{};
    GameSettings settings_;
    SurvivalUiContext* survival_{};
    bool dirty_{true},settingsChanged_{},saveRequested_{},searchFocused_{};
    bool leaveRequested_{};
    bool externalChrome_{};
    ui::TextEdit search_{256};
    ui::WidgetTree widgets_;
    ui::Theme theme_;
    ItemCategory category_{ItemCategory::All};
    std::vector<ItemId> catalog_;
    std::vector<std::size_t> recipes_;
    std::vector<std::string> craftedItems_;
    CommandConsole console_;
    commands::Registry commandRegistry_{commands::builtinCommands()};
    commands::CompletionContext completion_;
    std::deque<ConsoleMessage> notifications_;
    std::optional<std::size_t> dragSource_;
    std::string clipboard_;
    std::uint64_t itemRevision_{},creativeRevision_{};
    std::size_t registrySize_{},page_{};
    unsigned health_{20};
    int width_{},height_{};
    float mouseX_{},mouseY_{};
    double now_{};
    int notificationFrame_{};
};
}
