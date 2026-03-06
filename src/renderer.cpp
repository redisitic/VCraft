#include "renderer.h"
#include "world.h"
#include "raytracing.h"
#include "camera.h"
#include "ui_overlay.h"

void Renderer::init(Engine* engine) {
    this->engine = engine;
}

void Renderer::updateUniformBuffer(uint32_t frameIndex, const DayNightState& dayNight) {
    extern Camera gCamera;

    UniformBufferObject ubo{};
    float aspect = (float)engine->swapChainExtent.width / (float)engine->swapChainExtent.height;
    glm::mat4 proj = gCamera.getProjectionMatrix(aspect);
    glm::mat4 view = gCamera.getViewMatrix();
    ubo.viewProj = proj * view;
    ubo.camPos = gCamera.position;
    ubo.camRight = gCamera.right;
    ubo.camUp = gCamera.up;
    ubo.camFront = gCamera.front;
    ubo.tanHalfFov = tanf(glm::radians(gCamera.fov) * 0.5f);
    ubo.aspectRatio = aspect;
    ubo.timeOfDay = dayNight.timeOfDay;
    ubo.sunDir = dayNight.sunDir;
    ubo._pad0 = 0.0f;
    ubo.moonDir = dayNight.moonDir;
    ubo._pad1 = 0.0f;
    ubo.skyColor = dayNight.skyColor;
    ubo.sunIntensity = dayNight.sunIntensity;

    memcpy(engine->uniformBuffersMapped[frameIndex], &ubo, sizeof(ubo));
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex, World& world,
                                    const DayNightState& dayNight, UIOverlay* uiOverlay) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = engine->renderPass;
    rpInfo.framebuffer = engine->swapChainFramebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = engine->swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuf, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)engine->swapChainExtent.width;
    viewport.height = (float)engine->swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = engine->swapChainExtent;
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->skyPipeline);

    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine->skyPipelineLayout, 0, 1, &engine->uboDescSets[engine->currentFrame], 0, nullptr);

    vkCmdDraw(cmdBuf, 3, 1, 0, 0);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->graphicsPipeline);

    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine->pipelineLayout, 0, 1, &engine->uboDescSets[engine->currentFrame], 0, nullptr);

    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine->pipelineLayout, 1, 1, &engine->textureDescSet, 0, nullptr);

    for (auto* mesh : world.meshesForRendering) {
        if (!mesh->uploaded || mesh->vertices.empty()) continue;

        VkBuffer vertexBuffers[] = {mesh->vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuf, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmdBuf, static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
    }

    if (uiOverlay) {
        uiOverlay->draw(cmdBuf);
    }

    vkCmdEndRenderPass(cmdBuf);

    if (vkEndCommandBuffer(cmdBuf) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void Renderer::drawFrame(World& world, RayTracing& rt, const DayNightState& dayNight, UIOverlay* uiOverlay) {
    vkWaitForFences(engine->device, 1, &engine->inFlightFences[engine->currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(engine->device, engine->swapChain, UINT64_MAX,
        engine->imageAvailableSemaphores[engine->currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        engine->recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetFences(engine->device, 1, &engine->inFlightFences[engine->currentFrame]);

    updateUniformBuffer(engine->currentFrame, dayNight);

    vkResetCommandBuffer(engine->commandBuffers[engine->currentFrame], 0);
    recordCommandBuffer(engine->commandBuffers[engine->currentFrame], imageIndex, world, dayNight, uiOverlay);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {engine->imageAvailableSemaphores[engine->currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &engine->commandBuffers[engine->currentFrame];

    VkSemaphore signalSemaphores[] = {engine->renderFinishedSemaphores[engine->currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, engine->inFlightFences[engine->currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {engine->swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(engine->presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || engine->framebufferResized) {
        engine->framebufferResized = false;
        engine->recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    engine->currentFrame = (engine->currentFrame + 1) % Engine::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::cleanup() {
}
