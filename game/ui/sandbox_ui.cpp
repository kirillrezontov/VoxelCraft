#include "game/ui/sandbox_ui.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace vc {
SandboxUiLayout::SandboxUiLayout(int width,int height,double userScale) {
    if(width<=0||height<=0||!std::isfinite(userScale)||userScale<=0) throw std::invalid_argument("Invalid UI viewport or scale");
    const float w=static_cast<float>(width),h=static_cast<float>(height);
    scale=std::min({h/720*static_cast<float>(userScale),(w-16)/800,(h-16)/636});
    scale=std::max(0.01f,scale);
    panel={(w-760*scale)/2,(h-556*scale)/2-20*scale,760*scale,556*scale};
    barX=(w-496*scale)/2;barY=h-66*scale;
}
UiRect SandboxUiLayout::hotbarSlot(std::size_t i) const {
    if(i>=9) throw std::out_of_range("Hotbar index");
    return {barX+static_cast<float>(i)*56*scale,barY,48*scale,48*scale};
}
UiRect SandboxUiLayout::catalogCell(std::size_t i) const {
    if(i>=pageCapacity) throw std::out_of_range("Catalog index");
    return {panel.x+(36+static_cast<float>(i%8)*88)*scale,panel.y+(176+static_cast<float>(i/8)*76)*scale,72*scale,64*scale};
}
UiRect SandboxUiLayout::backpackCell(std::size_t i) const {
    if(i>=27) throw std::out_of_range("Backpack index");
    return {panel.x+(48+static_cast<float>(i%9)*74)*scale,panel.y+(148+static_cast<float>(i/9)*76)*scale,64*scale,64*scale};
}
UiRect SandboxUiLayout::recipeButton(std::size_t i) const {
    if(i>=recipePageCapacity) throw std::out_of_range("Recipe index");
    return {panel.x+24*scale,panel.y+(128+static_cast<float>(i)*74)*scale,712*scale,66*scale};
}
UiRect SandboxUiLayout::settingsButton(std::size_t i) const {
    if(i<4) return {panel.x+(i%2==0?552:628)*scale,panel.y+(108+static_cast<float>(i/2)*80)*scale,60*scale,40*scale};
    if(i==4) return {panel.x+552*scale,panel.y+268*scale,136*scale,40*scale};
    if(i==5||i==6) return {panel.x+(i==5?40:544)*scale,panel.y+464*scale,176*scale,44*scale};
    if(i==7||i==8) return {panel.x+(i==7?552:628)*scale,panel.y+348*scale,60*scale,40*scale};
    throw std::out_of_range("Settings index");
}
UiRect SandboxUiLayout::search() const { return {panel.x+24*scale,panel.y+76*scale,712*scale,38*scale}; }
UiRect SandboxUiLayout::category(std::size_t i) const { return {panel.x+(24+static_cast<float>(i)*103)*scale,panel.y+126*scale,97*scale,32*scale}; }
UiRect SandboxUiLayout::pauseButton(std::size_t i) const { return {panel.x+180*scale,panel.y+(172+static_cast<float>(i)*72)*scale,400*scale,52*scale}; }

