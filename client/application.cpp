#include "client/application.hpp"
#if VC_HAS_VOXELUI
#include "client/declarative_ui.hpp"
#endif
#include "client/session_environment.hpp"
#include "engine/core/log.hpp"
#include "engine/core/utf8.hpp"
#include "engine/network/address.hpp"
#include "engine/ui/widgets.hpp"
#include "game/content/atlas.hpp"
#include "game/ui/resource_theme.hpp"
#include "game/player/profile.hpp"
#include "game/player/camera.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace vc::client {
namespace {
void rectangle(Mesh& mesh,ui::Rect r,glm::vec4 color) {
    const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
    for(const auto p:std::array<glm::vec2,4>{{{r.x,r.y},{r.x+r.width,r.y},{r.x+r.width,r.y+r.height},{r.x,r.y+r.height}}})
        mesh.vertices.push_back({{p.x,p.y,0},glm::vec3(color),{}, {},color.a});
    for(auto i:{0u,1u,2u,2u,3u,0u}) mesh.indices.push_back(base+i);
}
}
struct ClientApplication::Impl {
    Options options;
    Window window;
    Preferences preferences;
    ResourceCatalog resources;
    ui::Theme theme;
    VulkanRenderer renderer;
    ScreenManager screens;
    SessionEnvironment host;
    ui::WidgetTree widgets;
    ui::TextEdit address{300},name{32};
    bool running{true},active{},dirty{true},shutdown{};
    int width{},height{};
    ApplicationScreen painted{ApplicationScreen::Error};
    std::string paintedMessage;
    float scale{1},left{},top{};
#if VC_HAS_VOXELUI
    std::unique_ptr<DeclarativeUi> declarative;
#endif
    GameSession session; // Destroy first, before every borrowed application owner.

