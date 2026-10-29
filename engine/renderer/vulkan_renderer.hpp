#pragma once
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/atmosphere.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

namespace vc {
class Window;
class Camera;
struct RenderMetrics { double drawMilliseconds{},uploadMilliseconds{};std::size_t drawCalls{}; };
struct ValidationReport { bool enabled{}; std::uint32_t errors{}, warnings{}; };
class VulkanRenderer {
public:
    VulkanRenderer(Window& window, const ImageRgba& atlas, bool requireValidation = false,bool vsync=true);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    void upload(std::span<const Mesh> meshes);
    // Replace meshes by origin. Empty index arrays remove previously uploaded meshes.
    void updateMeshes(std::span<const Mesh> meshes);
    void setSelection(std::optional<glm::i64vec3> block);
    // Pixel coordinates from top-left. HUD data is generated outside the renderer.
    void setHud(const Mesh& colored,const Mesh& textured,bool crosshairVisible=true,const Mesh& foreground={});
    void setProgressIndicator(std::optional<float> progress);
    void setAtmosphere(const Atmosphere& atmosphere);
    // Generic reusable opaque models and camera-relative animated instances.
    void setModels(std::span<const Mesh> models);
    void setInstances(std::span<const RenderInstance> instances);
    std::size_t drawnInstanceCount() const;
    RenderMetrics metrics() const;
    void requestResize();
    // Returns false when minimized or while recreating the swapchain.
    bool draw(const Camera& camera, const std::filesystem::path& screenshot = {});
    std::size_t meshCount() const;
    std::size_t triangleCount() const;
    // Terrain submitted in the most recently rendered frame (excludes overlays).
    std::size_t drawnMeshCount() const;
    std::size_t drawnTriangleCount() const;
    // Release GPU resources before checking the report, including shutdown diagnostics.
    // Only validationReport() and destruction are valid after shutdown().
    void shutdown();
    ValidationReport validationReport() const;
private:
    struct ValidationState;
    std::unique_ptr<ValidationState> validation_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
