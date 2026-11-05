#include "client/declarative_ui.hpp"
#include "tests/temp_directory.hpp"
#include <voxelui/software.hpp>
#include <gtest/gtest.h>
#include <fstream>

namespace {
void tab(vc::client::DeclarativeUi& ui) { vc::Input in;in.text.tab=true;ui.process(in); }
void replace(vc::client::DeclarativeUi& ui,std::string text) {
    vc::Input in;in.text.selectAll=true;ui.process(in);in={};in.text.text=std::move(text);ui.process(in);
}
}
TEST(VoxelUiHost, RealMenuEditsDataQueuesActionsAndPreservesValuesAcrossScreens) {
    const std::filesystem::path assets=VC_UI_ASSET_DIR;
    vc::ResourceCatalog resources(assets/"resources");
    vc::client::DeclarativeUi ui(resources,assets/"fonts","127.0.0.1:25565","Player");
    ui.update(vc::client::ApplicationScreen::MainMenu,"",960,640,1);
    ASSERT_FALSE(ui.draw().commands.empty());
    tab(ui);ASSERT_TRUE(ui.editing());replace(ui,"example.test:25565");EXPECT_EQ(ui.address(),"example.test:25565");
    tab(ui);replace(ui,"Игрок");EXPECT_EQ(ui.name(),"Игрок");tab(ui);
    vc::Input enter;enter.text.enter=true;ui.process(enter);
    EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::Connect);EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::None);
    ui.update(vc::client::ApplicationScreen::Loading,"Connecting",640,480,1.5f);
    ui.update(vc::client::ApplicationScreen::Error,"Connection refused",640,480,1.5f);
    vc::Input back;back.closeUi=true;ui.process(back);EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::Back);
    ui.update(vc::client::ApplicationScreen::MainMenu,"",640,480,1.5f);
    EXPECT_EQ(ui.address(),"example.test:25565");EXPECT_EQ(ui.name(),"Игрок");
    voxelui::SoftwareRenderer renderer;renderer.render(ui.draw());EXPECT_GT(renderer.width(),0);
    ui.process(enter);EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::Play);
}
TEST(VoxelUiHost, InvalidPackScreenAndStylesRecoverThroughBuiltinProvider) {
    vc::test::TempDirectory temp;const auto root=temp.path()/"pack";
    std::filesystem::create_directories(root/"resources/voxelcraft/ui");
    std::ofstream(root/"resource-pack.json")<<R"({"id":"broken","name":"Broken","version":"1"})";
    std::ofstream(root/"resources/voxelcraft/ui/main-menu.xml")<<"<Screen>broken";
    const std::filesystem::path assets=VC_UI_ASSET_DIR;vc::ResourceCatalog resources(assets/"resources");resources.activate({root});
    vc::client::DeclarativeUi ui(resources,assets/"fonts","localhost:25565","Player");
    EXPECT_NO_THROW(ui.update(vc::client::ApplicationScreen::MainMenu,"",960,640,1));
    EXPECT_FALSE(ui.draw().commands.empty());
    std::ofstream(root/"resources/voxelcraft/ui/main-menu.xml")<<R"(<Screen version="1" stylesheet="voxelcraft:ui/menu.css"/>)";
    std::ofstream(root/"resources/voxelcraft/ui/menu.css")<<"* { invalid-property:1 }";
    ui.reload();EXPECT_NO_THROW(ui.update(vc::client::ApplicationScreen::MainMenu,"",960,640,1));
    vc::Input enter;enter.text.enter=true;ui.process(enter);EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::Play);
}
TEST(VoxelUiHost, OrderedTextEditsAndRepeatedBackspaceAreAppliedWithinTheSamePoll) {
    const std::filesystem::path assets=VC_UI_ASSET_DIR;vc::ResourceCatalog resources(assets/"resources");
    vc::client::DeclarativeUi ui(resources,assets/"fonts","localhost","Player");
    ui.update(vc::client::ApplicationScreen::MainMenu,"",960,700,1);tab(ui);replace(ui,"");
    vc::Input input;input.orderedUiEvents=true;
    using E=vc::UiInputEvent;using T=E::Type;
    input.uiEvents={{.type=T::textInput,.text="ac"},{.type=T::keyDown,.key=SDL_SCANCODE_LEFT},
        {.type=T::textInput,.text="b"},{.type=T::keyDown,.key=SDL_SCANCODE_BACKSPACE}};
    ui.process(input);EXPECT_EQ(ui.address(),"ac");
    input.uiEvents={{.type=T::keyDown,.key=SDL_SCANCODE_END},{.type=T::keyDown,.key=SDL_SCANCODE_BACKSPACE},
        {.type=T::keyDown,.key=SDL_SCANCODE_BACKSPACE}};
    ui.process(input);EXPECT_EQ(ui.address(),"");
    // Enter queues Play; the following Tab is discarded with the old screen.
    ui.update(vc::client::ApplicationScreen::Error,"error",960,700,1);
    input.uiEvents={{.type=T::keyDown,.key=SDL_SCANCODE_RETURN},{.type=T::keyDown,.key=SDL_SCANCODE_TAB}};
    ui.process(input);EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::Back);
}
TEST(VoxelUiHost, PublicWidgetRegistrationUnregisterAndOwningSnapshots) {
    struct Gauge {
        voxelui::Vec2 measure(const voxelui::Node&,voxelui::Vec2) { return {30,12}; }
        void draw(const voxelui::WidgetContext& context,voxelui::DrawList& list) {
            list.commands.emplace_back(voxelui::DrawRectangle{context.bounds,{40,160,80,255},{},0,0});
        }
    };
    voxelui::DrawList snapshot;
    {
        voxelui::Runtime ui;auto widget=std::make_shared<Gauge>();std::weak_ptr<Gauge> lifetime=widget;
        ASSERT_TRUE(ui.components().registerWidget("HostGauge",widget));widget.reset();
        ASSERT_TRUE(ui.loadXml(R"(<Screen version="1"><HostGauge id="gauge"/></Screen>)"));
        snapshot=ui.buildDrawList();ASSERT_FALSE(lifetime.expired());
        ASSERT_TRUE(ui.components().unregisterWidget("HostGauge"));EXPECT_TRUE(lifetime.expired());
        ui.update();ui.close();
    }
    voxelui::SoftwareRenderer renderer;EXPECT_NO_THROW(renderer.render(snapshot));
}
TEST(VoxelUiHost, GameplayHudRetainsSnapshotsAndPauseUsesSessionStateMachine) {
    const std::filesystem::path assets=VC_UI_ASSET_DIR;
    vc::ResourceCatalog resources(assets/"resources");
    vc::client::DeclarativeUi ui(resources,assets/"fonts","localhost:25565","Player");
    vc::voxel::BlockRegistry blocks;vc::CreativeInventory inventory(blocks);
    vc::SandboxUiController controller(inventory);
    const auto update=[&](bool remote=false) { return ui.updateGameplay(controller.screen(),true,15,remote,960,700,1); };
    const auto process=[&](vc::Input input) { return ui.processGameplay(controller,input,960,700); };
    ASSERT_TRUE(update());const auto hud=ui.draw();ASSERT_TRUE(update());EXPECT_EQ(ui.draw(),hud);
    ASSERT_TRUE(ui.updateGameplay(controller.screen(),true,7,false,960,700,1));
    const auto changed=ui.draw();EXPECT_NE(changed,hud);
    voxelui::SoftwareRenderer renderer;renderer.render(hud,{0,0,0,0});const auto before=renderer.pixels();
    renderer.render(changed,{0,0,0,0});EXPECT_NE(renderer.pixels(),before);
    vc::Input pause;pause.pause=true;EXPECT_TRUE(process(pause));ASSERT_TRUE(controller.paused());ASSERT_TRUE(update());
    vc::Input enter;enter.text.enter=true;EXPECT_TRUE(process(enter));EXPECT_FALSE(controller.open());
    EXPECT_TRUE(process(pause));ASSERT_TRUE(update());
    vc::Input next;next.text.tab=true;process(next);EXPECT_TRUE(process(enter));
    EXPECT_EQ(controller.screen(),vc::UiScreen::Settings);EXPECT_TRUE(controller.paused());EXPECT_FALSE(update());
    vc::Input back;back.closeUi=true;EXPECT_TRUE(process(back));EXPECT_EQ(controller.screen(),vc::UiScreen::Pause);
    ASSERT_TRUE(update(true));const auto remotePause=ui.draw();ASSERT_TRUE(update());EXPECT_NE(remotePause,ui.draw());
    process(next);process(next);process(enter);EXPECT_TRUE(controller.takeSaveRequested());EXPECT_TRUE(controller.paused());
    process(next);process(enter);EXPECT_TRUE(controller.takeLeaveRequested());EXPECT_FALSE(controller.takeLeaveRequested());
    // Hidden legacy overlays clear focus/capture; returning to gameplay does not
    // reuse a queued pause action or expose a stale snapshot over inventory.
    controller.closeMenus();vc::Input inventoryKey;inventoryKey.inventory=true;process(inventoryKey);
    EXPECT_FALSE(update());EXPECT_EQ(controller.screen(),vc::UiScreen::Inventory);
    process(back);ASSERT_TRUE(update());EXPECT_EQ(ui.takeAction(),vc::client::MenuAction::None);
    ui.reload();ASSERT_TRUE(update());EXPECT_FALSE(ui.draw().commands.empty());
    EXPECT_FALSE(ui.updateGameplay(vc::UiScreen::Gameplay,true,0,false,960,700,1));
    EXPECT_FALSE(ui.updateGameplay(vc::UiScreen::Gameplay,true,10,false,0,0,1));
    for(const auto size:{voxelui::Vec2{640,480},voxelui::Vec2{1920,1080}}) {
        for(const auto screen:{vc::UiScreen::Gameplay,vc::UiScreen::Pause}) {
            ASSERT_TRUE(ui.updateGameplay(screen,true,10,false,static_cast<int>(size.x),static_cast<int>(size.y),1.75f));
            const auto incremental=ui.draw();renderer.render(incremental,{0,0,0,0});const auto pixels=renderer.pixels();
            vc::client::DeclarativeUi fresh(resources,assets/"fonts","localhost:25565","Player");
            ASSERT_TRUE(fresh.updateGameplay(screen,true,10,false,static_cast<int>(size.x),static_cast<int>(size.y),1.75f));
            renderer.render(fresh.draw(),{0,0,0,0});EXPECT_EQ(renderer.pixels(),pixels);
            EXPECT_EQ(renderer.width(),static_cast<int>(size.x));EXPECT_EQ(renderer.height(),static_cast<int>(size.y));
        }
    }
}
TEST(VoxelUiHost, PausePointerCaptureCancelsOnFocusLossAndScreenReplacement) {
    const std::filesystem::path assets=VC_UI_ASSET_DIR;vc::ResourceCatalog resources(assets/"resources");
    vc::client::DeclarativeUi ui(resources,assets/"fonts","localhost:25565","Player");
    vc::voxel::BlockRegistry blocks;vc::CreativeInventory inventory(blocks);vc::SandboxUiController controller(inventory);
    const auto update=[&] { return ui.updateGameplay(controller.screen(),false,20,false,960,700,1); };
    const auto process=[&](vc::Input input) { return ui.processGameplay(controller,input,960,700); };
    vc::Input pause;pause.pause=true;process(pause);ASSERT_TRUE(update());
    const auto list=ui.draw();voxelui::Rect resume;
    for(const auto& command:list.commands) if(const auto* rect=std::get_if<voxelui::DrawRectangle>(&command);
        rect&&rect->fill==voxelui::Color{61,81,71,255}) { resume=rect->bounds;break; }
    ASSERT_GT(resume.w,0);vc::Input press;press.cursorX=resume.x+resume.w/2;press.cursorY=resume.y+resume.h/2;
    press.primaryClick=press.primaryHeld=true;process(press);EXPECT_TRUE(controller.paused());
    vc::Input lost;lost.focusLost=true;process(lost);
    auto release=press;release.primaryClick=release.primaryHeld=false;release.primaryRelease=true;process(release);
    EXPECT_TRUE(controller.paused());
    process(press);vc::Input settings;settings.settings=true;process(settings);EXPECT_EQ(controller.screen(),vc::UiScreen::Settings);
    EXPECT_FALSE(update());process(release);EXPECT_EQ(controller.screen(),vc::UiScreen::Settings);
    const vc::SandboxUiLayout layout(960,700);const auto setting=layout.settingsButton(1);
    vc::Input change;change.primaryClick=true;change.cursorX=setting.x+setting.width/2;change.cursorY=setting.y+setting.height/2;
    process(change);const auto fieldOfView=controller.settings().fieldOfView;
    vc::Input coalesced;coalesced.pause=true;coalesced.text.enter=true;process(coalesced);
    EXPECT_EQ(controller.screen(),vc::UiScreen::Pause);EXPECT_EQ(controller.settings().fieldOfView,fieldOfView);
    ASSERT_TRUE(update());
    process(press);process(release);EXPECT_EQ(controller.screen(),vc::UiScreen::Gameplay);
}

TEST(VoxelUiHost, AnimationUsesElapsedTimeAndBoundsResumeDelta) {
    vc::client::UiFrameClock clock;
    using namespace std::chrono_literals;
    const auto start=vc::client::UiFrameClock::Clock::time_point{};
    EXPECT_FLOAT_EQ(clock.advance(start),0.f);
    EXPECT_FLOAT_EQ(clock.advance(start+8ms),.008f);
    EXPECT_FLOAT_EQ(clock.advance(start+8ms),0.f);
    EXPECT_FLOAT_EQ(clock.advance(start+24ms),.016f);
    EXPECT_FLOAT_EQ(clock.advance(start+5s),.1f);
    clock.reset();
    EXPECT_FLOAT_EQ(clock.advance(start+10s),0.f);
}
