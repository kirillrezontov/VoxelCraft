#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/mesher.hpp"
#include "engine/voxel/storage.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/gameplay/interaction.hpp"
#include "game/gameplay/inventory.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <stdexcept>
#include <cmath>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void push(SDL_Event event) { require(SDL_PushEvent(&event),SDL_GetError()); }
void key(vc::Window& window,SDL_Scancode scan,bool down) {
    SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
    event.key.windowID=window.id();event.key.scancode=scan;event.key.down=down;push(event);
}
void button(vc::Window& window,Uint8 which,bool down,float pixelX=0,float pixelY=0) {
    auto* native=SDL_GetWindowFromID(window.id());int width{},height{};
    require(SDL_GetWindowSize(native,&width,&height),SDL_GetError());
    const auto [pw,ph]=window.pixelSize();
    SDL_Event event{};event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID=window.id();event.button.button=which;event.button.down=down;
    event.button.x=pixelX*static_cast<float>(width)/static_cast<float>(pw);
    event.button.y=pixelY*static_cast<float>(height)/static_cast<float>(ph);push(event);
}
}
int main(int argc,char** argv) {
    try {
        vc::Window window;vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);
        auto registry=vc::builtinBlocks();vc::voxel::World world;
        for(int x=-1;x<=0;++x) for(int z=-1;z<=0;++z) {
            vc::voxel::Chunk chunk({x,0,z});
            for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},registry.require("voxelcraft:stone"));
            world.insert(std::move(chunk));
        }
        const auto remesh=[&] {
            std::vector<vc::Mesh> meshes;
            for(const auto p:world.takeDirtyChunks()) meshes.push_back(vc::voxel::meshChunk(*world.findChunk(p),world,registry,vc::builtinAtlasLayout()));
            renderer.updateMeshes(meshes);
        };
        remesh();vc::CreativeInventory inventory(registry);vc::SandboxUiController ui(inventory);
        vc::PlayerController player;player.flying=true;player.camera.position={-0.5,4.5,0.5};player.camera.yaw=0;player.camera.pitch=-1.55;
        unsigned edits=0;
        const auto step=[&](vc::Input input,const std::filesystem::path& screenshot=std::filesystem::path{}) {
            require(!input.quit,"Inventory playtest interrupted");
            if(input.resized) renderer.requestResize();
            const auto [w,h]=window.pixelSize();
            if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,
                    input.material,input.scroll,input.cursorX,input.cursorY,input.settings},w,h)) {
                window.pointerMode(ui.open());window.capture(!ui.open());
                input.forward=input.right=input.up=input.mouseX=input.mouseY=0;
                input.destroy=input.place=input.jump=input.toggleFlight=false;input.captured=!ui.open();
            }
            player.camera.fieldOfView=static_cast<float>(ui.settings().fieldOfView);
            player.camera.mouseSensitivity=ui.settings().mouseSensitivity;player.camera.invertY=ui.settings().invertY;
            player.update({{input.forward,input.right,input.up,input.mouseX,input.mouseY},input.jump,input.toggleFlight,input.captured},1.0/60,world,registry);
            const auto hit=vc::voxel::raycast(world,player.camera.position,player.camera.forward());
            bool changed=false;
            if(hit&&input.destroy) changed=vc::destroyBlock(world,*hit);
            else if(hit&&input.place) changed=vc::placeBlock(world,registry,*hit,inventory.selectedBlock().value_or(vc::voxel::air),player.bounds());
            if(changed) { ++edits;remesh(); }
            renderer.setSelection(hit&&!ui.open()?std::optional(glm::i64vec3(hit->block.x,hit->block.y,hit->block.z)):std::nullopt);
            if(auto hud=ui.rebuild(registry,vc::builtinAtlasLayout(),w,h,vc::Window::seconds())) renderer.setHud(hud->colored,hud->textured,!ui.open(),hud->foreground);
            return renderer.draw(player.camera,screenshot);
        };
        const auto await=[&](auto ready) {
            for(int n=0;n<16;++n) {
                const auto input=window.poll();require(!input.quit,"Inventory playtest interrupted");
                if(input.resized) renderer.requestResize();
                if(ready(input)) return input;
            }
            throw std::runtime_error("Inventory playtest SDL event was not acknowledged");
        };
        const auto frame=[&]{step(window.poll());};
        (void)window.poll();window.capture(true);frame();
        key(window,SDL_SCANCODE_W,true);key(window,SDL_SCANCODE_E,true);
        step(await([](const auto& input){return input.inventory;}));
        require(ui.open(),"E did not open inventory");
        require(player.camera.position==glm::dvec3(-0.5,4.5,0.5),"Opening inventory moved the player");
        key(window,SDL_SCANCODE_E,false);key(window,SDL_SCANCODE_W,false);key(window,SDL_SCANCODE_9,true);
        step(await([](const auto& input){return input.material==9;}));key(window,SDL_SCANCODE_9,false);frame();
        require(inventory.selectedSlot()==8&&!inventory.selectedBlock(),"Ninth empty slot was not selected");
        const auto [w,h]=window.pixelSize();const vc::SandboxUiLayout layout(w,h);const auto cell=layout.catalogCell(1);
        button(window,SDL_BUTTON_LEFT,true,cell.x+cell.width/2,cell.y+cell.height/2);
        step(await([](const auto& input){return input.primaryClick;}));
        require(inventory.selectedBlock()==registry.require("voxelcraft:dirt"),"Pixel-scaled catalog click did not assign dirt");
        require(edits==0,"Catalog click edited terrain");
        button(window,SDL_BUTTON_LEFT,false);frame();
        if(argc>1) {
            const auto screenshot=vc::utf8Path(argv[1]);
            for(int n=0;n<4;++n) if(step(window.poll(),screenshot)) break;
            require(std::filesystem::exists(screenshot),"Inventory screenshot was not saved");
        }
        vc::test::TempDirectory temp;
        {
            vc::voxel::WorldStore store(temp.path());vc::PlayerSaveState state;
            state.camera=player.camera;state.flying=true;state.hotbar=inventory.names(registry);state.selectedSlot=8;
            store.save(world,registry,{},vc::encodePlayer(state));
            auto restoredRegistry=vc::builtinBlocks();const auto archive=store.load(restoredRegistry);require(archive.has_value(),"Inventory save did not reopen");
            const auto decoded=vc::decodePlayer(archive->gameData);vc::CreativeInventory restored(restoredRegistry);
            restored.restore(decoded.hotbar,decoded.selectedSlot,restoredRegistry);
            require(restored.selectedSlot()==8&&restored.selectedBlock()==restoredRegistry.require("voxelcraft:dirt"),"Hotbar assignment did not persist");
        }
        key(window,SDL_SCANCODE_ESCAPE,true);step(await([](const auto& input){return input.closeUi;}));
        require(!ui.open(),"Escape did not close inventory");key(window,SDL_SCANCODE_ESCAPE,false);frame();
        button(window,SDL_BUTTON_RIGHT,true);step(await([](const auto& input){return input.place;}));
        require(edits==1&&world.block({-1,1,0})==registry.require("voxelcraft:dirt"),"Assigned hotbar material was not placed after closing inventory");
        button(window,SDL_BUTTON_RIGHT,false);frame();
        key(window,SDL_SCANCODE_E,true);step(await([](const auto& input){return input.inventory;}));key(window,SDL_SCANCODE_E,false);frame();
        const auto slot=vc::SandboxUiLayout(window.pixelSize().first,window.pixelSize().second).hotbarSlot(8);
        button(window,SDL_BUTTON_RIGHT,true,slot.x+slot.width/2,slot.y+slot.height/2);
        step(await([](const auto& input){return input.secondaryClick;}));
        require(!inventory.selectedBlock()&&edits==1,"Clearing a hotbar slot edited terrain or retained material");
        button(window,SDL_BUTTON_RIGHT,false);frame();
        key(window,SDL_SCANCODE_E,true);step(await([](const auto& input){return input.inventory;}));key(window,SDL_SCANCODE_E,false);frame();
        button(window,SDL_BUTTON_RIGHT,true);step(await([](const auto& input){return input.place;}));require(edits==1,"Empty hotbar slot placed a block");
        button(window,SDL_BUTTON_RIGHT,false);frame();
        if(argc>2) {
            const auto screenshot=vc::utf8Path(argv[2]);
            for(int n=0;n<4;++n) if(step(window.poll(),screenshot)) break;
            require(std::filesystem::exists(screenshot),"Hotbar screenshot was not saved");
        }
        key(window,SDL_SCANCODE_F10,true);step(await([](const auto& input){return input.settings;}));key(window,SDL_SCANCODE_F10,false);frame();
        require(ui.settingsOpen(),"F10 did not open settings");
        const auto clickSetting=[&](std::size_t which) {
            const vc::SandboxUiLayout current(window.pixelSize().first,window.pixelSize().second);const auto area=current.settingsButton(which);
            button(window,SDL_BUTTON_LEFT,true,area.x+area.width/2,area.y+area.height/2);
            step(await([](const auto& input){return input.primaryClick;}));button(window,SDL_BUTTON_LEFT,false);frame();
        };
        clickSetting(1);clickSetting(3);clickSetting(4);
        require(ui.settings().fieldOfView==80&&ui.settings().mouseSensitivity==0.003&&ui.settings().invertY,"Settings buttons did not update actual camera controls");
        const auto settingsPath=temp.path()/"settings.vcs";vc::saveSettings(settingsPath,ui.settings());
        require(vc::loadSettings(settingsPath)->settings==ui.settings(),"Settings did not persist");
        if(argc>3) {
            const auto screenshot=vc::utf8Path(argv[3]);
            for(int n=0;n<4;++n) if(step(window.poll(),screenshot)) break;
            require(std::filesystem::exists(screenshot),"Settings screenshot was not saved");
        }
        clickSetting(6);require(!ui.open(),"Settings Close button did not restore capture");
        player.camera.pitch=0;const auto yaw=player.camera.yaw;
        SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=window.id();motion.motion.xrel=20;motion.motion.yrel=10;push(motion);
        step(await([](const auto& input){return input.mouseX!=0;}));
        require(std::abs(player.camera.yaw-yaw-0.06)<1e-9&&std::abs(player.camera.pitch-0.03)<1e-9,"Configured mouse sensitivity/inversion did not affect SDL look");
        renderer.shutdown();const auto validation=renderer.validationReport();
        require(validation.enabled&&validation.errors==0&&validation.warnings==0,"Inventory playtest reported Vulkan diagnostics");
        vc::log("Inventory/settings playtest passed: catalog assignment, hotbar persistence, capture, placement/clearing, saved settings and configured SDL look; zero Vulkan validation diagnostics");
        return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
