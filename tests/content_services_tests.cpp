#include "game/content/services.hpp"
#include "game/content/blocks.hpp"
#include "game/network/content_fingerprint.hpp"
#include <gtest/gtest.h>

TEST(ContentServices, MatchesExistingContentCompositionAndWireFingerprint) {
    auto blocks = vc::builtinBlocks();
    vc::ModRuntime mods({}, blocks);
    auto items = vc::builtinItems(blocks);
    auto recipes = vc::builtinRecipes(items);
    mods.installItems(items, blocks, recipes);
    vc::MiningRules mining(blocks, items);
    mods.installMining(mining, blocks, items);
    vc::ContentServices content;
    EXPECT_EQ(content.blocks().size(), blocks.size());
    EXPECT_EQ(content.items().size(), items.size());
    EXPECT_EQ(vc::netgame::contentFingerprint(content.blocks(), content.items(), content.recipes(), content.mining(), content.mods()),
        vc::netgame::contentFingerprint(blocks, items, recipes, mining, mods));
}

TEST(ContentServices, RebuildAfterPaletteRestoreRetainsObjectIdentityAndMissingNames) {
    vc::ContentServices content;
    auto* items = &content.items();
    auto* rules = &content.mining();
    auto* mods = &content.mods();
    // Same append operation performed by WorldStore/RemoteSession palette restore.
    const auto missing = content.blocks().add({"absent:saved_block", true, {1, 0, 1}});
    content.rebuildGameplay();
    EXPECT_EQ(&content.items(), items);
    EXPECT_EQ(&content.mining(), rules);
    EXPECT_EQ(&content.mods(), mods);
    const auto item = content.items().require("absent:saved_block");
    EXPECT_EQ(content.items().definition(item).block, missing);
    EXPECT_EQ(content.mining().rule(missing).drop, item);
    const std::array<std::string, 9> hotbar{"absent:saved_item"};
    vc::retainHotbarItems(content.items(), hotbar);
    EXPECT_TRUE(content.items().find("absent:saved_item"));
}
