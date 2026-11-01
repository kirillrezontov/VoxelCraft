#include "game/content/services.hpp"
#include "game/content/blocks.hpp"
#include <utility>

namespace vc {
ContentServices::ContentServices(const std::filesystem::path& directory,const std::filesystem::path& nativeRoot)
    : blocks_(builtinBlocks()), mods_(directory, blocks_,nativeRoot) {
    rebuildGameplay();
}
void ContentServices::rebuildGameplay() {
    auto items = builtinItems(blocks_);
    auto recipes = builtinRecipes(items);
    mods_.installItems(items, blocks_, recipes);
    MiningRules mining(blocks_, items);
    mods_.installMining(mining, blocks_, items);
    // Parsing/registration can throw. Publish only a complete set of values.
    items_ = std::move(items);
    recipes_ = std::move(recipes);
    mining_ = std::move(mining);
}
}
