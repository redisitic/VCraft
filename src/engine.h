#pragma once

#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <array>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>

// Forward declarations
struct Camera;
class World;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
};

// 12 floats per vertex: pos(3) + normal(3) + texcoord(2) + lighting(3) + aoFactor(1)
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 lighting;
    float aoFactor;     // Minecraft-style baked vertex AO (0=fully occluded, 1=open)

    static std::array<VkVertexInputBindingDescription, 1> getBindingDescriptions();
    static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();
};

struct UniformBufferObject {
    glm::mat4 viewProj;
    glm::vec3 camPos;
    float timeOfDay;       // 0-24 hour clock
    glm::vec3 sunDir;
    float _pad0;
    glm::vec3 moonDir;
    float _pad1;
    glm::vec3 skyColor;
    float sunIntensity;    // 0 at night, 1 at noon
    glm::vec3 camRight;    // camera right unit vector (for sky ray reconstruction)
    float tanHalfFov;      // tan(fov/2) (for sky ray reconstruction)
    glm::vec3 camUp;       // camera up unit vector (for sky ray reconstruction)
    float aspectRatio;     // viewport width/height (for sky ray reconstruction)
    glm::vec3 camFront;    // camera front unit vector (for sky ray reconstruction)
    float _pad2;
};

// UI vertex for overlay rendering (console, crosshair, etc.)
struct UIVertex {
    glm::vec2 pos;       // screen-space pixel position
    glm::vec2 uv;        // texture coordinate into font atlas
    glm::vec4 color;     // RGBA tint

    static std::array<VkVertexInputBindingDescription, 1> getBindingDescriptions() {
        std::array<VkVertexInputBindingDescription, 1> bindings{};
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(UIVertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindings;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attrs{};
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
        attrs[0].offset = offsetof(UIVertex, pos);
        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset = offsetof(UIVertex, uv);
        attrs[2].binding = 0;
        attrs[2].location = 2;
        attrs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[2].offset = offsetof(UIVertex, color);
        return attrs;
    }
};

struct UIPushConstants {
    float screenWidth;
    float screenHeight;
};

struct LightParams {
    glm::vec3 sunDirection;
    float sunIntensity;
    glm::vec3 sunColor;
    float ambientIntensity;
    glm::vec3 skyColor;
    uint32_t vertexCount;
    uint32_t frameCount;      // monotonic frame counter for RNG seeding
    uint32_t torchCount;      // number of active torches in SSBO
    uint32_t restirSamples;   // number of spatial/temporal samples per frame
    uint32_t restirBounces;   // number of GI bounces (0 or 1)
};

struct ChunkMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer reservoirBuffer;   // ReSTIR reservoir SSBO (8 floats per vertex)
    uint32_t reservoirVertexCount = 0; // vertex count when reservoir was allocated
    bool dirty = true;
    bool uploaded = false;
    glm::ivec2 chunkPos;
};

// Ray tracing function pointers loaded dynamically
struct RayTracingFunctions {
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
};

class Engine {
public:
    void init();
    void run();
    void cleanup();
    void recreateSwapChain();

    // Public state
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilies;
    RayTracingFunctions rtFuncs;

    // Swapchain
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Depth buffer
    AllocatedImage depthImage;
    VkFormat depthFormat;

    // Render pass & pipelines
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    // Sky pipeline (fullscreen triangle for sky + sun/moon)
    VkPipelineLayout skyPipelineLayout = VK_NULL_HANDLE;
    VkPipeline skyPipeline = VK_NULL_HANDLE;

    // UI overlay pipeline (console text, crosshair)
    VkPipelineLayout uiPipelineLayout = VK_NULL_HANDLE;
    VkPipeline uiPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout uiFontDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool uiDescPool = VK_NULL_HANDLE;
    VkDescriptorSet uiFontDescSet = VK_NULL_HANDLE;
    AllocatedImage fontAtlasImage;
    VkSampler fontSampler = VK_NULL_HANDLE;

    // Compute pipelines for vertex lighting
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;        // ReSTIR (default)
    VkPipeline computePipelineSimple = VK_NULL_HANDLE;   // Simple shadow+AO
    VkDescriptorSetLayout computeDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool computeDescPool = VK_NULL_HANDLE;
    VkDescriptorSet computeDescSet = VK_NULL_HANDLE;

    // Descriptors for graphics
    VkDescriptorSetLayout uboDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> uboDescSets;
    VkDescriptorSet textureDescSet = VK_NULL_HANDLE;

    // Uniform buffers (one per frame in flight)
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<AllocatedBuffer> uniformBuffers;
    std::vector<void*> uniformBuffersMapped;

    // Command buffers
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // Sync
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    // Texture atlas
    AllocatedImage textureAtlas;
    VkSampler textureSampler = VK_NULL_HANDLE;

    // Ray tracing
    VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
    AllocatedBuffer tlasBuffer;
    AllocatedBuffer blasBuffer;
    VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
    AllocatedBuffer instanceBuffer;
    AllocatedBuffer scratchBuffer;

    // Light params uniform for compute
    AllocatedBuffer lightParamsBuffer;
    void* lightParamsMapped = nullptr;

    // Torch light SSBO for compute (uploaded each frame)
    AllocatedBuffer torchLightBuffer;
    void* torchLightMapped = nullptr;

    bool framebufferResized = false;
    int windowWidth = 1280;
    int windowHeight = 720;

    // Helper functions
    AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage, VmaAllocationCreateFlags flags = 0);
    void destroyBuffer(AllocatedBuffer& buf);
    AllocatedImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage);
    void destroyImage(AllocatedImage& img);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmdBuf);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readFile(const std::string& filename);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);

private:
    void initWindow();
    void initVulkan();
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayouts();
    void createGraphicsPipeline();
    void createSkyPipeline();
    void createUIPipeline();
    void createComputePipeline();
    void createDepthResources();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createDescriptorPool();
    void createUniformBuffers();
    void createDescriptorSets();
    void loadRayTracingFunctions();
    void cleanupSwapChain();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
};
