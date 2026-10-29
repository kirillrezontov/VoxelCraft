#include "game/content/atlas.hpp"
#include <algorithm>
#include <array>

namespace vc {
AtlasLayout builtinAtlasLayout() { return {16, 23, 1}; }
ImageRgba builtinAtlas() {
    const auto layout = builtinAtlasLayout();
    ImageRgba image{layout.width(), layout.height(), {}};
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    for (TextureTile tile = 0; tile < layout.tileCount(); ++tile) {
        const auto origin = layout.tileOrigin(tile);
        const int size = static_cast<int>(layout.tileSize()), gutter = static_cast<int>(layout.gutter());
        for (int y = -gutter; y < size + gutter; ++y) for (int x = -gutter; x < size + gutter; ++x) {
            // Generate gutters from the nearest interior texel, including all four corners.
            const auto px = static_cast<std::uint32_t>(std::clamp(x, 0, size - 1));
            const auto py = static_cast<std::uint32_t>(std::clamp(y, 0, size - 1));
            auto hash = (px * 73856093u) ^ (py * 19349663u) ^ (static_cast<std::uint32_t>(tile) * 83492791u);
            hash ^= hash >> 13; hash *= 0x85ebca6bu; hash ^= hash >> 16;
            const int variation = static_cast<int>(hash % 33) - 16;
            std::array<int, 3> color{255, 255, 255};
            if (tile == tiles::stone) color = {123, 126, 132};
            if (tile == tiles::dirt || tile == tiles::grassSide) color = {127, 87, 51};
            if (tile == tiles::grassTop || (tile == tiles::grassSide && py < 2 + (px * 7 % 3))) color = {79, 140, 43};
            if(tile==tiles::logSide) color=px%5==0?std::array<int,3>{72,45,23}:std::array<int,3>{108,76,39};
            if(tile==tiles::logTop) {
                const auto ring=std::max(std::abs(static_cast<int>(px)-7),std::abs(static_cast<int>(py)-7));
                color=ring%3==0?std::array<int,3>{121,85,42}:std::array<int,3>{166,125,65};
            }
            if(tile==tiles::planks) color=py%5==0?std::array<int,3>{100,68,34}:std::array<int,3>{165,122,64};
            if(tile==tiles::leaves) color=hash%5==0?std::array<int,3>{43,83,24}:std::array<int,3>{63,116,35};
            if(tile==tiles::cobblestone) color=(px+py/4*2)%5==0||py%5==0?std::array<int,3>{70,74,78}:std::array<int,3>{121,125,129};
            if(tile==tiles::coalOre||tile==tiles::ironOre) {
                color={123,126,132};
                const auto spot=(px/3*13+py/3*7)%9;
                if(spot<3) color=tile==tiles::coalOre?std::array<int,3>{36,38,42}:std::array<int,3>{169,126,93};
            }
            if(tile==tiles::sand) color={205,186,127};
            if(tile==tiles::sandstone) color=py%5==0?std::array<int,3>{162,143,88}:std::array<int,3>{202,180,116};
            if(tile==tiles::snow) color={221,235,243};
            if(tile==tiles::water) color=(px+2*py)%7<2?std::array<int,3>{51,109,179}:std::array<int,3>{37,91,162};
            if(tile==tiles::lamp) color=px<2||px>13||py<2||py>13?std::array<int,3>{73,52,25}:std::array<int,3>{255,218,104};
            if (tile != tiles::white) for (auto& channel : color) channel = std::clamp(channel + variation, 0, 255);
            const auto imageX = static_cast<std::uint32_t>(static_cast<int>(origin.x) + x);
            const auto imageY = static_cast<std::uint32_t>(static_cast<int>(origin.y) + y);
            const auto offset = (static_cast<std::size_t>(imageY) * image.width + imageX) * 4;
            for (std::size_t channel = 0; channel < 3; ++channel) image.pixels[offset + channel] = static_cast<std::uint8_t>(color[channel]);
            image.pixels[offset + 3] = 255;
            if(tile>=tiles::itemStick) {
                const int ix=static_cast<int>(px),iy=static_cast<int>(py);std::array<int,3> ink{220,210,180};bool paint=false;
                if(tile==tiles::itemStick) { paint=ix>=3&&ix<=12&&std::abs(ix+iy-16)<=1;ink={153,104,58}; }
                if(tile==tiles::itemCoal) { paint=(ix-8)*(ix-8)*2+(iy-8)*(iy-8)<64;ink=ix+iy<14?std::array<int,3>{65,77,87}:std::array<int,3>{26,34,42}; }
                if(tile==tiles::itemMeat) { paint=(ix-8)*(ix-8)+(iy-8)*(iy-8)*2<63;ink=std::abs(ix+iy-16)<2?std::array<int,3>{245,196,169}:std::array<int,3>{195,75,77}; }
                if(tile==tiles::woodenPickaxe||tile==tiles::stonePickaxe) {
                    if(ix>=3&&iy>=4&&std::abs(ix+iy-16)<=1) { paint=true;ink={153,104,58}; }
                    if((iy>=2&&iy<=4&&ix>=2&&ix<=12)||(ix>=11&&ix<=13&&iy>=3&&iy<=8)) {
                        paint=true;ink=tile==tiles::stonePickaxe?std::array<int,3>{164,185,196}:std::array<int,3>{205,154,88};
                    }
                }
                if(tile==tiles::missingItem) { paint=ix>=2&&ix<=13&&iy>=2&&iy<=13;ink=(ix/3+iy/3)%2?std::array<int,3>{181,84,190}:std::array<int,3>{58,43,69}; }
                for(std::size_t c=0;c<3;++c) image.pixels[offset+c]=static_cast<std::uint8_t>(ink[c]);
                image.pixels[offset+3]=paint?255:0;
            }
        }
    }
    image.validate();
    return image;
}
}
