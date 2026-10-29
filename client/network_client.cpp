#include "client/network_client.hpp"
#include "client/preferences.hpp"
#include "engine/core/log.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesh_worker.hpp"
#include "engine/voxel/raycast.hpp"
#include "engine/voxel/streaming.hpp"
#include "game/content/blocks.hpp"
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
void shutdown(VulkanRenderer& renderer) {
    renderer.shutdown();const auto validation=renderer.validationReport();
    if(validation.enabled) {
        log("Vulkan validation: "+std::to_string(validation.errors)+" errors, "+std::to_string(validation.warnings)+" warnings (including shutdown)");
        if(validation.errors) throw std::runtime_error("Network client Vulkan validation failed");
    }
}
}
int runRemote(const Options& options) {
    Window window;const auto preferences=loadPreferences(options);auto blocks=builtinBlocks();auto directory=options.modsDirectory;
    if(directory.empty()&&!options.noMods&&!options.frameLimit) directory=Window::userDataPath()/"mods";
    ModRuntime mods(directory,blocks);const auto atlas=mods.atlas().layout();
    auto items=builtinItems(blocks);auto recipes=builtinRecipes(items);mods.installItems(items,blocks,recipes);
    MiningRules mining(blocks,items);mods.installMining(mining,blocks,items);
    const auto fingerprint=netgame::contentFingerprint(blocks,items,recipes,mining,mods);
    VulkanRenderer renderer(window,mods.atlas().image(),options.requireValidation);
    netgame::RemoteSession session(options.connectHost,options.connectPort,options.playerName,fingerprint,blocks);
    log("Connecting to "+options.connectHost+':'+std::to_string(options.connectPort)+" as "+options.playerName);
    while(!session.welcome()) {
        const auto input=window.poll();if(input.quit) { session.disconnect();shutdown(renderer);return options.frameLimit?1:0; }
        if(input.resized) renderer.requestResize();
        session.pump();
        if(!session.open()) throw std::runtime_error("Server closed during handshake");
        window.title("VoxelCraft: connecting to server");renderer.draw(Camera{});
    }
    const auto welcome=*session.welcome();log("Connected as "+welcome.profile.name+" (player "+std::to_string(welcome.id)+")");
    if(welcome.survival&&options.fly) throw std::invalid_argument("Flight is disabled by this survival server");
    items=builtinItems(blocks);recipes=builtinRecipes(items);mods.installItems(items,blocks,recipes);
    CreativeInventory inventory(blocks);inventory.restore(welcome.profile.hotbar,welcome.profile.selectedSlot,blocks);
    SurvivalState survival;survival.inventory.restore(welcome.profile.items,welcome.profile.selectedSlot,items);survival.restoreHealth(welcome.profile.health);
    SurvivalUiContext survivalUi{&survival,&items,&recipes,[&](InventoryAction action){session.inventory(action);}};
    SandboxUiController ui(inventory,preferences.settings,welcome.survival?&survivalUi:nullptr);Camera camera=welcome.profile.camera;
    camera.fieldOfView=static_cast<float>(preferences.settings.fieldOfView);camera.mouseSensitivity=preferences.settings.mouseSensitivity;camera.invertY=preferences.settings.invertY;
    LivingWorld living(welcome.metadata.seed);DayCycle day(welcome.worldTime);voxel::Lighting lighting;voxel::BackgroundMeshQueue meshes(blocks,atlas);
    std::vector<Mesh> models;for(auto& model:builtinCreatureModels()) models.push_back(std::move(model));models.push_back(playerAvatarModel());renderer.setModels(models);
    std::map<std::uint64_t,Avatar> avatars;
    auto lastHotbar=inventory.names(blocks);auto lastSlot=inventory.selectedSlot();
    auto knownItems=welcome.profile.items;
    double last=Window::seconds(),diagnostic=last,lastSent=last-1,waitingSince=last;unsigned frames=0,diagnosticFrames=0;
    std::uint64_t stateTick=welcome.tick;std::uint32_t actions=options.fly&&!welcome.profile.flying?netgame::ToggleFlight:0u;
    bool capturedScreenshot=false,quitWithoutSaving=false,everReady=false;
    const auto notify=[&](std::string message) { ui.status(std::move(message),Window::seconds()); };
    notify("CONNECTED AS "+welcome.profile.name);window.pointerMode(false);window.capture(true);
    for(;;) {
        auto input=window.poll();if(input.quit) { quitWithoutSaving=input.quitWithoutSaving;break; }
        if(input.resized) renderer.requestResize();
        session.pump();
        if(!session.open()) throw std::runtime_error("Server disconnected");
        const auto now=Window::seconds(),delta=std::clamp(now-last,0.0,0.1);last=now;
        if(welcome.survival&&session.state().survival) {
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
        if(welcome.survival&&!survival.alive()&&input.respawn) actions|=netgame::Respawn;
        const auto [width,height]=window.pixelSize();
        if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,input.material,input.scroll,input.cursorX,input.cursorY,input.settings,input.crafting},width,height)) {
            window.pointerMode(ui.open());window.capture(!ui.open());actions=0;
            input.forward=input.right=input.up=input.mouseX=input.mouseY=0;input.destroy=input.place=input.jump=input.toggleFlight=false;input.captured=!ui.open();
        }
        camera.fieldOfView=static_cast<float>(ui.settings().fieldOfView);camera.mouseSensitivity=ui.settings().mouseSensitivity;camera.invertY=ui.settings().invertY;
        if(ui.takeSettingsChanged()) {
            if(preferences.path.empty()) notify("SETTINGS APPLY FOR THIS SESSION");
            else { try { saveSettings(preferences.path,ui.settings());notify("SETTINGS SAVED"); } catch(const std::exception& error) { log(error.what());notify("SETTINGS SAVE FAILED"); } }
        }
        const bool active=input.captured&&!ui.open();
        if(active) camera.update({0,0,0,input.mouseX,input.mouseY},delta);
        if(!active) actions&=netgame::Respawn;
        if(active&&input.jump) actions|=netgame::Jump;
        if(active&&!welcome.survival&&input.toggleFlight) actions|=netgame::ToggleFlight;
        if(active&&input.destroy) actions|=netgame::Destroy;
        if(active&&input.place) actions|=netgame::Place;
        const auto hotbar=inventory.names(blocks);
        if(!welcome.survival&&(hotbar!=lastHotbar||inventory.selectedSlot()!=lastSlot)) {
            session.hotbar({hotbar,static_cast<std::uint32_t>(inventory.selectedSlot())});lastHotbar=hotbar;lastSlot=inventory.selectedSlot();
        }
        if(now-lastSent>=1.0/60) {
            netgame::Input intent;intent.yaw=camera.yaw;intent.pitch=camera.pitch;
            intent.slot=static_cast<std::uint32_t>(welcome.survival?survival.inventory.selectedSlot():inventory.selectedSlot());
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
        const auto hit=voxel::raycast(session.world(),camera.position,camera.forward(),6,welcome.survival?&blocks:nullptr);
        renderer.setSelection(hit&&!ui.open()?std::optional(glm::i64vec3(hit->block.x,hit->block.y,hit->block.z)):std::nullopt);
        const auto& status=session.state().survival;
        renderer.setProgressIndicator(welcome.survival&&status&&status->mining&&!ui.open()?std::optional(static_cast<float>(status->miningProgress)):std::nullopt);
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
        const bool capture=!capturedScreenshot&&!options.screenshot.empty()&&everReady&&frames>=(options.frameLimit?options.frameLimit-1:60);
        if(renderer.draw(camera,capture?options.screenshot:std::filesystem::path{})) {
            if(everReady) { ++frames;++diagnosticFrames; }if(capture) capturedScreenshot=true;
        } else Window::idle();
        if(now-diagnostic>=1) {
            const auto fps=static_cast<double>(diagnosticFrames)/(now-diagnostic);std::ostringstream title;
            title<<std::fixed<<std::setprecision(1)<<"VoxelCraft " VC_VERSION_STRING " | "<<welcome.profile.name<<" | "<<fps<<" FPS | "<<state.players.size()<<" players"
                <<" | "<<(welcome.survival?"Survival":session.flying()?"Fly":"Walk")<<" | xyz "<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z
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
    shutdown(renderer);return 0;
}
}
