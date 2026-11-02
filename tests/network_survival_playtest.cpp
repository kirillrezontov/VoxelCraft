#include "game/modding/runtime.hpp"
#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "game/content/blocks.hpp"
#include "game/network/client.hpp"
#include "game/player/save_state.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "server/host.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void push(SDL_Event event) { require(SDL_PushEvent(&event),SDL_GetError()); }
void key(vc::Window& window,SDL_Scancode scan,bool down) {
    SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
    event.key.windowID=window.id();event.key.scancode=scan;event.key.down=down;push(event);
}
void button(vc::Window& window,Uint8 which,bool down,vc::UiRect area={}) {
    SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID=window.id();event.button.button=which;event.button.down=down;
    event.button.x=area.x+area.width/2;event.button.y=area.y+area.height/2;push(event);
}
void prepare(const std::filesystem::path& path,const vc::voxel::BlockRegistry& blocks) {
    vc::voxel::World world({0,31});const auto stone=blocks.require("voxelcraft:stone");
    for(int x=-1;x<=1;++x) for(int z=-1;z<=1;++z) {
        vc::voxel::Chunk chunk({x,0,z});
        for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},stone);
        world.insert(std::move(chunk));
    }
    for(int x=-3;x<=3;++x) for(int z=1;z<=8;++z) world.setBlock({x,26,z},stone);
    for(int z:{0,-1}) world.setBlock({0,28,z},blocks.require("voxelcraft:log"));
    world.setBlock({0,28,-2},stone);
    vc::PlayerSaveState state;state.survival=true;state.camera.position={0.5,28.62,3.5};state.camera.yaw=0;state.camera.pitch=0;
    vc::PlayerProfile alice;alice.name="Alice";alice.camera=state.camera;
    vc::PlayerProfile bob=alice;bob.name="Bob";bob.camera.position={2.5,28.62,2.5};
    state.profiles={alice,bob};vc::voxel::WorldStore store(path);store.save(world,blocks,{42,{0,31},1},vc::encodePlayer(state));
}
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"Network survival playtest needs mining/crafting/death captures");
        vc::Window window;auto blocks=vc::builtinBlocks();vc::ModRuntime mods({},blocks);
        vc::VulkanRenderer renderer(window,mods.atlas().image(),true);vc::test::TempDirectory temp;prepare(temp.path(),blocks);
        vc::server::HostConfig config;config.worldDirectory=temp.path();config.port=0;config.radius=1;config.populate=false;
        vc::server::Host host(config);auto bobBlocks=blocks;
        vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),blocks);
        vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bobBlocks);
        auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
        vc::SurvivalState mirror;vc::CreativeInventory creative(blocks);
        vc::SurvivalUiContext context{&mirror,&items,&recipes,[&](auto action){alice.inventory(action);}};
        vc::SandboxUiController ui(creative,{},&context);vc::voxel::Lighting lighting;vc::voxel::ChunkMeshQueue meshes;
        std::uint64_t stateTick=0;bool previouslyAlive=true;unsigned commands=0;
        const auto step=[&](vc::Input input,const std::filesystem::path& capture={}) {
            require(!input.quit,"Network survival playtest interrupted");if(input.resized) renderer.requestResize();
            alice.pump();bob.pump();
            if(alice.welcome()) {
                if(const auto& state=alice.state();state.survival&&state.tick!=stateTick) {
                    mirror.inventory.restore(state.survival->items,mirror.inventory.selectedSlot(),items);mirror.restoreHealth(state.survival->health);stateTick=state.tick;
                    if(mirror.alive()!=previouslyAlive) {
                        ui.closeMenus();window.pointerMode(!mirror.alive());window.capture(mirror.alive());previouslyAlive=mirror.alive();
                        input={};input.captured=mirror.alive();
                    }
                }
                const auto [width,height]=window.pixelSize();
                if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,input.material,input.scroll,
                        input.cursorX,input.cursorY,input.settings,input.crafting},width,height)) {
                    window.pointerMode(ui.open());window.capture(!ui.open());input={};input.captured=!ui.open();
                }
                vc::netgame::Input intent;intent.yaw=0;intent.pitch=0;intent.slot=static_cast<std::uint32_t>(mirror.inventory.selectedSlot());
                const bool active=input.captured&&!ui.open();intent.flags=active?vc::netgame::Active:0u;
                if(active) {
                    intent.forward=input.forward;intent.right=input.right;intent.up=input.up;
                    if(input.destroyHeld) intent.flags|=vc::netgame::AttackHeld;
                    if(input.place) intent.flags|=vc::netgame::Place;
                    if(input.jump) intent.flags|=vc::netgame::Jump;
                }
                if(input.respawn) intent.flags|=vc::netgame::Respawn;
                alice.input(intent);++commands;if(input.save) alice.requestSave();
            }
            alice.pump();bob.pump();host.tick(1.0/60);alice.pump();bob.pump();
            const auto dirty=alice.takeDirtyChunks();const auto lightDirty=lighting.refresh(alice.world(),blocks,dirty);meshes.invalidate(dirty);meshes.invalidate(lightDirty);
            const auto camera=alice.welcome()?alice.camera():vc::Camera{};
            renderer.updateMeshes(meshes.rebuild(alice.world(),blocks,mods.atlas().layout(),{0,28,3},4,&lighting));
            const auto hit=vc::voxel::raycast(alice.world(),camera.position,camera.forward(),6,&blocks);
            renderer.setSelection(hit&&!ui.open()?std::optional(glm::i64vec3(hit->block.x,hit->block.y,hit->block.z)):std::nullopt);
            const auto& survival=alice.state().survival;
            renderer.setProgressIndicator(survival&&survival->mining&&!ui.open()?std::optional(static_cast<float>(survival->miningProgress)):std::nullopt);
            const auto [width,height]=window.pixelSize();if(auto hud=ui.rebuild(blocks,mods.atlas().layout(),width,height,vc::Window::seconds()))
                renderer.setHud(hud->colored,hud->textured,!ui.open(),hud->foreground);
            return renderer.draw(camera,capture);
        };
        const auto frame=[&]{step(window.poll());};
        const auto frames=[&](int count){for(int i=0;i<count;++i) frame();};
        const auto until=[&](auto ready) {
            const auto deadline=vc::Window::seconds()+20;
            do { frame();if(ready()) return; } while(vc::Window::seconds()<deadline);
            throw std::runtime_error("Network survival action did not settle");
        };
        const auto count=[&](std::string_view name) {
            return mirror.inventory.count(items.require(name));
        };
        const auto await=[&](auto ready) {
            for(int i=0;i<16;++i) { const auto input=window.poll();if(ready(input)) { step(input);return; }step(input); }
            throw std::runtime_error("Network survival SDL event was not acknowledged");
        };
        const auto menu=[&](SDL_Scancode scan) {
            key(window,scan,true);await([&](const auto& input){return scan==SDL_SCANCODE_C?input.crafting:input.inventory;});key(window,scan,false);frames(2);
        };
        const auto click=[&](vc::UiRect area,Uint8 which=SDL_BUTTON_LEFT) {
            button(window,which,true,area);await([&](const auto& input){return which==SDL_BUTTON_LEFT?input.primaryClick:input.secondaryClick;});
            button(window,which,false,area);frames(4);
        };
        const auto select=[&](int slot) {
            const auto scan=static_cast<SDL_Scancode>(SDL_SCANCODE_1+slot-1);key(window,scan,true);
            await([&](const auto& input){return input.material==slot;});key(window,scan,false);frames(2);
        };
        until([&]{return alice.welcome()&&bob.welcome()&&alice.state().survival&&alice.world().chunks().size()==9&&!meshes.pendingCount();});
        require(alice.welcome()->survival,"Server did not restore survival mode");window.capture(true);frames(2);
        button(window,SDL_BUTTON_LEFT,true);await([](const auto& input){return input.destroyHeld;});frames(35);
        require(alice.state().survival->miningProgress>0.3&&alice.state().survival->miningProgress<0.8,"Server mining progress was not rendered");
        require(step(window.poll(),vc::utf8Path(argv[1])),"Network mining capture failed");
        until([&]{return count("voxelcraft:log")==2;});button(window,SDL_BUTTON_LEFT,false);frames(4);
        require(host.world().block({0,28,0})==vc::voxel::air&&bob.world().block({0,28,-1})==vc::voxel::air,"Second client missed survival terrain changes");
        menu(SDL_SCANCODE_C);const auto [width,height]=window.pixelSize();const vc::SandboxUiLayout layout(width,height);
        click(layout.recipeButton(0));until([&]{return count("voxelcraft:planks")==4;});
        click(layout.recipeButton(0));until([&]{return count("voxelcraft:planks")==8;});
        click(layout.recipeButton(1));until([&]{return count("voxelcraft:stick")==4;});
        click(layout.recipeButton(2));until([&]{return count("voxelcraft:wooden_pickaxe")==1;});
        require(step(window.poll(),vc::utf8Path(argv[2])),"Network crafting capture failed");
        menu(SDL_SCANCODE_C);select(3);button(window,SDL_BUTTON_LEFT,true);await([](const auto& input){return input.destroyHeld;});
        until([&]{return count("voxelcraft:cobblestone")==1;});button(window,SDL_BUTTON_LEFT,false);frames(4);
        require(mirror.inventory.selected().durability==58,"Server mining failed to wear the actual tool");
        menu(SDL_SCANCODE_E);click(layout.hotbarSlot(2),SDL_BUTTON_RIGHT);
        until([&]{return mirror.inventory.slots()[9].item==items.require("voxelcraft:wooden_pickaxe")&&!mirror.inventory.slots()[9].empty();});
        click(layout.backpackCell(0));until([&]{return !mirror.inventory.selected().empty()&&mirror.inventory.selected().item==items.require("voxelcraft:wooden_pickaxe");});
        menu(SDL_SCANCODE_E);const auto before=mirror.inventory.save(items);
        key(window,SDL_SCANCODE_A,true);await([](const auto& input){return input.right==-1;});
        until([&]{return !mirror.alive();});key(window,SDL_SCANCODE_A,false);frames(4);
        require(ui.open()&&mirror.inventory.save(items)==before,"Network death did not retain inventory and open the death UI");
        require(step(window.poll(),vc::utf8Path(argv[3])),"Network death capture failed");
        key(window,SDL_SCANCODE_R,true);await([](const auto& input){return input.respawn;});key(window,SDL_SCANCODE_R,false);
        until([&]{return mirror.alive();});require(mirror.health()==20&&!ui.open()&&mirror.inventory.save(items)==before,"Server respawn lost inventory or capture state");
        require(bob.state().survival&&bob.state().survival->health==20,"Other player inherited Alice's damage");
        for(const auto& item:bob.state().survival->items) require(item.identifier.empty(),"Other player's private inventory changed");
        key(window,SDL_SCANCODE_F5,true);await([](const auto& input){return input.save;});key(window,SDL_SCANCODE_F5,false);frames(4);
        alice.disconnect();bob.disconnect();for(int i=0;i<4;++i) host.tick(1.0/60);host.save();
        renderer.shutdown();const auto validation=renderer.validationReport();require(validation.enabled&&!validation.errors&&!validation.warnings,"Network survival reported Vulkan diagnostics");
        vc::log("Network survival graphics passed: SDL held mining, server crafting/tool wear, backpack, independent health/items, fatal fall, R respawn and F5 save; "+std::to_string(commands)+" input commands; zero Vulkan diagnostics");return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
