#include "engine/core/utf8.hpp"
#include "engine/ui/font.hpp"
#include "engine/ui/text_edit.hpp"
#include "engine/ui/widgets.hpp"
#include <gtest/gtest.h>
#include <algorithm>

TEST(Utf8, CyrillicScalarsBoundariesAndMalformedInput) {
    const std::string text="Привет, мир!";EXPECT_EQ(vc::utf8::decode(text).size(),12u);
    EXPECT_EQ(vc::utf8::previous(text,2),0u);EXPECT_EQ(vc::utf8::next(text,0),2u);
    EXPECT_EQ(vc::utf8::truncate(text,3),"П");
    for(const std::string& bad:{std::string("\xc0\x80",2),std::string("\xed\xa0\x80",3),std::string("\xf4\x90\x80\x80",4),std::string("\xd0",1)}) EXPECT_FALSE(vc::utf8::valid(bad));
    EXPECT_EQ(vc::utf8::singleLine("Hello\nмир",40),"Hello мир");
}
TEST(TextEditing, UnicodeSelectionInsertDeleteAndBoundedPaste) {
    vc::ui::TextEdit edit(12);edit.set("Привет");edit.process({.backspace=true});EXPECT_EQ(edit.text(),"Приве");
    edit.process({.home=true});edit.process({.right=true,.shift=true});EXPECT_EQ(edit.selected(),"П");
    edit.process({.text="Hello"});EXPECT_EQ(edit.text(),"Hellриве"); // byte limit keeps a valid boundary.
    EXPECT_TRUE(vc::utf8::valid(edit.text()));EXPECT_LE(edit.text().size(),12u);
    edit.process({.selectAll=true});edit.process({.text="мир"});EXPECT_EQ(edit.text(),"мир");
    edit.process({.home=true});edit.process({.erase=true});EXPECT_EQ(edit.text(),"ир");
    EXPECT_THROW(edit.select(1,2),std::invalid_argument);
    edit.process({.selectAll=true});edit.process({.cut=true});EXPECT_TRUE(edit.text().empty());
}
TEST(FontAtlas, TrueTypeCyrillicMetricsCachingAndClipping) {
    vc::ui::FontAtlas font;
    const auto latin=font.measure("VoxelCraft",24),cyrillic=font.measure("Кириллица",24);
    EXPECT_GT(latin.x,60);EXPECT_GT(cyrillic.x,60);EXPECT_GT(latin.y,20);
    const auto revision=font.revision();font.measure("VoxelCraft",24);EXPECT_EQ(font.revision(),revision);
    const auto monoA=font.measure("iiii",24,true),monoB=font.measure("WWWW",24,true);EXPECT_FLOAT_EQ(monoA.x,monoB.x);
    EXPECT_LT(font.measure("iiii",24).x,font.measure("WWWW",24).x);
    vc::Mesh mesh;font.append(mesh,"Привет /help",-10,0,24,{1,1,1},{0,0,70,30});ASSERT_FALSE(mesh.vertices.empty());
    for(const auto& vertex:mesh.vertices) { EXPECT_GE(vertex.position.x,0);EXPECT_LE(vertex.position.x,70);EXPECT_GE(vertex.position.y,0);EXPECT_LE(vertex.position.y,30);EXPECT_LT(vertex.tileBounds.z,0); }
    EXPECT_TRUE(std::ranges::any_of(font.image().pixels,[](auto byte){return byte!=0;}));
    EXPECT_THROW(font.glyph(U'A',0),std::invalid_argument);
}
TEST(Widgets, FocusDisabledHoverAndBoundedScrolling) {
    vc::ui::WidgetTree tree;tree.add({"panel",vc::ui::WidgetKind::Panel,{0,0,200,100}});
    tree.add({"one",vc::ui::WidgetKind::Button,{8,8,50,30},true,true});tree.add({"disabled",vc::ui::WidgetKind::Button,{60,8,50,30},false,true});
    tree.add({"two",vc::ui::WidgetKind::TextInput,{8,50,100,30},true,true});
    EXPECT_TRUE(tree.hover(10,10));EXPECT_EQ(tree.hovered(),"one");EXPECT_FALSE(tree.hover(10,10));
    tree.focus("one");tree.navigate(1);EXPECT_EQ(tree.focused(),"two");tree.navigate(1);EXPECT_EQ(tree.focused(),"one");
    vc::ui::ScrollState scroll{0,4,10};scroll.scroll(100);EXPECT_EQ(scroll.offset,6u);scroll.scroll(-100);EXPECT_EQ(scroll.offset,0u);
}