SandboxUiController::SandboxUiController(CreativeInventory& inventory,GameSettings settings,SurvivalUiContext* survival)
    :inventory_(inventory),settings_(settings),survival_(survival) {
    settings_.validate();completion_.items=&inventory.items();completion_.permission=commands::Permission::Administrator;refreshCatalog();
}
void SandboxUiController::closeMenus() { screen_=UiScreen::Gameplay;searchFocused_=false;dragSource_.reset();dirty_=true; }
void SandboxUiController::setSurvivalContext(SurvivalUiContext* context) {
    if(survival_==context) return;
    survival_=context;dragSource_.reset();itemRevision_=0;dirty_=true;refreshCatalog();
    if(!context&&screen_==UiScreen::Crafting) screen_=UiScreen::Inventory;
}
void SandboxUiController::setCompletionContext(commands::CompletionContext context) { completion_=std::move(context); }
void SandboxUiController::refreshCatalog() {
    catalog_=inventory_.filter(search_.text(),category_);recipes_.clear();registrySize_=inventory_.items().size();
    if(survival_) for(std::size_t i=0;i<survival_->recipes->size();++i) {
        const auto& item=survival_->items->definition(survival_->recipes->at(i).output);
        const auto query=utf8::lower(search_.text());
        if(query.empty()||utf8::lower(item.identifier+" "+item.presentation.name).find(query)!=std::string::npos) recipes_.push_back(i);
    }
}
void SandboxUiController::layoutWidgets(int width,int height) {
    widgets_.clear();if(width<=0||height<=0) return;
    const SandboxUiLayout l(width,height,settings_.uiScale);
    const auto add=[&](std::string id,ui::WidgetKind kind,UiRect bounds,std::string tip={},bool enabled=true) {
        widgets_.add({std::move(id),kind,bounds,enabled,true,std::move(tip)});
    };
    if(screen_==UiScreen::Settings) {
        for(std::size_t i=0;i<9;++i) add("setting"+std::to_string(i),ui::WidgetKind::Button,l.settingsButton(i));
    } else if(screen_==UiScreen::Pause) {
        for(std::size_t i=0;i<4;++i) add("pause"+std::to_string(i),ui::WidgetKind::Button,l.pauseButton(i));
    } else if(screen_==UiScreen::Inventory||screen_==UiScreen::Crafting) {
        const auto tip=[&](const ItemDefinition& item,const ItemStack* stack=nullptr) {
            auto value=item.presentation.name+"\n"+item.identifier;
            if(!item.presentation.description.empty()) value+='\n'+item.presentation.description;
            if(item.durability&&stack) value+="\nDurability "+std::to_string(stack->durability)+" / "+std::to_string(item.durability);
            return value;
        };
        if(screen_==UiScreen::Crafting||!survival_) add("search",ui::WidgetKind::TextInput,l.search(),"Search names and identifiers");
        if(!survival_&&screen_==UiScreen::Inventory) {
            for(std::size_t i=0;i<7;++i) add("category"+std::to_string(i),ui::WidgetKind::Button,l.category(i));
            for(std::size_t i=0;i<32&&page_*32+i<catalog_.size();++i)
                add("catalog"+std::to_string(i),ui::WidgetKind::Slot,l.catalogCell(i),tip(inventory_.items().definition(catalog_[page_*32+i])));
        } else if(screen_==UiScreen::Crafting&&survival_) {
            for(std::size_t i=0;i<5&&page_*5+i<recipes_.size();++i) {
                const auto& recipe=survival_->recipes->at(recipes_[page_*5+i]);
                add("recipe"+std::to_string(i),ui::WidgetKind::Button,l.recipeButton(i),tip(survival_->items->definition(recipe.output)),
                    survival_->state->inventory.canCraft(recipe,*survival_->items)==CraftResult::Crafted);
            }
        } else if(survival_) for(std::size_t i=0;i<27;++i) {
            const auto& stack=survival_->state->inventory.slots()[9+i];
            add("slot"+std::to_string(9+i),ui::WidgetKind::Slot,l.backpackCell(i),stack.empty()?"Empty slot":tip(survival_->items->definition(stack.item),&stack));
        }
        for(std::size_t i=0;i<9;++i) {
            std::string text="Empty slot";
            if(survival_) { const auto& stack=survival_->state->inventory.slots()[i];if(!stack.empty()) text=tip(survival_->items->definition(stack.item),&stack); }
            else if(inventory_.slots()[i]) text=tip(inventory_.items().definition(*inventory_.slots()[i]));
            add("slot"+std::to_string(i),ui::WidgetKind::Slot,l.hotbarSlot(i),std::move(text));
        }
    }
    widgets_.hover(mouseX_,mouseY_);
}
InventoryActionResult SandboxUiController::transact(InventoryAction action) {
    if(!survival_) return InventoryActionResult::NoChange;
    dirty_=true;
    if(survival_->request) { survival_->request(action);return InventoryActionResult::NoChange; }
    return applyInventoryAction(survival_->state->inventory,action,*survival_->items,*survival_->recipes);
}
void SandboxUiController::activate(std::string_view id,const SandboxUiInput& input) {
    const auto index=[&](std::string_view prefix) { return static_cast<std::size_t>(std::stoul(std::string(id.substr(prefix.size())))); };
    if(id=="search") { searchFocused_=true;widgets_.focus(id);dirty_=true;return; }
    searchFocused_=false;
    if(id.starts_with("category")) { category_=static_cast<ItemCategory>(index("category"));page_=0;refreshCatalog();dirty_=true; }
    else if(id.starts_with("catalog")) {
        const auto n=page_*32+index("catalog");if(n<catalog_.size()) dirty_=inventory_.assignItem(inventory_.selectedSlot(),catalog_[n])||dirty_;
    } else if(id.starts_with("recipe")&&survival_) {
        const auto n=page_*5+index("recipe");if(n>=recipes_.size()) return;
        const auto recipeIndex=recipes_[n];const auto& recipe=survival_->recipes->at(recipeIndex);
        if(survival_->state->inventory.canCraft(recipe,*survival_->items)!=CraftResult::Crafted) return;
        const auto result=transact({InventoryActionKind::Craft,0,0,static_cast<std::uint32_t>(recipeIndex)});
        if(result==InventoryActionResult::Changed) craftedItems_.push_back(survival_->items->definition(recipe.output).identifier);
        status(survival_->request?"Craft requested":result==InventoryActionResult::Changed?"Item crafted":"Craft unavailable",now_);
    } else if(id.starts_with("setting")) {
        const auto i=index("setting");const auto before=settings_;
        if(i<2) settings_.fieldOfView=std::clamp(settings_.fieldOfView+(i==0?-5:5),50.0,110.0);
        else if(i<4) settings_.mouseSensitivity=std::clamp((std::round(settings_.mouseSensitivity*2000)+(i==2?-1:1))/2000,0.0005,0.01);
        else if(i==4) settings_.invertY=!settings_.invertY;
        else if(i==5) settings_=GameSettings{};
        else if(i==6) screen_=settingsReturn_;
        else settings_.uiScale=std::clamp(std::round((settings_.uiScale+(i==7?-0.1:0.1))*100)/100,0.75,1.75);
        settingsChanged_=settingsChanged_||before!=settings_;dirty_=true;
    } else if(id.starts_with("pause")) {
        const auto i=index("pause");if(i==0) closeMenus();
        else if(i==1) { settingsReturn_=UiScreen::Pause;screen_=UiScreen::Settings;dirty_=true; }
        else if(i==2) { saveRequested_=true;status("Save requested",now_); }
        else leaveRequested_=true;
    } else if(id.starts_with("slot")) {
        const auto i=index("slot");if(i>=36) return;
        if(!survival_) {
            if(i<9) { if(input.secondary) dirty_=inventory_.assignItem(i,std::nullopt)||dirty_;else dirty_=inventory_.select(i)||dirty_; }
            return;
        }
        auto& inv=survival_->state->inventory;
        if(input.shift) { (void)transact({InventoryActionKind::QuickTransfer,static_cast<std::uint32_t>(i)});dragSource_.reset(); }
        else if(input.secondary) {
            // Direct half-stack transfer to the selected hotbar slot. If the
            // destination is occupied, quick-transfer the hotbar to the backpack.
            if(i<9) (void)transact({InventoryActionKind::QuickTransfer,static_cast<std::uint32_t>(i)});
            else (void)transact({InventoryActionKind::TransferHalf,static_cast<std::uint32_t>(i),static_cast<std::uint32_t>(inv.selectedSlot())});
        } else if(input.primaryHeld) {
            if(i<9) dirty_=inv.select(i)||dirty_;
            if(!inv.slots()[i].empty()) dragSource_=i;
            else if(i>=9) (void)transact({InventoryActionKind::Swap,static_cast<std::uint32_t>(i),static_cast<std::uint32_t>(inv.selectedSlot())});
        }
        else if(i<9) dirty_=inv.select(i)||dirty_;
        else (void)transact({InventoryActionKind::Swap,static_cast<std::uint32_t>(i),static_cast<std::uint32_t>(inv.selectedSlot())});
    }
}
bool SandboxUiController::process(const SandboxUiInput& input,int width,int height) {
    const bool wasOpen=open();const auto oldScreen=screen_;
    mouseX_=input.x;mouseY_=input.y;
    if(survival_&&!survival_->state->alive()&&screen_!=UiScreen::Console&&!input.console) { dragSource_.reset();return false; }
    if(input.console) {
        if(screen_!=UiScreen::Console) previousScreen_=screen_;
        screen_=UiScreen::Console;searchFocused_=false;dragSource_.reset();console_.begin(input.console==1,input.console==3);dirty_=true;
    } else if(input.close||input.text.escape) {
        if(screen_==UiScreen::Console) screen_=previousScreen_;
        else if(searchFocused_) searchFocused_=false;
        else if(screen_==UiScreen::Settings) screen_=settingsReturn_;
        else closeMenus();
        dragSource_.reset();dirty_=true;
    } else if(input.pause) { screen_=screen_==UiScreen::Pause?UiScreen::Gameplay:UiScreen::Pause;searchFocused_=false;dirty_=true; }
    else if(input.settingsToggle) { settingsReturn_=UiScreen::Gameplay;screen_=screen_==UiScreen::Settings?UiScreen::Gameplay:UiScreen::Settings;searchFocused_=false;dirty_=true; }
    else if(input.craftingToggle&&survival_) { screen_=screen_==UiScreen::Crafting?UiScreen::Gameplay:UiScreen::Crafting;searchFocused_=false;search_.set("");refreshCatalog();page_=0;dirty_=true; }
    else if(input.toggle) { screen_=screen_==UiScreen::Inventory?UiScreen::Gameplay:UiScreen::Inventory;searchFocused_=false;search_.set("");refreshCatalog();page_=0;dirty_=true; }
    if(screen_==UiScreen::Console) {
        if(!input.console) dirty_=console_.process(input.text,input.scroll,commandRegistry_,completion_,now_)||dirty_;
        return wasOpen!=open()||oldScreen!=screen_;
    }
    if(registrySize_!=inventory_.items().size()) { refreshCatalog();dirty_=true; }
    if(input.slot&&!searchFocused_) dirty_=(survival_?survival_->state->inventory.select(static_cast<std::size_t>(input.slot-1)):inventory_.select(static_cast<std::size_t>(input.slot-1)))||dirty_;
    if(!open()) {
        if(input.scroll) dirty_=(survival_?survival_->state->inventory.cycle(-(input.scroll%9)):inventory_.cycle(-(input.scroll%9)))||dirty_;
        return wasOpen!=open()||oldScreen!=screen_;
    }
    if(searchFocused_) {
        if(input.text.copy||input.text.cut) clipboard_=search_.selected();
        try { if(search_.process(input.text)) { refreshCatalog();page_=0;dirty_=true; } } catch(const std::invalid_argument&) { }
    }
    if(input.scroll&&(screen_==UiScreen::Inventory||screen_==UiScreen::Crafting)) {
        const auto count=screen_==UiScreen::Crafting?recipes_.size():catalog_.size();
        const auto capacity=screen_==UiScreen::Crafting?5u:32u;
        const auto pages=std::max<std::size_t>(1,(count+capacity-1)/capacity);
        const auto next=std::clamp(static_cast<std::int64_t>(page_)-input.scroll,std::int64_t{0},static_cast<std::int64_t>(pages-1));
        if(page_!=static_cast<std::size_t>(next)) { page_=static_cast<std::size_t>(next);dirty_=true; }
    }
    const auto hovered=widgets_.hovered();layoutWidgets(width,height);dirty_=hovered!=widgets_.hovered()||dirty_;
    // Rebuilding keeps focus; hover changes are compared separately below.
    if(input.text.tab||(!searchFocused_&&(input.text.down||input.text.right||input.text.up||input.text.left))) {
        dirty_=widgets_.navigate((input.text.shift||input.text.up||input.text.left)?-1:1)||dirty_;
        searchFocused_=widgets_.focused()=="search";
    }
    if(input.primary||input.secondary) {
        if(const auto* widget=widgets_.hit(input.x,input.y)) { const auto id=widget->id;widgets_.focus(id);activate(id,input);dirty_=true; }
        else if(searchFocused_) { searchFocused_=false;dirty_=true; }
    }
    if(input.text.enter&&!searchFocused_&&!widgets_.focused().empty()) {
        auto activation=input;activation.primary=true;activation.primaryHeld=false;activate(widgets_.focused(),activation);
    }
    if(input.primaryRelease&&dragSource_) {
        const auto from=*dragSource_;dragSource_.reset();dirty_=true;
        if(const auto* widget=widgets_.hit(input.x,input.y);widget&&widget->id.starts_with("slot")) {
            const auto to=static_cast<std::size_t>(std::stoul(widget->id.substr(4)));
            if(to==from) {
                if(to<9) survival_->state->inventory.select(to);
                else (void)transact({InventoryActionKind::Swap,static_cast<std::uint32_t>(from),static_cast<std::uint32_t>(survival_->state->inventory.selectedSlot())});
            }
            else if(to!=from) {
                const auto& slots=survival_->state->inventory.slots();
                const auto kind=slots[to].empty()||slots[from].item==slots[to].item?InventoryActionKind::TransferAll:InventoryActionKind::Swap;
                (void)transact({kind,static_cast<std::uint32_t>(from),static_cast<std::uint32_t>(to)});
            }
        }
    }
    return wasOpen!=open()||oldScreen!=screen_;
}
void SandboxUiController::status(std::string message,double now,NotificationSeverity severity) {
    message=utf8::singleLine(message,240);console_.message(message,severity,now);
    if(!notifications_.empty()&&notifications_.back().text==message&&now-notifications_.back().time<0.5) return;
    notifications_.push_back({std::move(message),severity,now});while(notifications_.size()>8) notifications_.pop_front();dirty_=true;
}
void SandboxUiController::postCommandResult(const commands::Result& result,double now) {
    console_.post(result,now);if(!result.lines.empty()) {
        notifications_.push_back({result.lines.front(),result.success()?NotificationSeverity::Success:NotificationSeverity::Error,now});
        while(notifications_.size()>8) notifications_.pop_front();
    }dirty_=true;
}
bool SandboxUiController::takeSettingsChanged() { return std::exchange(settingsChanged_,false); }
bool SandboxUiController::takeSaveRequested() { return std::exchange(saveRequested_,false); }
std::vector<std::string> SandboxUiController::takeCraftedItems() { return std::exchange(craftedItems_,{}); }
std::string SandboxUiController::takeClipboard() { auto text=console_.takeClipboard();return text.empty()?std::exchange(clipboard_,{}):text; }
std::optional<HudGeometry> SandboxUiController::rebuild(const voxel::BlockRegistry& registry,const AtlasLayout& atlas,int width,int height,double now) {
    if((screen_==UiScreen::Console||searchFocused_)&&static_cast<int>(now*2)!=static_cast<int>(now_*2)) dirty_=true;
    now_=now;
    const int transition=static_cast<int>(now*20);
    if(notificationFrame_!=transition&&std::ranges::any_of(notifications_,[&](const auto& notification){return now-notification.time>3.75;})) dirty_=true;
    notificationFrame_=transition;
    while(!notifications_.empty()&&now-notifications_.front().time>=4) { notifications_.pop_front();dirty_=true; }
    if(survival_&&(itemRevision_!=survival_->state->inventory.revision()||health_!=survival_->state->health())) {
        itemRevision_=survival_->state->inventory.revision();health_=survival_->state->health();dirty_=true;
    }
    if(creativeRevision_!=inventory_.revision()) { creativeRevision_=inventory_.revision();dirty_=true; }
    if(width<=0||height<=0||(!dirty_&&width==width_&&height==height_)) return std::nullopt;
    layoutWidgets(width,height);
    SandboxUiView view;view.screen=screen_;view.catalog=&catalog_;view.recipes=&recipes_;view.query=search_.text();view.category=category_;
    view.searchEditor=&search_;view.searchFocused=searchFocused_;
    view.hovered=widgets_.hovered();view.focused=widgets_.focused();view.console=&console_;view.notifications=&notifications_;
    view.dragSource=dragSource_;view.mouseX=mouseX_;view.mouseY=mouseY_;view.now=now;
    const auto tooltip=std::ranges::find_if(widgets_.widgets(),[&](const auto& widget){return widget.id==view.hovered;});
    if(tooltip!=widgets_.widgets().end()) view.tooltip=tooltip->tooltip;
    auto result=sandboxHud(inventory_,registry,atlas,width,height,screen_==UiScreen::Inventory,page_,{},screen_==UiScreen::Settings,settings_,survival_,screen_==UiScreen::Crafting,&view,theme_);
    width_=width;height_=height;dirty_=false;return result;
}
}
