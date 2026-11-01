#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vc {
struct ResourcePackInfo {
    std::string id,name,version;
    int priority{};
    std::filesystem::path directory,resources;
};
// Owner-thread resolver. No XML/SVG execution or parsing occurs here. Consumers
// validate decoded resources; semantic failures can explicitly use loadBuiltin.
class ResourceCatalog {
public:
    static constexpr std::size_t maxResourceBytes=16*1024*1024,maxPacks=32;
    explicit ResourceCatalog(std::filesystem::path builtinResources);
    static ResourcePackInfo inspect(const std::filesystem::path& directory);
    static bool validIdentifier(std::string_view identifier);
    // Parse and validate all mounts before replacing the active set.
    void activate(const std::vector<std::filesystem::path>& packs);
    const std::vector<ResourcePackInfo>& active() const { return active_; }
    std::uint64_t revision() const { return revision_; }
    std::optional<std::vector<std::uint8_t>> load(std::string_view id,std::size_t maximum=maxResourceBytes);
    std::optional<std::vector<std::uint8_t>> loadBuiltin(std::string_view id,std::size_t maximum=maxResourceBytes) const;
    std::optional<std::vector<std::uint8_t>> operator()(std::string_view id) { return load(id); }
    std::vector<std::string> takeDiagnostics();
private:
    std::filesystem::path builtin_;
    std::vector<ResourcePackInfo> active_;
    std::uint64_t revision_{1};
    std::vector<std::string> diagnostics_;
};
}
