#include "game/content/materials.hpp"
#include "game/content/atlas.hpp"
#include "engine/voxel/registry.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace vc {
ContentAtlas::ContentAtlas() {
    constexpr std::array<std::string_view,23> names{"white","stone","dirt","grass_top","grass_side",
        "log","log_top","planks","leaves","cobblestone","coal_ore","iron_ore","sand","sandstone","snow","water","lamp","item_stick","item_coal","item_meat","wooden_pickaxe","stone_pickaxe","missing_item"};
    const auto source=builtinAtlas();const auto sourceLayout=builtinAtlasLayout();
    for(std::size_t i=0;i<names.size();++i) {
        const auto id=static_cast<TextureTile>(i);const auto origin=sourceLayout.tileOrigin(id);
        ImageRgba tile{tileSize,tileSize,std::vector<std::uint8_t>(tileSize*tileSize*4)};
        for(std::uint32_t y=0;y<tileSize;++y) {
            const auto begin=(static_cast<std::size_t>(origin.y+y)*source.width+origin.x)*4;
            std::copy_n(source.pixels.begin()+static_cast<std::ptrdiff_t>(begin),tileSize*4,
                tile.pixels.begin()+static_cast<std::ptrdiff_t>(y*tileSize*4));
        }
        tiles_.push_back(std::move(tile));names_.emplace(std::string(names[i]),id);names_.emplace("voxelcraft:"+std::string(names[i]),id);
    }
}
TextureTile ContentAtlas::add(std::string identifier,ImageRgba image) {
    if(identifier.size()>256||!voxel::BlockRegistry::validIdentifier(identifier)) throw std::invalid_argument("Invalid texture identifier: "+identifier);
    if(names_.contains(identifier)) throw std::invalid_argument("Duplicate texture identifier: "+identifier);
    image.validate();
    if(image.width!=tileSize||image.height!=tileSize) throw std::invalid_argument("Texture tiles must be 16x16 pixels");
    if(tiles_.size()>=maxTiles) throw std::runtime_error("Content atlas exceeds 4096 tiles");
    const auto id=static_cast<TextureTile>(tiles_.size());tiles_.push_back(std::move(image));
    try { names_.emplace(std::move(identifier),id); } catch(...) { tiles_.pop_back();throw; }
    return id;
}
std::optional<TextureTile> ContentAtlas::find(std::string_view identifier) const {
    const auto found=names_.find(identifier);return found==names_.end()?std::nullopt:std::optional(found->second);
}
TextureTile ContentAtlas::require(std::string_view identifier) const {
    const auto id=find(identifier);if(!id) throw std::invalid_argument("Unknown texture: "+std::string(identifier));return *id;
}
AtlasLayout ContentAtlas::layout() const {
    const auto count=static_cast<std::uint32_t>(tiles_.size());const auto columns=std::min(32u,count);
    return {tileSize,columns,(count+columns-1)/columns};
}
ImageRgba ContentAtlas::image() const {
    const auto atlas=layout();ImageRgba result{atlas.width(),atlas.height(),{}};
    result.pixels.resize(static_cast<std::size_t>(result.width)*result.height*4);
    for(std::size_t id=0;id<tiles_.size();++id) {
        const auto origin=atlas.tileOrigin(static_cast<TextureTile>(id));const auto& tile=tiles_[id];
        for(int y=-1;y<=static_cast<int>(tileSize);++y) for(int x=-1;x<=static_cast<int>(tileSize);++x) {
            const auto sx=static_cast<std::uint32_t>(std::clamp(x,0,static_cast<int>(tileSize)-1));
            const auto sy=static_cast<std::uint32_t>(std::clamp(y,0,static_cast<int>(tileSize)-1));
            const auto dx=static_cast<std::uint32_t>(static_cast<int>(origin.x)+x),dy=static_cast<std::uint32_t>(static_cast<int>(origin.y)+y);
            const auto source=(static_cast<std::size_t>(sy)*tileSize+sx)*4,target=(static_cast<std::size_t>(dy)*result.width+dx)*4;
            std::copy_n(tile.pixels.begin()+static_cast<std::ptrdiff_t>(source),4,result.pixels.begin()+static_cast<std::ptrdiff_t>(target));
        }
    }
    return result;
}
}
