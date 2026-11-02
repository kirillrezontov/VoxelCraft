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