    explicit Impl(Options config):options(std::move(config)),window(options.width,options.height,options.fullscreen),
        preferences(loadPreferences(options)),resources(window.assetPath()/"resources"),renderer(window,builtinAtlas(),options.requireValidation,options.vsync),
        host{window,renderer,preferences,screens,theme,{},false,false,false} {
        address.set(options.connectHost.empty()?"127.0.0.1:25565":
            (options.connectHost.find(':')!=std::string::npos?"["+options.connectHost+"]":options.connectHost)+":"+std::to_string(options.connectPort));
        name.set(options.playerName);resetInput();
        try { activate(options.resourcePacks); }
        catch(const std::exception& error) { log(std::string("Invalid resource pack; using builtin: ")+error.what());activate({}); }
#if VC_HAS_VOXELUI
        declarative=std::make_unique<DeclarativeUi>(resources,window.assetPath()/"fonts",address.text(),name.text());
        host.declarative=declarative.get();
#endif
        log("ClientApplication ready: main menu without a world");
    }
    void resetInput() { window.capture(false);window.pointerMode(true);window.textInput(false);window.resetInput(); }
    void activate(const std::vector<std::filesystem::path>& packs) {
        if(active) throw std::logic_error("Return to menu before switching resource packs");
        auto candidate=resources;candidate.activate(packs);std::vector<std::string> diagnostics;
        const auto next=loadResourceTheme(candidate,diagnostics);
        for(const auto& error:diagnostics) log(error);
        for(const auto& error:candidate.takeDiagnostics()) log(error);
        resources=std::move(candidate);theme=next;options.resourcePacks=packs;dirty=true;
#if VC_HAS_VOXELUI
        if(declarative) declarative->reload();
#endif
    }
    void start(Options config,bool remote) {
        if(active) throw std::logic_error("Leave the current session before starting another");
        if(shutdown) throw std::logic_error("Application is shut down");
        resetInput();renderer.clearSession();
        host.input={};host.leaveRequested=host.quitRequested=false;
        screens.show(ApplicationScreen::Loading,remote?"Connecting — Escape to cancel":"Loading world — Escape to cancel");
        session=remote?remoteSession(host,std::move(config)):localSession(host,std::move(config));
        active=true;dirty=true;
    }
    void finish(SessionExit exit) {
        session.reset();active=false; // Joins generation/meshing/snapshot work and releases save lock.
        renderer.clearSession();resetInput();
        screens.show(ApplicationScreen::MainMenu);dirty=true;
        if(exit!=SessionExit::Menu) running=false;
    }
    void layout() {
        const auto focus=widgets.focused();widgets.clear();
        scale=std::min(static_cast<float>(width)/680,static_cast<float>(height)/580);
        scale=std::min(scale,static_cast<float>(preferences.settings.uiScale));
        left=(static_cast<float>(width)-600*scale)/2;top=(static_cast<float>(height)-540*scale)/2;
        const auto add=[&](std::string id,ui::WidgetKind kind,float y) {
            widgets.add({std::move(id),kind,{left+40*scale,top+y*scale,520*scale,44*scale},true,true,{}});
        };
        if(screens.current()==ApplicationScreen::MainMenu) {
            add("play",ui::WidgetKind::Button,120);add("address",ui::WidgetKind::TextInput,215);
            add("name",ui::WidgetKind::TextInput,295);add("connect",ui::WidgetKind::Button,360);add("quit",ui::WidgetKind::Button,425);
        } else if(screens.current()==ApplicationScreen::Error) add("back",ui::WidgetKind::Button,425);
        const bool retained=std::any_of(widgets.widgets().begin(),widgets.widgets().end(),[&](const auto& widget){return widget.id==focus;});
        widgets.focus(retained?focus:screens.current()==ApplicationScreen::Error?"back":"play");dirty=true;
    }
    void chrome() {
        if(screens.current()==ApplicationScreen::Gameplay) {
#if VC_HAS_VOXELUI
            if(painted!=ApplicationScreen::Gameplay&&!host.gameplayUiPrepared) renderer.clearUi();
            painted=ApplicationScreen::Gameplay;
#endif
            return;
        }
        const auto size=window.pixelSize();
        if(size.first<=0||size.second<=0) { dirty=true;return; }
#if VC_HAS_VOXELUI
        width=size.first;height=size.second;painted=screens.current();
        declarative->update(screens.current(),screens.message(),width,height,static_cast<float>(preferences.settings.uiScale));
        renderer.setUi(declarative->draw());
        return;
#endif
        if(width!=size.first||height!=size.second||painted!=screens.current()||paintedMessage!=screens.message()) {
            width=size.first;height=size.second;painted=screens.current();paintedMessage=screens.message();layout();
        }
        if(!dirty) return;
        Mesh colors,text;
        rectangle(colors,{0,0,static_cast<float>(width),static_cast<float>(height)},glm::vec4(glm::vec3(theme.panel)*0.6f,1));
        rectangle(colors,{left,top,600*scale,540*scale},theme.panel);
        auto& font=ui::fonts();const auto label=[&](std::string_view value,float x,float y,float pixels,glm::vec3 color=glm::vec3(0.92f)) {
            font.append(text,value,left+x*scale,top+y*scale,pixels*scale,color,{left,top,600*scale,540*scale});
        };
        label("VoxelCraft",40,30,36);
        if(screens.current()==ApplicationScreen::MainMenu) {
            label("Choose a session",40,80,18);label("Server address",40,187,17);label("Player name",40,267,17);
        } else {
            label(screens.current()==ApplicationScreen::Error?"Session could not continue":"Please wait",40,115,23);
            // Logs retain the complete diagnostic; bound the transitional menu payload.
            const auto message=utf8::singleLine(screens.message(),240);
            std::string row;float y=175;
            for(std::size_t begin=0;begin<message.size()&&y<380;) {
                const auto end=message.find(' ',begin);const auto word=message.substr(begin,end==std::string::npos?message.size()-begin:end-begin);
                if(!row.empty()&&font.measure(row+" "+word,16*scale).x>520*scale) {
                    label(row,40,y,16,{1,0.65f,0.6f});row.clear();y+=24;
                }
                if(!row.empty()) row+=' ';
                row+=word;if(end==std::string::npos) break;begin=end+1;
            }
            label(row,40,y,16,{1,0.65f,0.6f});
        }
        for(const auto& widget:widgets.widgets()) {
            const bool selected=widget.id==widgets.hovered()||widget.id==widgets.focused();
            rectangle(colors,widget.bounds,selected?theme.hover:theme.surface);
            const std::string_view value=widget.id=="play"?"Play sandbox":widget.id=="connect"?"Connect":widget.id=="quit"?"Quit":
                widget.id=="back"?"Back to main menu":widget.id=="address"?std::string_view(address.text()):std::string_view(name.text());
            font.append(text,value,widget.bounds.x+12*scale,widget.bounds.y+10*scale,20*scale,glm::vec3(theme.text),widget.bounds);
            if(widget.kind==ui::WidgetKind::TextInput&&widget.id==widgets.focused()) {
                const auto& editor=widget.id=="address"?address:name;
                const auto prefix=std::string_view(editor.text()).substr(0,editor.cursor());
                const float x=std::min(widget.bounds.x+12*scale+font.measure(prefix,20*scale).x,widget.bounds.x+widget.bounds.width-4*scale);
                rectangle(colors,{x,widget.bounds.y+9*scale,2*scale,25*scale},{0.8f,1,0.9f,1});
            }
        }
        renderer.setHud(colors,{},false,text);dirty=false;
    }
    void menuInput(const Input& input) {
        std::string activate;
#if VC_HAS_VOXELUI
        declarative->process(input);window.textInput(declarative->editing());
        switch(declarative->takeAction()) {
        case MenuAction::Play: activate="play";break;
        case MenuAction::Connect: activate="connect";break;
        case MenuAction::Quit: activate="quit";break;
        case MenuAction::Back: activate="back";break;
        case MenuAction::None: break;
        case MenuAction::Resume:case MenuAction::Settings:case MenuAction::Save:case MenuAction::Leave:break;
        }
        address.set(declarative->address());name.set(declarative->name());
#else
        if(widgets.hover(input.cursorX,input.cursorY)) dirty=true;
        if(input.text.tab||input.text.up||input.text.down) {
            widgets.navigate(input.text.up||(input.text.tab&&input.text.shift)?-1:1);dirty=true;
        }
        if(input.primaryClick) if(const auto* hit=widgets.hit(input.cursorX,input.cursorY)) { widgets.focus(hit->id);activate=hit->id;dirty=true; }
        if(input.text.enter) activate=widgets.focused();
        const bool editing=widgets.focused()=="address"||widgets.focused()=="name";
        window.textInput(editing);
        if(editing) dirty=(widgets.focused()=="address"?address:name).process(input.text)||dirty;
        if(input.closeUi&&screens.current()==ApplicationScreen::Error) activate="back";
#endif
        if(activate=="play") { auto config=options;config.connectHost.clear();config.frameLimit=0;config.smoke=false;start(std::move(config),false); }
        else if(activate=="connect") {
            try {
                const auto endpoint=net::parseEndpoint(address.text());
                if(!validPlayerName(name.text())) throw std::invalid_argument("Player name must contain 1..24 letters, digits, underscores or hyphens");
                auto config=options;config.worldDirectory.clear();config.connectHost=endpoint.host;config.connectPort=endpoint.port;
                config.playerName=name.text();config.frameLimit=0;config.smoke=false;start(std::move(config),true);
            } catch(const std::exception& error) { screens.show(ApplicationScreen::Error,error.what());dirty=true;log(error.what()); }
        } else if(activate=="quit") running=false;
        else if(activate=="back") { screens.show(ApplicationScreen::MainMenu);dirty=true; }
    }
    void frame(Input input) {
        if(shutdown) throw std::logic_error("Application is shut down");
        if(!running) return;
        if(input.resized) renderer.requestResize();
#if VC_HAS_VOXELUI
        host.gameplayUiPrepared=false;
#endif
        chrome();host.presented=false;
        if(active) {
            if(input.quit) host.quitRequested=true;
            if(screens.current()==ApplicationScreen::Loading&&(input.closeUi||input.pause)) host.leaveRequested=true;
            host.input=std::move(input);session.resume();
            if(session.done()) {
                try { const auto exit=session.result();finish(exit); }
                catch(const std::exception& error) {
                    finish(SessionExit::Menu);log(error.what());
                    if(options.frameLimit) throw;
                    screens.show(ApplicationScreen::Error,error.what());dirty=true;
                }
            }
        } else {
            if(input.quit) running=false;
            else menuInput(input);
        }
        chrome();
        if(!host.presented&&running) { if(!renderer.draw(Camera{})) Window::idle(); }
    }
};
ClientApplication::ClientApplication(Options options):impl_(std::make_unique<Impl>(std::move(options))) {}
ClientApplication::~ClientApplication()=default;
void ClientApplication::frame(Input input) { impl_->frame(std::move(input)); }
void ClientApplication::startLocal(Options options) { impl_->start(std::move(options),false); }
void ClientApplication::startRemote(Options options) { impl_->start(std::move(options),true); }
void ClientApplication::requestLeave() { impl_->host.leaveRequested=true; }
void ClientApplication::requestQuit() { if(impl_->active) impl_->host.quitRequested=true;else impl_->running=false; }
bool ClientApplication::hasSession() const { return impl_->active; }
bool ClientApplication::running() const { return impl_->running; }
const GameSettings& ClientApplication::settings() const { return impl_->preferences.settings; }
const ResourceCatalog& ClientApplication::resources() const { return impl_->resources; }
void ClientApplication::activateResourcePacks(const std::vector<std::filesystem::path>& packs) { impl_->activate(packs); }
ApplicationScreen ClientApplication::screen() const { return impl_->screens.current(); }
std::uint32_t ClientApplication::windowId() const { return impl_->window.id(); }
VulkanRenderer& ClientApplication::renderer() { return impl_->renderer; }
void ClientApplication::shutdown() {
    if(impl_->active) throw std::logic_error("Finish or cancel the session before application shutdown");
    impl_->renderer.shutdown();impl_->shutdown=true;
}
ValidationReport ClientApplication::validationReport() const { return impl_->renderer.validationReport(); }
int ClientApplication::run() {
    if(impl_->options.frameLimit||!impl_->options.worldDirectory.empty()||!impl_->options.connectHost.empty())
        impl_->start(impl_->options,!impl_->options.connectHost.empty());
    while(impl_->running) frame(impl_->window.poll());
    shutdown();const auto report=validationReport();
    if(report.enabled) {
        log("Vulkan validation: "+std::to_string(report.errors)+" errors, "+std::to_string(report.warnings)+" warnings (including shutdown)");
        if(report.errors) throw std::runtime_error("Application Vulkan validation failed");
    }
    return 0;
}
}
