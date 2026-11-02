#if VC_HAS_VOXELUI
#include <voxelui/software.hpp>
#endif
#include "engine/core/paths.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/renderer/overlay.hpp"
#include "engine/renderer/frustum.hpp"
#include "engine/core/log.hpp"
#include "engine/ui/font.hpp"
#include "engine/platform/window.hpp"
#include "game/player/camera.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace vc {
namespace {
template<class T> T vkStructure(VkStructureType type) {
    T value{}; value.sType = type; return value;
}
void vkCheck(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " failed (VkResult " + std::to_string(result) + ")");
}
// Move-only ownership also cleans up partially initialized buffers on exceptions.
struct Buffer {
    VkDevice device{};
    VkBuffer handle{};
    VkDeviceMemory memory{};
    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept { swap(other); }
    Buffer& operator=(Buffer&& other) noexcept { Buffer temporary(std::move(other)); swap(temporary); return *this; }
    ~Buffer() {
        if (handle) vkDestroyBuffer(device, handle, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
    }
    void swap(Buffer& other) noexcept {
        std::swap(device, other.device); std::swap(handle, other.handle); std::swap(memory, other.memory);
    }
};
struct GpuMesh {
    glm::i64vec3 origin{};MeshBounds bounds;Buffer vertices,indices,translucentIndices;
    std::uint32_t indexCount{},translucentCount{};
};
struct PushConstants { glm::mat4 viewProjection;glm::vec4 offset,sun,sky,fog; };
static_assert(sizeof(PushConstants)==128);
struct InstanceData { glm::vec3 offset{};float yaw{};glm::vec2 light{1,0};float phase{},padding{}; };
static_assert(sizeof(InstanceData)==32);
struct Texture {
    VkDevice device{};
    VkImage image{};
    VkDeviceMemory memory{};
    VkImageView view{};
    VkSampler sampler{};
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    ~Texture() {
        if (sampler) vkDestroySampler(device, sampler, nullptr);
        if (view) vkDestroyImageView(device, view, nullptr);
        if (image) vkDestroyImage(device, image, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
    }
};
}

struct VulkanRenderer::ValidationState {
    bool enabled{};
    std::atomic<std::uint32_t> errors{}, warnings{};
    static VKAPI_ATTR VkBool32 VKAPI_CALL callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) noexcept {
        auto& state = *static_cast<ValidationState*>(user);
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++state.errors;
        else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ++state.warnings;
        try { log(std::string("Vulkan validation: ") + data->pMessage); } catch (...) {}
        return VK_FALSE;
    }
};

struct VulkanRenderer::Impl {
#if VC_HAS_VOXELUI
    std::optional<voxelui::DrawList> uiList;
    voxelui::SoftwareRenderer uiRaster;
    std::unique_ptr<Texture> uiTexture;
    GpuMesh uiQuad;
    VkDescriptorSet uiDescriptor{};
    UiRenderMetrics uiMetrics;
    bool uiDirty{};

