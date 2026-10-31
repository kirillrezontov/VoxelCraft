#pragma once
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/texture.hpp"
#include <memory>
#include <span>
#include <string_view>

namespace vc::ui {
struct Rect {
    float x{},y{},width{},height{};
    bool contains(float px,float py) const { return px>=x&&py>=y&&px<x+width&&py<y+height; }
};
std::span<const unsigned char> bundledFont(bool monospace);
struct Glyph { glm::vec2 uvMin{},uvMax{},offset{},size{};float advance{}; };
class FontAtlas {
public:
    FontAtlas();
    ~FontAtlas();
    FontAtlas(const FontAtlas&)=delete;
    FontAtlas& operator=(const FontAtlas&)=delete;
    Glyph glyph(char32_t codepoint,float pixels,bool monospace=false);
    glm::vec2 measure(std::string_view text,float pixels,bool monospace=false);
    void append(Mesh& mesh,std::string_view text,float x,float y,float pixels,glm::vec3 color={1,1,1},
        Rect clip={-1e6f,-1e6f,2e6f,2e6f},bool monospace=false,float opacity=1);
    const ImageRgba& image() const;
    std::uint64_t revision() const;
    std::size_t glyphCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// UI-thread resource cache shared by HUDs. Font bytes are embedded once, with no
// dependency on OS font installation, source checkout or current directory.
FontAtlas& fonts();
}
