#include "game/content/png.hpp"
#include "engine/core/paths.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace {
constexpr std::size_t decoderLimit=1024*1024;
thread_local std::size_t decoderBytes{};
struct alignas(std::max_align_t) Allocation { std::size_t size; };
void* pngAllocate(std::size_t size) {
    if(size>decoderLimit-decoderBytes) return nullptr;
    auto* allocation=static_cast<Allocation*>(std::malloc(sizeof(Allocation)+size));
    if(!allocation) return nullptr;
    allocation->size=size;decoderBytes+=size;return allocation+1;
}
void pngFree(void* pointer) {
    if(!pointer) return;
    auto* allocation=static_cast<Allocation*>(pointer)-1;decoderBytes-=allocation->size;std::free(allocation);
}
void* pngReallocate(void* pointer,std::size_t size) {
    if(!pointer) return pngAllocate(size);
    if(!size) { pngFree(pointer);return nullptr; }
    auto* allocation=static_cast<Allocation*>(pointer)-1;const auto oldSize=allocation->size;
    if(size>oldSize&&size-oldSize>decoderLimit-decoderBytes) return nullptr;
    auto* next=static_cast<Allocation*>(std::realloc(allocation,sizeof(Allocation)+size));
    if(!next) return nullptr;
    next->size=size;decoderBytes=decoderBytes-oldSize+size;return next+1;
}
}
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS 16
#define STBI_MALLOC(size) pngAllocate(static_cast<std::size_t>(size))
#define STBI_REALLOC_SIZED(pointer,oldSize,size) pngReallocate(pointer,static_cast<std::size_t>(size))
#define STBI_FREE(pointer) pngFree(pointer)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace vc {
ImageRgba loadPngTile(const std::filesystem::path& path) {
    const auto bytes=std::filesystem::file_size(path);
    if(bytes<8||bytes>131072) throw std::runtime_error("PNG tile must be at most 128 KiB: "+pathText(path));
    std::vector<std::uint8_t> source(static_cast<std::size_t>(bytes));std::ifstream file(path,std::ios::binary);
    if(!file||!file.read(reinterpret_cast<char*>(source.data()),static_cast<std::streamsize>(source.size())))
        throw std::runtime_error("Cannot read PNG tile: "+pathText(path));
    constexpr std::array<std::uint8_t,8> signature{137,80,78,71,13,10,26,10};
    if(!std::equal(signature.begin(),signature.end(),source.begin())) throw std::runtime_error("Expected PNG tile: "+pathText(path));
    int width{},height{},channels{};
    if(!stbi_info_from_memory(source.data(),static_cast<int>(source.size()),&width,&height,&channels)||width!=16||height!=16)
        throw std::runtime_error("PNG tiles must be 16x16 pixels: "+pathText(path));
    const std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(source.data(),static_cast<int>(source.size()),&width,&height,&channels,4),stbi_image_free);
    if(!pixels) throw std::runtime_error("Cannot decode PNG tile within supported limits: "+pathText(path));
    return {16,16,std::vector<std::uint8_t>(pixels.get(),pixels.get()+16*16*4)};
}
}