    // Called only after the host frame fence. Upload uses the existing graphics
    // queue and command buffer; no second context or GPU work in setUi().
    void prepareUi() {
        uiMetrics = {};
        if (!uiList) {
            uiTexture.reset();uiQuad={};
            return;
        }
        if (uiDirty) {
            const auto begin = std::chrono::steady_clock::now();
            uiRaster.render(*uiList, {0,0,0,0});
            ImageRgba image{static_cast<std::uint32_t>(uiRaster.width()),static_cast<std::uint32_t>(uiRaster.height()),uiRaster.pixels()};
            auto next = createTexture(image);
            Mesh quad;
            const float w=static_cast<float>(image.width),h=static_cast<float>(image.height);
            quad.vertices={{{0,0,0},{1,1,1},{0,0}},{{w,0,0},{1,1,1},{1,0}},{{w,h,0},{1,1,1},{1,1}},{{0,h,0},{1,1,1},{0,1}}};
            quad.indices={0,1,2,2,3,0};
            auto mesh=createMesh(quad);
            updateTextureDescriptor(0,*next,uiDescriptor);
            updateTextureDescriptor(1,*next,uiDescriptor);
            uiTexture=std::move(next);uiQuad=std::move(mesh);uiDirty=false;
            uiMetrics.uploadBytes=image.pixels.size();
            uiMetrics.prepareMicroseconds=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
        }
        uiMetrics.residentBytes=static_cast<std::size_t>(uiRaster.width())*static_cast<std::size_t>(uiRaster.height())*4;
    }
#endif
    Window& window;
    ValidationState& validation;
    bool requireValidation{},vsync{true};
    RenderMetrics metrics;
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debug{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    std::uint32_t graphicsFamily{}, presentFamily{};
    VkQueue graphicsQueue{}, presentQueue{};
    VkCommandPool commandPool{};
    VkCommandBuffer command{};
    VkFence frameFence{};
    VkSemaphore acquired{};
    VkSwapchainKHR swapchain{};
    VkFormat colorFormat{}, depthFormat{};
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkSemaphore> presented;
    VkImage depthImage{};
    VkDeviceMemory depthMemory{};
    VkImageView depthView{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline pipeline{};
    VkPipeline hudPipeline{};
    VkPipeline translucentPipeline{};
    std::unique_ptr<Texture> atlasTexture,fontTexture;
    std::uint64_t fontRevision{};
    VkDescriptorSetLayout descriptorLayout{};
    VkDescriptorPool descriptorPool{};
    VkDescriptorSet descriptor{};
    std::vector<GpuMesh> meshes;
    std::vector<GpuMesh> models;
    std::vector<RenderInstance> instances;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> instanceRanges;
    Buffer instanceBuffer;
    std::size_t instanceCapacity{},drawnInstances{};
    GpuMesh outline, crosshair, hudColors, hudTextures,hudForeground,progressBack,progressFill;
    bool crosshairVisible{true};
    std::optional<float> progress;
    Atmosphere atmosphere;
    std::optional<glm::i64vec3> selection;
    bool dirty{true}, canCapture{};
    std::size_t triangles{};
    std::size_t drawnMeshes{}, drawnTriangles{};

    Impl(Window& w, ValidationState& state, bool require) : window(w), validation(state), requireValidation(require) {}
    ~Impl() {
        if (device) {
            // Best effort during shutdown, including after device loss.
            const auto result = vkDeviceWaitIdle(device);
            if (result != VK_SUCCESS) log("Device wait during shutdown returned " + std::to_string(result));
#if VC_HAS_VOXELUI
            uiTexture.reset();uiQuad={};uiList.reset();
#endif
            meshes.clear();
            models.clear();instanceBuffer={};
            outline = {}; crosshair = {};hudColors={};hudTextures={};hudForeground={};progressBack={};progressFill={};
            destroySwapchain();
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            if (descriptorLayout) vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
            atlasTexture.reset();fontTexture.reset();
            if (acquired) vkDestroySemaphore(device, acquired, nullptr);
            if (frameFence) vkDestroyFence(device, frameFence, nullptr);
            if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (debug) {
            const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(instance, debug, nullptr);
        }
        if (instance) vkDestroyInstance(instance, nullptr);
    }

    void initialize(const ImageRgba& atlas) {
        atlas.validate();
        createInstance();
        surface = window.createSurface(instance);
        selectDevice();
        createDevice();
        auto pool = vkStructure<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = graphicsFamily;
        vkCheck(vkCreateCommandPool(device, &pool, nullptr, &commandPool), "Create command pool");
        auto alloc = vkStructure<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        alloc.commandPool = commandPool; alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount = 1;
        vkCheck(vkAllocateCommandBuffers(device, &alloc, &command), "Allocate command buffer");
        auto fence = vkStructure<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO); fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCheck(vkCreateFence(device, &fence, nullptr, &frameFence), "Create frame fence");
        auto semaphore = vkStructure<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        vkCheck(vkCreateSemaphore(device, &semaphore, nullptr, &acquired), "Create acquire semaphore");
        atlasTexture=createTexture(atlas);
        fontTexture=createTexture(ui::fonts().image(),true);fontRevision=ui::fonts().revision();
        createDescriptors();
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
        auto layout = vkStructure<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
        layout.setLayoutCount = 1; layout.pSetLayouts = &descriptorLayout;
        vkCheck(vkCreatePipelineLayout(device, &layout, nullptr, &pipelineLayout), "Create pipeline layout");
        recreateSwapchain();
        outline = createMesh(selectionOutline()); crosshair = createMesh(crosshairMesh());
        progressBack=createMesh(progressBarMesh({0.04f,0.04f,0.04f}));progressFill=createMesh(progressBarMesh({1,0.72f,0.08f}));
        const InstanceData identity;
        instanceBuffer=createBuffer(sizeof(identity),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,&identity);instanceCapacity=1;
    }

    void createInstance() {
        auto extensions = window.instanceExtensions();
        std::vector<const char*> layers;
        auto debugInfo = vkStructure<VkDebugUtilsMessengerCreateInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = ValidationState::callback;
        debugInfo.pUserData = &validation;
        bool wantValidation = requireValidation;
#ifndef NDEBUG
        wantValidation = true;
#endif
        if (wantValidation) {
            std::uint32_t count{};
            vkCheck(vkEnumerateInstanceLayerProperties(&count, nullptr), "Enumerate validation layers");
            std::vector<VkLayerProperties> available(count);
            vkCheck(vkEnumerateInstanceLayerProperties(&count, available.data()), "Enumerate validation layers");
            bool hasLayer = std::ranges::any_of(available, [](const auto& layer) { return std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0; });
            vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr), "Enumerate instance extensions");
            std::vector<VkExtensionProperties> availableExtensions(count);
            vkCheck(vkEnumerateInstanceExtensionProperties(nullptr, &count, availableExtensions.data()), "Enumerate instance extensions");
            const bool hasDebug = std::ranges::any_of(availableExtensions, [](const auto& extension) { return std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0; });
            if (hasLayer && hasDebug) {
                layers.push_back("VK_LAYER_KHRONOS_validation");
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                validation.enabled = true;
                log("Khronos validation enabled");
            } else {
                if (requireValidation) throw std::runtime_error("--require-validation needs VK_LAYER_KHRONOS_validation and VK_EXT_debug_utils");
                log("Khronos validation unavailable; install validation layers for API diagnostics");
            }
        }
        auto app = vkStructure<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        app.pApplicationName = "VoxelCraft";
        app.applicationVersion = VK_MAKE_API_VERSION(0, VC_VERSION_MAJOR, VC_VERSION_MINOR, VC_VERSION_PATCH);
        app.pEngineName = "VoxelCraft"; app.apiVersion = VK_API_VERSION_1_3;
        auto info = vkStructure<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        info.pApplicationInfo = &app;
        info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()); info.ppEnabledExtensionNames = extensions.data();
        info.enabledLayerCount = static_cast<std::uint32_t>(layers.size()); info.ppEnabledLayerNames = layers.data();
        if (!layers.empty()) info.pNext = &debugInfo;
        vkCheck(vkCreateInstance(&info, nullptr, &instance), "Create Vulkan 1.3 instance (check graphics driver)");
        if (!layers.empty()) {
            const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
            if (!create) throw std::runtime_error("VK_EXT_debug_utils entry point unavailable");
            vkCheck(create(instance, &debugInfo, nullptr, &debug), "Create validation messenger");
        }
    }

    void selectDevice() {
        std::uint32_t count{};
        vkCheck(vkEnumeratePhysicalDevices(instance, &count, nullptr), "Enumerate GPUs");
        std::vector<VkPhysicalDevice> devices(count);
        vkCheck(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "Enumerate GPUs");
        int bestScore = -1;
        for (auto candidate : devices) {
            VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(candidate, &props);
            if (props.apiVersion < VK_API_VERSION_1_3) continue;
            if(props.limits.maxPushConstantsSize<sizeof(PushConstants)) continue;
            auto features13 = vkStructure<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
            auto features = vkStructure<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2); features.pNext = &features13;
            vkGetPhysicalDeviceFeatures2(candidate, &features);
            if (!features13.dynamicRendering) continue;
            std::uint32_t extensionCount{};
            vkCheck(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr), "Enumerate device extensions");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkCheck(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, extensions.data()), "Enumerate device extensions");
            if (!std::ranges::any_of(extensions, [](const auto& e) { return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0; })) continue;
            std::uint32_t queueCount{}; vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueCount, nullptr);
            std::vector<VkQueueFamilyProperties> queues(queueCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueCount, queues.data());
            std::optional<std::uint32_t> graphics, present;
            for (std::uint32_t i = 0; i < queueCount; ++i) {
                VkBool32 supported{};
                vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &supported), "Query presentation support");
                if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = i;
                if (supported) present = i;
                if (supported && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { graphics = present = i; break; }
            }
            if (!graphics || !present) continue;
            std::uint32_t formatCount{}, modeCount{};
            vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &formatCount, nullptr), "Query surface formats");
            vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &modeCount, nullptr), "Query present modes");
            if (!formatCount || !modeCount) continue;
            VkFormat depth = VK_FORMAT_UNDEFINED;
            for (auto format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
                VkFormatProperties formatProps{}; vkGetPhysicalDeviceFormatProperties(candidate, format, &formatProps);
                if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) { depth = format; break; }
            }
            if (depth == VK_FORMAT_UNDEFINED) continue;
            const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 100 : 10;
            if (score > bestScore) {
                physical = candidate; graphicsFamily = *graphics; presentFamily = *present; depthFormat = depth; bestScore = score;
            }
        }
        if (!physical) throw std::runtime_error("No suitable Vulkan 1.3 GPU: need dynamicRendering, swapchain, graphics/present queues and depth attachment support");
        VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(physical, &props);
        log(std::string("Vulkan GPU: ") + props.deviceName);
    }

    void createDevice() {
        const float priority = 1;
        std::vector<VkDeviceQueueCreateInfo> queues;
        for (auto family : {graphicsFamily, presentFamily}) {
            if (!queues.empty() && family == graphicsFamily) continue;
            auto queue = vkStructure<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
            queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority; queues.push_back(queue);
        }
        auto features = vkStructure<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES); features.dynamicRendering = VK_TRUE;
        const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        auto info = vkStructure<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        info.pNext = &features; info.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size()); info.pQueueCreateInfos = queues.data();
        info.enabledExtensionCount = 1; info.ppEnabledExtensionNames = &extension;
        vkCheck(vkCreateDevice(physical, &info, nullptr, &device), "Create logical device");
        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);
    }

    std::uint32_t memoryType(std::uint32_t bits, VkMemoryPropertyFlags required) const {
        VkPhysicalDeviceMemoryProperties properties{}; vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        for (std::uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & required) == required) return i;
        throw std::runtime_error("No GPU memory type supports the required flags");
    }
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data = nullptr) const {
        Buffer result; result.device = device;
        auto info = vkStructure<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO); info.size = size; info.usage = usage; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateBuffer(device, &info, nullptr, &result.handle), "Create mesh/readback buffer");
        VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device, result.handle, &requirements);
        auto alloc = vkStructure<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkCheck(vkAllocateMemory(device, &alloc, nullptr, &result.memory), "Allocate buffer memory");
        vkCheck(vkBindBufferMemory(device, result.handle, result.memory, 0), "Bind buffer memory");
        if (data) {
            void* mapped{}; vkCheck(vkMapMemory(device, result.memory, 0, size, 0, &mapped), "Map mesh memory");
            std::memcpy(mapped, data, static_cast<std::size_t>(size)); vkUnmapMemory(device, result.memory);
        }
        return result;
    }
    GpuMesh createMesh(const Mesh& mesh) const {
        GpuMesh gpu; gpu.origin = mesh.origin;
        if (mesh.indices.empty()&&mesh.translucentIndices.empty()) return gpu;
        for(const auto* indices:{&mesh.indices,&mesh.translucentIndices}) {
            if(indices->size()>UINT32_MAX||indices->size()%3!=0) throw std::invalid_argument("Invalid mesh index count");
            for(const auto index:*indices) if(index>=mesh.vertices.size()) throw std::invalid_argument("Mesh index outside vertex buffer");
        }
        gpu.bounds.min = gpu.bounds.max = glm::dvec3(mesh.vertices.front().position);
        for (const auto& vertex : mesh.vertices) {
            for (int axis = 0; axis < 3; ++axis)
                if (!std::isfinite(vertex.position[axis])) throw std::invalid_argument("Non-finite mesh vertex");
            gpu.bounds.min = glm::min(gpu.bounds.min, glm::dvec3(vertex.position));
            gpu.bounds.max = glm::max(gpu.bounds.max, glm::dvec3(vertex.position));
        }
        gpu.indexCount = static_cast<std::uint32_t>(mesh.indices.size());
        gpu.translucentCount=static_cast<std::uint32_t>(mesh.translucentIndices.size());
        gpu.vertices = createBuffer(mesh.vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertices.data());
        if(gpu.indexCount) gpu.indices = createBuffer(mesh.indices.size() * sizeof(std::uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indices.data());
        if(gpu.translucentCount) gpu.translucentIndices=createBuffer(mesh.translucentIndices.size()*sizeof(std::uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,mesh.translucentIndices.data());
        return gpu;
    }
    void drawMesh(const GpuMesh& mesh,const PushConstants& push,bool translucent=false,std::uint32_t count=1,std::uint32_t first=0) {
        vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
        const std::array<VkBuffer,2> buffers{mesh.vertices.handle,instanceBuffer.handle};
        constexpr std::array<VkDeviceSize,2> offsets{0,0};
        vkCmdBindVertexBuffers(command,0,static_cast<std::uint32_t>(buffers.size()),buffers.data(),offsets.data());
        vkCmdBindIndexBuffer(command,translucent?mesh.translucentIndices.handle:mesh.indices.handle,0,VK_INDEX_TYPE_UINT32);
        ++metrics.drawCalls;
        vkCmdDrawIndexed(command,translucent?mesh.translucentCount:mesh.indexCount,count,0,0,first);
    }
    VkImageView createView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const {
        auto info = vkStructure<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        info.image = image; info.viewType = VK_IMAGE_VIEW_TYPE_2D; info.format = format;
        info.subresourceRange = {aspect, 0, 1, 0, 1};
        VkImageView view{}; vkCheck(vkCreateImageView(device, &info, nullptr, &view), "Create image view"); return view;
    }

    std::unique_ptr<Texture> createTexture(const ImageRgba& image,bool linear=false) {
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical, &properties);
        if (image.width > properties.limits.maxImageDimension2D || image.height > properties.limits.maxImageDimension2D)
            throw std::runtime_error("Texture atlas exceeds GPU image dimensions");
        VkFormatProperties format{}; vkGetPhysicalDeviceFormatProperties(physical, VK_FORMAT_R8G8B8A8_SRGB, &format);
        const auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if ((format.optimalTilingFeatures & required) != required) throw std::runtime_error("GPU cannot sample/upload RGBA8 sRGB textures");
        auto texture = std::make_unique<Texture>(); texture->device = device;
        auto info = vkStructure<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        info.imageType = VK_IMAGE_TYPE_2D; info.format = VK_FORMAT_R8G8B8A8_SRGB;
        info.extent = {image.width, image.height, 1}; info.mipLevels = 1; info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT; info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateImage(device, &info, nullptr, &texture->image), "Create texture atlas image");
        VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device, texture->image, &requirements);
        auto alloc = vkStructure<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkCheck(vkAllocateMemory(device, &alloc, nullptr, &texture->memory), "Allocate texture atlas memory");
        vkCheck(vkBindImageMemory(device, texture->image, texture->memory, 0), "Bind texture atlas memory");
        auto staging = createBuffer(image.pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, image.pixels.data());
        auto begin = vkStructure<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(vkResetCommandBuffer(command,0),"Reset texture upload commands");
        vkCheck(vkBeginCommandBuffer(command, &begin), "Begin atlas upload");
        barrier(texture->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = info.extent;
        vkCmdCopyBufferToImage(command, staging.handle, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier(texture->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        vkCheck(vkEndCommandBuffer(command), "End atlas upload");
        auto submit = vkStructure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        vkCheck(vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE), "Submit atlas upload");
        vkCheck(vkQueueWaitIdle(graphicsQueue), "Wait for atlas upload");
        texture->view = createView(texture->image, info.format, VK_IMAGE_ASPECT_COLOR_BIT);
        auto sampler = vkStructure<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        sampler.magFilter = sampler.minFilter = linear?VK_FILTER_LINEAR:VK_FILTER_NEAREST; sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCheck(vkCreateSampler(device, &sampler, nullptr, &texture->sampler), "Create atlas sampler");
        return texture;
    }

    void createDescriptors() {
        const std::array<VkDescriptorSetLayoutBinding,2> bindings{{
            {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}}};
        auto layout=vkStructure<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        layout.bindingCount=2;layout.pBindings=bindings.data();
        vkCheck(vkCreateDescriptorSetLayout(device,&layout,nullptr,&descriptorLayout),"Create texture descriptor layout");
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4};
        auto pool=vkStructure<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        pool.maxSets=2;pool.poolSizeCount=1;pool.pPoolSizes=&size;
        vkCheck(vkCreateDescriptorPool(device,&pool,nullptr,&descriptorPool),"Create texture descriptor pool");
        auto alloc=vkStructure<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        alloc.descriptorPool=descriptorPool;alloc.descriptorSetCount=1;alloc.pSetLayouts=&descriptorLayout;
        vkCheck(vkAllocateDescriptorSets(device,&alloc,&descriptor),"Allocate texture descriptors");
        updateTextureDescriptor(0,*atlasTexture);updateTextureDescriptor(1,*fontTexture);
#if VC_HAS_VOXELUI
        vkCheck(vkAllocateDescriptorSets(device,&alloc,&uiDescriptor),"Allocate UI texture descriptors");
#endif
    }
    void updateTextureDescriptor(std::uint32_t binding,const Texture& texture,VkDescriptorSet target={}) {
        const VkDescriptorImageInfo image{texture.sampler,texture.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        auto write=vkStructure<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        write.dstSet=target?target:descriptor;write.dstBinding=binding;write.descriptorCount=1;
        write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    }

    void destroySwapchain() {
        if(translucentPipeline) vkDestroyPipeline(device,translucentPipeline,nullptr);
        translucentPipeline=VK_NULL_HANDLE;
        if(hudPipeline) vkDestroyPipeline(device,hudPipeline,nullptr);
        hudPipeline=VK_NULL_HANDLE;
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
        if (depthView) vkDestroyImageView(device, depthView, nullptr);
        if (depthImage) vkDestroyImage(device, depthImage, nullptr);
        if (depthMemory) vkFreeMemory(device, depthMemory, nullptr);
        depthView = VK_NULL_HANDLE; depthImage = VK_NULL_HANDLE; depthMemory = VK_NULL_HANDLE;
        for (auto view : views) vkDestroyImageView(device, view, nullptr);
        for (auto semaphore : presented) if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
        views.clear(); presented.clear(); images.clear();
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    bool recreateSwapchain() {
        auto [width, height] = window.pixelSize();
        if (width == 0 || height == 0) return false;
        vkCheck(vkDeviceWaitIdle(device), "Wait before swapchain recreation");

        destroySwapchain();
        VkSurfaceCapabilitiesKHR caps{};
        vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps), "Query surface capabilities");
        extent = caps.currentExtent;
        if (extent.width == std::numeric_limits<std::uint32_t>::max()) {
            extent.width = std::clamp(static_cast<std::uint32_t>(width), caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(static_cast<std::uint32_t>(height), caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (extent.width == 0 || extent.height == 0) return false;
        std::uint32_t count{};
        vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr), "Query surface formats");
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data()), "Query surface formats");
        std::optional<VkSurfaceFormatKHR> selected;
        for (auto format : formats) {
            if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) continue;
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB) { selected = format; break; }
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) selected = format;
            if (format.format == VK_FORMAT_UNDEFINED) selected = VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, format.colorSpace};
        }
        if (!selected) throw std::runtime_error("Surface needs a BGRA8/RGBA8 SDR format");
        colorFormat = selected->format;
        auto imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        for (auto flag : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
            if (caps.supportedCompositeAlpha & flag) { alpha = flag; break; }
        auto info = vkStructure<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
        info.surface = surface; info.minImageCount = imageCount; info.imageFormat = colorFormat; info.imageColorSpace = selected->colorSpace;
        info.imageExtent = extent; info.imageArrayLayers = 1; info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        canCapture = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
        if (canCapture) info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        const std::array families{graphicsFamily, presentFamily};
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (graphicsFamily != presentFamily) {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; info.queueFamilyIndexCount = 2; info.pQueueFamilyIndices = families.data();
        }
        info.preTransform = caps.currentTransform; info.compositeAlpha = alpha; info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if(!vsync) {
            std::uint32_t modeCount{};
            vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,nullptr),"Query present mode count");
            std::vector<VkPresentModeKHR> modes(modeCount);
            vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,&modeCount,modes.data()),"Query present modes");
            for(const auto preferred:{VK_PRESENT_MODE_IMMEDIATE_KHR,VK_PRESENT_MODE_MAILBOX_KHR})
                if(std::ranges::find(modes,preferred)!=modes.end()) { info.presentMode=preferred;break; }
        } info.clipped = VK_TRUE;
        vkCheck(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Create swapchain");
        vkCheck(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr), "Get swapchain images");
        images.resize(count); views.reserve(count); presented.resize(count);
        vkCheck(vkGetSwapchainImagesKHR(device, swapchain, &count, images.data()), "Get swapchain images");
        auto semaphore = vkStructure<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        for (std::uint32_t i = 0; i < count; ++i) {
            views.push_back(createView(images[i], colorFormat, VK_IMAGE_ASPECT_COLOR_BIT));
            // Presentation waits are indexed by acquired image, not by frame-in-flight.
            vkCheck(vkCreateSemaphore(device, &semaphore, nullptr, &presented[i]), "Create presentation semaphore");
        }
        auto depth = vkStructure<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        depth.imageType = VK_IMAGE_TYPE_2D; depth.format = depthFormat; depth.extent = {extent.width, extent.height, 1};
        depth.mipLevels = 1; depth.arrayLayers = 1; depth.samples = VK_SAMPLE_COUNT_1_BIT; depth.tiling = VK_IMAGE_TILING_OPTIMAL;
        depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; depth.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateImage(device, &depth, nullptr, &depthImage), "Create depth image");
        VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device, depthImage, &requirements);
        auto alloc = vkStructure<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO); alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = memoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkCheck(vkAllocateMemory(device, &alloc, nullptr, &depthMemory), "Allocate depth memory");
        vkCheck(vkBindImageMemory(device, depthImage, depthMemory, 0), "Bind depth memory");
        depthView = createView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        createPipeline();
        dirty = false;
        log("Swapchain: " + std::to_string(extent.width) + "x" + std::to_string(extent.height));
        return true;
    }

    VkShaderModule shader(const char* name) const {
        const auto path = window.assetPath() / "shaders" / name;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Cannot load shader: " + vc::pathText(path));
        auto size = file.tellg();
        if (size <= 0 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V shader size: " + vc::pathText(path));
        std::vector<std::uint32_t> code(static_cast<std::size_t>(size) / 4);
        file.seekg(0); file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));
        if (!file) throw std::runtime_error("Cannot read shader: " + vc::pathText(path));
        auto info = vkStructure<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO); info.codeSize = static_cast<std::size_t>(size); info.pCode = code.data();
        VkShaderModule module{}; vkCheck(vkCreateShaderModule(device, &info, nullptr, &module), "Create shader module"); return module;
    }
    void createPipeline() {
        const auto vertex = shader("voxel.vert.spv");
        VkShaderModule fragment{};
        try {
            fragment = shader("voxel.frag.spv");
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertex; stages[0].pName = "main";
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragment; stages[1].pName = "main";
            const std::array<VkVertexInputBindingDescription,2> bindings{{
                {0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX},{1,sizeof(InstanceData),VK_VERTEX_INPUT_RATE_INSTANCE}}};
            std::array<VkVertexInputAttributeDescription, 12> attributes{{
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
                {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
                {3,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,normal)},
                {4,0,VK_FORMAT_R32_SFLOAT,offsetof(Vertex,opacity)},
                {5,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,light)},
                {6,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,sway)},
                {7,1,VK_FORMAT_R32G32B32_SFLOAT,offsetof(InstanceData,offset)},
                {8,1,VK_FORMAT_R32_SFLOAT,offsetof(InstanceData,yaw)},
                {9,1,VK_FORMAT_R32G32_SFLOAT,offsetof(InstanceData,light)},
                {10,1,VK_FORMAT_R32_SFLOAT,offsetof(InstanceData,phase)},
                {11,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(Vertex,tileBounds)}}};
            auto vertexInput = vkStructure<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
            vertexInput.vertexBindingDescriptionCount=static_cast<std::uint32_t>(bindings.size());vertexInput.pVertexBindingDescriptions=bindings.data();
            vertexInput.vertexAttributeDescriptionCount=static_cast<std::uint32_t>(attributes.size());vertexInput.pVertexAttributeDescriptions=attributes.data();
            auto assembly = vkStructure<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO); assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            auto viewport = vkStructure<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO); viewport.viewportCount = viewport.scissorCount = 1;
            auto raster = vkStructure<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
            raster.polygonMode = VK_POLYGON_MODE_FILL; raster.cullMode = VK_CULL_MODE_BACK_BIT; raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth = 1;
            auto multisample = vkStructure<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO); multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            auto depth = vkStructure<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
            depth.depthTestEnable = depth.depthWriteEnable = VK_TRUE; depth.depthCompareOp = VK_COMPARE_OP_LESS;
            VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            auto color = vkStructure<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO); color.attachmentCount = 1; color.pAttachments = &blend;
            const std::array dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            auto dynamic = vkStructure<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO); dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamics.data();
            auto rendering = vkStructure<VkPipelineRenderingCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO);
            rendering.colorAttachmentCount = 1; rendering.pColorAttachmentFormats = &colorFormat; rendering.depthAttachmentFormat = depthFormat;
            auto info = vkStructure<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
            info.pNext = &rendering; info.stageCount = 2; info.pStages = stages.data();
            info.pVertexInputState = &vertexInput; info.pInputAssemblyState = &assembly; info.pViewportState = &viewport;
            info.pRasterizationState = &raster; info.pMultisampleState = &multisample; info.pDepthStencilState = &depth;
            info.pColorBlendState = &color; info.pDynamicState = &dynamic; info.layout = pipelineLayout;
            vkCheck(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "Create voxel graphics pipeline");
            depth.depthWriteEnable=VK_FALSE;raster.cullMode=VK_CULL_MODE_NONE;
            blend.blendEnable=VK_TRUE;blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp=VK_BLEND_OP_ADD;blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.alphaBlendOp=VK_BLEND_OP_ADD;
            vkCheck(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&translucentPipeline),"Create translucent graphics pipeline");
            // HUD panels and cached glyphs use ordinary alpha blending.
            depth.depthTestEnable=depth.depthWriteEnable=VK_FALSE;
            raster.cullMode=VK_CULL_MODE_NONE;
            vkCheck(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&hudPipeline),"Create HUD graphics pipeline");
        } catch (...) {
            if (fragment) vkDestroyShaderModule(device, fragment, nullptr);
            vkDestroyShaderModule(device, vertex, nullptr); throw;
        }
        vkDestroyShaderModule(device, fragment, nullptr); vkDestroyShaderModule(device, vertex, nullptr);
    }

    void barrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to,
            VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage,
            VkAccessFlags sourceAccess, VkAccessFlags destinationAccess) const {
        auto transition = vkStructure<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
        transition.srcAccessMask = sourceAccess; transition.dstAccessMask = destinationAccess;
        transition.oldLayout = from; transition.newLayout = to;
        transition.srcQueueFamilyIndex = transition.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        transition.image = image; transition.subresourceRange = {aspect, 0, 1, 0, 1};
        vkCmdPipelineBarrier(command, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &transition);
    }

    void saveScreenshot(const Buffer& buffer, const std::filesystem::path& path) const {
        std::ofstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot write screenshot: " + vc::pathText(path));
        file << "P6\n" << extent.width << ' ' << extent.height << "\n255\n";
        const auto bytes = static_cast<VkDeviceSize>(extent.width) * extent.height * 4;
        void* mapped{}; vkCheck(vkMapMemory(device, buffer.memory, 0, bytes, 0, &mapped), "Map screenshot");
        const auto* data = static_cast<const unsigned char*>(mapped);
        const bool bgra = colorFormat == VK_FORMAT_B8G8R8A8_SRGB || colorFormat == VK_FORMAT_B8G8R8A8_UNORM;
        for (VkDeviceSize i = 0; i < bytes; i += 4) {
            const std::array rgb{data[i + (bgra ? 2 : 0)], data[i + 1], data[i + (bgra ? 0 : 2)]};
            file.write(reinterpret_cast<const char*>(rgb.data()), 3);
        }
        vkUnmapMemory(device, buffer.memory);
        if (!file) throw std::runtime_error("Screenshot write failed: " + vc::pathText(path));
        log("Screenshot saved: " + vc::pathText(path));
    }

    void prepareInstances(const Camera& camera) {
        std::vector<InstanceData> data(1); // Identity slot for terrain and HUD.
        const Frustum frustum(camera.viewProjection(static_cast<float>(extent.width)/static_cast<float>(extent.height)));
        instanceRanges.clear();drawnInstances=0;
        for(std::size_t model=0;model<models.size();++model) {
            const auto first=static_cast<std::uint32_t>(data.size());
            for(const auto& entry:instances) {
                if(entry.model!=model) continue;
                const auto offset=entry.position-camera.position;
                if(!frustum.intersects(models[model].bounds,offset)) continue;
                if(glm::length(offset)>static_cast<double>(atmosphere.fogEnd)+glm::length(models[model].bounds.max)) continue;
                data.push_back({glm::vec3(offset),entry.yaw,entry.light,entry.phase,0});++drawnInstances;
            }
            instanceRanges.emplace_back(first,static_cast<std::uint32_t>(data.size())-first);
        }
        if(data.size()>instanceCapacity) {
            instanceCapacity=std::max(data.size(),instanceCapacity*2);
            instanceBuffer=createBuffer(instanceCapacity*sizeof(InstanceData),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        }
        void* mapped{};const auto bytes=data.size()*sizeof(InstanceData);
        vkCheck(vkMapMemory(device,instanceBuffer.memory,0,bytes,0,&mapped),"Map instance data");
        std::memcpy(mapped,data.data(),bytes);vkUnmapMemory(device,instanceBuffer.memory);
    }

    bool draw(const Camera& camera, const std::filesystem::path& screenshot) {
        if (window.pixelSize().first == 0 || window.pixelSize().second == 0) return false;
        if (dirty && !recreateSwapchain()) return false;
        vkCheck(vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX), "Wait for frame");

#if VC_HAS_VOXELUI
        prepareUi();
#endif
        prepareInstances(camera);
        Buffer readback;
        if (!screenshot.empty()) {
            if (!canCapture) throw std::runtime_error("Surface does not support screenshot readback");
            readback = createBuffer(static_cast<VkDeviceSize>(extent.width) * extent.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        }
        std::uint32_t imageIndex{};
        auto result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { dirty = true; return false; }
        if (result == VK_SUBOPTIMAL_KHR) dirty = true;
        else vkCheck(result, "Acquire swapchain image");
        vkCheck(vkResetCommandBuffer(command, 0), "Reset command buffer");
        auto begin = vkStructure<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO); begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkCheck(vkBeginCommandBuffer(command, &begin), "Begin frame commands");

        // Chain the layout transition to the acquire wait at COLOR_ATTACHMENT_OUTPUT;
        // TOP_OF_PIPE would let the transition race presentation's read of this image.
        barrier(images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        barrier(depthImage, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        auto color = vkStructure<VkRenderingAttachmentInfo>(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
        color.imageView = views[imageIndex]; color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = {{atmosphere.skyColor.r,atmosphere.skyColor.g,atmosphere.skyColor.b,1}};
        auto depth = vkStructure<VkRenderingAttachmentInfo>(VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO);
        depth.imageView = depthView; depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; depth.clearValue.depthStencil = {1, 0};
        auto rendering = vkStructure<VkRenderingInfo>(VK_STRUCTURE_TYPE_RENDERING_INFO);
        rendering.renderArea = {{0, 0}, extent}; rendering.layerCount = 1; rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &color; rendering.pDepthAttachment = &depth;
        vkCmdBeginRendering(command, &rendering);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptor, 0, nullptr);
        VkViewport viewport{0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1};
        VkRect2D scissor{{0, 0}, extent}; vkCmdSetViewport(command, 0, 1, &viewport); vkCmdSetScissor(command, 0, 1, &scissor);
        PushConstants push{camera.viewProjection(static_cast<float>(extent.width)/static_cast<float>(extent.height)),{},
            glm::vec4(glm::normalize(atmosphere.sunDirection),atmosphere.sunlight),glm::vec4(atmosphere.skyColor,atmosphere.ambient),
            {atmosphere.fogStart,atmosphere.fogEnd,atmosphere.horizontalFog?1.0f:0.0f,0}};
        const Frustum frustum(push.viewProjection);
        drawnMeshes = drawnTriangles = 0;
        std::vector<const GpuMesh*> translucent;
        for (const auto& mesh : meshes) {
            const auto offset = glm::dvec3(mesh.origin) - camera.position;
            if (!frustum.intersects(mesh.bounds, offset)) continue;
            auto closest=glm::clamp(glm::dvec3(0),mesh.bounds.min+offset,mesh.bounds.max+offset);
            if(atmosphere.horizontalFog) closest.y=0;
            if(glm::dot(closest,closest)>static_cast<double>(atmosphere.fogEnd)*atmosphere.fogEnd) continue;
            push.offset = glm::vec4(glm::vec3(offset), 0);
            if(mesh.indexCount) drawMesh(mesh, push);
            if(mesh.translucentCount) translucent.push_back(&mesh);
            ++drawnMeshes; drawnTriangles += (mesh.indexCount+static_cast<std::size_t>(mesh.translucentCount)) / 3;
        }
        push.offset={0,0,0,3}; // Lit vertex-color models.
        for(std::size_t model=0;model<models.size();++model) {
            const auto [first,count]=instanceRanges[model];
            if(count&&models[model].indexCount) drawMesh(models[model],push,false,count,first);
        }
        std::sort(translucent.begin(),translucent.end(),[&](const auto* a,const auto* b) {
            const auto distance=[&](const auto* mesh) { const auto p=glm::dvec3(mesh->origin)+(mesh->bounds.min+mesh->bounds.max)*0.5-camera.position;return glm::dot(p,p); };
            return distance(a)>distance(b);
        });
        if(!translucent.empty()) {
            vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,translucentPipeline);
            for(const auto* mesh:translucent) { push.offset=glm::vec4(glm::vec3(glm::dvec3(mesh->origin)-camera.position),0);drawMesh(*mesh,push,true); }
            vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        }
        if (selection) {
            push.offset = glm::vec4(glm::vec3(glm::dvec3(*selection) - camera.position), 1);
            drawMesh(outline, push);
        }
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,hudPipeline);
        push.viewProjection = glm::mat4(1);
        push.viewProjection[0][0] = 2.0f / static_cast<float>(extent.width);
        push.viewProjection[1][1] = 2.0f / static_cast<float>(extent.height);
        push.offset = {0,0,0,1};
        if(crosshairVisible) drawMesh(crosshair, push);
        if(crosshairVisible&&progress) {
            push.viewProjection[3][0]=-100.0f/static_cast<float>(extent.width);
            push.viewProjection[3][1]=48.0f/static_cast<float>(extent.height);
            drawMesh(progressBack,push);push.viewProjection[0][0]*=*progress;drawMesh(progressFill,push);
            push.viewProjection[0][0]=2.0f/static_cast<float>(extent.width);
            push.viewProjection[3][0]=push.viewProjection[3][1]=0;
        }
        push.offset={-static_cast<float>(extent.width)/2,-static_cast<float>(extent.height)/2,0,1};
        if(hudColors.indexCount) drawMesh(hudColors,push);
        push.offset.w=2;
        if(hudTextures.indexCount) drawMesh(hudTextures,push);
        push.offset.w=1;
        if(hudForeground.indexCount) drawMesh(hudForeground,push);
#if VC_HAS_VOXELUI
        if(uiList&&uiQuad.indexCount) {
            vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&uiDescriptor,0,nullptr);
            push.offset.w=2;drawMesh(uiQuad,push);uiMetrics.drawCalls=1;
        }
