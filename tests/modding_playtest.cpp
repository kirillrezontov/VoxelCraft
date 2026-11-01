#include "client/application.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/storage.hpp"
#include "game/player/camera.hpp"
#include "game/player/save_state.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
template<class Condition,class Pump> void until(Condition condition,Pump pump) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    do { pump();if(condition()) return;vc::Window::idle(); } while(std::chrono::steady_clock::now()<deadline);
    throw std::runtime_error("Modding playtest timed out");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==5,"Expected native/data/resource/capture directories");
        vc::test::TempDirectory temporary;vc::client::Options config;
        config.requireValidation=true;config.vsync=false;config.noSettings=true;config.radius=1;config.seed=42;config.generatorVersion=1;
        config.nativeModsDirectory=vc::utf8Path(argv[1]);config.modsDirectory=vc::utf8Path(argv[2]);config.resourcePacks={vc::utf8Path(argv[3])};
        config.worldDirectory=temporary.path()/"world";const auto captures=vc::utf8Path(argv[4]);std::filesystem::create_directories(captures);
        vc::client::ClientApplication app(config);const auto tick=[&]{app.frame({});};tick();
        require(app.resources().active().size()==1&&app.resources().active()[0].id=="amber","Resource pack was not activated in the real application");
        until([&]{return app.renderer().draw(vc::Camera{},captures/"resource-menu.ppm");},tick);
        app.startLocal(config);until([&]{return app.screen()==vc::client::ApplicationScreen::Gameplay;},tick);
        bool rejected=false;try { app.activateResourcePacks({}); } catch(const std::logic_error&) { rejected=true; }
        require(rejected,"Resource mounts changed during a live session");
        const auto command=[&](std::string value) {
            vc::Input open;open.console=1;app.frame(open);
            vc::Input text;text.text.text=std::move(value);app.frame(text);
            vc::Input enter;enter.text.enter=true;app.frame(enter);
            vc::Input close;close.closeUi=true;app.frame(close);
        };
        command("native_sample:stamp 0 60 0");
        command("give @s signal:beacon 1");
        vc::Input place;place.place=true;place.captured=true;app.frame(place);
        for(int i=0;i<6;++i) tick();
        vc::Input inventory;inventory.inventory=true;app.frame(inventory);
        until([&]{return app.renderer().draw(vc::Camera{},captures/"native-data-inventory.ppm");},tick);
        app.requestLeave();until([&]{return !app.hasSession();},tick);
        require(app.screen()==vc::client::ApplicationScreen::MainMenu&&app.renderer().meshCount()==0,"Mod session did not release its scene");
        {
            vc::voxel::WorldStore store(config.worldDirectory);vc::voxel::BlockRegistry blocks;const auto archive=store.load(blocks);require(archive.has_value(),"Modded world did not save");
            require(archive->world.block({0,60,0})==blocks.require("native_sample:beacon"),"Native command did not modify/save the actual client world");
            const auto signal=blocks.require("signal:beacon");bool placed=false;
            for(const auto& [position,chunk]:archive->world.chunks()) {
                (void)position;for(int x=0;x<32;++x) for(int y=0;y<32;++y) for(int z=0;z<32;++z) if(chunk.get({x,y,z})==signal) placed=true;
            }
            require(placed,"Declarative mod block did not reach the real placement/inventory path");
            const auto player=vc::decodePlayer(archive->gameData);
            require(std::any_of(player.mods.begin(),player.mods.end(),[](const auto& mod){return mod.id=="native_sample";}),"Native mod identity was not persisted");
            require(std::any_of(player.mods.begin(),player.mods.end(),[](const auto& mod){return mod.id=="signal";}),"Data mod identity was not persisted");
        }
        app.activateResourcePacks({});require(app.resources().active().empty(),"Builtin resource restoration failed");tick();
        app.shutdown();const auto report=app.validationReport();require(report.enabled&&!report.errors&&!report.warnings,"Modding Vulkan validation failed");
        std::cout<<"Native callback/command, declarative item placement, resource override, save and shutdown passed\n";return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
