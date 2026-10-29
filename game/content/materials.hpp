#pragma once
#include "engine/renderer/texture.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace vc {
// CPU asset data. Built-in tile indices stay fixed; mods append named tiles.
class ContentAtlas {
public:
    static constexpr std::uint32_t tileSize=16;
    static constexpr std::size_t maxTiles=4096;
    ContentAtlas();
    TextureTile add(std::string identifier,ImageRgba image);
    std::optional<TextureTile> find(std::string_view identifier) const;
    TextureTile require(std::string_view identifier) const;
    std::size_t size() const { return tiles_.size(); }
    AtlasLayout layout() const;
    ImageRgba image() const;
private:
    std::vector<ImageRgba> tiles_;
    std::map<std::string,TextureTile,std::less<>> names_;
};
}
