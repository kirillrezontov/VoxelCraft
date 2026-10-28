#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/storage.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/content/creature_models.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/simulation/creatures.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void button(vc::Window& window,bool down) {
    SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID=window.id();event.button.button=SDL_BUTTON_LEFT;event.button.down=down;
    require(SDL_PushEvent(&event),SDL_GetError());
}
void key(vc::Window& window,SDL_Scancode scan,bool down) {
    SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
    event.key.windowID=window.id();event.key.scancode=scan;event.key.down=down;require(SDL_PushEvent(&event),SDL_GetError());
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Creature test requires portrait and combat capture paths");
        vc::Window window;vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);
        auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
        vc::voxel::World world;
        for(int x=-1;x<=0;++x) for(int z=-1;z<=0;++z) {
            vc::voxel::Chunk chunk({x,0,z});
            for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},blocks.require("voxelcraft:grass"));
            world.insert(std::move(chunk));
        }
        vc::voxel::Lighting lighting;const auto dirty=world.takeDirtyChunks();lighting.refresh(world,blocks,dirty);
        vc::voxel::ChunkMeshQueue queue;queue.invalidate(dirty);
        renderer.upload(queue.rebuild(world,blocks,vc::builtinAtlasLayout(),{0,1,0},128,&lighting));renderer.setModels(vc::builtinCreatureModels());
        vc::LivingWorld living(42);vc::SurvivalState survival;vc::CreativeInventory creative(blocks);
        vc::SurvivalUiContext context{&survival,&items,&recipes};vc::SandboxUiController ui(creative,{},&context);
        require(survival.inventory.add(items.require("voxelcraft:stone_pickaxe"),1,items),"Cannot equip combat tool");
        const auto grazer=living.spawn(vc::CreatureKind::Grazer,{0.5,1.001,-0.5});require(grazer.has_value(),"Grazer did not spawn");
        require(living.spawn(vc::CreatureKind::Stalker,{3.5,1.001,-3.5}).has_value(),"Stalker did not spawn");
        auto actors=living.save(items);for(auto& actor:actors) actor.yaw=3.141592653589793;living.restore(actors,items);
        vc::PlayerController player;player.camera.position={0.5,2.62,7};player.camera.yaw=0;player.camera.pitch=-0.1;
        const auto render=[&](const std::filesystem::path& path=std::filesystem::path{}) {
            std::vector<vc::RenderInstance> instances;
            for(const auto& actor:living.views()) instances.push_back({actor.model,actor.position,actor.yaw,actor.phase,{1,0}});
            // Repeated visible models share instance draws; out-of-view instances must cull.
            instances.push_back({0,{-3,1.001,-2},1.2f,1.4f,{1,0}});
            instances.push_back({0,{0,1,40},0,0,{1,0}});instances.push_back({1,{1000,1,0},0,0,{1,0}});
            renderer.setInstances(instances);const auto [width,height]=window.pixelSize();
            if(auto hud=ui.rebuild(blocks,vc::builtinAtlasLayout(),width,height,vc::Window::seconds())) renderer.setHud(hud->colored,hud->textured,true,hud->foreground);
            return renderer.draw(player.camera,path);
        };
        for(int i=0;i<8;++i) { const auto input=window.poll();require(!input.quit,"Creature playtest interrupted");if(input.resized) renderer.requestResize();render(); }
        require(render(vc::utf8Path(argv[1])),"Creature portrait capture failed");require(renderer.drawnInstanceCount()==3,"Instance visibility/group draw count failed");
        player.camera.position={0.5,2.62,2};player.camera.pitch=-0.15;
        window.capture(false);button(window,true);
        const auto await=[&](auto ready) {
            for(int i=0;i<16;++i) { auto input=window.poll();require(!input.quit,"Creature playtest interrupted");if(input.resized) renderer.requestResize();if(ready(input)) return input; }
            throw std::runtime_error("Creature SDL action was not acknowledged");
        };
        auto input=await([](const auto& in){return in.captured;});
        require(!input.destroyHeld,"Capture click attacked a creature");button(window,false);(void)window.poll();
        button(window,true);input=await([](const auto& in){return in.destroyHeld;});
        unsigned attacks=0,kills=0,pickups=0;
        const auto step=[&](vc::Input in) {
            require(!in.quit,"Creature combat interrupted");if(in.resized) renderer.requestResize();
            player.update({{in.forward,in.right,in.up,0,0},false,false,in.captured},1.0/60,world,blocks);
            const auto events=living.update(1.0/60,world,blocks,player.camera.position,&survival,items,false,false);pickups+=events.collected;
            if(const auto target=living.target(world,blocks,player.camera.position,player.camera.forward());target&&in.destroyHeld&&in.captured) {
                const auto result=living.attack(*target,player.camera.position,&survival,items);attacks+=result.hit;kills+=result.killed;
            }
            render();
        };
        step(input);require(attacks==1,"First held combat press did not attack");
        // Follow the fleeing grazer using actual held W input, stopping within reach.
        key(window,SDL_SCANCODE_W,true);input=await([](const auto& in){return in.forward==1;});
        for(int frame=0;frame<120&&!kills;++frame) {
            const auto views=living.views();const auto found=std::find_if(views.begin(),views.end(),[&](const auto& a){return a.id==*grazer;});
            if(found!=views.end()) {
                const auto toward=found->position+glm::dvec3(0,1,0)-player.camera.position;
                player.camera.yaw=std::atan2(toward.x,-toward.z);player.camera.pitch=std::atan2(toward.y,glm::length(glm::dvec2(toward.x,toward.z)));
                input=window.poll();if(glm::length(toward)<1.8) input.forward=0;
            }
            step(input);
        }
        button(window,false);key(window,SDL_SCANCODE_W,false);step(window.poll());
        require(kills==1&&attacks==2&&living.dropCount()==1,"Timed pursuit combat did not kill/drop with expected tool damage");
        require(survival.inventory.selected().durability==129,"Combat did not wear the held tool twice");
        require(render(vc::utf8Path(argv[2])),"Creature combat capture failed");
        const auto loot=living.views();const auto dropped=std::find_if(loot.begin(),loot.end(),[](const auto& a){return a.model==2;});require(dropped!=loot.end(),"Loot model missing");
        player.camera.position=dropped->position+glm::dvec3(0,1.62,0);for(int i=0;i<3;++i) step(window.poll());
        require(pickups==1&&survival.inventory.count(items.require("voxelcraft:raw_meat"))==1,"Walking into loot did not collect it");
        survival.damage(7);survival.inventory.select(1);require(survival.eat(items)&&survival.health()==16,"Loot food did not heal the player");
        vc::test::TempDirectory directory;
        { vc::voxel::WorldStore store(directory.path());vc::PlayerSaveState state;state.camera=player.camera;state.survival=true;state.health=survival.health();
          state.items=survival.inventory.save(items);state.actors=living.save(items);store.save(world,blocks,{},vc::encodePlayer(state));
          auto restoredBlocks=vc::builtinBlocks();const auto archive=store.load(restoredBlocks);require(archive.has_value(),"Actor world did not reload");
          const auto saved=vc::decodePlayer(archive->gameData);vc::LivingWorld restored(42);auto restoredItems=vc::builtinItems(restoredBlocks);restored.restore(saved.actors,restoredItems);
          require(restored.creatureCount()==1&&restored.save(restoredItems)[0].id==saved.actors[0].id,"Actor stable identity did not survive save/reload"); }
        // Grow the generic instance buffer, then clear it; terrain remains independently owned.
        std::vector<vc::RenderInstance> stress(256,{0,{0,1,-3},0,0,{1,0}});renderer.setInstances(stress);renderer.draw(player.camera);
        renderer.setInstances({});renderer.draw(player.camera);require(renderer.drawnInstanceCount()==0,"Instance removal left stale actors");
        renderer.shutdown();const auto validation=renderer.validationReport();require(validation.enabled&&!validation.errors&&!validation.warnings,"Creature playtest reported Vulkan diagnostics");
        vc::log("Creature playtest passed: visible animated instances/culling, SDL combat pursuit, tool wear, physical loot, pickup/food, saved actor identity and buffer growth/removal; zero Vulkan diagnostics");return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
