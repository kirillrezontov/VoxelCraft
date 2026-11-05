#pragma once
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/renderer/atmosphere.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

#if VC_HAS_VOXELUI
namespace voxelui { inline namespace v1 { struct DrawList; } }
#endif
namespace vc {
class Window;
class Camera;
struct RenderMetrics { double drawMilliseconds{},uploadMilliseconds{};std::size_t drawCalls{}; };
struct UiRenderMetrics { double prepareMicroseconds{},gpuMilliseconds{};std::size_t drawCalls{},uploadBytes{},tessellations{},residentBytes{};bool gpuTimingAvailable{}; };
struct ValidationReport { bool enabled{}; std::uint32_t errors{}, warnings{};bool synchronizationEnabled{}; };
struct ResourceCount {
    std::size_t live{},peak{},created{};
    void acquire() { ++live;++created;if(live>peak) peak=live; }
    void release() { --live; }
};
// Host-owned Vulkan objects, not driver/SDL heap allocations or VRAM usage.
struct GpuResourceMetrics {
    ResourceCount buffers,images,imageViews,memory,samplers,descriptorPools,descriptorSets,stagingBuffers,atlasPages,pipelines,pipelineLayouts,descriptorLayouts;
};
class VulkanRenderer {
public:
    VulkanRenderer(Window& window, const ImageRgba& atlas, bool requireValidation = false,bool vsync=true);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    void upload(std::span<const Mesh> meshes);
    // Application/session boundary; same device/window, wait before retiring GPU data.
    void clearSession();
    void replaceAtlas(const ImageRgba& image);
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
#if VC_HAS_VOXELUI
    // Owning CPU snapshot. Recording uses this renderer's existing device/frame.
    void setUi(voxelui::DrawList list);
    void clearUi();
    UiRenderMetrics uiMetrics() const;
#endif
    void requestResize();
    // Returns false when minimized or while recreating the swapchain.
    bool draw(const Camera& camera, const std::filesystem::path& screenshot = {});
    std::size_t meshCount() const;
    std::size_t triangleCount() const;
    // Terrain submitted in the most recently rendered frame (excludes overlays).
    std::size_t drawnMeshCount() const;
    std::size_t drawnTriangleCount() const;
    // Release GPU resources before checking the report, including shutdown diagnostics.
    // Only validationReport(), resourceMetrics() and destruction are valid after shutdown().
    void shutdown();
    ValidationReport validationReport() const;
    GpuResourceMetrics resourceMetrics() const; // Also valid after shutdown.
private:
    struct ValidationState;
    std::unique_ptr<ValidationState> validation_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
