#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/mesher.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/gameplay/interaction.hpp"
#include "game/player/controller.hpp"
#include <SDL3/SDL_main.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void push(SDL_Event event) { require(SDL_PushEvent(&event), SDL_GetError()); }
void key(vc::Window& window, SDL_Scancode scan, bool down) {
    SDL_Event event{}; event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
    event.key.windowID=window.id(); event.key.scancode=scan; event.key.down=down; push(event);
}
void button(vc::Window& window, Uint8 which, bool down) {
    SDL_Event event{}; event.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID=window.id(); event.button.button=which; event.button.down=down; push(event);
}
}

int main(int argc, char** argv) {
    try {
        vc::Window window;
        vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);
        const auto registry=vc::builtinBlocks();
        const std::array materials{registry.require("voxelcraft:grass"),registry.require("voxelcraft:dirt"),registry.require("voxelcraft:stone")};
        auto material=materials[0];
        vc::voxel::World world;
        for(int x=-1;x<=0;++x) for(int z=-1;z<=0;++z) for(int y=-1;y<=0;++y) {
            vc::voxel::Chunk chunk({x,y,z});
            for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,y==0?0:31,b},materials[2]);
            world.insert(std::move(chunk));
        }
        for(int y=1;y<=2;++y) for(int z=0;z<=6;++z) world.setBlock({10,y,z},materials[2]);
        const auto remesh=[&] {
            std::vector<vc::Mesh> meshes;
            for(const auto position:world.takeDirtyChunks())
                meshes.push_back(vc::voxel::meshChunk(*world.findChunk(position),world,registry,vc::builtinAtlasLayout()));
            renderer.updateMeshes(meshes);
        };
        remesh();
        vc::PlayerController player;
        player.camera.position={4.5,2.62,4.5};player.camera.yaw=0;player.camera.pitch=-0.55;
        unsigned edits=0;
        bool previousCapture=false;
        const auto step=[&](const vc::Input& input) {
            require(!input.quit,"SDL playtest interrupted");
            if(input.captured!=previousCapture) { vc::log(std::string("SDL playtest capture: ")+(input.captured?"on":"off"));previousCapture=input.captured; }
            if(input.mouseX!=0||input.mouseY!=0) vc::log("SDL playtest mouse: "+std::to_string(input.mouseX)+","+std::to_string(input.mouseY));
            if(input.resized) renderer.requestResize();
            player.update({{input.forward,input.right,input.up,input.mouseX,input.mouseY},input.jump,input.toggleFlight,input.captured},1.0/60,world,registry);
            if(input.material) material=materials.at(static_cast<std::size_t>(input.material-1));
            auto hit=vc::voxel::raycast(world,player.camera.position,player.camera.forward());
            bool changed=false;
            if(hit && input.destroy) changed=vc::destroyBlock(world,*hit);
            else if(hit && input.place) changed=vc::placeBlock(world,registry,*hit,material,player.bounds());
            if(changed) {++edits;remesh();hit=vc::voxel::raycast(world,player.camera.position,player.camera.forward());}
            renderer.setSelection(hit?std::optional(glm::i64vec3(hit->block.x,hit->block.y,hit->block.z)):std::nullopt);
            renderer.draw(player.camera);
            return input;
        };
        const auto frame=[&] { return step(window.poll()); };
        // SDL may leave newly pushed events behind its end-of-poll sentinel until
        // the next poll. Start each scripted phase on acknowledged input, rather
        // than assuming queue insertion and consumption happen in the same frame.
        const auto awaitInput=[&](auto ready) {
            for(int attempt=0;attempt<8;++attempt) {
                const auto input=window.poll();require(!input.quit,"SDL playtest interrupted");
                if(input.resized) renderer.requestResize();
                if(ready(input)) return input;
            }
            throw std::runtime_error("SDL playtest event was not acknowledged");
        };
        const auto frames=[&](int count){for(int i=0;i<count;++i)(void)frame();};
        (void)window.poll();window.capture(false);
        button(window,SDL_BUTTON_LEFT,true);
        auto input=step(awaitInput([](const auto& in){return in.captured;}));require(input.captured&&!input.destroy&&edits==0,"Capture click edited terrain");
        button(window,SDL_BUTTON_LEFT,false);(void)frame();
        key(window,SDL_SCANCODE_W,true);key(window,SDL_SCANCODE_D,true);
        (void)step(awaitInput([](const auto& in){return in.forward==1&&in.right==1;}));frames(59);
        vc::log("SDL playtest walking result: "+std::to_string(player.camera.position.x)+","+std::to_string(player.camera.position.y)+","+std::to_string(player.camera.position.z)+" yaw "+std::to_string(player.camera.yaw));
        require(std::abs(player.camera.position.x-(4.5+5/std::sqrt(2.0)))<1e-6,"Held diagonal walking distance incorrect");
        require(std::abs(player.camera.position.z-(4.5-5/std::sqrt(2.0)))<1e-6,"Held forward movement incorrect");
        key(window,SDL_SCANCODE_W,false);key(window,SDL_SCANCODE_D,false);(void)step(awaitInput([](const auto& in){return in.forward==0&&in.right==0;}));
        key(window,SDL_SCANCODE_S,true);key(window,SDL_SCANCODE_A,true);(void)step(awaitInput([](const auto& in){return in.forward==-1&&in.right==-1;}));frames(59);
        key(window,SDL_SCANCODE_S,false);key(window,SDL_SCANCODE_A,false);(void)step(awaitInput([](const auto& in){return in.forward==0&&in.right==0;}));
        require(glm::length(player.camera.position-glm::dvec3(4.5,2.62,4.5))<1e-5,"Reverse walking did not return to start");
        vc::log("SDL playtest: WASD held/released and diagonal speed verified");

        key(window,SDL_SCANCODE_D,true);(void)step(awaitInput([](const auto& in){return in.right==1;}));frames(89);
        key(window,SDL_SCANCODE_D,false);(void)step(awaitInput([](const auto& in){return in.right==0;}));
        require(std::abs(player.camera.position.x-9.7)<1e-5,"Walking penetrated a wall");
        const double oldZ=player.camera.position.z;
        key(window,SDL_SCANCODE_S,true);(void)step(awaitInput([](const auto& in){return in.forward==-1;}));frames(19);
        key(window,SDL_SCANCODE_S,false);(void)step(awaitInput([](const auto& in){return in.forward==0;}));
        require(player.camera.position.z>oldZ+1.6&&player.grounded,"Player could not slide along a wall");
        key(window,SDL_SCANCODE_SPACE,true);(void)step(awaitInput([](const auto& in){return in.jump;}));key(window,SDL_SCANCODE_SPACE,false);
        double highest=player.bounds().min.y;
        for(int i=0;i<70;++i){(void)frame();highest=std::max(highest,player.bounds().min.y);}
        require(highest>2.2&&player.grounded,"Space jump/landing failed");
        vc::log("SDL playtest: wall collision/sliding and grounded jump verified");

        SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.windowID=window.id();motion.motion.xrel=100;motion.motion.yrel=-40;
        const double oldYaw=player.camera.yaw,oldPitch=player.camera.pitch;push(motion);(void)step(awaitInput([](const auto& in){return in.mouseX!=0;}));
        require(player.camera.yaw>oldYaw+0.2&&player.camera.pitch>oldPitch+0.08,"Relative mouse look failed");
        key(window,SDL_SCANCODE_F,true);(void)step(awaitInput([](const auto& in){return in.toggleFlight;}));require(player.flying,"F did not enable flight");
        key(window,SDL_SCANCODE_F,false);key(window,SDL_SCANCODE_SPACE,true);
        (void)step(awaitInput([](const auto& in){return in.up==1;}));frames(9);
        key(window,SDL_SCANCODE_SPACE,false);(void)step(awaitInput([](const auto& in){return in.up==0;}));require(player.bounds().min.y>4,"Flight ascend failed");
        key(window,SDL_SCANCODE_F,true);(void)step(awaitInput([](const auto& in){return in.toggleFlight;}));require(!player.flying,"F did not restore walking");
        key(window,SDL_SCANCODE_F,false);frames(90);require(player.grounded,"Player did not land after flight");
        key(window,SDL_SCANCODE_2,true);(void)step(awaitInput([](const auto& in){return in.material==2;}));key(window,SDL_SCANCODE_2,false);(void)frame();
        require(material==materials[1],"Number key material selection failed");

        key(window,SDL_SCANCODE_W,true);key(window,SDL_SCANCODE_ESCAPE,true);input=step(awaitInput([](const auto& in){return !in.captured;}));
        require(!input.captured&&input.forward==0,"Escape left movement active");
        button(window,SDL_BUTTON_RIGHT,true);input=frame();require(!input.place,"Released right click edited terrain");
        button(window,SDL_BUTTON_RIGHT,false);button(window,SDL_BUTTON_LEFT,true);input=step(awaitInput([](const auto& in){return in.captured;}));
        require(input.captured&&!input.destroy&&input.forward==0,"Recapture resumed stale movement or edited");
        button(window,SDL_BUTTON_LEFT,false);key(window,SDL_SCANCODE_W,false);(void)frame();
        motion={};motion.type=SDL_EVENT_WINDOW_FOCUS_LOST;motion.window.windowID=window.id();push(motion);
        input=step(awaitInput([](const auto& in){return !in.captured;}));require(!input.captured,"Focus loss retained capture");
        button(window,SDL_BUTTON_LEFT,true);input=step(awaitInput([](const auto& in){return in.captured;}));require(input.captured&&!input.destroy,"Focus recapture edited terrain");
        button(window,SDL_BUTTON_LEFT,false);(void)frame();
        require(edits==0,"Control playtest unexpectedly edited terrain");
        vc::log("SDL playtest: mouse look, flight, materials, Escape and focus recovery verified");

        player.flying=true;player.camera.position={-0.5,4.5,0.5};player.camera.yaw=0;player.camera.pitch=-1.55;
        button(window,SDL_BUTTON_LEFT,true);input=step(awaitInput([](const auto& in){return in.destroy;}));
        require(input.destroy&&world.block({-1,0,0})==vc::voxel::air&&edits==1,"SDL left click did not destroy selected seam cell");
        frames(3);require(edits==1,"Held left click repeated without release");
        button(window,SDL_BUTTON_LEFT,false);button(window,SDL_BUTTON_RIGHT,true);input=step(awaitInput([](const auto& in){return in.place;}));
        require(input.place&&world.block({-1,0,0})==materials[1]&&edits==2,"SDL right click did not place selected dirt");
        button(window,SDL_BUTTON_RIGHT,false);frames(3);
        if(argc>1) require(renderer.draw(player.camera,vc::utf8Path(argv[1])),"SDL playtest capture failed");
        renderer.shutdown();const auto validation=renderer.validationReport();
        require(validation.enabled&&validation.errors==0&&validation.warnings==0,"SDL playtest reported Vulkan validation diagnostics");
        vc::log("SDL playtest passed: 2 mouse-driven edits, zero core/synchronization validation errors or warnings including shutdown");
        return 0;
    }catch(const std::exception& error){vc::log(error.what());return 1;}
}
