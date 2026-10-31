#include "client/session_environment.hpp"
#include "client/preferences.hpp"
#include "engine/core/log.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesh_worker.hpp"
#include "game/network/identity.hpp"
#include "engine/voxel/raycast.hpp"
#include "engine/voxel/streaming.hpp"
#include "game/content/services.hpp"
#include "game/content/creature_models.hpp"
#include "game/gameplay/inventory.hpp"
#include "game/modding/runtime.hpp"
#include "game/network/client.hpp"
#include "game/player/controller.hpp"
#include "game/simulation/creatures.hpp"
#include "game/simulation/day_cycle.hpp"
#include "game/ui/sandbox_ui.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vc::client {
namespace {
voxel::Position cell(glm::dvec3 p) {
    return {static_cast<std::int64_t>(std::floor(p.x)),static_cast<std::int64_t>(std::floor(p.y)),static_cast<std::int64_t>(std::floor(p.z))};
}
struct Avatar { glm::dvec3 position{};double phase{};bool initialized{}; };
}
GameSession remoteSession(SessionEnvironment& host,Options options) {
    if(host.quitRequested) co_return SessionExit::Quit;
    if(host.leaveRequested) co_return SessionExit::Menu;
    auto& window=host.window;auto& renderer=host.renderer;const auto& preferences=host.preferences;auto directory=options.modsDirectory;
    if(directory.empty()&&!options.noMods&&!options.frameLimit) directory=Window::userDataPath()/"mods";
    ContentServices content(directory);auto& blocks=content.blocks();auto& mods=content.mods();const auto atlas=mods.atlas().layout();
    auto& items=content.items();auto& recipes=content.recipes();auto& mining=content.mining();
    const auto fingerprint=netgame::contentFingerprint(blocks,items,recipes,mining,mods);
    renderer.replaceAtlas(mods.atlas().image());
    netgame::RemoteSession session(options.connectHost,options.connectPort,options.playerName,fingerprint,blocks,options.identityFile.empty()?std::string{}:netgame::readIdentityToken(options.identityFile));
    log("Connecting to "+options.connectHost+':'+std::to_string(options.connectPort)+" as "+options.playerName);
    while(!session.welcome()) {
        const auto input=(co_await host.nextFrame());if(input.quit||host.quitRequested) { session.disconnect();co_return SessionExit::Quit; }
        if(input.closeUi||input.pause||host.leaveRequested) { session.disconnect();co_return SessionExit::Menu; }
        if(input.resized) renderer.requestResize();
        session.pump();
        if(!session.open()) throw std::runtime_error("Server closed during handshake");
        window.title("VoxelCraft: connecting to server");host.draw(Camera{});
    }
    const auto welcome=*session.welcome();log("Connected as "+welcome.profile.name+" (player "+std::to_string(welcome.id)+")");
    bool survivalMode=welcome.survival;
    std::uint64_t poseRevision=0;
    if(survivalMode&&options.fly) throw std::invalid_argument("Flight is disabled by this survival server");
    content.rebuildGameplay();
    CreativeInventory inventory(items);inventory.restore(welcome.profile.hotbar,welcome.profile.selectedSlot,blocks);
    SurvivalState survival;survival.inventory.restore(welcome.profile.items,welcome.profile.selectedSlot,items);survival.restoreHealth(welcome.profile.health);
    SurvivalUiContext survivalUi{&survival,&items,&recipes,[&](InventoryAction action){session.inventory(action);}};
    SandboxUiController ui(inventory,preferences.settings,survivalMode?&survivalUi:nullptr);Camera camera=welcome.profile.camera;
    camera.fieldOfView=static_cast<float>(preferences.settings.fieldOfView);camera.mouseSensitivity=preferences.settings.mouseSensitivity;camera.invertY=preferences.settings.invertY;
    ui.setCompletionContext({commands::Permission::Player,&items,{welcome.profile.name}});
    LivingWorld living(welcome.metadata.seed);DayCycle day(welcome.worldTime);voxel::Lighting lighting;voxel::BackgroundMeshQueue meshes(blocks,atlas);
    std::vector<Mesh> models;for(auto& model:builtinCreatureModels()) models.push_back(std::move(model));models.push_back(playerAvatarModel());renderer.setModels(models);
    std::map<std::uint64_t,Avatar> avatars;
    auto lastHotbar=inventory.names(blocks);auto lastSlot=inventory.selectedSlot();
    auto knownItems=welcome.profile.items;
    double last=Window::seconds(),diagnostic=last,lastSent=last-1,waitingSince=last;unsigned frames=0,diagnosticFrames=0;
    std::uint64_t stateTick=welcome.tick;std::uint32_t actions=options.fly&&!welcome.profile.flying?netgame::ToggleFlight:0u;
    bool capturedScreenshot=false,quitWithoutSaving=false,everReady=false,exitApplication=false;
    host.screens.show(ApplicationScreen::Gameplay);
    const auto notify=[&](std::string message) { ui.status(std::move(message),Window::seconds()); };
    notify("CONNECTED AS "+welcome.profile.name);window.pointerMode(false);window.capture(true);
    bool consoleCaptureBefore=true;
    for(;;) {
        auto input=(co_await host.nextFrame());if(input.quit||host.quitRequested||host.leaveRequested) { quitWithoutSaving=input.quitWithoutSaving;exitApplication=input.quit||host.quitRequested;host.leaveRequested=false;break; }
        const auto updateStart=Window::seconds();
        if(input.resized) renderer.requestResize();
        session.pump();
        if(!session.open()) throw std::runtime_error("Server disconnected");
        const auto now=Window::seconds(),delta=std::clamp(now-last,0.0,0.1);last=now;
        if(session.state().tick&&survivalMode!=session.state().survival.has_value()) {
            survivalMode=session.state().survival.has_value();ui.setSurvivalContext(survivalMode?&survivalUi:nullptr);actions=0;
        }
        if(session.state().tick!=stateTick) {
            const auto& authoritative=session.state();
            if(authoritative.hotbar!=lastHotbar) { inventory.restore(authoritative.hotbar,authoritative.selectedSlot);lastHotbar=authoritative.hotbar;lastSlot=inventory.selectedSlot(); }
            commands::CompletionContext completion;completion.permission=authoritative.permission;completion.items=&items;
            for(const auto& peer:authoritative.players) completion.players.push_back(peer.name);
            ui.setCompletionContext(std::move(completion));
            if(authoritative.poseRevision!=poseRevision) {
                poseRevision=authoritative.poseRevision;camera=session.camera();actions=0;
                camera.fieldOfView=static_cast<float>(ui.settings().fieldOfView);camera.mouseSensitivity=ui.settings().mouseSensitivity;camera.invertY=ui.settings().invertY;
                survival.inventory.select(authoritative.selectedSlot);
            }
        }
        for(auto& reply:session.takeCommandReplies()) ui.postCommandResult(reply.result,now);
        if(survivalMode&&session.state().survival) {
            const auto& authoritative=*session.state().survival;const bool alive=survival.alive();
            if(knownItems!=authoritative.items) {
                survival.inventory.restore(authoritative.items,survival.inventory.selectedSlot(),items);knownItems=authoritative.items;
            }
            if(survival.health()!=authoritative.health) survival.restoreHealth(authoritative.health);
            if(alive!=survival.alive()) {
                ui.closeMenus();window.pointerMode(!survival.alive());window.capture(survival.alive());actions=0;
                input.forward=input.right=input.up=input.mouseX=input.mouseY=0;
                input.destroy=input.place=input.destroyHeld=input.jump=input.toggleFlight=false;input.captured=survival.alive();
                if(survival.alive()) { camera=session.camera();notify("RESPAWNED - INVENTORY KEPT"); }
                else notify("YOU DIED - R TO RESPAWN");
            }
        }
        if(survivalMode&&!survival.alive()&&input.respawn) actions|=netgame::Respawn;
        const auto [width,height]=window.pixelSize();
        const bool closingConsole=ui.screen()==vc::UiScreen::Console&&!input.console;
        if(input.console) consoleCaptureBefore=input.consoleCapturedBefore;
        if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,input.material,input.scroll,input.cursorX,input.cursorY,input.settings,input.crafting,input.console,input.pause,input.primaryRelease,input.primaryHeld,input.shift,input.text},width,height)) {
            window.pointerMode(ui.open());window.capture(!ui.open()&&(!closingConsole||consoleCaptureBefore));actions=0;
            input.forward=input.right=input.up=input.mouseX=input.mouseY=0;input.destroy=input.place=input.jump=input.toggleFlight=false;input.captured=!ui.open()&&(!closingConsole||consoleCaptureBefore);
        }
        window.textInput(ui.wantsTextInput());window.pointerMode(ui.open());
        if(auto clipboard=ui.takeClipboard();!clipboard.empty()) window.clipboard(clipboard);
        for(const auto& command:ui.takeCommands()) session.command(command);
        if(ui.takeLeaveRequested()) { host.leaveRequested=true;continue; }
        if(ui.takeSaveRequested()) session.requestSave();
        camera.fieldOfView=static_cast<float>(ui.settings().fieldOfView);camera.mouseSensitivity=ui.settings().mouseSensitivity;camera.invertY=ui.settings().invertY;
        if(ui.takeSettingsChanged()) {
            host.preferences.settings=ui.settings();
            if(preferences.path.empty()) notify("SETTINGS APPLY FOR THIS SESSION");
            else { try { host.applySettings(ui.settings());notify("SETTINGS SAVED"); } catch(const std::exception& error) { log(error.what());notify("SETTINGS SAVE FAILED"); } }
        }
        const bool active=input.captured&&!ui.open();
        if(active) camera.update({0,0,0,input.mouseX,input.mouseY},delta);
        if(!active) actions&=netgame::Respawn;
        if(active&&input.jump) actions|=netgame::Jump;
        if(active&&!survivalMode&&input.toggleFlight) actions|=netgame::ToggleFlight;
        if(active&&input.destroy) actions|=netgame::Destroy;
        if(active&&input.place) actions|=netgame::Place;
        const auto hotbar=inventory.names(blocks);
        if(!survivalMode&&(hotbar!=lastHotbar||inventory.selectedSlot()!=lastSlot)) {
            session.hotbar({hotbar,static_cast<std::uint32_t>(inventory.selectedSlot())});lastHotbar=hotbar;lastSlot=inventory.selectedSlot();
        }
        if(now-lastSent>=1.0/60) {
            netgame::Input intent;intent.yaw=camera.yaw;intent.pitch=camera.pitch;
            intent.slot=static_cast<std::uint32_t>(survivalMode?survival.inventory.selectedSlot():inventory.selectedSlot());
            intent.flags=actions|(active?netgame::Active:0u)|(active&&input.destroyHeld?netgame::AttackHeld:0u);
            intent.forward=active?input.forward:0;intent.right=active?input.right:0;intent.up=active?input.up:0;
            session.input(intent);actions=0;lastSent=now;
        }
        if(input.save) { session.requestSave();notify("SAVE REQUESTED"); }
        session.pump();for(auto& message:session.takeNotices()) { log(message);notify(std::move(message)); }
        const auto& state=session.state();
        if(state.tick!=stateTick) { living.restore(state.actors,items);day=DayCycle(state.worldTime);stateTick=state.tick; }
        else day.advance(delta);
        const auto target=session.camera().position;
        if(glm::length(target-camera.position)>16) camera.position=target;
        else camera.position=glm::mix(camera.position,target,std::min(1.0,delta*25));
        auto current=cell(camera.position);current.y=std::clamp(current.y,welcome.metadata.bounds.minY,welcome.metadata.bounds.maxY);
        const auto dirty=session.takeDirtyChunks();const auto lightDirty=lighting.refresh(session.world(),blocks,dirty);meshes.invalidate(dirty);meshes.invalidate(lightDirty);
        const auto replacements=meshes.update(session.world(),cell(camera.position),2,&lighting);renderer.updateMeshes(replacements);
        if(!everReady) {
            const auto wanted=voxel::streamingRegion(current,welcome.metadata.bounds,static_cast<int>(welcome.radius),welcome.metadata.generatorVersion);
            everReady=!meshes.pendingCount()&&std::all_of(wanted.begin(),wanted.end(),[&](auto p){return session.world().findChunk(p)!=nullptr;});
            if(!everReady&&now-waitingSince>60) throw std::runtime_error("Server terrain stream timed out");
        }
        const auto hit=voxel::raycast(session.world(),camera.position,camera.forward(),6,survivalMode?&blocks:nullptr);
        renderer.setSelection(hit&&!ui.open()?std::optional(glm::i64vec3(hit->block.x,hit->block.y,hit->block.z)):std::nullopt);
        const auto& status=session.state().survival;
        renderer.setProgressIndicator(survivalMode&&status&&status->mining&&!ui.open()?std::optional(static_cast<float>(status->miningProgress)):std::nullopt);
        Atmosphere atmosphere;const auto daylight=day.daylight();atmosphere.skyColor=daylight.skyColor;atmosphere.sunDirection=daylight.sunDirection;
        atmosphere.sunlight=daylight.sunlight;atmosphere.ambient=daylight.ambient;atmosphere.horizontalFog=true;
        atmosphere.fogStart=static_cast<float>(std::max(8.0,(welcome.radius-0.8)*32));atmosphere.fogEnd=static_cast<float>(welcome.radius*32);
        if(blocks.definition(session.world().block(cell(camera.position))).fluid) { atmosphere.skyColor={0.008f,0.055f,0.12f};atmosphere.fogStart=1;atmosphere.fogEnd=22;atmosphere.horizontalFog=false; }
        renderer.setAtmosphere(atmosphere);std::vector<RenderInstance> instances;
        for(const auto& view:living.views()) {
            const auto p=cell(view.position+glm::dvec3(0,0.8,0));if(session.world().loaded(p)) instances.push_back({view.model,view.position,view.yaw,view.phase,{lighting.sky(p),lighting.block(p)}});
        }
        std::set<std::uint64_t> present;
        for(const auto& peer:state.players) {
            if(peer.id==welcome.id||!peer.health) continue;
            present.insert(peer.id);
            auto& avatar=avatars[peer.id];const auto feet=peer.camera.position-glm::dvec3(0,PlayerController::eyeHeight,0);
            const auto before=avatar.position;
            if(!avatar.initialized||glm::length(avatar.position-feet)>16) { avatar.position=feet;avatar.initialized=true; }
            else avatar.position=glm::mix(avatar.position,feet,std::min(1.0,delta*25));
            if(avatar.initialized&&glm::length(avatar.position-before)<16) avatar.phase=std::fmod(avatar.phase+glm::length(glm::dvec2(avatar.position.x-before.x,avatar.position.z-before.z))*7,6.28318530718);
            const auto p=cell(avatar.position+glm::dvec3(0,0.8,0));if(session.world().loaded(p)) instances.push_back({3,avatar.position,static_cast<float>(peer.camera.yaw),static_cast<float>(avatar.phase),{lighting.sky(p),lighting.block(p)}});
        }
        std::erase_if(avatars,[&](const auto& entry){return !present.contains(entry.first);});renderer.setInstances(instances);
        if(auto hud=ui.rebuild(blocks,atlas,width,height,now)) renderer.setHud(hud->colored,hud->textured,!ui.open(),hud->foreground);
        const auto updateMilliseconds=(Window::seconds()-updateStart)*1000;
        const bool capture=!capturedScreenshot&&!options.screenshot.empty()&&everReady&&frames>=(options.frameLimit?options.frameLimit-1:60);
        if(host.draw(camera,capture?options.screenshot:std::filesystem::path{})) {
            if(everReady) { ++frames;++diagnosticFrames; }if(capture) capturedScreenshot=true;
        } else Window::idle();
        if(now-diagnostic>=1) {
            const auto fps=static_cast<double>(diagnosticFrames)/(now-diagnostic);std::ostringstream title;
            title<<std::fixed<<std::setprecision(1)<<"VoxelCraft " VC_VERSION_STRING " | "<<welcome.profile.name<<" | "<<fps<<" FPS | "<<state.players.size()<<" players"
                <<" | "<<(survivalMode?"Survival":session.flying()?"Fly":"Walk")<<" | xyz "<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z
                <<" | update/draw/upload ms "<<updateMilliseconds<<"/"<<renderer.metrics().drawMilliseconds<<"/"<<renderer.metrics().uploadMilliseconds<<" | calls "<<renderer.metrics().drawCalls
                <<" | triangles "<<renderer.drawnTriangleCount()<<"/"<<renderer.triangleCount()<<" | drawn chunks "<<renderer.drawnMeshCount()<<" | mesh ms "<<meshes.metrics().lastMilliseconds<<" | net KiB tx/rx "<<session.sentBytes()/1024<<"/"<<session.receivedBytes()/1024
                <<" | chunks "<<session.world().chunks().size()<<" | mesh pending "<<meshes.pendingCount();
            window.title(title.str());log(title.str());diagnostic=now;diagnosticFrames=0;
        }
        if(options.frameLimit&&frames>=options.frameLimit) break;
    }
    if(!options.frameLimit&&!quitWithoutSaving) { session.requestSave();session.pump(); }
    session.disconnect();
    if(options.frameLimit&&frames<options.frameLimit) throw std::runtime_error("Network bounded run interrupted");
    if(!options.screenshot.empty()&&!capturedScreenshot) throw std::runtime_error("Network screenshot was not captured");
    log("Network client rendered "+std::to_string(frames)+" frames; received terrain and "+std::to_string(session.state().players.size())+" players");
    co_return options.frameLimit?SessionExit::Completed:exitApplication?SessionExit::Quit:SessionExit::Menu;
}
}
