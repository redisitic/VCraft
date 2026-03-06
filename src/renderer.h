#pragma once

#include "engine.h"

class World;
class RayTracing;
class UIOverlay;

// All computed day/night cycle state, passed from main loop to renderer
struct DayNightState {
    float timeOfDay;        // 0-24 hour clock
    glm::vec3 sunDir;       // world-space direction TOWARD the sun
    glm::vec3 moonDir;      // world-space direction TOWARD the moon
    glm::vec3 skyColor;     // sky zenith color
    float sunIntensity;     // 0 at night, 1 at noon
};

class Renderer {
public:
    void init(Engine* engine);
    void drawFrame(World& world, RayTracing& rt, const DayNightState& dayNight, UIOverlay* uiOverlay);
    void cleanup();

private:
    Engine* engine = nullptr;
    void updateUniformBuffer(uint32_t frameIndex, const DayNightState& dayNight);
    void recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex, World& world,
                             const DayNightState& dayNight, UIOverlay* uiOverlay);
};
