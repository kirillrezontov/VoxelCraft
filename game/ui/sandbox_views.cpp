#include "game/ui/sandbox_ui.hpp"
#include "engine/ui/font.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace vc {
namespace {
struct Painter {
    HudGeometry hud;
    ui::Rect viewport;
    float s;
    const ui::Theme& theme;
    void quad(Mesh& mesh,const std::array<glm::vec2,4>& points,glm::vec4 color,const std::array<glm::vec2,4>& uv={}) {
        const auto start=static_cast<std::uint32_t>(mesh.vertices.size());
        for(std::size_t i=0;i<4;++i) { Vertex v{{points[i],0},glm::vec3(color),uv[i]};v.opacity=color.a;mesh.vertices.push_back(v); }
        for(const auto i:{0u,1u,2u,2u,3u,0u}) mesh.indices.push_back(start+i);
    }
    void rect(UiRect r,glm::vec4 color,bool front=false) {
        const auto x=std::max(r.x,0.0f),y=std::max(r.y,0.0f),right=std::min(r.x+r.width,viewport.width),bottom=std::min(r.y+r.height,viewport.height);
        if(right<=x||bottom<=y) return;
        quad(front?hud.foreground:hud.colored,{{{x,y},{right,y},{right,bottom},{x,bottom}}},color);
    }
    void frame(UiRect r,glm::vec4 color,float thickness=1,bool front=false) {
        const auto t=std::max(1.0f,thickness*s);
        rect({r.x,r.y,r.width,t},color,front);rect({r.x,r.y+r.height-t,r.width,t},color,front);
        rect({r.x,r.y,t,r.height},color,front);rect({r.x+r.width-t,r.y,t,r.height},color,front);
    }
    void label(std::string_view text,UiRect r,float pixels,glm::vec4 color,bool mono=false,bool centered=false) {
        auto clip=ui::Rect{std::max(0.f,r.x),std::max(0.f,r.y),std::min(r.x+r.width,viewport.width)-std::max(0.f,r.x),std::min(r.y+r.height,viewport.height)-std::max(0.f,r.y)};
        if(clip.width<=0||clip.height<=0) return;
        const auto size=std::max(8.0f,pixels*s);
        float x=r.x,y=r.y;
        if(centered) { const auto measure=ui::fonts().measure(text,size,mono);x+=(r.width-measure.x)/2;y+=(r.height-measure.y)/2; }
        ui::fonts().append(hud.foreground,text,x,y,size,glm::vec3(color),clip,mono,color.a);
    }
    void button(UiRect r,std::string_view text,std::string_view id,const SandboxUiView* view,bool enabled=true,bool active=false,float font=18) {
        const bool hover=view&&(view->hovered==id||view->focused==id);
        rect(r,hover&&enabled?theme.hover:theme.surface);frame(r,active||hover?theme.accent:theme.border);
        label(text,r,font,enabled?(active?theme.accent:theme.text):theme.muted,false,true);
    }
    void search(UiRect r,std::string_view placeholder,const SandboxUiView* view) {
        const bool active=view&&view->searchFocused;
        rect(r,theme.surface);frame(r,active?theme.accent:theme.border);
        const auto* editor=view?view->searchEditor:nullptr;
        if(!editor||editor->text().empty()) {
            label(placeholder,{r.x+12*s,r.y+8*s,r.width-24*s,r.height-8*s},18,theme.muted);
            return;
        }
        const float size=std::max(8.f,18*s);
        const float cursor=ui::fonts().measure(editor->text().substr(0,editor->cursor()),size).x;
        const float offset=active?std::max(0.f,cursor-(r.width-32*s)):0;
        const float origin=r.x+12*s-offset;
        const auto [a,b]=editor->selection();
        if(active&&a!=b) {
            const float left=std::max(r.x+8*s,origin+ui::fonts().measure(editor->text().substr(0,a),size).x);
            const float right=std::min(r.x+r.width-8*s,origin+ui::fonts().measure(editor->text().substr(0,b),size).x);
            if(right>left) rect({left,r.y+6*s,right-left,26*s},theme.hover,true);
        }
        ui::fonts().append(hud.foreground,editor->text(),origin,r.y+7*s,size,glm::vec3(theme.text),{r.x+8*s,r.y+4*s,r.width-16*s,r.height-8*s});
        if(active&&static_cast<int>(view->now*2)%2==0) rect({origin+cursor,r.y+7*s,1.5f*s,24*s},theme.accent,true);
    }
    void item(UiRect r,const ItemDefinition& item,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas) {
        if(item.block&&!item.presentation.iconTile) {
            const auto& block=blocks.definition(*item.block);
            const auto face=[&](std::array<glm::vec2,4> points,std::size_t f,float shade) {
                for(auto& point:points) point=glm::vec2(r.x,r.y)+point*glm::vec2(r.width,r.height);
                const auto tile=block.faceTextures[f];
                quad(hud.textured,points,glm::vec4(block.color*shade,1),{{atlas.uv(tile,{0,0}),atlas.uv(tile,{1,0}),atlas.uv(tile,{1,1}),atlas.uv(tile,{0,1})}});
            };
            face({{{.5f,0},{1,.25f},{.5f,.5f},{0,.25f}}},2,1);
            face({{{0,.25f},{.5f,.5f},{.5f,1},{0,.75f}}},1,.72f);
            face({{{.5f,.5f},{1,.25f},{1,.75f},{.5f,1}}},0,.86f);
        } else {
            const auto tile=item.presentation.iconTile.value_or(TextureTile{22});const auto safe=tile<atlas.tileCount()?tile:TextureTile{0};
            quad(hud.textured,{{{r.x,r.y},{r.x+r.width,r.y},{r.x+r.width,r.y+r.height},{r.x,r.y+r.height}}},{1,1,1,1},
                {{atlas.uv(safe,{0,0}),atlas.uv(safe,{1,0}),atlas.uv(safe,{1,1}),atlas.uv(safe,{0,1})}});
        }
    }
    void slot(UiRect r,const ItemDefinition* item,const ItemStack* stack,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas,bool selected,bool hover) {
        rect(r,hover?theme.hover:theme.surface);frame(r,selected?theme.accent:hover?theme.text:theme.border,selected?2:1);
        if(!item) return;
        const float size=std::min(r.width,r.height)-14*s;
        this->item({r.x+(r.width-size)/2,r.y+6*s,size,size},*item,blocks,atlas);
        if(stack&&stack->count>1) {
            const auto count=std::to_string(stack->count);const auto w=ui::fonts().measure(count,std::max(8.f,16*s)).x;
            rect({r.x+r.width-w-8*s,r.y+r.height-22*s,w+7*s,20*s},theme.panel,true);
            label(count,{r.x+r.width-w-5*s,r.y+r.height-22*s,w+4*s,22*s},16,theme.text);
        }
        if(stack&&item->durability) {
            rect({r.x+5*s,r.y+r.height-5*s,r.width-10*s,3*s},theme.border,true);
            rect({r.x+5*s,r.y+r.height-5*s,(r.width-10*s)*stack->durability/item->durability,3*s},theme.accent,true);
        }
    }
};
glm::vec4 severityColor(NotificationSeverity severity,const ui::Theme& theme) {
    switch(severity) { case NotificationSeverity::Success:return theme.success;case NotificationSeverity::Warning:return theme.warning;case NotificationSeverity::Error:return theme.error;default:return theme.accent; }
}
void heading(Painter& p,const SandboxUiLayout& l,std::string_view title,std::string_view subtitle) {
    const auto& theme=p.theme;
    const auto s=l.scale;p.rect(l.panel,theme.panel);p.frame(l.panel,theme.border);
    p.label(title,{l.panel.x+24*s,l.panel.y+18*s,480*s,38*s},26,theme.text);
    p.label(subtitle,{l.panel.x+24*s,l.panel.y+l.panel.height-38*s,l.panel.width-48*s,28*s},14,theme.muted);
}
void drawHotbar(Painter& p,const SandboxUiLayout& l,const CreativeInventory& inventory,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas,const SurvivalUiContext* survival,const SandboxUiView* view,bool showHealth) {
    const auto& theme=p.theme;
    const auto s=l.scale;p.rect({l.barX-8*s,l.barY-8*s,512*s,64*s},theme.panel);
    const auto& items=survival?*survival->items:inventory.items();const auto selected=survival?survival->state->inventory.selectedSlot():inventory.selectedSlot();
    for(std::size_t i=0;i<9;++i) {
        const ItemStack* stack=survival?&survival->state->inventory.slots()[i]:nullptr;
        const ItemDefinition* item=stack?(stack->empty()?nullptr:&items.definition(stack->item)):(inventory.slots()[i]?&items.definition(*inventory.slots()[i]):nullptr);
        const auto r=l.hotbarSlot(i);p.slot(r,item,stack,blocks,atlas,i==selected,view&&view->hovered=="slot"+std::to_string(i));
        p.label(std::to_string(i+1),{r.x+3*s,r.y+1*s,16*s,18*s},11,theme.muted);
    }
    const auto selectedItem=survival?(survival->state->inventory.selected().empty()?std::optional<ItemId>{}:survival->state->inventory.selected().item):inventory.selectedItem();
    if(selectedItem) p.label(items.definition(*selectedItem).presentation.name,{l.barX,l.barY-30*s,496*s,24*s},17,theme.text,false,true);
    if(survival&&showHealth) {
        p.label("HEALTH",{l.barX,l.barY-65*s,74*s,22*s},12,theme.muted);
        for(unsigned i=0;i<20;++i) p.rect({l.barX+(78+static_cast<float>(i)*12)*s,l.barY-58*s,9*s,9*s},i<survival->state->health()?theme.success:theme.surface);
        p.label(std::to_string(survival->state->health())+" / 20",{l.barX+332*s,l.barY-65*s,164*s,22*s},14,theme.text);
    }
}
void drawInventory(Painter& p,const SandboxUiLayout& l,const CreativeInventory& inventory,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas,const SurvivalUiContext* survival,std::size_t page,const SandboxUiView* view) {
    const auto& theme=p.theme;
    const auto s=l.scale;
    heading(p,l,survival?"Backpack":"Creative inventory",survival?"Drag to move  ·  Right click to split  ·  Shift click to transfer":"Select an item for the hotbar  ·  Scroll for pages  ·  E / Esc to close");
    if(survival) {
        p.label("ITEMS",{l.panel.x+48*s,l.panel.y+108*s,600*s,28*s},14,theme.muted);
        for(std::size_t i=0;i<27;++i) {
            const auto& stack=survival->state->inventory.slots()[9+i];
            p.slot(l.backpackCell(i),stack.empty()?nullptr:&survival->items->definition(stack.item),&stack,blocks,atlas,false,view&&view->hovered=="slot"+std::to_string(9+i));
        }
        p.label("Your inventory is preserved when changing game mode.",{l.panel.x+48*s,l.panel.y+402*s,664*s,52*s},17,theme.muted);
    } else {
        p.search(l.search(),"Search items by name or identifier…",view);
        for(std::size_t i=0;i<7;++i) {
            constexpr std::array<std::string_view,7> labels{"All","Building","Natural","Tools","Materials","Food","Other"};
            p.button(l.category(i),labels[i],"category"+std::to_string(i),view,true,view&&static_cast<std::size_t>(view->category)==i,14);
        }
        const auto& catalog=view&&view->catalog?*view->catalog:inventory.catalog();
        for(std::size_t i=0;i<32&&page*32+i<catalog.size();++i) {
            const auto id=catalog[page*32+i];const bool hover=view&&(view->hovered=="catalog"+std::to_string(i)||view->focused=="catalog"+std::to_string(i));
            p.slot(l.catalogCell(i),&inventory.items().definition(id),nullptr,blocks,atlas,false,hover);
        }
        if(catalog.empty()) p.label("No matching items",{l.panel.x+36*s,l.panel.y+200*s,688*s,40*s},20,theme.muted,false,true);
        p.label(std::to_string(catalog.size())+" items   ·   Page "+std::to_string(page+1)+" / "+std::to_string(std::max<std::size_t>(1,(catalog.size()+31)/32)),
            {l.panel.x+36*s,l.panel.y+488*s,688*s,24*s},14,theme.muted);
    }
}
void drawCrafting(Painter& p,const SandboxUiLayout& l,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas,const SurvivalUiContext* survival,std::size_t page,const SandboxUiView* view) {
    const auto& theme=p.theme;
    heading(p,l,"Crafting","Required / available ingredients  ·  Scroll for recipes  ·  C / Esc to close");if(!survival) return;
    p.search(l.search(),"Search recipes…",view);
    std::vector<std::size_t> all;if(!view||!view->recipes) { for(std::size_t i=0;i<survival->recipes->size();++i) all.push_back(i); }
    const auto& indices=view&&view->recipes?*view->recipes:all;const auto s=l.scale;
    for(std::size_t i=0;i<5&&page*5+i<indices.size();++i) {
        const auto& recipe=survival->recipes->at(indices[page*5+i]);const auto& item=survival->items->definition(recipe.output);
        const auto r=l.recipeButton(i);const bool available=survival->state->inventory.canCraft(recipe,*survival->items)==CraftResult::Crafted;
        p.rect(r,view&&view->hovered=="recipe"+std::to_string(i)?theme.hover:theme.surface);p.frame(r,available?theme.border:theme.surface);
        p.item({r.x+8*s,r.y+8*s,42*s,42*s},item,blocks,atlas);
        p.label(item.presentation.name+" ×"+std::to_string(recipe.count),{r.x+62*s,r.y+4*s,420*s,26*s},18,available?theme.text:theme.muted);
        float x=r.x+62*s;
        for(const auto& ingredient:recipe.ingredients) {
            const auto& definition=survival->items->definition(ingredient.item);p.item({x,r.y+34*s,22*s,22*s},definition,blocks,atlas);
            const auto have=survival->state->inventory.count(ingredient.item);
            p.label(std::to_string(ingredient.count)+" / "+std::to_string(have),{x+28*s,r.y+34*s,90*s,24*s},14,have>=ingredient.count?theme.muted:theme.warning);x+=128*s;
        }
        p.button({r.x+r.width-104*s,r.y+14*s,92*s,38*s},available?"Craft":"Missing","recipe"+std::to_string(i),view,available,available,16);
    }
    if(indices.empty()) p.label("No matching recipes",{l.panel.x+24*s,l.panel.y+144*s,712*s,42*s},20,theme.muted,false,true);
}
void drawSettings(Painter& p,const SandboxUiLayout& l,const GameSettings& settings,const SandboxUiView* view) {
    const auto& theme=p.theme;
    heading(p,l,"Settings","Changes apply immediately  ·  Esc to return");const auto s=l.scale;
    std::ostringstream sensitivity;sensitivity<<std::fixed<<std::setprecision(4)<<settings.mouseSensitivity;
    const std::array<std::string,4> labels{"Field of view  "+std::to_string(static_cast<int>(settings.fieldOfView))+"°","Mouse sensitivity  "+sensitivity.str(),"Invert mouse Y","UI scale  "+std::to_string(static_cast<int>(std::round(settings.uiScale*100)))+"%"};
    for(std::size_t i=0;i<4;++i) p.label(labels[i],{l.panel.x+40*s,l.panel.y+(114+static_cast<float>(i)*80)*s,492*s,36*s},20,theme.text);
    for(std::size_t i=0;i<9;++i) {
        const auto label=i==4?(settings.invertY?"On":"Off"):i==5?"Reset defaults":i==6?"Done":(i==0||i==2||i==7)?"−":"+";
        p.button(l.settingsButton(i),label,"setting"+std::to_string(i),view);
    }
}
void drawPause(Painter& p,const SandboxUiLayout& l,const SandboxUiView* view) {
    heading(p,l,"Paused","Single player pauses simulation  ·  Multiplayer keeps running");
    for(std::size_t i=0;i<4;++i) p.button(l.pauseButton(i),std::array<std::string_view,4>{"Resume","Settings","Save world","Return to main menu"}[i],"pause"+std::to_string(i),view);
}
void drawConsole(Painter& p,const SandboxUiLayout& l,const SandboxUiView& view) {
    const auto& theme=p.theme;
    if(!view.console) return;
    const auto& console=*view.console;const auto s=l.scale;
    const float margin=24*s,w=std::min(p.viewport.width-2*margin,1100*s),x=margin;
    const float height=console.expanded()?p.viewport.height-2*margin:std::min(360*s,p.viewport.height-2*margin);
    const float y=p.viewport.height-margin-height;
    p.rect({x,y,w,height},theme.panel);p.frame({x,y,w,height},theme.border);
    p.label(console.expanded()?"Developer console":"Chat & commands",{x+16*s,y+10*s,w-32*s,32*s},20,theme.text);
    const float inputY=y+height-50*s;
    const float lineHeight=24*s;const auto rows=static_cast<std::size_t>(std::max(0.0f,(height-110*s)/lineHeight));
    const auto end=console.output().size()-std::min(console.output().size(),console.outputOffset());
    const auto start=end>rows?end-rows:0;
    for(auto i=start;i<end;++i) {
        const auto& message=console.output()[i];
        std::ostringstream stamp;stamp<<std::setfill('0')<<std::setw(2)<<(static_cast<int>(message.time)/60)%60<<':'<<std::setw(2)<<static_cast<int>(message.time)%60;
        const float top=y+48*s+static_cast<float>(i-start)*lineHeight;
        p.label(stamp.str(),{x+16*s,top,64*s,lineHeight},12,theme.muted,true);
        p.label(message.text,{x+86*s,top,w-102*s,lineHeight},16,severityColor(message.severity,theme),true);
    }
    bool validSyntax=true;
    if(console.editor().text().starts_with('/')&&console.editor().text().size()>1) { try { (void)commands::parse(console.editor().text()); } catch(const commands::Error&) { validSyntax=false; } }
    p.rect({x+12*s,inputY,w-24*s,38*s},theme.surface);p.frame({x+12*s,inputY,w-24*s,38*s},validSyntax?theme.accent:theme.error);
    const auto& edit=console.editor();const auto size=std::max(8.f,18*s);
    const auto prefixWidth=ui::fonts().measure(edit.text().substr(0,edit.cursor()),size,true).x;
    const auto offset=std::max(0.f,prefixWidth-(w-70*s));const auto origin=x+24*s-offset;
    const auto selection=edit.selection();
    if(selection.first!=selection.second) {
        const auto left=ui::fonts().measure(edit.text().substr(0,selection.first),size,true).x;
        const auto right=ui::fonts().measure(edit.text().substr(0,selection.second),size,true).x;
        const auto a=std::max(x+20*s,origin+left),b=std::min(x+w-20*s,origin+right);
        if(b>a) p.rect({a,inputY+7*s,b-a,24*s},theme.hover,true);
    }
    auto clip=ui::Rect{x+20*s,inputY+4*s,w-40*s,30*s};
    ui::fonts().append(p.hud.foreground,edit.text(),origin,inputY+6*s,size,glm::vec3(theme.text),clip,true);
    if(static_cast<int>(view.now*2)%2==0) p.rect({origin+prefixWidth,inputY+7*s,1.5f*s,24*s},theme.accent,true);
    if(edit.text().empty()) p.label("/help   ·   Tab complete   ·   ↑↓ history   ·   Esc close",{x+24*s,inputY+8*s,w-48*s,26*s},15,theme.muted,true);
    if(!console.suggestions().empty()) {
        const auto available=static_cast<std::size_t>(std::max(0.f,std::min(5.f,(y-margin)/(26*s))));
        const auto count=std::min(available,console.suggestions().size());const float top=y-static_cast<float>(count)*26*s-8*s;
        p.rect({x,top,std::min(w,560*s),static_cast<float>(count)*26*s},theme.panel);
        for(std::size_t i=0;i<count;++i) p.label(console.suggestions()[i].label,{x+16*s,top+static_cast<float>(i)*26*s,std::min(w,560*s)-32*s,26*s},16,i==console.selectedSuggestion()?theme.accent:theme.muted,true);
    }
}
void drawTooltip(Painter& p,const SandboxUiView& view) {
    const auto& theme=p.theme;
    if(view.tooltip.empty()||view.dragSource) return;
    const float s=p.s;const float w=std::min(390*s,p.viewport.width-16*s);std::vector<std::string_view> lines;
    std::string_view remaining=view.tooltip;
    while(!remaining.empty()&&lines.size()<5) { const auto pos=remaining.find('\n');lines.push_back(remaining.substr(0,pos));if(pos==std::string_view::npos) break;remaining.remove_prefix(pos+1); }
    const float h=(static_cast<float>(lines.size())*24+20)*s;
    const auto x=std::clamp(view.mouseX+16*s,8*s,p.viewport.width-w-8*s),y=std::clamp(view.mouseY+20*s,8*s,std::max(8*s,p.viewport.height-h-8*s));
    p.rect({x,y,w,h},theme.panel,true);p.frame({x,y,w,h},theme.border,1,true);
    for(std::size_t i=0;i<lines.size();++i) p.label(lines[i],{x+12*s,y+(10+static_cast<float>(i)*24)*s,w-24*s,24*s},i==0?18:14,i==0?theme.text:theme.muted,i==1);
}
}
HudGeometry sandboxHud(const CreativeInventory& inventory,const voxel::BlockRegistry& blocks,const AtlasLayout& atlas,int width,int height,bool inventoryOpen,std::size_t page,std::string_view status,bool settingsOpen,const GameSettings& settings,const SurvivalUiContext* survival,bool craftingOpen,const SandboxUiView* view,const ui::Theme& theme) {
    const SandboxUiLayout layout(width,height,settings.uiScale);Painter p{{},{0,0,static_cast<float>(width),static_cast<float>(height)},layout.scale,theme};
    const auto screen=view?view->screen:settingsOpen?UiScreen::Settings:craftingOpen?UiScreen::Crafting:inventoryOpen?UiScreen::Inventory:UiScreen::Gameplay;
    if(screen==UiScreen::Gameplay||screen==UiScreen::Inventory||screen==UiScreen::Crafting)
        drawHotbar(p,layout,inventory,blocks,atlas,survival,view,screen==UiScreen::Gameplay);
    if(screen==UiScreen::Inventory) drawInventory(p,layout,inventory,blocks,atlas,survival,page,view);
    else if(screen==UiScreen::Crafting) drawCrafting(p,layout,blocks,atlas,survival,page,view);
    else if(screen==UiScreen::Settings) drawSettings(p,layout,settings,view);
    else if(screen==UiScreen::Pause) drawPause(p,layout,view);
    else if(screen==UiScreen::Console&&view) drawConsole(p,layout,*view);
    if(survival&&!survival->state->alive()&&screen!=UiScreen::Console) {
        p.rect(layout.panel,theme.panel,true);p.frame(layout.panel,theme.border,1,true);
        p.label("You died",{layout.panel.x,layout.panel.y+140*layout.scale,layout.panel.width,60*layout.scale},32,theme.error,false,true);
        p.label("R to respawn  ·  Inventory is kept",{layout.panel.x,layout.panel.y+220*layout.scale,layout.panel.width,40*layout.scale},20,theme.text,false,true);
    }
    if(view&&view->notifications&&screen!=UiScreen::Console) {
        std::size_t index=0;for(auto it=view->notifications->rbegin();it!=view->notifications->rend()&&index<(screen==UiScreen::Gameplay?3u:1u);++it,++index) {
            const auto s=layout.scale;const float w=std::min(460*s,p.viewport.width-32*s),x=p.viewport.width-w-16*s,y=(20+static_cast<float>(index)*42)*s;
            const float alpha=static_cast<float>(std::clamp((4-(view->now-it->time))/.25,0.,1.));
            auto panel=theme.panel,accent=severityColor(it->severity,theme),text=theme.text;panel.a*=alpha;accent.a*=alpha;text.a*=alpha;
            p.rect({x,y,w,34*s},panel,true);p.rect({x,y,3*s,34*s},accent,true);
            p.label(it->text,{x+12*s,y+5*s,w-24*s,28*s},16,text);
        }
    }
    if(!status.empty()) p.label(status,{16*layout.scale,16*layout.scale,p.viewport.width-32*layout.scale,32*layout.scale},18,theme.text);
    if(view&&screen!=UiScreen::Console) drawTooltip(p,*view);
    return std::move(p.hud);
}
}
