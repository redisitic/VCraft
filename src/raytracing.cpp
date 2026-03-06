#include "raytracing.h"
#include "world.h"

#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// init / cleanup
// ---------------------------------------------------------------------------

void RayTracing::init(Engine* eng) {
    engine = eng;
}

void RayTracing::cleanup() {
    if (engine->torchLightMapped) {
        vmaUnmapMemory(engine->allocator, engine->torchLightBuffer.allocation);
        engine->torchLightMapped = nullptr;
    }
    if (engine->torchLightBuffer.buffer != VK_NULL_HANDLE) {
        engine->destroyBuffer(engine->torchLightBuffer);
    }
    for (auto& as : blasList) {
        if (as.handle != VK_NULL_HANDLE) {
            engine->rtFuncs.vkDestroyAccelerationStructureKHR(engine->device, as.handle, nullptr);
            as.handle = VK_NULL_HANDLE;
        }
        if (as.buffer.buffer != VK_NULL_HANDLE) {
            engine->destroyBuffer(as.buffer);
        }
    }
    blasList.clear();
    if (tlas.handle != VK_NULL_HANDLE) {
        engine->rtFuncs.vkDestroyAccelerationStructureKHR(engine->device, tlas.handle, nullptr);
        tlas.handle = VK_NULL_HANDLE;
    }
    if (tlas.buffer.buffer != VK_NULL_HANDLE) {
        engine->destroyBuffer(tlas.buffer);
    }

    accelBuilt = false;
}

// ---------------------------------------------------------------------------
// Scratch buffer helper
// ---------------------------------------------------------------------------

AllocatedBuffer RayTracing::createScratchBuffer(VkDeviceSize size) {
    return engine->createBuffer(
        size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
}

// ---------------------------------------------------------------------------
// ensureReservoirBuffer – create/resize the per-vertex reservoir SSBO
// ---------------------------------------------------------------------------

void RayTracing::ensureReservoirBuffer(ChunkMesh& mesh) {
    uint32_t vertCount = static_cast<uint32_t>(mesh.vertices.size());
    if (vertCount == 0) return;
    if (mesh.reservoirBuffer.buffer != VK_NULL_HANDLE && mesh.reservoirVertexCount == vertCount) {
        return;
    }
    if (mesh.reservoirBuffer.buffer != VK_NULL_HANDLE) {
        engine->destroyBuffer(mesh.reservoirBuffer);
        mesh.reservoirBuffer = {};
        mesh.reservoirVertexCount = 0;
    }
    VkDeviceSize bufferSize = sizeof(float) * 8 * vertCount;

    mesh.reservoirBuffer = engine->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    mesh.reservoirVertexCount = vertCount;
    VkCommandBuffer cmd = engine->beginSingleTimeCommands();
    vkCmdFillBuffer(cmd, mesh.reservoirBuffer.buffer, 0, bufferSize, 0);
    VkBufferMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = mesh.reservoirBuffer.buffer;
    barrier.offset              = 0;
    barrier.size                = bufferSize;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 1, &barrier, 0, nullptr);

    engine->endSingleTimeCommands(cmd);
}

// ---------------------------------------------------------------------------
// buildBLAS – one bottom-level accel structure per chunk mesh
// ---------------------------------------------------------------------------