#endif
        vkCmdEndRendering(command);
        if (readback.handle) {
            barrier(images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{}; copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; copy.imageExtent = {extent.width, extent.height, 1};
            vkCmdCopyImageToBuffer(command, images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.handle, 1, &copy);
            barrier(images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0);
        } else {
            barrier(images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0);
        }
        vkCheck(vkEndCommandBuffer(command), "End frame commands");
        // Reset only after acquisition and recording succeeded, avoiding an unsignaled-fence deadlock on resize.
        vkCheck(vkResetFences(device, 1, &frameFence), "Reset frame fence");
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        auto submit = vkStructure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &acquired; submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &presented[imageIndex];
        vkCheck(vkQueueSubmit(graphicsQueue, 1, &submit, frameFence), "Submit frame");
        auto present = vkStructure<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
        present.waitSemaphoreCount = 1; present.pWaitSemaphores = &presented[imageIndex];
        present.swapchainCount = 1; present.pSwapchains = &swapchain; present.pImageIndices = &imageIndex;
        result = vkQueuePresentKHR(presentQueue, &present);
        // Readback must outlive its GPU commands even when present returns an error.
        if (readback.handle) vkCheck(vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX), "Wait for readback");
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) dirty = true;
        else vkCheck(result, "Present frame");
        if (readback.handle) saveScreenshot(readback, screenshot);
        return true;
    }
};

