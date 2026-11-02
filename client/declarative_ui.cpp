#include "client/declarative_ui.hpp"
#include "engine/core/log.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <utility>
#include <stdexcept>

namespace vc::client {
DeclarativeUi::DeclarativeUi(ResourceCatalog& catalog,const std::filesystem::path& fonts,std::string address,std::string name):catalog_(catalog) {
    runtime_.resources().mountDirectory("ui",fonts);
    runtime_.resources().mount([this](std::string_view id) { return builtinOnly_?catalog_.loadBuiltin(id):catalog_.load(id); });
    runtime_.data().set("menu.address",std::move(address));runtime_.data().set("menu.name",std::move(name));
    for(auto [id,action]:{std::pair{"voxelcraft:play",MenuAction::Play},{"voxelcraft:connect",MenuAction::Connect},
                        {"voxelcraft:quit",MenuAction::Quit},{"voxelcraft:back",MenuAction::Back},
                        {"voxelcraft:resume",MenuAction::Resume},{"voxelcraft:settings",MenuAction::Settings},
                        {"voxelcraft:save",MenuAction::Save},{"voxelcraft:leave",MenuAction::Leave}})
        runtime_.actions().registerAction(id,{{},{},[this,action](const auto&)->std::optional<std::string> { pending_=action;return {}; }});
    runtime_.clipboard([] {
        char* text=SDL_GetClipboardText();std::string result=text?text:"";SDL_free(text);return result;
    },[](std::string text) { if(!SDL_SetClipboardText(text.c_str())) log(SDL_GetError()); });
}
void DeclarativeUi::reload() { builtinOnly_=false;current_.reset();loadedResource_.clear();pending_=MenuAction::None;runtime_.resources().clear(); }
void DeclarativeUi::show(std::string_view id,const voxelui::FrameInfo& frame,bool focus) {
    runtime_.update(frame);
    if(loadedResource_!=id) {
        pending_=MenuAction::None;
        if(!runtime_.load(id)) {
            for(const auto& d:runtime_.diagnostics()) log(d.source+": "+d.message);
            builtinOnly_=true;runtime_.resources().clear();
            if(!runtime_.load(id)) {
                std::string error="Builtin declarative screen failed";
                for(const auto& d:runtime_.diagnostics()) error+="\n"+d.source+": "+d.message;
                throw std::runtime_error(error);
            }
        }
        loadedResource_=id;
        voxelui::Event event;event.type=voxelui::EventType::keyDown;event.key=voxelui::Key::tab;
        if(focus&&runtime_.focusedId().empty()) runtime_.processEvent(event);
    }
    runtime_.update(frame);
}
void DeclarativeUi::update(ApplicationScreen screen,std::string_view message,int width,int height,float scale) {
    runtime_.data().set("menu.message",std::string(message));
    runtime_.data().set("menu.title",screen==ApplicationScreen::Error?"Session could not continue":"Please wait");
    // Input already uses physical framebuffer pixels; apply UI scale once.
    voxelui::FrameInfo frame;frame.viewport={static_cast<float>(width),static_cast<float>(height)};
    frame.uiScale=std::min({scale,static_cast<float>(width)/720.f,static_cast<float>(height)/700.f});
    const std::string_view id=screen==ApplicationScreen::MainMenu?"voxelcraft:ui/main-menu.xml":
        screen==ApplicationScreen::Error?"voxelcraft:ui/error.xml":"voxelcraft:ui/status.xml";
    show(id,frame,true);current_=screen;
}
bool DeclarativeUi::updateGameplay(UiScreen screen,bool survival,unsigned health,bool remote,int width,int height,float scale) {
    if(width<=0||height<=0) return false;
    if((screen!=UiScreen::Gameplay&&screen!=UiScreen::Pause)||(survival&&health==0)) {
        if(!loadedResource_.empty()) {
            voxelui::Event event;event.type=voxelui::EventType::focusLost;runtime_.processEvent(event);
            runtime_.close();loadedResource_.clear();pending_=MenuAction::None;
        }
        return false;
    }
    runtime_.data().set("game.mode",survival?"Survival":"Creative");
    runtime_.data().set("game.health",static_cast<double>(std::min(health,20u)));
    runtime_.data().set("game.healthText",std::to_string(health)+" / 20");
    runtime_.data().set("game.survival",survival);
    runtime_.data().set("game.pauseHint",remote?"Multiplayer world keeps running":"Single player simulation is paused");
    voxelui::FrameInfo frame;frame.viewport={static_cast<float>(width),static_cast<float>(height)};
    frame.uiScale=std::min({scale,static_cast<float>(width)/800.f,static_cast<float>(height)/700.f});
    show(screen==UiScreen::Pause?"voxelcraft:ui/pause.xml":"voxelcraft:ui/hud.xml",frame,screen==UiScreen::Pause);
    current_=ApplicationScreen::Gameplay;return true;
}
bool DeclarativeUi::processGameplay(SandboxUiController& ui,const Input& input,int width,int height) {
    ui.externalChrome(true);
    const auto before=ui.screen();const bool pause=before==UiScreen::Pause;
    MenuAction action=MenuAction::None;
    if(pause) { process(input);action=takeAction(); }
    else if(input.focusLost) { voxelui::Event event;event.type=voxelui::EventType::focusLost;runtime_.processEvent(event); }
    auto text=input.text;
    if(pause) { text={};text.escape=input.text.escape; }
    const bool changed=ui.process({input.inventory,input.closeUi,!pause&&input.primaryClick,!pause&&input.secondaryClick,
        pause?0:input.material,pause?0:input.scroll,input.cursorX,input.cursorY,input.settings,input.crafting,input.console,
        input.pause,!pause&&input.primaryRelease,!pause&&input.primaryHeld,input.shift,std::move(text)},width,height);
    // A coalesced Escape or screen toggle wins over a stale queued click.
    if(pause&&ui.screen()==UiScreen::Pause) {
        switch(action) {
        case MenuAction::Resume:ui.pauseAction(PauseAction::Resume);break;
        case MenuAction::Settings:ui.pauseAction(PauseAction::Settings);break;
        case MenuAction::Save:ui.pauseAction(PauseAction::Save);break;
        case MenuAction::Leave:ui.pauseAction(PauseAction::Leave);break;
        default:break;
        }
    }
    return changed||before!=ui.screen();
}
void DeclarativeUi::process(const Input& input) {
    using namespace voxelui;
    Event e;e.position={input.cursorX,input.cursorY};e.shift=input.text.shift;
    if(input.focusLost) { e.type=EventType::focusLost;runtime_.processEvent(e);return; }
    e.type=EventType::pointerMove;runtime_.processEvent(e);
    if(input.primaryClick) { e.type=EventType::pointerDown;runtime_.processEvent(e); }
    if(input.primaryRelease||(input.primaryClick&&!input.primaryHeld)) { e.type=EventType::pointerUp;runtime_.processEvent(e); }
    if(input.scroll) { e.type=EventType::wheel;e.delta=static_cast<float>(input.scroll);runtime_.processEvent(e); }
    const auto key=[&](bool pressed,Key k,bool control=false,bool shift=false) {
        if(!pressed) return;
        Event event;event.type=EventType::keyDown;event.key=k;event.control=control;event.shift=shift;runtime_.processEvent(event);
    };
    const auto& t=input.text;
    key(t.tab||t.up||t.down,Key::tab,false,t.shift||t.up);key(t.enter,Key::enter);
    key(t.left,Key::left,false,t.shift);key(t.right,Key::right,false,t.shift);
    key(t.home,Key::home,false,t.shift);key(t.end,Key::end,false,t.shift);
    key(t.backspace,Key::backspace);key(t.erase,Key::del);
    key(t.selectAll,Key::a,true);key(t.copy,Key::c,true);key(t.cut,Key::x,true);
    // Window::poll already appends pasted bytes; don't paste a second time.
    key(t.paste&&t.text.empty(),Key::v,true);
    if(!t.text.empty()) { Event text;text.type=EventType::textInput;text.text=t.text;runtime_.processEvent(text); }
    if(t.compositionChanged) { Event text;text.type=EventType::composition;text.text=t.composition;runtime_.processEvent(text); }
    if(input.closeUi&&current_==ApplicationScreen::Error) pending_=MenuAction::Back;
}
MenuAction DeclarativeUi::takeAction() { return std::exchange(pending_,MenuAction::None); }
std::string DeclarativeUi::address() const { const auto* value=runtime_.data().get("menu.address");return value?value->string():""; }
std::string DeclarativeUi::name() const { const auto* value=runtime_.data().get("menu.name");return value?value->string():""; }
bool DeclarativeUi::editing() const { const auto* focused=runtime_.find(runtime_.focusedId());return focused&&focused->node.tag=="TextInput"; }
voxelui::DrawList DeclarativeUi::draw() { return runtime_.buildDrawList(); }
}