AccelStructure RayTracing::buildBLAS(ChunkMesh& mesh) {
    AccelStructure result{};

    // ------------------------------------------------------------------
    // 1. Describe the triangle geometry
    // ------------------------------------------------------------------
    VkDeviceAddress vertexAddress = engine->getBufferDeviceAddress(mesh.vertexBuffer.buffer);
    VkDeviceAddress indexAddress  = engine->getBufferDeviceAddress(mesh.indexBuffer.buffer);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride  = sizeof(Vertex);                       // 44 bytes
    triangles.maxVertex     = static_cast<uint32_t>(mesh.vertices.size() - 1);
    triangles.indexType     = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    // ------------------------------------------------------------------
    // 2. Query the required sizes
    // ------------------------------------------------------------------
    uint32_t primitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    engine->rtFuncs.vkGetAccelerationStructureBuildSizesKHR(
        engine->device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    // ------------------------------------------------------------------
    // 3. Create the BLAS buffer
    // ------------------------------------------------------------------
    result.buffer = engine->createBuffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // ------------------------------------------------------------------
    // 4. Create the acceleration structure object
    // ------------------------------------------------------------------
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = result.buffer.buffer;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    if (engine->rtFuncs.vkCreateAccelerationStructureKHR(
            engine->device, &createInfo, nullptr, &result.handle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create BLAS");
    }

    // ------------------------------------------------------------------
    // 5. Allocate scratch buffer
    // ------------------------------------------------------------------
    AllocatedBuffer scratch = createScratchBuffer(sizeInfo.buildScratchSize);

    // ------------------------------------------------------------------
    // 6. Build the BLAS on the GPU
    // ------------------------------------------------------------------
    buildInfo.dstAccelerationStructure  = result.handle;
    buildInfo.scratchData.deviceAddress = engine->getBufferDeviceAddress(scratch.buffer);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount  = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex     = 0;
    rangeInfo.transformOffset = 0;

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VkCommandBuffer cmd = engine->beginSingleTimeCommands();
    engine->rtFuncs.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
    engine->endSingleTimeCommands(cmd);

    // ------------------------------------------------------------------
    // 7. Clean up scratch
    // ------------------------------------------------------------------
    engine->destroyBuffer(scratch);

    return result;
}

// ---------------------------------------------------------------------------
// buildTLAS – one top-level accel structure over all chunk BLASes
// ---------------------------------------------------------------------------

void RayTracing::buildTLAS(const std::vector<ChunkMesh*>& meshes,
                           const std::vector<AccelStructure>& blasArray) {
    uint32_t instanceCount = static_cast<uint32_t>(blasArray.size());
    if (instanceCount == 0) return;

    // ------------------------------------------------------------------
    // 1. Build instance descriptors
    // ------------------------------------------------------------------
    std::vector<VkAccelerationStructureInstanceKHR> instances(instanceCount);

    for (uint32_t i = 0; i < instanceCount; ++i) {
        VkTransformMatrixKHR transform{};
        transform.matrix[0][0] = 1.0f;
        transform.matrix[1][1] = 1.0f;
        transform.matrix[2][2] = 1.0f;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = blasArray[i].handle;

        VkDeviceAddress blasAddr =
            engine->rtFuncs.vkGetAccelerationStructureDeviceAddressKHR(engine->device, &addrInfo);

        instances[i] = {};
        instances[i].transform                              = transform;
        instances[i].instanceCustomIndex                    = i;
        instances[i].mask                                   = 0xFF;
        instances[i].instanceShaderBindingTableRecordOffset = 0;
        instances[i].flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instances[i].accelerationStructureReference         = blasAddr;
    }

    // ------------------------------------------------------------------
    // 2. Upload instances to device-visible buffer
    // ------------------------------------------------------------------
    VkDeviceSize instancesSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;

    AllocatedBuffer instanceStagingBuf = engine->createBuffer(
        instancesSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    void* mapped = nullptr;
    vmaMapMemory(engine->allocator, instanceStagingBuf.allocation, &mapped);
    std::memcpy(mapped, instances.data(), instancesSize);
    vmaUnmapMemory(engine->allocator, instanceStagingBuf.allocation);

    AllocatedBuffer instanceBuf = engine->createBuffer(
        instancesSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    {
        VkCommandBuffer cmd = engine->beginSingleTimeCommands();
        VkBufferCopy region{};
        region.size = instancesSize;
        vkCmdCopyBuffer(cmd, instanceStagingBuf.buffer, instanceBuf.buffer, 1, &region);

        VkMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);

        engine->endSingleTimeCommands(cmd);
    }

    engine->destroyBuffer(instanceStagingBuf);

    // ------------------------------------------------------------------
    // 3. Describe the TLAS geometry (instances)
    // ------------------------------------------------------------------
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.arrayOfPointers    = VK_FALSE;
    instancesData.data.deviceAddress = engine->getBufferDeviceAddress(instanceBuf.buffer);

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
    tlasGeometry.geometry.instances = instancesData;

    // ------------------------------------------------------------------
    // 4. Query sizes
    // ------------------------------------------------------------------
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &tlasGeometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    engine->rtFuncs.vkGetAccelerationStructureBuildSizesKHR(
        engine->device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instanceCount,
        &sizeInfo
    );

    // ------------------------------------------------------------------
    // 5. Create TLAS buffer and acceleration structure
    // ------------------------------------------------------------------
    tlas.buffer = engine->createBuffer(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = tlas.buffer.buffer;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    if (engine->rtFuncs.vkCreateAccelerationStructureKHR(
            engine->device, &createInfo, nullptr, &tlas.handle) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create TLAS");
    }

    // ------------------------------------------------------------------
    // 6. Build
    // ------------------------------------------------------------------
    AllocatedBuffer scratch = createScratchBuffer(sizeInfo.buildScratchSize);

    buildInfo.dstAccelerationStructure  = tlas.handle;
    buildInfo.scratchData.deviceAddress = engine->getBufferDeviceAddress(scratch.buffer);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount  = instanceCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex     = 0;
    rangeInfo.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    {
        VkCommandBuffer cmd = engine->beginSingleTimeCommands();
        engine->rtFuncs.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRangeInfo);
        engine->endSingleTimeCommands(cmd);
    }

    // ------------------------------------------------------------------
    // 7. Clean up temporaries
    // ------------------------------------------------------------------
    engine->destroyBuffer(scratch);
    engine->destroyBuffer(instanceBuf);
}

// ---------------------------------------------------------------------------
// computeVertexLighting – per-vertex ray-query lighting via compute shader
// ---------------------------------------------------------------------------

void RayTracing::computeVertexLighting(ChunkMesh& mesh, const glm::vec3& sunDir,
                                        float sunIntensity, const glm::vec3& sunColor,
                                        float ambientIntensity, const glm::vec3& skyColor,
                                        bool useReSTIR, uint32_t restirSamples, uint32_t restirBounces) {
    if (mesh.vertices.empty() || !accelBuilt) return;

    uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());

    // ------------------------------------------------------------------
    // 1. Ensure reservoir buffer exists (needed for both modes since both
    //    shaders declare the binding, even if simple mode ignores it)
    // ------------------------------------------------------------------
    ensureReservoirBuffer(mesh);

    // ------------------------------------------------------------------
    // 2. Update the LightParams uniform buffer
    // ------------------------------------------------------------------
    LightParams params{};
    params.sunDirection    = glm::normalize(sunDir);
    params.sunIntensity    = sunIntensity;
    params.sunColor        = sunColor;
    params.ambientIntensity = ambientIntensity;
    params.skyColor        = skyColor;
    params.vertexCount     = vertexCount;
    params.frameCount      = frameCount;
    params.torchCount      = currentTorchCount;
    params.restirSamples   = restirSamples;
    params.restirBounces   = restirBounces;

    std::memcpy(engine->lightParamsMapped, &params, sizeof(LightParams));

    // ------------------------------------------------------------------
    // 3. Update the compute descriptor set
    //    binding 0 – TLAS (acceleration structure)
    //    binding 1 – light params UBO
    //    binding 2 – vertex buffer (SSBO, read-write)
    //    binding 3 – reservoir buffer (SSBO, read-write)
    //    binding 4 – torch light buffer (SSBO, read-only)
    // ------------------------------------------------------------------

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures    = &tlas.handle;

    VkWriteDescriptorSet descWrites[5] = {};

    descWrites[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[0].pNext           = &asWrite;
    descWrites[0].dstSet          = engine->computeDescSet;
    descWrites[0].dstBinding      = 0;
    descWrites[0].descriptorCount = 1;
    descWrites[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    VkDescriptorBufferInfo lightBufInfo{};
    lightBufInfo.buffer = engine->lightParamsBuffer.buffer;
    lightBufInfo.offset = 0;
    lightBufInfo.range  = sizeof(LightParams);

    descWrites[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[1].dstSet          = engine->computeDescSet;
    descWrites[1].dstBinding      = 1;
    descWrites[1].descriptorCount = 1;
    descWrites[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descWrites[1].pBufferInfo     = &lightBufInfo;

    VkDescriptorBufferInfo vertBufInfo{};
    vertBufInfo.buffer = mesh.vertexBuffer.buffer;
    vertBufInfo.offset = 0;
    vertBufInfo.range  = VK_WHOLE_SIZE;

    descWrites[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[2].dstSet          = engine->computeDescSet;
    descWrites[2].dstBinding      = 2;
    descWrites[2].descriptorCount = 1;
    descWrites[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descWrites[2].pBufferInfo     = &vertBufInfo;

    VkDescriptorBufferInfo reservoirBufInfo{};
    reservoirBufInfo.buffer = mesh.reservoirBuffer.buffer;
    reservoirBufInfo.offset = 0;
    reservoirBufInfo.range  = VK_WHOLE_SIZE;

    descWrites[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[3].dstSet          = engine->computeDescSet;
    descWrites[3].dstBinding      = 3;
    descWrites[3].descriptorCount = 1;
    descWrites[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descWrites[3].pBufferInfo     = &reservoirBufInfo;

    VkDescriptorBufferInfo torchBufInfo{};
    torchBufInfo.buffer = engine->torchLightBuffer.buffer;
    torchBufInfo.offset = 0;
    torchBufInfo.range  = VK_WHOLE_SIZE;

    descWrites[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrites[4].dstSet          = engine->computeDescSet;
    descWrites[4].dstBinding      = 4;
    descWrites[4].descriptorCount = 1;
    descWrites[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descWrites[4].pBufferInfo     = &torchBufInfo;

    vkUpdateDescriptorSets(engine->device, 5, descWrites, 0, nullptr);

    // ------------------------------------------------------------------
    // 4. Record & submit the compute command buffer
    // ------------------------------------------------------------------
    VkCommandBuffer cmd = engine->beginSingleTimeCommands();

    VkPipeline pipeline = useReSTIR ? engine->computePipeline : engine->computePipelineSimple;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            engine->computePipelineLayout,
                            0, 1, &engine->computeDescSet, 0, nullptr);

    VkBufferMemoryBarrier preBarrier{};
    preBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    preBarrier.srcAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    preBarrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    preBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarrier.buffer              = mesh.vertexBuffer.buffer;
    preBarrier.offset              = 0;
    preBarrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 1, &preBarrier, 0, nullptr);

    uint32_t groupCount = (vertexCount + 63) / 64;
    vkCmdDispatch(cmd, groupCount, 1, 1);

    VkBufferMemoryBarrier postBarrier{};
    postBarrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    postBarrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    postBarrier.dstAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    postBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarrier.buffer              = mesh.vertexBuffer.buffer;
    postBarrier.offset              = 0;
    postBarrier.size                = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                         0, 0, nullptr, 1, &postBarrier, 0, nullptr);

    engine->endSingleTimeCommands(cmd);
}

// ---------------------------------------------------------------------------
// rebuildAccelStructures – tear down and rebuild every AS from scratch
// ---------------------------------------------------------------------------

void RayTracing::rebuildAccelStructures(World& world) {
    vkDeviceWaitIdle(engine->device);

    // ------------------------------------------------------------------
    // 1. Destroy old structures
    // ------------------------------------------------------------------
    for (auto& as : blasList) {
        if (as.handle != VK_NULL_HANDLE) {
            engine->rtFuncs.vkDestroyAccelerationStructureKHR(engine->device, as.handle, nullptr);
        }
        if (as.buffer.buffer != VK_NULL_HANDLE) {
            engine->destroyBuffer(as.buffer);
        }
    }
    blasList.clear();

    if (tlas.handle != VK_NULL_HANDLE) {
        engine->rtFuncs.vkDestroyAccelerationStructureKHR(engine->device, tlas.handle, nullptr);
        tlas.handle = VK_NULL_HANDLE;
    }
    if (tlas.buffer.buffer != VK_NULL_HANDLE) {
        engine->destroyBuffer(tlas.buffer);
        tlas.buffer = {};
    }
    accelBuilt = false;

    // ------------------------------------------------------------------
    // 2. Build a BLAS for each mesh that has geometry
    // ------------------------------------------------------------------
    std::vector<ChunkMesh*> validMeshes;

    for (ChunkMesh* mesh : world.meshesForRendering) {
        if (!mesh || mesh->vertices.empty() || !mesh->uploaded) continue;
        blasList.push_back(buildBLAS(*mesh));
        validMeshes.push_back(mesh);
    }

    if (blasList.empty()) return;

    // ------------------------------------------------------------------
    // 3. Build the TLAS over all BLASes
    // ------------------------------------------------------------------
    buildTLAS(validMeshes, blasList);
    accelBuilt = true;
}

// ---------------------------------------------------------------------------
// updateLighting – dispatch compute lighting for every visible chunk
// ---------------------------------------------------------------------------

void RayTracing::updateLighting(World& world, const glm::vec3& sunDir,
                                float sunIntensity, const glm::vec3& sunColor,
                                float ambientIntensity, const glm::vec3& skyColor,
                                bool useReSTIR, uint32_t restirSamples, uint32_t restirBounces) {
    if (!accelBuilt) return;

    for (ChunkMesh* mesh : world.meshesForRendering) {
        if (!mesh || mesh->vertices.empty() || !mesh->uploaded) continue;
        computeVertexLighting(*mesh, sunDir, sunIntensity, sunColor, ambientIntensity, skyColor, useReSTIR, restirSamples, restirBounces);
    }

    if (useReSTIR) {
        frameCount++;
    }
}

// ---------------------------------------------------------------------------
// ensureTorchBuffer – create the persistent torch light SSBO (CPU-visible)
// ---------------------------------------------------------------------------

void RayTracing::ensureTorchBuffer() {
    if (engine->torchLightBuffer.buffer != VK_NULL_HANDLE) return;

    VkDeviceSize bufferSize = sizeof(TorchLightGPU) * MAX_TORCHES;

    engine->torchLightBuffer = engine->createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    vmaMapMemory(engine->allocator, engine->torchLightBuffer.allocation, &engine->torchLightMapped);

    std::memset(engine->torchLightMapped, 0, bufferSize);
}

// ---------------------------------------------------------------------------
// uploadTorchData – collect all torches from the world and upload to SSBO
// ---------------------------------------------------------------------------

uint32_t RayTracing::uploadTorchData(World& world) {
    ensureTorchBuffer();

    TorchLightGPU torchData[MAX_TORCHES];
    uint32_t count = 0;

    for (const auto& [pos, data] : world.torches) {
        if (count >= MAX_TORCHES) break;

        torchData[count].posX = static_cast<float>(pos.x) + 0.5f;
        torchData[count].posY = static_cast<float>(pos.y) + 0.8f;
        torchData[count].posZ = static_cast<float>(pos.z) + 0.5f;
        torchData[count].radius = data.radius;
        torchData[count].colorR = data.color.r;
        torchData[count].colorG = data.color.g;
        torchData[count].colorB = data.color.b;
        torchData[count].intensity = data.intensity;
        count++;
    }

    std::memcpy(engine->torchLightMapped, torchData, sizeof(TorchLightGPU) * count);

    if (count < MAX_TORCHES) {
        std::memset(
            reinterpret_cast<uint8_t*>(engine->torchLightMapped) + sizeof(TorchLightGPU) * count,
            0,
            sizeof(TorchLightGPU) * (MAX_TORCHES - count));
    }

    currentTorchCount = count;
    return count;
}
