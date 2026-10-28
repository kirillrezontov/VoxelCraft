#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/storage.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/gameplay/interaction.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/player/spawn.hpp"
#include "game/ui/sandbox_ui.hpp"
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
void button(vc::Window& window,Uint8 which,bool down,float x=0,float y=0) {
    int width{},height{};require(SDL_GetWindowSize(SDL_GetWindowFromID(window.id()),&width,&height),SDL_GetError());
    const auto [pw,ph]=window.pixelSize();SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID=window.id();event.button.button=which;event.button.down=down;
    event.button.x=x*static_cast<float>(width)/static_cast<float>(pw);event.button.y=y*static_cast<float>(height)/static_cast<float>(ph);push(event);
}
}
int main(int argc,char** argv) {
    try {
        vc::Window window;vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);
        auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
        const vc::MiningRules rules(blocks,items);vc::SurvivalState state;
        vc::CreativeInventory creative(blocks);vc::SurvivalUiContext context{&state,&items,&recipes};vc::SandboxUiController ui(creative,{},&context);
        vc::voxel::World world;
        for(int x=-1;x<=0;++x) for(int z=-1;z<=0;++z) {
            vc::voxel::Chunk chunk({x,0,z});
            for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},blocks.require("voxelcraft:stone"));
            world.insert(std::move(chunk));
        }
        world.setBlock({-1,2,0},blocks.require("voxelcraft:log"));world.setBlock({-1,2,-1},blocks.require("voxelcraft:log"));
        for(int z=-2;z>=-4;--z) world.setBlock({-1,2,z},blocks.require("voxelcraft:stone"));
        world.setBlock({-1,2,-5},blocks.require("voxelcraft:iron_ore"));
        vc::PlayerController player;player.camera.position={-0.5,2.62,2.5};player.camera.yaw=player.camera.pitch=0;
        const vc::voxel::TerrainGenerator spawnGenerator(42,{0,0},blocks);
        vc::voxel::ChunkMeshQueue meshes;
        const auto step=[&](vc::Input input,const std::filesystem::path& screenshot=std::filesystem::path{}) {
            require(!input.quit,"Survival playtest interrupted");if(input.resized) renderer.requestResize();
            if(!state.alive()&&input.respawn) {
                player=vc::PlayerController{};player.camera.position=vc::findSpawn(spawnGenerator,blocks,&world);
                state.respawn();ui.closeMenus();window.pointerMode(false);window.capture(true);input.captured=true;
            }
            const auto [width,height]=window.pixelSize();
            if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,input.material,input.scroll,
                    input.cursorX,input.cursorY,input.settings,input.crafting},width,height)) {
                window.pointerMode(ui.open());window.capture(!ui.open());input.forward=input.right=input.up=input.mouseX=input.mouseY=0;
                input.destroy=input.place=input.jump=false;input.captured=!ui.open();
            }
            player.camera.fieldOfView=static_cast<float>(ui.settings().fieldOfView);player.camera.mouseSensitivity=ui.settings().mouseSensitivity;player.camera.invertY=ui.settings().invertY;
            if(state.alive()) {
                player.update({{input.forward,input.right,input.up,input.mouseX,input.mouseY},input.jump,false,input.captured},1.0/60,world,blocks);
                if(state.land(player.landingSpeed)&&!state.alive()) { ui.closeMenus();window.pointerMode(true);window.capture(false); }
            }
            auto hit=vc::voxel::raycast(world,player.camera.position,player.camera.forward());
            const auto mined=state.mine(input.destroyHeld&&input.captured&&!ui.open(),hit,1.0/60,world,items,rules);
            if(!mined.changed&&hit&&input.place&&!input.destroyHeld&&!ui.open()&&state.alive()) {
                const auto& selected=state.inventory.selected();const auto block=selected.empty()?vc::voxel::air:items.definition(selected.item).block.value_or(vc::voxel::air);
                if(vc::placeBlock(world,blocks,*hit,block,player.bounds())) state.inventory.consumeSelected();
            }
            if(mined.changed) hit=vc::voxel::raycast(world,player.camera.position,player.camera.forward());
            meshes.collect(world);const auto replacements=meshes.rebuild(world,blocks,vc::builtinAtlasLayout(),{-1,2,0},4);renderer.updateMeshes(replacements);
            renderer.setSelection(hit&&!ui.open()?std::optional(glm::i64vec3(hit->block.x,hit->block.y,hit->block.z)):std::nullopt);
            renderer.setProgressIndicator(!ui.open()&&state.mining()?std::optional(static_cast<float>(state.miningProgress())):std::nullopt);
            if(auto hud=ui.rebuild(blocks,vc::builtinAtlasLayout(),width,height,vc::Window::seconds())) renderer.setHud(hud->colored,hud->textured,!ui.open(),hud->foreground);
            return renderer.draw(player.camera,screenshot);
        };
        const auto await=[&](auto ready) {
            for(int i=0;i<16;++i) { const auto input=window.poll();require(!input.quit,"Survival playtest interrupted");if(input.resized) renderer.requestResize();if(ready(input)) return input; }
            throw std::runtime_error("Survival SDL action was not acknowledged");
        };
        const auto frame=[&]{step(window.poll());};const auto frames=[&](int count){for(int i=0;i<count;++i) frame();};
        const auto menu=[&](SDL_Scancode scan) { key(window,scan,true);step(await([&](const auto& input){return scan==SDL_SCANCODE_C?input.crafting:input.inventory;}));key(window,scan,false);frame(); };
        const auto click=[&](vc::UiRect area,Uint8 which=SDL_BUTTON_LEFT) {
            button(window,which,true,area.x+area.width/2,area.y+area.height/2);
            step(await([&](const auto& input){return which==SDL_BUTTON_LEFT?input.primaryClick:input.secondaryClick;}));button(window,which,false);frame();
        };
        const auto select=[&](int slot) {
            const auto scan=static_cast<SDL_Scancode>(SDL_SCANCODE_1+slot-1);key(window,scan,true);
            step(await([&](const auto& input){return input.material==slot;}));key(window,scan,false);frame();
        };
        (void)window.poll();window.capture(false);button(window,SDL_BUTTON_LEFT,true);
        step(await([](const auto& input){return input.captured;}));frames(12);
        require(world.block({-1,2,0})==blocks.require("voxelcraft:log")&&!state.mining(),"Capture click began survival mining");
        button(window,SDL_BUTTON_LEFT,false);frame();button(window,SDL_BUTTON_LEFT,true);step(await([](const auto& input){return input.destroyHeld;}));frames(38);
        require(state.miningProgress()>0.4&&state.miningProgress()<0.8,"Mining progress did not advance over time");
        if(argc>1) require(step(window.poll(),vc::utf8Path(argv[1])),"Mining screenshot failed");
        frames(125);button(window,SDL_BUTTON_LEFT,false);frame();
        require(state.inventory.count(items.require("voxelcraft:log"))==2,"Held mining did not gather two logs");
        button(window,SDL_BUTTON_RIGHT,true);step(await([](const auto& input){return input.place;}));button(window,SDL_BUTTON_RIGHT,false);frame();
        require(world.block({-1,2,-1})==blocks.require("voxelcraft:log")&&state.inventory.count(items.require("voxelcraft:log"))==1,"Survival placement did not consume one item");
        button(window,SDL_BUTTON_LEFT,true);step(await([](const auto& input){return input.destroyHeld;}));frames(80);button(window,SDL_BUTTON_LEFT,false);frame();
        require(state.inventory.count(items.require("voxelcraft:log"))==2&&world.block({-1,2,-1})==vc::voxel::air,"Placed material could not be mined back");
        menu(SDL_SCANCODE_C);require(ui.craftingOpen(),"C did not open survival crafting");
        auto layout=vc::SandboxUiLayout(window.pixelSize().first,window.pixelSize().second);
        click(layout.recipeButton(0));click(layout.recipeButton(0));click(layout.recipeButton(1));click(layout.recipeButton(2));
        require(state.inventory.count(items.require("voxelcraft:wooden_pickaxe"))==1,"Crafting progression did not produce a wooden pickaxe");
        menu(SDL_SCANCODE_C);select(3);require(items.definition(state.inventory.selected().item).tool==vc::ToolKind::Pickaxe,"Wooden pickaxe was not selected");
        button(window,SDL_BUTTON_LEFT,true);step(await([](const auto& input){return input.destroyHeld;}));frames(96);button(window,SDL_BUTTON_LEFT,false);frame();
        require(state.inventory.count(items.require("voxelcraft:cobblestone"))==3&&state.inventory.selected().durability==56,"Wooden pickaxe did not mine stone with expected durability");
        menu(SDL_SCANCODE_C);layout=vc::SandboxUiLayout(window.pixelSize().first,window.pixelSize().second);click(layout.recipeButton(3));
        require(state.inventory.count(items.require("voxelcraft:stone_pickaxe"))==1,"Stone pickaxe recipe failed");
        if(argc>2) require(step(window.poll(),vc::utf8Path(argv[2])),"Crafting screenshot failed");
        menu(SDL_SCANCODE_C);select(1);key(window,SDL_SCANCODE_W,true);step(await([](const auto& input){return input.forward==1;}));frames(29);
        key(window,SDL_SCANCODE_W,false);step(await([](const auto& input){return input.forward==0;}));
        button(window,SDL_BUTTON_LEFT,true);step(await([](const auto& input){return input.destroyHeld;}));frames(45);button(window,SDL_BUTTON_LEFT,false);frame();
        require(state.inventory.count(items.require("voxelcraft:iron_ore"))==1&&state.inventory.selected().durability==130,"Stone pickaxe did not harvest iron");
        menu(SDL_SCANCODE_E);layout=vc::SandboxUiLayout(window.pixelSize().first,window.pixelSize().second);
        click(layout.hotbarSlot(2),SDL_BUTTON_RIGHT);require(state.inventory.slots()[9].item==items.require("voxelcraft:wooden_pickaxe"),"Quick move did not preserve the wooden tool");
        click(layout.backpackCell(0));require(state.inventory.selected().item==items.require("voxelcraft:wooden_pickaxe"),"Backpack swap failed");
        select(3);click(layout.backpackCell(0),SDL_BUTTON_RIGHT);
        require(state.inventory.selected().item==items.require("voxelcraft:stone_pickaxe")&&state.inventory.selected().durability==130,"Single-stack transfer lost tool identity/durability");
        vc::test::TempDirectory temp;
        {
            vc::voxel::WorldStore store(temp.path());vc::PlayerSaveState saved;saved.camera=player.camera;saved.survival=true;saved.health=state.health();
            saved.selectedSlot=static_cast<std::uint32_t>(state.inventory.selectedSlot());saved.items=state.inventory.save(items);
            store.save(world,blocks,{},vc::encodePlayer(saved));auto restoredBlocks=vc::builtinBlocks();const auto loaded=store.load(restoredBlocks);
            require(loaded.has_value(),"Survival world did not reload");const auto decoded=vc::decodePlayer(loaded->gameData);
            auto restoredItems=vc::builtinItems(restoredBlocks);vc::SurvivalInventory inventory;inventory.restore(decoded.items,decoded.selectedSlot,restoredItems);
            require(decoded.survival&&decoded.health==20&&inventory.selected().durability==130&&inventory.count(restoredItems.require("voxelcraft:iron_ore"))==1,"Survival save lost mode/health/items/durability");
        }
        menu(SDL_SCANCODE_E);player=vc::PlayerController{};player.camera.position={4.5,31.62,4.5};frames(150);
        require(!state.alive()&&ui.open(),"Fatal fall did not open death/respawn state");
        if(argc>3) require(step(window.poll(),vc::utf8Path(argv[3])),"Death screenshot failed");
        const auto before=state.inventory.save(items);key(window,SDL_SCANCODE_R,true);step(await([](const auto& input){return input.respawn;}));key(window,SDL_SCANCODE_R,false);frames(3);
        require(state.alive()&&state.health()==20&&!ui.open()&&state.inventory.save(items)==before,"R respawn lost inventory or failed to restore play");
        renderer.shutdown();const auto validation=renderer.validationReport();require(validation.enabled&&!validation.errors&&!validation.warnings,"Survival playtest reported Vulkan diagnostics");
        vc::log("Survival playtest passed: held mining, wood/stone crafting progression, iron harvest, tool wear, backpack transfers, save/reload, fall death and R respawn; zero Vulkan diagnostics");
        return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
