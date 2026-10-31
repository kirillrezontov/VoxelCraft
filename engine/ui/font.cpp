#include "engine/ui/font.hpp"
#include "engine/core/utf8.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>

namespace vc::ui {
struct FontAtlas::Impl {
    std::array<stbtt_fontinfo,2> faces{};
    ImageRgba image{2048,2048,std::vector<std::uint8_t>(2048*2048*4)};
    std::map<std::tuple<char32_t,unsigned,bool>,Glyph> glyphs;
    unsigned x{1},y{1},rowHeight{};
    std::uint64_t revision{1};
    Impl() {
        for(std::size_t i=0;i<image.pixels.size();i+=4) image.pixels[i]=image.pixels[i+1]=image.pixels[i+2]=255;
        for(unsigned i=0;i<2;++i) {
            const auto bytes=bundledFont(i!=0);
            if(bytes.empty()||!stbtt_InitFont(&faces[i],bytes.data(),0)) throw std::runtime_error("Bundled TrueType font initialization failed");
        }
    }
};
FontAtlas::FontAtlas():impl_(std::make_unique<Impl>()) {}
FontAtlas::~FontAtlas()=default;
Glyph FontAtlas::glyph(char32_t codepoint,float pixels,bool mono) {
    if(!std::isfinite(pixels)||pixels<6||pixels>128) throw std::invalid_argument("Font size must be 6..128 pixels");
    const auto size=static_cast<unsigned>(std::clamp(std::round(pixels/2)*2,8.0f,128.0f));
    const auto key=std::tuple(codepoint,size,mono);
    if(const auto found=impl_->glyphs.find(key);found!=impl_->glyphs.end()) return found->second;
    auto& face=impl_->faces[mono?1:0];int index=stbtt_FindGlyphIndex(&face,static_cast<int>(codepoint));
    if(!index&&codepoint!=U'\ufffd') return glyph(U'\ufffd',pixels,mono);
    const float scale=stbtt_ScaleForPixelHeight(&face,static_cast<float>(size));
    int advance{},bearing{},x0{},y0{},x1{},y1{};stbtt_GetGlyphHMetrics(&face,index,&advance,&bearing);
    stbtt_GetGlyphBitmapBox(&face,index,scale,scale,&x0,&y0,&x1,&y1);
    const auto w=static_cast<unsigned>(x1-x0),h=static_cast<unsigned>(y1-y0);
    if(impl_->x+w+1>=impl_->image.width) { impl_->x=1;impl_->y+=impl_->rowHeight+2;impl_->rowHeight=0; }
    if(impl_->glyphs.size()>=8192||impl_->y+h+1>=impl_->image.height) {
        // Bound cache memory. Existing sizes/glyphs remain usable after saturation.
        const auto fallback=impl_->glyphs.find(std::tuple(U'?',18u,false));
        if(fallback!=impl_->glyphs.end()) {
            auto result=fallback->second;const float ratio=static_cast<float>(size)/18;
            result.offset*=ratio;result.size*=ratio;result.advance*=ratio;return result;
        }
        throw std::runtime_error("Glyph atlas is full");
    }
    std::vector<unsigned char> bitmap(static_cast<std::size_t>(w)*h);
    if(w&&h) stbtt_MakeGlyphBitmap(&face,bitmap.data(),static_cast<int>(w),static_cast<int>(h),static_cast<int>(w),scale,scale,index);
    for(unsigned row=0;row<h;++row) for(unsigned col=0;col<w;++col) {
        const auto target=(static_cast<std::size_t>(impl_->y+row)*impl_->image.width+impl_->x+col)*4;
        impl_->image.pixels[target]=impl_->image.pixels[target+1]=impl_->image.pixels[target+2]=255;
        impl_->image.pixels[target+3]=bitmap[static_cast<std::size_t>(row)*w+col];
    }
    int ascent{},descent{},gap{};stbtt_GetFontVMetrics(&face,&ascent,&descent,&gap);
    Glyph result{{static_cast<float>(impl_->x)/2048,static_cast<float>(impl_->y)/2048},
        {static_cast<float>(impl_->x+w)/2048,static_cast<float>(impl_->y+h)/2048},
        {static_cast<float>(x0),static_cast<float>(y0)+static_cast<float>(ascent)*scale},
        {w,h},static_cast<float>(advance)*scale};
    impl_->x+=w+2;impl_->rowHeight=std::max(impl_->rowHeight,h);impl_->glyphs.emplace(key,result);++impl_->revision;return result;
}
glm::vec2 FontAtlas::measure(std::string_view text,float pixels,bool mono) {
    float width=0,line=0;unsigned lines=1;
    for(const auto point:utf8::decode(text)) {
        if(point.value==U'\n') { width=std::max(width,line);line=0;++lines; }
        else line+=glyph(point.value,pixels,mono).advance*(pixels/(std::round(std::clamp(pixels,8.0f,128.0f)/2)*2));
    }
    return {std::max(width,line),pixels*1.25f*static_cast<float>(lines)};
}
void FontAtlas::append(Mesh& mesh,std::string_view text,float x,float y,float pixels,glm::vec3 color,Rect clip,bool mono,float opacity) {
    const float start=x;
    for(const auto point:utf8::decode(text)) {
        if(point.value==U'\n') { x=start;y+=pixels*1.25f;continue; }
        const auto g=glyph(point.value,pixels,mono);const auto factor=pixels/(std::round(std::clamp(pixels,8.0f,128.0f)/2)*2);
        const auto low=glm::vec2(x,y)+g.offset*factor,high=low+g.size*factor;
        const auto a=glm::max(low,glm::vec2(clip.x,clip.y)),b=glm::min(high,glm::vec2(clip.x+clip.width,clip.y+clip.height));
        if(b.x>a.x&&b.y>a.y&&g.size.x>0&&g.size.y>0) {
            const auto uv0=glm::mix(g.uvMin,g.uvMax,(a-low)/(high-low)),uv1=glm::mix(g.uvMin,g.uvMax,(b-low)/(high-low));
            const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
            const std::array<glm::vec2,4> positions{{a,{b.x,a.y},b,{a.x,b.y}}};
            const std::array<glm::vec2,4> uvs{{uv0,{uv1.x,uv0.y},uv1,{uv0.x,uv1.y}}};
            for(std::size_t i=0;i<4;++i) {
                Vertex vertex{{positions[i],0},color,uvs[i]};vertex.opacity=opacity;vertex.tileBounds.z=-1;mesh.vertices.push_back(vertex);
            }
            for(const auto index:{0u,1u,2u,2u,3u,0u}) mesh.indices.push_back(base+index);
        }
        x+=g.advance*factor;
    }
}
const ImageRgba& FontAtlas::image() const { return impl_->image; }
std::uint64_t FontAtlas::revision() const { return impl_->revision; }
std::size_t FontAtlas::glyphCount() const { return impl_->glyphs.size(); }
FontAtlas& fonts() {
    static FontAtlas atlas;
    static const bool warmed=[&] {
        (void)atlas.glyph(U'?',18);
        for(const bool mono:{false,true}) for(const float size:{14.0f,18.0f,20.0f,24.0f,28.0f,32.0f,36.0f,40.0f}) {
            for(char32_t c=32;c<127;++c) (void)atlas.glyph(c,size,mono);
            for(char32_t c=0x400;c<0x460;++c) (void)atlas.glyph(c,size,mono);
        }
        return true;
    }();
    (void)warmed;return atlas;
}
}
