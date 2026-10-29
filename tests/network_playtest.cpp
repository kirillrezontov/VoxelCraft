#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "game/content/blocks.hpp"
#include "game/content/creature_models.hpp"
#include "game/network/client.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "server/host.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void key(vc::Window& window,SDL_Scancode scan,bool down) {
    SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
    event.key.windowID=window.id();event.key.scancode=scan;event.key.down=down;
    require(SDL_PushEvent(&event),SDL_GetError());
}
vc::voxel::Position cell(glm::dvec3 p) {
    return {static_cast<std::int64_t>(std::floor(p.x)),static_cast<std::int64_t>(std::floor(p.y)),static_cast<std::int64_t>(std::floor(p.z))};
}
void prepare(const std::filesystem::path& path,const vc::voxel::BlockRegistry& blocks) {
    vc::voxel::World world({0,31});const auto stone=blocks.require("voxelcraft:stone");
    for(int x=-1;x<=1;++x) for(int z=-1;z<=1;++z) {
        vc::voxel::Chunk chunk({x,0,z});
        for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},stone);
        world.insert(std::move(chunk));
    }
    vc::PlayerSaveState state;state.camera.position={0.5,3.62,6.5};state.camera.yaw=0;state.camera.pitch=-0.3;state.flying=true;
    vc::PlayerProfile alice;alice.name="Alice";alice.camera=state.camera;alice.flying=true;
    vc::PlayerProfile bob=alice;bob.name="Bob";bob.camera.position={1.5,2.62,0.5};bob.camera.yaw=3.14159265359;
    state.profiles={alice,bob};
    vc::voxel::WorldStore store(path);store.save(world,blocks,{42,{0,31},1},vc::encodePlayer(state));
}
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"Network playtest needs two captures and the example mod root");
        vc::Window window;auto blocks=vc::builtinBlocks();vc::ModRuntime mods(vc::utf8Path(argv[3]),blocks);
        vc::VulkanRenderer renderer(window,mods.atlas().image(),true);
        const std::array models{vc::playerAvatarModel()};renderer.setModels(models);
        vc::test::TempDirectory temp;prepare(temp.path(),blocks);
        vc::server::HostConfig config;config.worldDirectory=temp.path();config.modsDirectory=vc::utf8Path(argv[3]);
        config.port=0;config.radius=1;config.populate=false;
        glm::dvec3 finalPosition;vc::voxel::Position placed;
        {
            vc::server::Host host(config);auto bobBlocks=blocks;
            vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),blocks);
            vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bobBlocks);
            vc::voxel::Lighting lighting;vc::voxel::ChunkMeshQueue meshes;
            vc::CreativeInventory inventory(blocks);vc::SandboxUiController ui(inventory);
            vc::Camera camera;camera.position={0.5,3.62,6.5};camera.yaw=0;camera.pitch=-0.3;
            const auto step=[&](const vc::Input& input,const std::filesystem::path& capture={}) {
                require(!input.quit,"Network playtest interrupted");if(input.resized) renderer.requestResize();
                alice.pump();bob.pump();host.tick(1.0/60);alice.pump();bob.pump();
                const auto dirty=alice.takeDirtyChunks();const auto lightDirty=lighting.refresh(alice.world(),blocks,dirty);
                meshes.invalidate(dirty);meshes.invalidate(lightDirty);
                renderer.updateMeshes(meshes.rebuild(alice.world(),blocks,mods.atlas().layout(),cell(camera.position),4,&lighting));
                std::vector<vc::RenderInstance> avatars;
                if(alice.welcome()) for(const auto& peer:alice.state().players) if(peer.id!=alice.welcome()->id) {
                    const auto feet=peer.camera.position-glm::dvec3(0,vc::PlayerController::eyeHeight,0);
                    avatars.push_back({0,feet,static_cast<float>(peer.camera.yaw),0,{1,0}});
                }
                renderer.setInstances(avatars);
                const auto [width,height]=window.pixelSize();
                if(auto hud=ui.rebuild(blocks,mods.atlas().layout(),width,height,vc::Window::seconds())) renderer.setHud(hud->colored,hud->textured,true,hud->foreground);
                return renderer.draw(camera,capture);
            };
            const auto settle=[&](auto ready) {
                const auto end=vc::Window::seconds()+20;
                do { step(window.poll());if(ready()) return; } while(vc::Window::seconds()<end);
                throw std::runtime_error("Network graphics did not settle");
            };
            settle([&]{return alice.welcome()&&bob.welcome()&&alice.world().chunks().size()==9&&bob.world().chunks().size()==9&&
                alice.state().players.size()==2&&!meshes.pendingCount();});
            require(renderer.drawnInstanceCount()==1,"Other player was not drawn as a Vulkan instance");
            ui.status("TWO REAL TCP PLAYERS - SHARED MODDED WORLD",vc::Window::seconds());
            require(step(window.poll(),vc::utf8Path(argv[1])),"Network avatar capture failed");
            vc::netgame::Hotbar hotbar;hotbar.names[0]="demo:glowstone";alice.hotbar(hotbar);
            inventory.assign(0,blocks.require("demo:glowstone"));
            vc::netgame::Input intent;intent.flags=vc::netgame::Active|vc::netgame::Place;intent.pitch=-1.5;
            const auto hit=vc::voxel::raycast(alice.world(),alice.camera().position,vc::Camera{.yaw=0,.pitch=-1.5}.forward());
            require(hit.has_value(),"Network placement ray missed the floor");const auto adjacent=vc::voxel::offsetPosition(hit->block,hit->normal);
            require(adjacent.has_value(),"Network placement was outside coordinate range");placed=*adjacent;alice.input(intent);
            settle([&]{return alice.world().block(placed)==blocks.require("demo:glowstone")&&bob.world().block(placed)==bobBlocks.require("demo:glowstone");});
            camera.pitch=-1.0;ui.status("SERVER-AUTHORITATIVE CUSTOM BLOCK SHARED BY BOTH PLAYERS",vc::Window::seconds());
            settle([&]{return !meshes.pendingCount();});require(step(window.poll(),vc::utf8Path(argv[2])),"Network edit capture failed");
            intent.flags=vc::netgame::Active|vc::netgame::Destroy;alice.input(intent);
            settle([&]{return alice.world().block(placed)==vc::voxel::air&&bob.world().block(placed)==vc::voxel::air;});
            window.capture(true);(void)window.poll();key(window,SDL_SCANCODE_W,true);
            const auto before=host.players().front().camera.position;bool acknowledged=false;
            for(int i=0;i<35;++i) {
                const auto input=window.poll();acknowledged=acknowledged||input.forward==1;
                intent.flags=vc::netgame::Active;intent.pitch=0;intent.forward=input.forward;alice.input(intent);step(input);
            }
            key(window,SDL_SCANCODE_W,false);intent.forward=0;alice.input(intent);
            settle([&]{return alice.camera().position==host.players().front().camera.position;});
            require(acknowledged&&alice.camera().position.z<before.z-2,"SDL movement intent was not simulated by the server");
            finalPosition=alice.camera().position;alice.requestSave();
            settle([&]{for(const auto& notice:alice.takeNotices()) if(notice.starts_with("Saved world")) return true;return false;});
            alice.disconnect();bob.disconnect();settle([&]{return host.playerCount()==0;});host.save();
        }
        {
            vc::server::Host reopened(config);auto registry=blocks;
            vc::netgame::RemoteSession alice("127.0.0.1",reopened.port(),"ALICE",reopened.fingerprint(),registry);
            const auto end=vc::Window::seconds()+20;
            do { alice.pump();reopened.tick(1.0/60);alice.pump();vc::Window::idle(); } while(!alice.welcome()&&vc::Window::seconds()<end);
            require(alice.welcome()&&alice.camera().position==finalPosition,"Reconnect lost the saved independent player pose");
            require(alice.welcome()->profile.hotbar[0]=="demo:glowstone","Reconnect lost the saved mod hotbar");
            alice.disconnect();
        }
        renderer.shutdown();const auto validation=renderer.validationReport();
        require(validation.enabled&&!validation.errors&&!validation.warnings,"Network playtest reported Vulkan diagnostics");
        vc::log("Network graphics passed: two TCP players, visible avatar, shared custom block, SDL movement intent, save and reconnect; zero Vulkan diagnostics");return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
