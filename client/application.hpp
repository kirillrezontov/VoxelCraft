#pragma once
#include "client/options.hpp"
#include "game/settings.hpp"
#include "client/screen_manager.hpp"
#include "engine/resources/catalog.hpp"
#include <cstdint>
#include <memory>

namespace vc {
struct Input;
struct ValidationReport;
class VulkanRenderer;
namespace client {
class ClientApplication {
public:
    explicit ClientApplication(Options options);
    ~ClientApplication();
    ClientApplication(const ClientApplication&)=delete;
    ClientApplication& operator=(const ClientApplication&)=delete;
    int run();
    void frame(Input input);
    void startLocal(Options options);
    void startRemote(Options options);
    void requestLeave();
    void requestQuit();
    bool hasSession() const;
    bool running() const;
    ApplicationScreen screen() const;
    const GameSettings& settings() const;
    const ResourceCatalog& resources() const;
    void activateResourcePacks(const std::vector<std::filesystem::path>& packs);
    std::uint32_t windowId() const;
    VulkanRenderer& renderer();
    void shutdown();
    ValidationReport validationReport() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
}
