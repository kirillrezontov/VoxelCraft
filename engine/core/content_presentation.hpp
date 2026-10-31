#pragma once
#include "engine/renderer/texture.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace vc {
enum class ItemCategory { All,Building,Natural,Tools,Materials,Food,Miscellaneous };
std::string_view categoryName(ItemCategory category);
ItemCategory parseCategory(std::string_view category);
struct ContentPresentation {
    std::string name{},description{},category{},icon{};
    std::optional<TextureTile> iconTile{};
};
void validatePresentation(const ContentPresentation& presentation);
}
