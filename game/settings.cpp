#include "engine/core/paths.hpp"
#include "game/settings.hpp"
#include "engine/core/binary.hpp"
#include "engine/platform/file_lock.hpp"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace vc {
namespace {
GameSettings read(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    if(size>1024) throw std::runtime_error("Settings file exceeds size limit");
    binary::Bytes bytes(static_cast<std::size_t>(size));std::ifstream file(path,std::ios::binary);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read settings: "+vc::pathText(path));
    auto in=binary::Reader::checked(bytes);in.magic("VCSETT01");
    if(in.u32()!=1) throw std::runtime_error("Unsupported settings version");
    GameSettings settings;settings.fieldOfView=in.f64();settings.mouseSensitivity=in.f64();
    const auto inverted=in.u32();if(inverted>1) throw std::runtime_error("Invalid invert-Y setting");
    settings.invertY=inverted==1;in.finish();settings.validate();return settings;
}
std::filesystem::path suffix(std::filesystem::path path,const char* ending) { path+=ending;return path; }
}
void GameSettings::validate() const {
    if(!std::isfinite(fieldOfView)||fieldOfView<50||fieldOfView>110)
        throw std::invalid_argument("Field of view must be 50..110 degrees");
    if(!std::isfinite(mouseSensitivity)||mouseSensitivity<0.0005||mouseSensitivity>0.01)
        throw std::invalid_argument("Mouse sensitivity must be 0.0005..0.01 radians/pixel");
}
std::optional<LoadedSettings> loadSettings(const std::filesystem::path& path) {
    const auto backup=suffix(path,".bak");
    if(std::filesystem::exists(path)) {
        try { return LoadedSettings{read(path),false}; }
        catch(...) { if(!std::filesystem::exists(backup)) throw; }
    }
    if(std::filesystem::exists(backup)) return LoadedSettings{read(backup),true};
    return std::nullopt;
}
void saveSettings(const std::filesystem::path& path,const GameSettings& settings) {
    settings.validate();
    if(path.empty()) throw std::invalid_argument("Settings path must not be empty");
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    FileLock lock(suffix(path,".lock"));
    binary::Writer out;out.raw("VCSETT01");out.u32(1);out.f64(settings.fieldOfView);out.f64(settings.mouseSensitivity);
    out.u32(settings.invertY?1:0);out.seal();
    const auto temporary=suffix(path,".tmp"),backup=suffix(path,".bak");
    try {
        std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(out.bytes.data()),static_cast<std::streamsize>(out.bytes.size()));file.close();
        if(!file) throw std::runtime_error("Cannot write settings: "+vc::pathText(path));
        if(std::filesystem::exists(path)) {
            std::filesystem::remove(backup);std::filesystem::rename(path,backup);
        }
        try { std::filesystem::rename(temporary,path); }
        catch(...) {
            if(!std::filesystem::exists(path)&&std::filesystem::exists(backup)) std::filesystem::rename(backup,path);
            throw;
        }
    } catch(...) { std::error_code ignored;std::filesystem::remove(temporary,ignored);throw; }
}
}
