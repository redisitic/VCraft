#pragma once

#include "engine.h"

// Forward declare
class World;
struct ChunkMesh;

struct AccelStructure {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    AllocatedBuffer buffer;
};

// GPU-side torch light data (8 floats = 32 bytes per torch, std430 compatible)
struct TorchLightGPU {
    float posX, posY, posZ;  // world position (center of block: wx+0.5, wy+0.5, wz+0.5)
    float radius;            // light falloff radius in blocks
    float colorR, colorG, colorB;
    float intensity;
};

static constexpr uint32_t MAX_TORCHES = 128;

class RayTracing {
public:
    void init(Engine* engine);
    void cleanup();
    
    // Build BLAS for a single chunk mesh
    AccelStructure buildBLAS(ChunkMesh& mesh);
    
    // Build TLAS over all chunk BLASes
    void buildTLAS(const std::vector<ChunkMesh*>& meshes, const std::vector<AccelStructure>& blasList);
    
    // Run the per-vertex lighting compute shader (ReSTIR or simple)
    void computeVertexLighting(ChunkMesh& mesh, const glm::vec3& sunDir,
                               float sunIntensity, const glm::vec3& sunColor,
                               float ambientIntensity, const glm::vec3& skyColor,
                               bool useReSTIR, uint32_t restirSamples, uint32_t restirBounces);
    
    // Rebuild all acceleration structures for current world state
    void rebuildAccelStructures(World& world);
    
    // Run lighting on all visible chunks
    void updateLighting(World& world, const glm::vec3& sunDir,
                        float sunIntensity, const glm::vec3& sunColor,
                        float ambientIntensity, const glm::vec3& skyColor,
                        bool useReSTIR, uint32_t restirSamples, uint32_t restirBounces);

    // Ensure a chunk mesh has a properly-sized reservoir buffer
    void ensureReservoirBuffer(ChunkMesh& mesh);

    // Upload torch light data to GPU SSBO; returns the torch count
    uint32_t uploadTorchData(World& world);

    // Ensure the torch light SSBO exists
    void ensureTorchBuffer();

    // Reset all reservoir frame counters (e.g. when time-of-day changes)
    void resetReservoirs() { frameCount = 0; }

    AccelStructure tlas;
    std::vector<AccelStructure> blasList;
    bool accelBuilt = false;
    uint32_t frameCount = 0;   // monotonic frame counter for ReSTIR RNG seeding
    uint32_t currentTorchCount = 0;  // torches uploaded this frame

private:
    Engine* engine = nullptr;
    
    AllocatedBuffer createScratchBuffer(VkDeviceSize size);
};
