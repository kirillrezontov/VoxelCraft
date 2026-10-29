#include "game/ui/sandbox_ui.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace vc {
namespace {
void quad(Mesh& mesh,const std::array<glm::vec2,4>& corners,glm::vec3 color,
        const std::array<glm::vec2,4>& uv={}) {
    const auto start=static_cast<std::uint32_t>(mesh.vertices.size());
    for(std::size_t i=0;i<4;++i) mesh.vertices.push_back({{corners[i],0},color,uv[i]});
    for(const auto i:{0u,1u,2u,2u,3u,0u}) mesh.indices.push_back(start+i);
}
void rect(Mesh& mesh,UiRect area,glm::vec3 color) {
    quad(mesh,{{{area.x,area.y},{area.x+area.width,area.y},{area.x+area.width,area.y+area.height},{area.x,area.y+area.height}}},color);
}
void frame(Mesh& mesh,UiRect area,float thickness,glm::vec3 color) {
    rect(mesh,{area.x,area.y,area.width,thickness},color);
    rect(mesh,{area.x,area.y+area.height-thickness,area.width,thickness},color);
    rect(mesh,{area.x,area.y,thickness,area.height},color);
    rect(mesh,{area.x+area.width-thickness,area.y,thickness,area.height},color);
}
// Small built-in bitmap font drawn as mesh quads; no OS font or texture dependency.
std::array<unsigned,7> glyph(char c) {
    if(c>='a'&&c<='z') c=static_cast<char>(c-'a'+'A');
    switch(c) {
    case 'A':return {14,17,17,31,17,17,17};case 'B':return {30,17,17,30,17,17,30};
    case 'C':return {14,17,16,16,16,17,14};case 'D':return {30,17,17,17,17,17,30};
    case 'E':return {31,16,16,30,16,16,31};case 'F':return {31,16,16,30,16,16,16};
    case 'G':return {14,17,16,23,17,17,15};case 'H':return {17,17,17,31,17,17,17};
    case 'I':return {14,4,4,4,4,4,14};case 'J':return {7,2,2,2,2,18,12};
    case 'K':return {17,18,20,24,20,18,17};case 'L':return {16,16,16,16,16,16,31};
    case 'M':return {17,27,21,21,17,17,17};case 'N':return {17,25,21,19,17,17,17};
    case 'O':return {14,17,17,17,17,17,14};case 'P':return {30,17,17,30,16,16,16};
    case 'Q':return {14,17,17,17,21,18,13};case 'R':return {30,17,17,30,20,18,17};
    case 'S':return {15,16,16,14,1,1,30};case 'T':return {31,4,4,4,4,4,4};
    case 'U':return {17,17,17,17,17,17,14};case 'V':return {17,17,17,17,17,10,4};
    case 'W':return {17,17,17,21,21,21,10};case 'X':return {17,17,10,4,10,17,17};
    case 'Y':return {17,17,10,4,4,4,4};case 'Z':return {31,1,2,4,8,16,31};
    case '0':return {14,17,19,21,25,17,14};case '1':return {4,12,4,4,4,4,14};
    case '2':return {14,17,1,2,4,8,31};case '3':return {30,1,1,14,1,1,30};
    case '4':return {2,6,10,18,31,2,2};case '5':return {31,16,16,30,1,1,30};
    case '6':return {14,16,16,30,17,17,14};case '7':return {31,1,2,4,8,8,8};
    case '8':return {14,17,17,14,17,17,14};case '9':return {14,17,17,15,1,1,14};
    case ':':return {0,4,4,0,4,4,0};case '.':return {0,0,0,0,0,6,6};
    case '-':return {0,0,0,31,0,0,0};case '_':return {0,0,0,0,0,0,31};
    case '/':return {1,2,2,4,8,8,16};case '|':return {4,4,4,4,4,4,4};
    case '+':return {0,4,4,31,4,4,0};case '!':return {4,4,4,4,4,0,4};
    default:return {};
    }
}
void text(Mesh& mesh,std::string_view value,float x,float y,float unit,glm::vec3 color={1,1,1}) {
    for(const char c:value) {
        const auto rows=glyph(c);
        for(unsigned row=0;row<7;++row) for(unsigned col=0;col<5;++col)
            if(rows[row]&(1u<<(4-col))) rect(mesh,{x+static_cast<float>(col)*unit,y+static_cast<float>(row)*unit,unit,unit},color);
        x+=6*unit;
    }
}
void icon(Mesh& mesh,UiRect area,const voxel::BlockDefinition& block,const AtlasLayout& atlas) {
    const auto face=[&](std::array<glm::vec2,4> corners,std::size_t material,float shade) {
        for(auto& p:corners) p=glm::vec2(area.x,area.y)+p*glm::vec2(area.width,area.height);
        const std::array<glm::vec2,4> uv{{atlas.uv(block.faceTextures[material],{0,0}),atlas.uv(block.faceTextures[material],{1,0}),
            atlas.uv(block.faceTextures[material],{1,1}),atlas.uv(block.faceTextures[material],{0,1})}};
        quad(mesh,corners,block.color*shade,uv);
    };
    face({{{0.5f,0},{1,0.25f},{0.5f,0.5f},{0,0.25f}}},2,1);
    face({{{0,0.25f},{0.5f,0.5f},{0.5f,1},{0,0.75f}}},1,0.72f);
    face({{{0.5f,0.5f},{1,0.25f},{1,0.75f},{0.5f,1}}},0,0.86f);
}
void itemIcon(HudGeometry& hud,UiRect area,const ItemDefinition& item,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas) {
    if(item.block) { icon(hud.textured,area,blocks.definition(*item.block),atlas);return; }
    const auto point=[&](float x,float y){return glm::vec2(area.x+x*area.width,area.y+y*area.height);};
    const glm::vec3 wood{0.42f,0.23f,0.08f};
    if(item.tool==ToolKind::Pickaxe) {
        quad(hud.colored,{{point(0.2f,0.9f),point(0.3f,1),point(0.8f,0.25f),point(0.67f,0.17f)}},wood);
        const auto head=item.tier>=2?glm::vec3(0.52f):glm::vec3(0.7f,0.43f,0.17f);
        quad(hud.colored,{{point(0.15f,0.17f),point(0.85f,0.1f),point(0.95f,0.3f),point(0.2f,0.35f)}},head);
        quad(hud.colored,{{point(0.75f,0.2f),point(0.95f,0.3f),point(1,0.65f),point(0.84f,0.6f)}},head);
    } else if(item.identifier=="voxelcraft:stick") {
        quad(hud.colored,{{point(0.2f,0.8f),point(0.35f,0.95f),point(0.85f,0.2f),point(0.7f,0.05f)}},wood);
    } else if(item.identifier=="voxelcraft:raw_meat") {
        quad(hud.colored,{{point(0.1f,0.3f),point(0.65f,0.1f),point(0.95f,0.65f),point(0.35f,0.95f)}},{0.7f,0.16f,0.12f});
        rect(hud.colored,{area.x+area.width*0.35f,area.y+area.height*0.4f,area.width*0.4f,area.height*0.12f},{0.95f,0.72f,0.52f});
    } else if(item.identifier=="voxelcraft:coal") {
        quad(hud.colored,{{point(0.15f,0.35f),point(0.65f,0.1f),point(0.9f,0.7f),point(0.3f,0.95f)}},{0.03f,0.035f,0.04f});
    } else rect(hud.colored,area,{0.8f,0,0.8f});
}
void itemSlot(HudGeometry& hud,UiRect area,const ItemStack& stack,const ItemRegistry& items,
        const voxel::BlockRegistry& blocks,const AtlasLayout& atlas,float scale) {
    if(stack.empty()) return;
    const auto& item=items.definition(stack.item);
    itemIcon(hud,{area.x+7*scale,area.y+6*scale,34*scale,34*scale},item,blocks,atlas);
    if(stack.count>1) {
        const auto count=std::to_string(stack.count);
        text(hud.foreground,count,area.x+area.width-(static_cast<float>(count.size())*6+3)*scale,area.y+36*scale,scale);
    }
    if(item.durability) {
        rect(hud.colored,{area.x+6*scale,area.y+43*scale,36*scale,2*scale},{0.02f,0.02f,0.02f});
        rect(hud.colored,{area.x+6*scale,area.y+43*scale,36*scale*static_cast<float>(stack.durability)/item.durability,2*scale},{0.2f,0.9f,0.25f});
    }
}
}
SandboxUiLayout::SandboxUiLayout(int width,int height) {
    if(width<=0||height<=0) throw std::invalid_argument("UI viewport must be positive");
    scale=std::min(static_cast<float>(width)/600,static_cast<float>(height)/720);
    const float w=static_cast<float>(width),h=static_cast<float>(height);
    barX=(w-464*scale)/2;barY=h-64*scale;
    panel={(w-520*scale)/2,(h-360*scale)/2,520*scale,360*scale};
}
UiRect SandboxUiLayout::hotbarSlot(std::size_t index) const {
    if(index>=CreativeInventory::slotCount) throw std::out_of_range("UI hotbar slot outside 0..8");
    return {barX+static_cast<float>(index)*52*scale,barY,48*scale,48*scale};
}
UiRect SandboxUiLayout::catalogCell(std::size_t index) const {
    if(index>=pageCapacity) throw std::out_of_range("UI catalog cell outside page");
    return {panel.x+(36+static_cast<float>(index%8)*56)*scale,panel.y+(76+static_cast<float>(index/8)*56)*scale,48*scale,48*scale};
}
UiRect SandboxUiLayout::settingsButton(std::size_t index) const {
    if(index<4) return {panel.x+(index%2==0?356:424)*scale,panel.y+(80+static_cast<float>(index/2)*62)*scale,48*scale,32*scale};
    if(index==4) return {panel.x+356*scale,panel.y+204*scale,116*scale,32*scale};
    if(index==5) return {panel.x+20*scale,panel.y+280*scale,150*scale,32*scale};
    if(index==6) return {panel.x+350*scale,panel.y+280*scale,150*scale,32*scale};
    throw std::out_of_range("Settings UI button index outside 0..6");
}
UiRect SandboxUiLayout::backpackCell(std::size_t index) const {
    if(index>=27) throw std::out_of_range("Backpack UI cell outside 0..26");
    return {panel.x+(28+static_cast<float>(index%9)*52)*scale,panel.y+(88+static_cast<float>(index/9)*56)*scale,48*scale,48*scale};
}
UiRect SandboxUiLayout::recipeButton(std::size_t index) const {
    if(index>=recipePageCapacity) throw std::out_of_range("Crafting UI recipe outside page");
    return {panel.x+20*scale,panel.y+(76+static_cast<float>(index)*48)*scale,480*scale,40*scale};
}
HudGeometry sandboxHud(const CreativeInventory& inventory,const voxel::BlockRegistry& registry,
        const AtlasLayout& atlas,int width,int height,bool inventoryOpen,std::size_t page,std::string_view status,
        bool settingsOpen,const GameSettings& settings,const SurvivalUiContext* survival,bool craftingOpen) {
    const SandboxUiLayout layout(width,height);const float s=layout.scale;
    HudGeometry hud;
    const glm::vec3 background{0.025f,0.035f,0.045f},slotColor{0.09f,0.11f,0.13f},gold{1,0.72f,0.08f};
    const auto drawSlot=[&](UiRect area,std::optional<voxel::BlockId> block,bool selected) {
        rect(hud.colored,area,slotColor);frame(hud.colored,area,(selected?3:1)*s,selected?gold:glm::vec3(0.25f));
        if(block) icon(hud.textured,{area.x+7*s,area.y+6*s,34*s,34*s},registry.definition(*block),atlas);
    };
    rect(hud.colored,{layout.barX-8*s,layout.barY-8*s,480*s,68*s},background);
    const auto selectedSlot=survival?survival->state->inventory.selectedSlot():inventory.selectedSlot();
    for(std::size_t i=0;i<CreativeInventory::slotCount;++i) {
        const auto area=layout.hotbarSlot(i);drawSlot(area,survival?std::nullopt:inventory.slots()[i],i==selectedSlot);
        if(survival) itemSlot(hud,area,survival->state->inventory.slots()[i],*survival->items,registry,atlas,s);
        text(hud.foreground,std::to_string(i+1),area.x+4*s,area.y+3*s,s);
    }
    auto selected=inventory.selectedBlock()?registry.definition(*inventory.selectedBlock()).identifier:std::string("EMPTY SLOT");
    if(survival) {
        const auto& stack=survival->state->inventory.selected();selected=stack.empty()?"EMPTY SLOT":survival->items->definition(stack.item).identifier;
        rect(hud.colored,{layout.barX-8*s,layout.barY-54*s,480*s,18*s},background);
        text(hud.foreground,"HP "+std::to_string(survival->state->health())+"/20",layout.barX,layout.barY-50*s,1.3f*s,{1,0.3f,0.25f});
        for(unsigned i=0;i<20;++i) rect(hud.colored,{layout.barX+(100+static_cast<float>(i)*8)*s,layout.barY-49*s,6*s,8*s},
            i<survival->state->health()?glm::vec3(0.9f,0.08f,0.03f):glm::vec3(0.12f));
        text(hud.foreground,"SURVIVAL",layout.barX+345*s,layout.barY-50*s,1.3f*s,gold);
    }
    if(selected.size()>48) selected=selected.substr(0,45)+"...";
    rect(hud.colored,{layout.barX-8*s,layout.barY-30*s,480*s,18*s},background);
    text(hud.foreground,selected,layout.barX,layout.barY-26*s,1.5f*s,gold);
    text(hud.foreground,survival?"HOLD LEFT: MINE  E: PACK  C: CRAFT  F5: SAVE  F10: SETTINGS":"1-9: SELECT  E: INVENTORY  F5: SAVE  F10: SETTINGS",layout.barX+8*s,layout.barY+52*s,s);
    if(survival&&!survival->state->alive()) {
        rect(hud.colored,layout.panel,background);frame(hud.colored,layout.panel,2*s,{0.9f,0.1f,0.04f});
        text(hud.foreground,"YOU DIED",layout.panel.x+160*s,layout.panel.y+100*s,3*s,{1,0.25f,0.1f});
        text(hud.foreground,"R: RESPAWN",layout.panel.x+180*s,layout.panel.y+180*s,2*s,gold);
        text(hud.foreground,"INVENTORY IS KEPT ON RESPAWN",layout.panel.x+90*s,layout.panel.y+240*s,1.5f*s);
    } else if(settingsOpen) {
        rect(hud.colored,layout.panel,background);frame(hud.colored,layout.panel,2*s,gold);
        text(hud.foreground,"SETTINGS",layout.panel.x+20*s,layout.panel.y+18*s,2*s,gold);
        text(hud.foreground,"CHANGES APPLY AND SAVE AUTOMATICALLY",layout.panel.x+20*s,layout.panel.y+44*s,1.3f*s);
        text(hud.foreground,"FIELD OF VIEW",layout.panel.x+20*s,layout.panel.y+90*s,1.5f*s);
        text(hud.foreground,std::to_string(static_cast<int>(settings.fieldOfView)),layout.panel.x+250*s,layout.panel.y+90*s,1.5f*s);
        text(hud.foreground,"MOUSE SENSITIVITY",layout.panel.x+20*s,layout.panel.y+152*s,1.5f*s);
        std::ostringstream sensitivity;sensitivity<<std::fixed<<std::setprecision(1)<<settings.mouseSensitivity/0.0025<<'X';
        text(hud.foreground,sensitivity.str(),layout.panel.x+250*s,layout.panel.y+152*s,1.5f*s);
        text(hud.foreground,"INVERT MOUSE Y",layout.panel.x+20*s,layout.panel.y+214*s,1.5f*s);
        const std::array<std::string,7> labels{"-","+","-","+",settings.invertY?"ON":"OFF","RESET DEFAULTS","CLOSE"};
        for(std::size_t i=0;i<labels.size();++i) {
            const auto area=layout.settingsButton(i);rect(hud.colored,area,slotColor);frame(hud.colored,area,s,glm::vec3(0.4f));
            text(hud.foreground,labels[i],area.x+(area.width-static_cast<float>(labels[i].size())*7.8f*s)/2,area.y+11*s,1.3f*s);
        }
        text(hud.foreground,"F10 / ESC: CLOSE",layout.panel.x+20*s,layout.panel.y+334*s,1.3f*s);
    } else if(survival&&craftingOpen) {
        rect(hud.colored,layout.panel,background);frame(hud.colored,layout.panel,2*s,gold);
        text(hud.foreground,"CRAFTING",layout.panel.x+20*s,layout.panel.y+18*s,2*s,gold);
        text(hud.foreground,"CLICK A RECIPE - MATERIALS ARE CONSUMED",layout.panel.x+20*s,layout.panel.y+44*s,1.3f*s);
        const auto capacity=SandboxUiLayout::recipePageCapacity;
        const auto pages=std::max<std::size_t>(1,(survival->recipes->size()+capacity-1)/capacity);
        page=std::min(page,pages-1);
        for(std::size_t i=0;i<capacity&&page*capacity+i<survival->recipes->size();++i) {
            const auto& recipe=survival->recipes->at(page*capacity+i);const auto area=layout.recipeButton(i);
            auto trial=survival->state->inventory;const bool available=trial.craft(recipe,*survival->items)==CraftResult::Crafted;
            rect(hud.colored,area,available?glm::vec3(0.12f,0.19f,0.1f):slotColor);frame(hud.colored,area,s,available?gold:glm::vec3(0.3f));
            itemIcon(hud,{area.x+8*s,area.y+5*s,30*s,30*s},survival->items->definition(recipe.output),registry,atlas);
            text(hud.foreground,recipe.name,area.x+60*s,area.y+8*s,1.5f*s,available?gold:glm::vec3(0.55f));
            std::string ingredients;
            for(const auto& part:recipe.ingredients) {
                auto name=survival->items->definition(part.item).identifier;name=name.substr(name.find(':')+1);
                if(!ingredients.empty()) ingredients+=" + ";
                ingredients+=std::to_string(part.count)+' '+name;
            }
            text(hud.foreground,ingredients,area.x+60*s,area.y+28*s,s);
        }
        text(hud.foreground,"WHEEL: PAGE "+std::to_string(page+1)+'/'+std::to_string(pages),layout.panel.x+20*s,layout.panel.y+316*s,1.3f*s);
        text(hud.foreground,"C / ESC: CLOSE   E: BACKPACK",layout.panel.x+20*s,layout.panel.y+340*s,1.3f*s);
    } else if(survival&&inventoryOpen) {
        rect(hud.colored,layout.panel,background);frame(hud.colored,layout.panel,2*s,gold);
        text(hud.foreground,"BACKPACK",layout.panel.x+20*s,layout.panel.y+18*s,2*s,gold);
        text(hud.foreground,"CLICK: SWAP WITH SELECTED HOTBAR SLOT",layout.panel.x+20*s,layout.panel.y+44*s,1.3f*s);
        for(std::size_t i=0;i<27;++i) {
            const auto area=layout.backpackCell(i);drawSlot(area,std::nullopt,false);
            itemSlot(hud,area,survival->state->inventory.slots()[9+i],*survival->items,registry,atlas,s);
        }
        text(hud.foreground,"RIGHT CLICK STACK: MOVE ONE TO HOTBAR",layout.panel.x+20*s,layout.panel.y+282*s,1.3f*s);
        text(hud.foreground,"RIGHT CLICK HOTBAR: MOVE TO BACKPACK",layout.panel.x+20*s,layout.panel.y+306*s,1.3f*s);
        text(hud.foreground,"E / ESC: CLOSE   C: CRAFTING",layout.panel.x+20*s,layout.panel.y+334*s,1.3f*s);
    } else if(inventoryOpen) {
        rect(hud.colored,layout.panel,background);frame(hud.colored,layout.panel,2*s,gold);
        text(hud.foreground,"CREATIVE INVENTORY",layout.panel.x+20*s,layout.panel.y+18*s,2*s,gold);
        text(hud.foreground,"CLICK A BLOCK TO ASSIGN THE SELECTED SLOT",layout.panel.x+20*s,layout.panel.y+44*s,1.3f*s);
        const auto& catalog=inventory.catalog();
        const auto pages=std::max<std::size_t>(1,(catalog.size()+31)/32);
        page=std::min(page,pages-1);
        for(std::size_t i=0;i<32&&page*32+i<catalog.size();++i)
            drawSlot(layout.catalogCell(i),catalog[page*32+i],false);
        text(hud.foreground,"WHEEL: PAGE "+std::to_string(page+1)+'/'+std::to_string(pages),layout.panel.x+20*s,layout.panel.y+310*s,1.3f*s);
        text(hud.foreground,"RIGHT CLICK SLOT: CLEAR   E / ESC: CLOSE",layout.panel.x+20*s,layout.panel.y+334*s,1.3f*s);
    }
    if(!status.empty()) {
        auto message=std::string(status.substr(0,75));
        rect(hud.colored,{8*s,8*s,(static_cast<float>(message.size())*9+16)*s,25*s},background);
        text(hud.foreground,message,16*s,15*s,1.5f*s);
    }
    return hud;
}
bool SandboxUiController::process(const SandboxUiInput& input,int width,int height) {
    if(input.slot<0||input.slot>9) throw std::invalid_argument("UI slot selection must be 0..9");
    if(survival_&&!survival_->state->alive()) return false;
    if(input.settingsToggle) { open_=!(open_&&settingsOpen_);settingsOpen_=true;craftingOpen_=false;dirty_=true;return true; }
    if(input.craftingToggle&&survival_) { open_=!(open_&&craftingOpen_);craftingOpen_=true;settingsOpen_=false;page_=0;dirty_=true;return true; }
    if(input.toggle) { open_=!(open_&&!settingsOpen_&&!craftingOpen_);settingsOpen_=craftingOpen_=false;page_=0;dirty_=true;return true; }
    if(open_&&input.close) { open_=false;dirty_=true;return true; }
    if(open_&&settingsOpen_) {
        if(width<=0||height<=0||!input.primary) return false;
        const SandboxUiLayout layout(width,height);
        const auto previous=settings_;
        for(std::size_t i=0;i<7;++i) {
            if(!layout.settingsButton(i).contains(input.x,input.y)) continue;
            if(i<2) settings_.fieldOfView=std::clamp(settings_.fieldOfView+(i==0?-5:5),50.0,110.0);
            else if(i<4) settings_.mouseSensitivity=std::clamp((std::round(settings_.mouseSensitivity*2000)+(i==2?-1:1))/2000,0.0005,0.01);
            else if(i==4) settings_.invertY=!settings_.invertY;
            else if(i==5) settings_=GameSettings{};
            else { open_=false;dirty_=true;return true; }
        }
        if(previous!=settings_) { settingsChanged_=true;dirty_=true; }
        return false;
    }
    if(input.slot) dirty_=(survival_?survival_->state->inventory.select(static_cast<std::size_t>(input.slot-1)):
        inventory_.select(static_cast<std::size_t>(input.slot-1)))||dirty_;
    if(!open_) {
        if(input.scroll) dirty_=(survival_?survival_->state->inventory.cycle(-(input.scroll%9)):inventory_.cycle(-(input.scroll%9)))||dirty_;
        return false;
    }
    if(survival_) {
        if(craftingOpen_&&input.scroll) {
            const auto capacity=SandboxUiLayout::recipePageCapacity;
            const auto pages=std::max<std::size_t>(1,(survival_->recipes->size()+capacity-1)/capacity);
            const auto next=std::clamp(static_cast<std::int64_t>(page_)-input.scroll,std::int64_t{0},static_cast<std::int64_t>(pages-1));
            if(page_!=static_cast<std::size_t>(next)) { page_=static_cast<std::size_t>(next);dirty_=true; }
        }
        if(width<=0||height<=0||(!input.primary&&!input.secondary)) return false;
        const SandboxUiLayout layout(width,height);auto& state=survival_->state->inventory;
        const auto request=[&](InventoryAction action) {
            if(survival_->request) { survival_->request(action);return InventoryActionResult::NoChange; }
            return applyInventoryAction(state,action,*survival_->items,*survival_->recipes);
        };
        if(craftingOpen_) {
            const auto capacity=SandboxUiLayout::recipePageCapacity;
            for(std::size_t i=0;i<capacity&&page_*capacity+i<survival_->recipes->size();++i) {
                if(!input.primary||!layout.recipeButton(i).contains(input.x,input.y)) continue;
                const auto& recipe=survival_->recipes->at(page_*capacity+i);
                const auto result=request({InventoryActionKind::Craft,0,0,static_cast<std::uint32_t>(page_*capacity+i)});
                if(result==InventoryActionResult::Changed) craftedItems_.push_back(survival_->items->definition(recipe.output).identifier);
                status_=survival_->request?"CRAFT REQUESTED":result==InventoryActionResult::Changed?"CRAFTED":result==InventoryActionResult::NoSpace?"INVENTORY FULL":"MISSING MATERIALS";
                statusUntil_=0;dirty_=true;
            }
        } else {
            for(std::size_t i=0;i<9;++i) if(layout.hotbarSlot(i).contains(input.x,input.y)) {
                if(input.primary) dirty_=state.select(i)||dirty_;
                if(input.secondary) dirty_=(request({InventoryActionKind::MoveToBackpack,static_cast<std::uint32_t>(i)})==InventoryActionResult::Changed)||dirty_;
            }
            for(std::size_t i=0;i<27;++i) if(layout.backpackCell(i).contains(input.x,input.y)) {
                const auto from=static_cast<std::uint32_t>(9+i),to=static_cast<std::uint32_t>(state.selectedSlot());
                if(input.primary) dirty_=(request({InventoryActionKind::Swap,from,to})==InventoryActionResult::Changed)||dirty_;
                if(input.secondary) dirty_=(request({InventoryActionKind::TransferOne,from,to})==InventoryActionResult::Changed)||dirty_;
            }
        }
        return false;
    }
    if(input.scroll) {
        const auto pages=std::max<std::size_t>(1,(inventory_.catalog().size()+31)/32);
        const auto next=std::clamp(static_cast<std::int64_t>(page_)-input.scroll,std::int64_t{0},static_cast<std::int64_t>(pages-1));
        if(page_!=static_cast<std::size_t>(next)) { page_=static_cast<std::size_t>(next);dirty_=true; }
    }
    if(width<=0||height<=0||(!input.primary&&!input.secondary)) return false;
    const SandboxUiLayout layout(width,height);
    for(std::size_t i=0;i<CreativeInventory::slotCount;++i) {
        if(!layout.hotbarSlot(i).contains(input.x,input.y)) continue;
        if(input.primary) dirty_=inventory_.select(i)||dirty_;
        if(input.secondary) dirty_=inventory_.assign(i,std::nullopt)||dirty_;
    }
    for(std::size_t i=0;i<32&&page_*32+i<inventory_.catalog().size();++i)
        if(input.primary&&layout.catalogCell(i).contains(input.x,input.y))
            dirty_=inventory_.assign(inventory_.selectedSlot(),inventory_.catalog()[page_*32+i])||dirty_;
    return false;
}
void SandboxUiController::status(std::string message,double now) { status_=std::move(message);statusUntil_=now+4;dirty_=true; }
bool SandboxUiController::takeSettingsChanged() { return std::exchange(settingsChanged_,false); }
std::vector<std::string> SandboxUiController::takeCraftedItems() { return std::exchange(craftedItems_,{}); }
std::optional<HudGeometry> SandboxUiController::rebuild(const voxel::BlockRegistry& registry,const AtlasLayout& atlas,
        int width,int height,double now) {
    if(!status_.empty()&&statusUntil_==0) statusUntil_=now+4;
    if(!status_.empty()&&now>=statusUntil_) { status_.clear();dirty_=true; }
    if(survival_&&(itemRevision_!=survival_->state->inventory.revision()||health_!=survival_->state->health())) {
        itemRevision_=survival_->state->inventory.revision();health_=survival_->state->health();dirty_=true;
    }
    if(width<=0||height<=0||(!dirty_&&width==width_&&height==height_)) return std::nullopt;
    auto result=sandboxHud(inventory_,registry,atlas,width,height,open_&&!settingsOpen_&&!craftingOpen_,page_,status_,
        open_&&settingsOpen_,settings_,survival_,open_&&craftingOpen_);
    width_=width;height_=height;dirty_=false;return result;
}
}
