#include "game/player/save_state.hpp"
#include "engine/core/binary.hpp"
#include <gtest/gtest.h>
#include <limits>

TEST(PlayerSave, ExactRoundTripAndValidation) {
    vc::PlayerSaveState state;
    state.camera.position={-32.125,18.625,1000000000000.5};state.camera.yaw=1.25;state.camera.pitch=-0.75;
    state.flying=true;state.hotbar[8]="example:custom/stone";state.selectedSlot=8;
    const auto data=vc::encodePlayer(state);
    const auto decoded=vc::decodePlayer(data);
    EXPECT_EQ(decoded.camera.position,state.camera.position);
    EXPECT_EQ(decoded.camera.yaw,state.camera.yaw);EXPECT_EQ(decoded.camera.pitch,state.camera.pitch);
    EXPECT_TRUE(decoded.flying);EXPECT_EQ(decoded.hotbar,state.hotbar);EXPECT_EQ(decoded.selectedSlot,8u);
    EXPECT_THROW(vc::decodePlayer(data.substr(0,20)),std::runtime_error);
    EXPECT_THROW(vc::decodePlayer(data+"trailing"),std::runtime_error);
    state.camera.position.x=std::numeric_limits<double>::infinity();
    EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
    state.camera.position.x=0;state.camera.pitch=2;
    EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
    state.camera.pitch=0;state.hotbar[0]="not-namespaced";
    EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
}

TEST(BinaryFormat, ExplicitLittleEndianFixtureAndChecksum) {
    vc::binary::Writer out;out.u16(0x1234);out.u32(0x56789abc);out.i64(-2);
    EXPECT_EQ(out.bytes,(vc::binary::Bytes{0x34,0x12,0xbc,0x9a,0x78,0x56,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff}));
    out.seal();
    auto in=vc::binary::Reader::checked(out.bytes);
    EXPECT_EQ(in.u16(),0x1234);EXPECT_EQ(in.u32(),0x56789abcu);EXPECT_EQ(in.i64(),-2);in.finish();
    out.bytes[0]^=1;
    EXPECT_THROW(vc::binary::Reader::checked(out.bytes),std::runtime_error);
}
TEST(PlayerSave, MigratesVersionOneSelectedMaterialIntoHotbar) {
    vc::binary::Writer out;out.raw("VCPLAYER");out.u32(1);
    for(const double value:{-1.5,20.0,0.5,0.0,-0.5}) out.f64(value);
    out.u32(1);out.text("voxelcraft:stone");
    const auto decoded=vc::decodePlayer({reinterpret_cast<const char*>(out.bytes.data()),out.bytes.size()});
    EXPECT_EQ(decoded.hotbar[0],"voxelcraft:stone");EXPECT_EQ(decoded.selectedSlot,0u);EXPECT_TRUE(decoded.flying);
}
TEST(PlayerSave, MigratesVersionTwoHotbarIntoCreativeWithEmptySurvivalInventory) {
    vc::binary::Writer out;out.raw("VCPLAYER");out.u32(2);
    for(const double value:{-1.5,20.0,0.5,0.0,-0.5}) out.f64(value);
    out.u32(0);out.u32(8);
    for(int slot=0;slot<9;++slot) out.text(slot==8?"voxelcraft:stone":"");
    const auto decoded=vc::decodePlayer({reinterpret_cast<const char*>(out.bytes.data()),out.bytes.size()});
    EXPECT_FALSE(decoded.survival);EXPECT_EQ(decoded.health,20u);EXPECT_EQ(decoded.selectedSlot,8u);EXPECT_EQ(decoded.hotbar[8],"voxelcraft:stone");
    for(const auto& stack:decoded.items) EXPECT_TRUE(stack.identifier.empty());
}