#include "game/ui/sandbox_ui.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
TEST(CommandConsole, UnicodeEditingCompletionHistoryChatAndBounds) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto registry=vc::commands::builtinCommands();
    vc::commands::CompletionContext context{vc::commands::Permission::Administrator,&items,{"Alice"}};
    vc::CommandConsole console;console.begin(true,true);
    console.process({.text="gamemode cr"},0,registry,context,1);ASSERT_FALSE(console.suggestions().empty());
    console.process({.tab=true},0,registry,context,1);EXPECT_EQ(console.editor().text(),"/gamemode creative");
    console.process({.enter=true},0,registry,context,1);EXPECT_EQ(console.takeCommands(),std::vector<std::string>{"/gamemode creative"});
    console.process({.up=true},0,registry,context,1);EXPECT_EQ(console.editor().text(),"/gamemode creative");
    console.process({.down=true},0,registry,context,1);EXPECT_TRUE(console.editor().text().empty());
    console.process({.text="Привет мир"},0,registry,context,1);console.process({.selectAll=true},0,registry,context,1);
    console.process({.copy=true},0,registry,context,1);EXPECT_EQ(console.takeClipboard(),"Привет мир");
    console.process({.enter=true},0,registry,context,1);EXPECT_EQ(console.takeCommands(),std::vector<std::string>{"/say \"Привет мир\""});
    for(int i=0;i<200;++i) { console.message("line",vc::NotificationSeverity::Info,i); }
    EXPECT_EQ(console.output().size(),128u);
    console.process({},10000,registry,context,1);EXPECT_LE(console.outputOffset(),128u);
    console.editor().set(std::string(1024,'a'));console.process({.text="б"},0,registry,context,1);EXPECT_EQ(console.editor().text().size(),1024u);
}
TEST(ModernUi, SearchCategoriesTooltipsKeyboardAndConsoleRestore) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);vc::CreativeInventory inventory(items);vc::SandboxUiController ui(inventory);
    const vc::SandboxUiLayout layout(1280,720);ui.process({.toggle=true},1280,720);auto r=layout.category(3);
    ui.process({.primary=true,.x=r.x+2,.y=r.y+2},1280,720);ASSERT_EQ(ui.filteredItems().size(),2u);
    auto cell=layout.catalogCell(0);ui.process({.primary=true,.x=cell.x+2,.y=cell.y+2},1280,720);
    EXPECT_EQ(inventory.selectedItem(),items.require("voxelcraft:wooden_pickaxe"));EXPECT_FALSE(inventory.selectedBlock());
    ASSERT_TRUE(ui.rebuild(blocks,vc::builtinAtlasLayout(),1280,720,0));
    const auto* widget=ui.widgets().hit(cell.x+2,cell.y+2);ASSERT_TRUE(widget);EXPECT_NE(widget->tooltip.find("voxelcraft:wooden_pickaxe"),std::string::npos);
    r=layout.search();ui.process({.primary=true,.x=r.x+2,.y=r.y+2},1280,720);EXPECT_TRUE(ui.wantsTextInput());
    ui.process({.text={.text="stone_pickaxe"}},1280,720);ASSERT_EQ(ui.filteredItems().size(),1u);EXPECT_EQ(ui.filteredItems()[0],items.require("voxelcraft:stone_pickaxe"));
    ui.process({.console=1},1280,720);EXPECT_EQ(ui.screen(),vc::UiScreen::Console);
    ui.process({.text={.text="help",.enter=true}},1280,720);EXPECT_EQ(ui.takeCommands(),std::vector<std::string>{"/help"});
    ui.process({.close=true},1280,720);EXPECT_EQ(ui.screen(),vc::UiScreen::Inventory);EXPECT_FALSE(ui.wantsTextInput());
    ui.process({.text={.tab=true}},1280,720);EXPECT_FALSE(ui.widgets().focused().empty());
    ui.process({.close=true},1280,720);ui.process({.pause=true},1280,720);EXPECT_TRUE(ui.paused());
    ui.process({.close=true},1280,720);EXPECT_FALSE(ui.paused());
}
TEST(ModernUi, ScaleViewportBoundsAndIdleGeometryRetention) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);vc::CreativeInventory inventory(items);
    for(const auto dimensions:{glm::ivec2{320,240},{1280,720},{1920,1080},{2560,1440}}) for(const double scale:{.75,1.,1.75}) {
        vc::GameSettings settings;settings.uiScale=scale;vc::SandboxUiController ui(inventory,settings);
        for(const auto screen:{vc::UiScreen::Inventory,vc::UiScreen::Settings,vc::UiScreen::Pause,vc::UiScreen::Console}) {
            ui.closeMenus();vc::SandboxUiInput input;
            if(screen==vc::UiScreen::Inventory) input.toggle=true;
            if(screen==vc::UiScreen::Settings) input.settingsToggle=true;
            if(screen==vc::UiScreen::Pause) input.pause=true;
            if(screen==vc::UiScreen::Console) input.console=3;
            ui.process(input,dimensions.x,dimensions.y);auto hud=ui.rebuild(blocks,vc::builtinAtlasLayout(),dimensions.x,dimensions.y,1);ASSERT_TRUE(hud);
            for(const auto* mesh:{&hud->colored,&hud->textured,&hud->foreground}) for(const auto& vertex:mesh->vertices) {
                EXPECT_GE(vertex.position.x,0);EXPECT_LE(vertex.position.x,dimensions.x);EXPECT_GE(vertex.position.y,0);EXPECT_LE(vertex.position.y,dimensions.y);
            }
            EXPECT_FALSE(ui.rebuild(blocks,vc::builtinAtlasLayout(),dimensions.x,dimensions.y,1.01));
            for(const auto& widget:ui.widgets().widgets()) { EXPECT_GE(widget.bounds.x,0);EXPECT_LE(widget.bounds.x+widget.bounds.width,dimensions.x);EXPECT_GE(widget.bounds.y,0);EXPECT_LE(widget.bounds.y+widget.bounds.height,dimensions.y); }
        }
    }
}
TEST(ModernUi, DragSplitQuickTransferAreTransactionalAndCancelLosesNothing) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
    vc::SurvivalState state;const auto stone=items.require("voxelcraft:stone");ASSERT_TRUE(state.inventory.add(stone,63,items));
    vc::CreativeInventory inventory(items);vc::SurvivalUiContext context{&state,&items,&recipes};vc::SandboxUiController ui(inventory,{},&context);
    const vc::SandboxUiLayout layout(1280,720);ui.process({.toggle=true},1280,720);
    auto from=layout.hotbarSlot(0),to=layout.backpackCell(0);
    ui.process({.primary=true,.x=from.x+2,.y=from.y+2,.primaryHeld=true},1280,720);
    ui.process({.x=to.x+2,.y=to.y+2,.primaryRelease=true},1280,720);EXPECT_TRUE(state.inventory.slots()[0].empty());EXPECT_EQ(state.inventory.slots()[9].count,63);
    ui.process({.secondary=true,.x=to.x+2,.y=to.y+2},1280,720);EXPECT_EQ(state.inventory.slots()[0].count,32);EXPECT_EQ(state.inventory.slots()[9].count,31);
    ui.process({.primary=true,.x=to.x+2,.y=to.y+2,.shift=true},1280,720);EXPECT_EQ(state.inventory.count(stone),63u);EXPECT_EQ(state.inventory.slots()[0].count,63);
    ui.process({.primary=true,.x=from.x+2,.y=from.y+2,.primaryHeld=true},1280,720);ui.closeMenus();EXPECT_EQ(state.inventory.count(stone),63u);
    ui.process({.primaryRelease=true},1280,720);EXPECT_EQ(state.inventory.count(stone),63u);
    const auto before=state.inventory.slots();const auto revision=state.inventory.revision();
    for(const auto& recipe:recipes) (void)state.inventory.canCraft(recipe,items);
    EXPECT_EQ(state.inventory.slots(),before);EXPECT_EQ(state.inventory.revision(),revision);
}
TEST(ModernUi, MouseHeldClickSelectsEmptyHotbarAndBackpackClickSwapsOnRelease) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
    vc::SurvivalState state;ASSERT_TRUE(state.inventory.add(items.require("voxelcraft:stone"),17,items));ASSERT_TRUE(state.inventory.quickTransfer(0,items));
    vc::CreativeInventory inventory(items);vc::SurvivalUiContext context{&state,&items,&recipes};vc::SandboxUiController ui(inventory,{},&context);ui.process({.toggle=true},1280,720);
    const vc::SandboxUiLayout layout(1280,720);const auto slot=layout.hotbarSlot(8),pack=layout.backpackCell(0);
    ui.process({.primary=true,.x=slot.x+1,.y=slot.y+1,.primaryHeld=true},1280,720);ui.process({.x=slot.x+1,.y=slot.y+1,.primaryRelease=true},1280,720);EXPECT_EQ(state.inventory.selectedSlot(),8u);
    ui.process({.primary=true,.x=pack.x+1,.y=pack.y+1,.primaryHeld=true},1280,720);EXPECT_TRUE(state.inventory.selected().empty());
    ui.process({.x=pack.x+1,.y=pack.y+1,.primaryRelease=true},1280,720);EXPECT_EQ(state.inventory.selected().count,17);EXPECT_EQ(state.inventory.count(items.require("voxelcraft:stone")),17u);
}
