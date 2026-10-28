#include "game/settings.hpp"
#include "game/player/camera.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "game/content/blocks.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <limits>

TEST(Settings, RoundTripBackupAndInterruptedCommitRecovery) {
    vc::test::TempDirectory temp;const auto path=temp.path()/"config/settings.vcs";
    EXPECT_FALSE(vc::loadSettings(path));
    vc::GameSettings settings;settings.fieldOfView=95;settings.mouseSensitivity=0.006;settings.invertY=true;
    vc::saveSettings(path,settings);
    auto loaded=vc::loadSettings(path);ASSERT_TRUE(loaded);EXPECT_EQ(loaded->settings,settings);EXPECT_FALSE(loaded->recoveredBackup);
    auto second=settings;second.fieldOfView=105;vc::saveSettings(path,second);
    EXPECT_EQ(vc::loadSettings(path)->settings,second);
    std::filesystem::remove(path); // Interrupted after moving the old primary to backup.
    loaded=vc::loadSettings(path);ASSERT_TRUE(loaded);EXPECT_EQ(loaded->settings,settings);EXPECT_TRUE(loaded->recoveredBackup);
    std::ofstream(path,std::ios::binary)<<"corrupt";
    loaded=vc::loadSettings(path);ASSERT_TRUE(loaded);EXPECT_TRUE(loaded->recoveredBackup);EXPECT_EQ(loaded->settings,settings);
}
TEST(Settings, InvalidValuesNeverReplaceValidSettings) {
    vc::test::TempDirectory temp;const auto path=temp.path()/"settings.vcs";vc::GameSettings settings;
    vc::saveSettings(path,settings);
    for(const auto value:{0.0,49.0,111.0,std::numeric_limits<double>::quiet_NaN()}) {
        auto invalid=settings;invalid.fieldOfView=value;
        EXPECT_THROW(vc::saveSettings(path,invalid),std::invalid_argument);
        EXPECT_EQ(vc::loadSettings(path)->settings,settings);
    }
    for(const auto value:{0.0,0.0001,0.02,std::numeric_limits<double>::infinity()}) {
        auto invalid=settings;invalid.mouseSensitivity=value;EXPECT_THROW(invalid.validate(),std::invalid_argument);
    }
    std::ofstream(path,std::ios::binary|std::ios::app).put('x');
    EXPECT_THROW(vc::loadSettings(path),std::runtime_error);
}
TEST(Settings, SettingsMenuAppliesChangesBoundsResetAndClose) {
    const auto registry=vc::builtinBlocks();vc::CreativeInventory inventory(registry);vc::SandboxUiController ui(inventory);
    EXPECT_TRUE(ui.process({.settingsToggle=true},1280,720));EXPECT_TRUE(ui.settingsOpen());
    const vc::SandboxUiLayout layout(1280,720);
    const auto click=[&](std::size_t button) {
        const auto area=layout.settingsButton(button);
        return ui.process({.primary=true,.x=area.x+1,.y=area.y+1},1280,720);
    };
    click(1);EXPECT_EQ(ui.settings().fieldOfView,80);EXPECT_TRUE(ui.takeSettingsChanged());EXPECT_FALSE(ui.takeSettingsChanged());
    click(3);EXPECT_DOUBLE_EQ(ui.settings().mouseSensitivity,0.003);
    click(4);EXPECT_TRUE(ui.settings().invertY);
    for(int i=0;i<30;++i) { click(1);click(3); }
    EXPECT_EQ(ui.settings().fieldOfView,110);EXPECT_DOUBLE_EQ(ui.settings().mouseSensitivity,0.01);
    click(5);EXPECT_EQ(ui.settings(),vc::GameSettings{});
    EXPECT_TRUE(click(6));EXPECT_FALSE(ui.open());
    ui.process({.toggle=true},1280,720);EXPECT_TRUE(ui.open());EXPECT_FALSE(ui.settingsOpen());
    ui.process({.settingsToggle=true},1280,720);EXPECT_TRUE(ui.settingsOpen());
    ui.process({.close=true},1280,720);EXPECT_FALSE(ui.open());
}
TEST(CameraSettings, SensitivityInversionAndProjectionAreFunctional) {
    vc::Camera camera;camera.position={};camera.yaw=camera.pitch=0;camera.mouseSensitivity=0.005;camera.invertY=true;
    camera.update({0,0,0,20,10},0);
    EXPECT_DOUBLE_EQ(camera.yaw,0.1);EXPECT_DOUBLE_EQ(camera.pitch,0.05);
    camera.invertY=false;camera.update({0,0,0,0,10},0);EXPECT_DOUBLE_EQ(camera.pitch,0);
    const auto before=camera.viewProjection(1);camera.fieldOfView=100;const auto after=camera.viewProjection(1);
    EXPECT_LT(after[0][0],before[0][0]);
    EXPECT_THROW(camera.viewProjection(std::numeric_limits<float>::infinity()),std::invalid_argument);
}