VulkanRenderer::VulkanRenderer(Window& window, const ImageRgba& atlas, bool requireValidation,bool vsync)
    : validation_(std::make_unique<ValidationState>()), impl_(std::make_unique<Impl>(window, *validation_, requireValidation)) {
    impl_->vsync=vsync;impl_->initialize(atlas);
}
VulkanRenderer::~VulkanRenderer() = default;
void VulkanRenderer::shutdown() { impl_.reset(); }
ValidationReport VulkanRenderer::validationReport() const {
    return {validation_->enabled, validation_->errors.load(), validation_->warnings.load()};
}
void VulkanRenderer::requestResize() { impl_->dirty = true; }
void VulkanRenderer::replaceAtlas(const ImageRgba& image) {
    image.validate();
    vkCheck(vkWaitForFences(impl_->device,1,&impl_->frameFence,VK_TRUE,UINT64_MAX),"Wait before atlas replacement");
    auto texture=impl_->createTexture(image);
    impl_->updateTextureDescriptor(0,*texture);
    impl_->atlasTexture=std::move(texture);
}
void VulkanRenderer::clearSession() {
#if VC_HAS_VOXELUI
    clearUi();
#endif
    upload({});setInstances({});setModels({});setHud({}, {}, false);
    setSelection(std::nullopt);setProgressIndicator(std::nullopt);setAtmosphere(Atmosphere{});
}
RenderMetrics VulkanRenderer::metrics() const { return impl_->metrics; }
bool VulkanRenderer::draw(const Camera& camera, const std::filesystem::path& screenshot) {
    const auto start=std::chrono::steady_clock::now();impl_->metrics.drawCalls=0;
    const auto drawn=impl_->draw(camera,screenshot);
    impl_->metrics.drawMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return drawn;
}
std::size_t VulkanRenderer::meshCount() const { return impl_->meshes.size(); }
std::size_t VulkanRenderer::triangleCount() const { return impl_->triangles; }
std::size_t VulkanRenderer::drawnMeshCount() const { return impl_->drawnMeshes; }
std::size_t VulkanRenderer::drawnTriangleCount() const { return impl_->drawnTriangles; }
void VulkanRenderer::setSelection(std::optional<glm::i64vec3> block) { impl_->selection = block; }
void VulkanRenderer::setHud(const Mesh& colored,const Mesh& textured,bool crosshairVisible,const Mesh& foreground) {
    vkCheck(vkWaitForFences(impl_->device,1,&impl_->frameFence,VK_TRUE,UINT64_MAX),"Wait before replacing HUD meshes");
    if(impl_->fontRevision!=ui::fonts().revision()) {
        auto texture=impl_->createTexture(ui::fonts().image(),true);
        impl_->updateTextureDescriptor(1,*texture);impl_->fontTexture=std::move(texture);impl_->fontRevision=ui::fonts().revision();
    }
    auto colors=impl_->createMesh(colored),textures=impl_->createMesh(textured),front=impl_->createMesh(foreground);
    impl_->hudColors=std::move(colors);impl_->hudTextures=std::move(textures);
    impl_->hudForeground=std::move(front);
    impl_->crosshairVisible=crosshairVisible;
}
void VulkanRenderer::setProgressIndicator(std::optional<float> progress) {
    if(progress&&(!std::isfinite(*progress)||*progress<0||*progress>1)) throw std::invalid_argument("Progress indicator must be within 0..1");
    impl_->progress=progress;
}
void VulkanRenderer::setAtmosphere(const Atmosphere& atmosphere) {
    const auto finite=[](glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    if(!finite(atmosphere.skyColor)||!finite(atmosphere.sunDirection)||!std::isfinite(glm::length(atmosphere.sunDirection))||glm::length(atmosphere.sunDirection)<0.001f||
        !std::isfinite(atmosphere.sunlight)||atmosphere.sunlight<0||!std::isfinite(atmosphere.ambient)||atmosphere.ambient<0||
        !std::isfinite(atmosphere.fogStart)||!std::isfinite(atmosphere.fogEnd)||atmosphere.fogStart<0||atmosphere.fogEnd<=atmosphere.fogStart)
        throw std::invalid_argument("Invalid atmosphere parameters");
    impl_->atmosphere=atmosphere;
}
std::size_t VulkanRenderer::drawnInstanceCount() const { return impl_->drawnInstances; }
void VulkanRenderer::setModels(std::span<const Mesh> models) {
    if(models.size()>64) throw std::invalid_argument("Too many reusable models");
    vkCheck(vkWaitForFences(impl_->device,1,&impl_->frameFence,VK_TRUE,UINT64_MAX),"Wait before model upload");
    std::vector<GpuMesh> uploaded;
    for(const auto& model:models) {
        if(!model.translucentIndices.empty()||model.origin!=glm::i64vec3(0)) throw std::invalid_argument("Instance models must be local and opaque");
        auto gpu=impl_->createMesh(model);double radius=0;
        for(const auto& vertex:model.vertices) {
            for(int axis=0;axis<3;++axis) if(!std::isfinite(vertex.sway[axis])) throw std::invalid_argument("Invalid model animation");
            radius=std::max(radius,glm::length(glm::dvec3(glm::abs(vertex.position)+glm::abs(vertex.sway))));
        }
        gpu.bounds={glm::dvec3(-radius),glm::dvec3(radius)};uploaded.push_back(std::move(gpu));
    }
    impl_->models=std::move(uploaded);impl_->instances.clear();
}
void VulkanRenderer::setInstances(std::span<const RenderInstance> instances) {
    if(instances.size()>1024) throw std::invalid_argument("Too many model instances");
    for(const auto& instance:instances) {
        if(instance.model>=impl_->models.size()||!std::isfinite(instance.yaw)||!std::isfinite(instance.phase)||
           !std::isfinite(instance.light.x)||!std::isfinite(instance.light.y)||glm::any(glm::lessThan(instance.light,glm::vec2(0)))||
           glm::any(glm::greaterThan(instance.light,glm::vec2(1)))) throw std::invalid_argument("Invalid model instance");
        for(int axis=0;axis<3;++axis) if(!std::isfinite(instance.position[axis])||std::abs(instance.position[axis])>1e15)
            throw std::invalid_argument("Invalid instance position");
    }
    impl_->instances.assign(instances.begin(),instances.end());
}
void VulkanRenderer::upload(std::span<const Mesh> meshes) {
    vkCheck(vkWaitForFences(impl_->device,1,&impl_->frameFence,VK_TRUE,UINT64_MAX), "Wait before mesh upload");
    std::vector<GpuMesh> uploaded;
    std::size_t triangles = 0;
    for (const auto& mesh : meshes) {
        if (mesh.indices.empty()&&mesh.translucentIndices.empty()) continue;
        auto gpu = impl_->createMesh(mesh);
        triangles += (mesh.indices.size()+mesh.translucentIndices.size())/3; uploaded.push_back(std::move(gpu));
    }
    impl_->meshes = std::move(uploaded); impl_->triangles = triangles;
}
void VulkanRenderer::updateMeshes(std::span<const Mesh> meshes) {
    if (meshes.empty()) return;
    const auto uploadStart=std::chrono::steady_clock::now();
    vkCheck(vkWaitForFences(impl_->device,1,&impl_->frameFence,VK_TRUE,UINT64_MAX), "Wait before replacing chunk meshes");
    std::vector<GpuMesh> replacements;
    for(const auto& mesh:meshes) replacements.push_back(impl_->createMesh(mesh));
    impl_->meshes.reserve(impl_->meshes.size()+replacements.size());
    for(auto& mesh:replacements) {
        const auto it=std::find_if(impl_->meshes.begin(),impl_->meshes.end(),[&](const auto& old){ return old.origin==mesh.origin; });
        if(!mesh.indexCount&&!mesh.translucentCount) { if(it!=impl_->meshes.end()) impl_->meshes.erase(it); }
        else if(it!=impl_->meshes.end()) *it=std::move(mesh);
        else impl_->meshes.push_back(std::move(mesh));
    }
    impl_->metrics.uploadMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-uploadStart).count();
    impl_->triangles=0;
    for(const auto& mesh:impl_->meshes) impl_->triangles+=(mesh.indexCount+static_cast<std::size_t>(mesh.translucentCount))/3;
}
}

#if VC_HAS_VOXELUI
void vc::VulkanRenderer::setUi(voxelui::DrawList list) {
    if(impl_->uiList && *impl_->uiList==list) return;
    impl_->uiList=std::move(list);impl_->uiDirty=true;
}
void vc::VulkanRenderer::clearUi() {
    // GPU data retires after the next host fence, or device idle on destruction.
    impl_->uiList.reset();impl_->uiDirty=false;
}
vc::UiRenderMetrics vc::VulkanRenderer::uiMetrics() const { return impl_->uiMetrics; }
#endif
