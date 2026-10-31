#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/player/save_state.hpp"
#include "game/ui/sandbox_ui.hpp"
#include <SDL3/SDL_main.h>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void key(vc::Window& window,SDL_Scancode scan,bool down=true) {
    SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;event.key.windowID=window.id();event.key.scancode=scan;event.key.down=down;
    require(SDL_PushEvent(&event),SDL_GetError());
}
void text(vc::Window& window,const char* value) {
    SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;event.text.windowID=window.id();event.text.text=value;require(SDL_PushEvent(&event),SDL_GetError());
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected capture directory");const auto captures=vc::utf8Path(argv[1]);std::filesystem::create_directories(captures);
        vc::Window window;window.title("VoxelCraft console verification v0.8.2");vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);
        auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
        vc::voxel::World world({0,31});vc::voxel::Chunk chunk({0,0,0});
        for(int x=0;x<32;++x) for(int z=0;z<32;++z) chunk.set({x,0,z},blocks.require("voxelcraft:stone"));
        world.insert(std::move(chunk));
        vc::PlayerController player;player.camera.position={2.5,2.62,2.5};vc::SurvivalState survival;bool survivalMode=false;
        vc::LivingWorld living{42};vc::DayCycle day;vc::CreativeInventory inventory(items);
        vc::SurvivalUiContext survivalUi{&survival,&items,&recipes};vc::SandboxUiController ui(inventory);
        const auto registry=vc::commands::builtinCommands();std::optional<glm::dvec3> spawn;std::uint64_t poseRevision{};
        const auto step=[&](const std::filesystem::path& capture=std::filesystem::path{}) {
            auto input=window.poll();require(!input.quit,"Console test interrupted");if(input.resized) renderer.requestResize();
            const auto [w,h]=window.pixelSize();
            if(ui.process({input.inventory,input.closeUi,input.primaryClick,input.secondaryClick,input.material,input.scroll,input.cursorX,input.cursorY,input.settings,input.crafting,input.console,input.pause,input.primaryRelease,input.primaryHeld,input.shift,input.text},w,h)) {
                window.pointerMode(ui.open());window.capture(!ui.open());
            }
            window.textInput(ui.wantsTextInput());
            for(const auto& command:ui.takeCommands()) {
                auto hotbar=inventory.names();auto slot=static_cast<std::uint32_t>(inventory.selectedSlot());
                vc::commands::Context context;context.permission=vc::commands::Permission::Administrator;context.issuer="Alice";
                context.origin=player.camera.position-glm::dvec3(0,1.62,0);context.world=&world;context.blocks=&blocks;context.items=&items;context.living=&living;context.day=&day;context.metadata={42,{0,31},1};
                context.players.push_back({"Alice",&player,&survival,&survivalMode,&hotbar,&slot,&spawn,&poseRevision,context.permission});
                context.broadcast=[&](std::string_view value) { ui.status(std::string(value),vc::Window::seconds()); };
                ui.postCommandResult(registry.execute(command,context),vc::Window::seconds());inventory.restore(hotbar,slot);ui.setSurvivalContext(survivalMode?&survivalUi:nullptr);
            }
            if(!ui.open()) player.update({{input.forward,input.right,input.up,input.mouseX,input.mouseY},input.jump,input.toggleFlight,input.captured},1./60,world,blocks);
            if(auto hud=ui.rebuild(blocks,vc::builtinAtlasLayout(),w,h,vc::Window::seconds())) renderer.setHud(hud->colored,hud->textured,!ui.open(),hud->foreground);
            return renderer.draw(player.camera,capture);
        };
        const auto frames=[&](unsigned count=4) { for(unsigned i=0;i<count;++i) step(); };
        const auto press=[&](SDL_Scancode scan) { key(window,scan);frames();key(window,scan,false);frames(); };
        const auto command=[&](const char* value) { text(window,value);frames();press(SDL_SCANCODE_RETURN); };
        frames();window.capture(true);press(SDL_SCANCODE_SLASH);require(ui.screen()==vc::UiScreen::Console&&ui.wantsTextInput(),"Slash did not focus console");
        require(SDL_TextInputActive(SDL_GetWindowFromID(window.id())),"SDL text input was not started");
        key(window,SDL_SCANCODE_W);command("give @s voxelcraft:wooden_pickaxe");key(window,SDL_SCANCODE_W,false);
        require(survival.inventory.count(items.require("voxelcraft:wooden_pickaxe"))==1,"Console give did not grant actual tool");
        require(player.camera.position==glm::dvec3(2.5,2.62,2.5),"Typing moved player");
        text(window,"/gamemode cr");frames();press(SDL_SCANCODE_TAB);require(ui.console().editor().text()=="/gamemode creative","Live autocomplete failed");press(SDL_SCANCODE_RETURN);
        command("/tp ~2 ~1 ~3");require(player.camera.position==glm::dvec3(4.5,3.62,5.5),"Console teleport failed");
        command("/summon voxelcraft:herbivore 8.5 1 8.5");require(living.creatureCount()==1,"Console summon failed");
        command("/time set night");require(day.seconds()==900,"Console time did not affect real cycle");
        command("/gamemode survival");require(survivalMode&&!player.flying,"Console game mode failed");
        command("/give @s unknown:item");command("Привет, мир! / Кириллица");
        text(window,"/give @s voxelcraft:sto");frames();require(!ui.console().suggestions().empty(),"Item completion is empty");
        for(const auto dimensions:{glm::ivec2{1280,720},{1920,1080},{2560,1440}}) {
            window.resize(dimensions.x,dimensions.y);renderer.requestResize();frames();
            const auto actual=window.pixelSize();
            vc::log("Console resize requested "+std::to_string(dimensions.x)+"x"+std::to_string(dimensions.y)+"; actual "+std::to_string(actual.first)+"x"+std::to_string(actual.second));
            const auto capture=captures/(std::to_string(dimensions.x)+"-console.ppm");bool drawn=false;
            for(int i=0;i<12&&!drawn;++i) drawn=step(capture);
            require(drawn&&std::filesystem::exists(capture),"Console screenshot failed");
        }
        press(SDL_SCANCODE_ESCAPE);require(!ui.open()&&!SDL_TextInputActive(SDL_GetWindowFromID(window.id())),"Console close did not restore input");
        const auto saved=survival.inventory.save(items);vc::PlayerSaveState state;state.survival=survivalMode;state.items=saved;state.spawnPoint=spawn;
        require(vc::decodePlayer(vc::encodePlayer(state)).items==saved,"Console inventory failed save roundtrip");
        renderer.shutdown();const auto validation=renderer.validationReport();require(validation.enabled&&!validation.errors&&!validation.warnings,"Console GPU validation diagnostics");
        vc::log("Console playtest: SDL Unicode, focus, completion, commands, real state and three resize requests passed; zero Vulkan diagnostics");return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
