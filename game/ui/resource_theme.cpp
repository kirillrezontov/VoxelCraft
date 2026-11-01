#include "game/ui/resource_theme.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <stdexcept>

namespace vc {
namespace {
ui::Theme parse(const std::vector<std::uint8_t>& bytes) {
    const auto data=nlohmann::json::parse(bytes.begin(),bytes.end(),[](int depth,auto,auto&){if(depth>4) throw std::runtime_error("Theme JSON depth limit");return true;});
    if(!data.is_object()) throw std::runtime_error("Theme must be an object");
    ui::Theme result;
    for(const auto& [key,value]:data.items()) {
        glm::vec4* target=key=="panel"?&result.panel:key=="surface"?&result.surface:key=="hover"?&result.hover:key=="border"?&result.border:
            key=="accent"?&result.accent:key=="text"?&result.text:key=="muted"?&result.muted:key=="error"?&result.error:key=="warning"?&result.warning:key=="success"?&result.success:nullptr;
        if(!target||!value.is_array()||value.size()!=4) throw std::runtime_error("Theme requires known four-channel colors");
        for(int i=0;i<4;++i) { const auto channel=value.at(static_cast<std::size_t>(i)).get<float>();if(!std::isfinite(channel)||channel<0||channel>1) throw std::runtime_error("Theme color is outside 0..1");(*target)[i]=channel; }
    }
    return result;
}
}
ui::Theme loadResourceTheme(ResourceCatalog& resources,std::vector<std::string>& diagnostics) {
    constexpr std::string_view id="voxelcraft:legacy-theme.json";
    try { if(const auto bytes=resources.load(id,16384)) return parse(*bytes); }
    catch(const std::exception& error) { diagnostics.push_back(std::string("Invalid resource theme; using builtin: ")+error.what()); }
    try { if(const auto bytes=resources.loadBuiltin(id,16384)) return parse(*bytes); }
    catch(const std::exception& error) { diagnostics.push_back(std::string("Invalid builtin resource theme; using compiled defaults: ")+error.what()); }
    return {};
}
}
