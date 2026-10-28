#include "game/gameplay/inventory.hpp"
#include "game/player/save_state.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include <gtest/gtest.h>
#include <limits>

TEST(Inventory, SelectionAssignmentAndEmptySlots) {
    const auto registry=vc::builtinBlocks();vc::CreativeInventory inventory(registry);
    EXPECT_EQ(inventory.catalog().size(),registry.size()-1);EXPECT_EQ(inventory.selectedBlock(),registry.require("voxelcraft:grass"));
    inventory.cycle(-1);EXPECT_EQ(inventory.selectedSlot(),8u);EXPECT_FALSE(inventory.selectedBlock());
    inventory.cycle(1);EXPECT_EQ(inventory.selectedSlot(),0u);
    inventory.cycle(std::numeric_limits<int>::min());EXPECT_LT(inventory.selectedSlot(),9u);
    inventory.select(8);inventory.assign(8,registry.require("voxelcraft:stone"));
    EXPECT_EQ(inventory.selectedBlock(),registry.require("voxelcraft:stone"));
    EXPECT_FALSE(inventory.assign(8,registry.require("voxelcraft:stone")));
    inventory.assign(8,std::nullopt);EXPECT_FALSE(inventory.selectedBlock());
    EXPECT_THROW(inventory.select(9),std::out_of_range);
    EXPECT_THROW(inventory.assign(0,vc::voxel::air),std::invalid_argument);
    EXPECT_THROW(inventory.assign(0,65535),std::invalid_argument);
}
TEST(Inventory, SaveRoundTripAndFailedRestoreIsAtomic) {
    const auto registry=vc::builtinBlocks();vc::CreativeInventory inventory(registry);
    inventory.assign(8,registry.require("voxelcraft:dirt"));inventory.assign(0,std::nullopt);inventory.select(8);
    vc::PlayerSaveState state;state.hotbar=inventory.names(registry);state.selectedSlot=8;
    const auto decoded=vc::decodePlayer(vc::encodePlayer(state));
    vc::CreativeInventory restored(registry);restored.restore(decoded.hotbar,decoded.selectedSlot,registry);
    EXPECT_EQ(restored.slots(),inventory.slots());EXPECT_EQ(restored.selectedSlot(),8u);
    auto invalid=decoded.hotbar;invalid[8]="unknown:missing";
    EXPECT_THROW(restored.restore(invalid,1,registry),std::invalid_argument);
    EXPECT_EQ(restored.slots(),inventory.slots());EXPECT_EQ(restored.selectedSlot(),8u);
}
TEST(SandboxUi, CatalogAssignmentClearAndCaptureTransitions) {
    const auto registry=vc::builtinBlocks();vc::CreativeInventory inventory(registry);vc::SandboxUiController ui(inventory);
    const vc::SandboxUiLayout layout(1280,720);
    EXPECT_TRUE(ui.process({.toggle=true},1280,720));EXPECT_TRUE(ui.open());
    ui.process({.slot=9},1280,720);EXPECT_EQ(inventory.selectedSlot(),8u);
    const auto cell=layout.catalogCell(0);
    ui.process({.primary=true,.x=cell.x+10,.y=cell.y+10},1280,720);
    EXPECT_EQ(inventory.selectedBlock(),inventory.catalog()[0]);
    const auto slot=layout.hotbarSlot(8);
    ui.process({.secondary=true,.x=slot.x+10,.y=slot.y+10},1280,720);EXPECT_FALSE(inventory.selectedBlock());
    EXPECT_TRUE(ui.process({.close=true},1280,720));EXPECT_FALSE(ui.open());
    ui.process({.scroll=-1},1280,720);EXPECT_EQ(inventory.selectedSlot(),0u);
    ui.process({.scroll=std::numeric_limits<int>::min()},1280,720);EXPECT_LT(inventory.selectedSlot(),9u);
}
TEST(SandboxUi, CatalogPaginationRebuildAndMinimize) {
    auto registry=vc::builtinBlocks();
    for(int i=0;i<40;++i) registry.add({"example:block"+std::to_string(i),true,{1,0,1}});
    vc::CreativeInventory inventory(registry);vc::SandboxUiController ui(inventory);
    EXPECT_TRUE(ui.rebuild(registry,vc::builtinAtlasLayout(),800,600,0));
    EXPECT_FALSE(ui.rebuild(registry,vc::builtinAtlasLayout(),800,600,1));
    ui.process({.toggle=true},800,600);ui.process({.scroll=-1},800,600);EXPECT_EQ(ui.page(),1u);
    const vc::SandboxUiLayout layout(800,600);const auto cell=layout.catalogCell(0);
    ui.process({.primary=true,.x=cell.x+1,.y=cell.y+1},800,600);
    EXPECT_EQ(inventory.selectedBlock(),inventory.catalog()[32]);
    ui.process({.scroll=std::numeric_limits<int>::min()},800,600);EXPECT_EQ(ui.page(),1u);
    ui.status("SAVED",1);EXPECT_TRUE(ui.rebuild(registry,vc::builtinAtlasLayout(),800,600,2));
    EXPECT_FALSE(ui.rebuild(registry,vc::builtinAtlasLayout(),0,0,3));
    EXPECT_TRUE(ui.rebuild(registry,vc::builtinAtlasLayout(),800,600,6));
    EXPECT_TRUE(ui.rebuild(registry,vc::builtinAtlasLayout(),1280,720,7));
}
TEST(SandboxUi, GeometryUsesValidIndicesUvsAndViewportBounds) {
    const auto registry=vc::builtinBlocks();vc::CreativeInventory inventory(registry);
    for(const auto dimensions:{glm::ivec2(1280,720),{600,1200},{320,200}}) {
        const auto hud=vc::sandboxHud(inventory,registry,vc::builtinAtlasLayout(),dimensions.x,dimensions.y,true);
        for(const auto* mesh:{&hud.colored,&hud.textured,&hud.foreground}) {
            EXPECT_FALSE(mesh->indices.empty());EXPECT_EQ(mesh->indices.size()%3,0u);
            for(const auto i:mesh->indices) ASSERT_LT(i,mesh->vertices.size());
            for(const auto& v:mesh->vertices) {
                EXPECT_GE(v.position.x,0);EXPECT_LE(v.position.x,dimensions.x);
                EXPECT_GE(v.position.y,0);EXPECT_LE(v.position.y,dimensions.y);
                EXPECT_GE(v.uv.x,0);EXPECT_LE(v.uv.x,1);EXPECT_GE(v.uv.y,0);EXPECT_LE(v.uv.y,1);
            }
        }
    }
}
