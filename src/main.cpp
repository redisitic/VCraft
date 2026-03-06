#include "engine.h"
#include "world.h"
#include "camera.h"
#include "input.h"
#include "texture.h"
#include "renderer.h"
#include "raytracing.h"
#include "console.h"
#include "ui_overlay.h"
#include "player.h"

#include <cmath>
#include <sstream>
#include <algorithm>
#include <string>

Camera gCamera;

// ---------------------------------------------------------------------------
// Color temperature to RGB (Tanner Helland approximation)
// ---------------------------------------------------------------------------
static glm::vec3 colorTempToRGB(float kelvin) {
    float temp = kelvin / 100.0f;
    float r, g, b;

    if (temp <= 66.0f) {
        r = 255.0f;
        g = 99.4708025861f * std::log(temp) - 161.1195681661f;
        if (temp <= 19.0f) {
            b = 0.0f;
        } else {
            b = 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;
        }
    } else {
        r = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);
        g = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);
        b = 255.0f;
    }

    r = std::clamp(r, 0.0f, 255.0f) / 255.0f;
    g = std::clamp(g, 0.0f, 255.0f) / 255.0f;
    b = std::clamp(b, 0.0f, 255.0f) / 255.0f;

    return glm::vec3(r, g, b);
}

static bool parseHexColor(const std::string& str, glm::vec3& out) {
    std::string hex = str;
    if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
    if (hex.size() != 6) return false;

    unsigned int val = 0;
    for (char c : hex) {
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        else return false;
    }

    out.x = ((val >> 16) & 0xFF) / 255.0f;
    out.y = ((val >> 8) & 0xFF) / 255.0f;
    out.z = (val & 0xFF) / 255.0f;
    return true;
}

static bool parseColorTemp(const std::string& str, glm::vec3& out) {
    if (str.empty()) return false;
    char last = str.back();
    if (last != 'K' && last != 'k') return false;

    std::string numStr = str.substr(0, str.size() - 1);
    try {
        float kelvin = std::stof(numStr);
        if (kelvin < 1000.0f || kelvin > 40000.0f) return false;
        out = colorTempToRGB(kelvin);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parseNamedColor(const std::string& name, glm::vec3& out) {
    if (name == "red")           { out = glm::vec3(1.0f, 0.0f, 0.0f); return true; }
    if (name == "green")         { out = glm::vec3(0.0f, 1.0f, 0.0f); return true; }
    if (name == "blue")          { out = glm::vec3(0.0f, 0.0f, 1.0f); return true; }
    if (name == "yellow")        { out = glm::vec3(1.0f, 1.0f, 0.0f); return true; }
    if (name == "white")         { out = glm::vec3(1.0f, 1.0f, 1.0f); return true; }
    if (name == "orange")        { out = glm::vec3(1.0f, 0.5f, 0.0f); return true; }
    if (name == "purple" || name == "magenta") { out = glm::vec3(1.0f, 0.0f, 1.0f); return true; }
    if (name == "cyan")          { out = glm::vec3(0.0f, 1.0f, 1.0f); return true; }
    if (name == "pink")          { out = glm::vec3(1.0f, 0.4f, 0.7f); return true; }
    return false;
}

// ---------------------------------------------------------------------------
// Day/night cycle helpers
// ---------------------------------------------------------------------------

static constexpr float PI = 3.14159265358979323846f;

// Compute a DayNightState from the current time of day (0-24 hours).
// 0/24  = midnight
// 6     = sunrise
// 12    = noon
// 18    = sunset
static DayNightState computeDayNight(float timeOfDay) {
    DayNightState state{};
    state.timeOfDay = timeOfDay;

    // Convert time to an angle: 0h = bottom (nadir), 6h = horizon, 12h = zenith, 18h = horizon
    // Sun angle: 0 at midnight -> PI at noon -> 2PI at next midnight
    float sunAngle = (timeOfDay / 24.0f) * 2.0f * PI - PI * 0.5f;
    // sunAngle: at t=0  -> -PI/2 (pointing down)
    //           at t=6  ->  0    (horizon east)
    //           at t=12 ->  PI/2 (zenith)
    //           at t=18 ->  PI   (horizon west)

    // Sun direction: rotate in the XY plane (Y=up)
    // Sun goes East (positive X) at sunrise, overhead at noon, West (negative X) at sunset
    state.sunDir = glm::normalize(glm::vec3(
        cosf(sunAngle),     // X: east-west
        sinf(sunAngle),     // Y: up-down
        0.3f
    ));

    state.moonDir = -state.sunDir;
    state.moonDir.y = -state.moonDir.y;
    float moonAngle = sunAngle + PI;
    state.moonDir = glm::normalize(glm::vec3(
        cosf(moonAngle),
        sinf(moonAngle),
        -0.3f
    ));

    float sunHeight = sinf(sunAngle);

    if (sunHeight <= 0.0f) {
        state.sunIntensity = 0.0f;
    } else if (sunHeight < 0.3f) {
        state.sunIntensity = sunHeight / 0.3f;
    } else {
        state.sunIntensity = 1.0f;
    }

    glm::vec3 daySky(0.4f, 0.65f, 1.0f);       // bright sky blue
    glm::vec3 nightSky(0.01f, 0.01f, 0.03f);    // very dark blue
    glm::vec3 sunriseSky(0.8f, 0.45f, 0.2f);    // warm orange

    if (state.sunIntensity <= 0.0f) {
        state.skyColor = nightSky;
    } else if (state.sunIntensity < 1.0f) {
        float t = state.sunIntensity;
        if (t < 0.5f) {
            state.skyColor = glm::mix(nightSky, sunriseSky, t * 2.0f);
        } else {
            state.skyColor = glm::mix(sunriseSky, daySky, (t - 0.5f) * 2.0f);
        }
    } else {
        state.skyColor = daySky;
    }

    return state;
}

static void getLightParams(const DayNightState& dn,
                           float& outSunIntensity, glm::vec3& outSunColor,
                           float& outAmbientIntensity, glm::vec3& outSkyColor) {
    if (dn.sunIntensity <= 0.0f) {
        outSunIntensity = 0.0f;
        outSunColor = glm::vec3(0.0f);
    } else if (dn.sunIntensity < 1.0f) {
        outSunIntensity = dn.sunIntensity * 0.8f;
        outSunColor = glm::mix(glm::vec3(1.0f, 0.4f, 0.1f), glm::vec3(1.0f, 0.95f, 0.85f), dn.sunIntensity);
    } else {
        outSunIntensity = 1.0f;
        outSunColor = glm::vec3(1.0f, 0.95f, 0.85f);
    }

    float nightFactor = 1.0f - dn.sunIntensity;
    outAmbientIntensity = glm::mix(0.15f, 0.04f, nightFactor);

    outSkyColor = glm::mix(
        glm::vec3(0.5f, 0.7f, 1.0f),
        glm::vec3(0.05f, 0.05f, 0.15f),
        nightFactor
    );
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main() {
    Engine engine;
    World world;
    Camera& camera = gCamera;
    InputHandler input;
    TextureLoader textureLoader;
    Renderer renderer;
    RayTracing rayTracing;
    Console console;
    UIOverlay uiOverlay;
    Player player;

    try {
        engine.init();

        input.init(engine.window, &camera);
        input.framebufferResized = &engine.framebufferResized;

        console.init(engine.window);
        input.console = &console;

        textureLoader.init(&engine);
        textureLoader.loadAtlas("ogsmp-RGBA.png");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = engine.textureAtlas.view;
        imageInfo.sampler = engine.textureSampler;

        VkWriteDescriptorSet descWrite{};
        descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descWrite.dstSet = engine.textureDescSet;
        descWrite.dstBinding = 0;
        descWrite.dstArrayElement = 0;
        descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descWrite.descriptorCount = 1;
        descWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(engine.device, 1, &descWrite, 0, nullptr);

        world.init(&engine);
        renderer.init(&engine);
        rayTracing.init(&engine);
        uiOverlay.init(&engine);

        camera.position = glm::vec3(8.0f, 80.0f, 8.0f);
        camera.updateVectors();

        player.init(&world, &camera);
        input.player = &player;

        float timeOfDay = 12.0f;

        bool useReSTIR = false;
        uint32_t restirSamples = 1;
        uint32_t restirBounces = 1;
        bool showHUD = true;

        console.setCommandHandler([&](const std::string& cmd) {
            // Parse commands: "/time set X", "/lighting simple|restir [samples] [bounces]"
            // cmd is the text AFTER the leading '/' (e.g. "time set 12.0")
            std::istringstream iss(cmd);
            std::string word1, word2;
            iss >> word1 >> word2;

            if (word1 == "time" && word2 == "set") {
                float val = 0.0f;
                if (iss >> val) {
                    if (val < 0.0f) val = 0.0f;
                    if (val > 24.0f) val = 24.0f;
                    timeOfDay = val;
                    console.addMessage("Time set to " + std::to_string(val));
                } else {
                    console.addMessage("Usage: /time set <0.0-24.0>");
                }
            } else if (word1 == "hud") {
                if (word2 == "on") {
                    showHUD = true;
                    console.addMessage("HUD enabled");
                } else if (word2 == "off") {
                    showHUD = false;
                    console.addMessage("HUD disabled");
                } else {
                    console.addMessage("Usage: /hud on|off");
                }
            } else if (word1 == "lighting") {
                if (word2 == "simple") {
                    useReSTIR = false;
                    console.addMessage("Lighting mode: simple (shadow + AO)");
                } else if (word2 == "restir") {
                    useReSTIR = true;
                    uint32_t samples = 1;
                    if (iss >> samples) {
                        restirSamples = samples;
                        uint32_t bounces = 1;
                        if (iss >> bounces) {
                            restirBounces = bounces;
                        }
                    }
                    rayTracing.resetReservoirs();
                    console.addMessage("Lighting mode: ReSTIR (GI) with " + std::to_string(restirSamples) + " samples, " + std::to_string(restirBounces) + " bounces");
                } else {
                    console.addMessage("Usage: /lighting simple|restir [samples] [bounces]");
                }
            } else if (word1 == "give") {
                // /give <blockname> [count]
                // /give torch [color_spec] [count]
                std::string blockName = word2;
                // Convert to lowercase
                std::transform(blockName.begin(), blockName.end(), blockName.begin(), ::tolower);

                if (blockName == "torch") {
                    // Parse optional color specifier and count
                    // Possible forms:
                    //   /give torch              -> default 3000K warm white, 64
                    //   /give torch red          -> named color, 64
                    //   /give torch #FF00FF      -> hex color, 64
                    //   /give torch 8000K        -> color temp, 64
                    //   /give torch red 32       -> named color, 32
                    //   /give torch #FF00FF 32   -> hex color, 32
                    //   /give torch 32           -> default color, 32

                    glm::vec3 torchColor = colorTempToRGB(3000.0f); // default warm white
                    int count = 64;

                    std::string arg;
                    if (iss >> arg) {
                        std::string argLower = arg;
                        std::transform(argLower.begin(), argLower.end(), argLower.begin(), ::tolower);

                        glm::vec3 parsedColor;
                        // Try named color
                        if (parseNamedColor(argLower, parsedColor)) {
                            torchColor = parsedColor;
                            int tmp = 0;
                            if (iss >> tmp && tmp > 0) count = std::min(tmp, 64 * HOTBAR_SLOTS);
                        }
                        // Try hex color
                        else if (parseHexColor(arg, parsedColor)) {
                            torchColor = parsedColor;
                            int tmp = 0;
                            if (iss >> tmp && tmp > 0) count = std::min(tmp, 64 * HOTBAR_SLOTS);
                        }
                        // Try color temperature
                        else if (parseColorTemp(arg, parsedColor)) {
                            torchColor = parsedColor;
                            int tmp = 0;
                            if (iss >> tmp && tmp > 0) count = std::min(tmp, 64 * HOTBAR_SLOTS);
                        }
                        // Try as count (plain number)
                        else {
                            try {
                                int tmp = std::stoi(arg);
                                if (tmp > 0) count = std::min(tmp, 64 * HOTBAR_SLOTS);
                            } catch (...) {
                                console.addMessage("Unknown torch color: " + arg);
                                console.addMessage("Usage: /give torch [red|blue|green|yellow|white|orange|purple|cyan|#RRGGBB|NNNNk] [count]");
                                return;
                            }
                        }
                    }

                    int added = player.inventory.giveTorch(count, torchColor);
                    console.addMessage("Gave " + std::to_string(added) + " torch");
                } else {
                    int count = 64;
                    int tmp = 0;
                    if (iss >> tmp && tmp > 0) count = std::min(tmp, 64 * HOTBAR_SLOTS);

                    BlockType bt = BlockType::Air;
                    if (blockName == "grass") bt = BlockType::Grass;
                    else if (blockName == "stone") bt = BlockType::Stone;
                    else if (blockName == "dirt") bt = BlockType::Dirt;
                    else if (blockName == "wood_planks" || blockName == "planks") bt = BlockType::WoodPlanks;
                    else if (blockName == "log" || blockName == "oak_log") bt = BlockType::Log;
                    else if (blockName == "leaves" || blockName == "oak_leaves") bt = BlockType::Leaves;

                    if (bt == BlockType::Air || blockName.empty()) {
                        console.addMessage("Usage: /give <grass|stone|dirt|planks|log|leaves|torch> [count]");
                    } else {
                        int added = player.inventory.give(bt, count);
                        console.addMessage("Gave " + std::to_string(added) + " " + blockName);
                    }
                }
            } else {
                console.addMessage("Unknown command: /" + cmd);
            }
        });

        auto startTime = std::chrono::high_resolution_clock::now();
        auto lastTime = startTime;

        float rtUpdateTimer = 0.0f;
        float rtUpdateInterval = 0.5f;
        float totalTime = 0.0f;
        bool firstFrame = true;
        bool rtLightingDirty = true;

        float prevTimeOfDay = timeOfDay;

        while (!input.shouldClose()) {
            glfwPollEvents();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;
            deltaTime = std::min(deltaTime, 0.1f);
            input.update(deltaTime);
            player.update(deltaTime);
            totalTime += deltaTime;
            if (timeOfDay != prevTimeOfDay) {
                rtLightingDirty = true;
                prevTimeOfDay = timeOfDay;
                rayTracing.resetReservoirs();
            }

            DayNightState dayNight = computeDayNight(timeOfDay);

            world.update(camera.position);
            rtUpdateTimer += deltaTime;
            bool blockChanged = player.blockModified;
            if (blockChanged) player.blockModified = false;

            bool shouldRebuildAS = rtUpdateTimer >= rtUpdateInterval || firstFrame || rtLightingDirty || blockChanged;
            bool shouldUpdateLighting = shouldRebuildAS || useReSTIR;

            if (shouldRebuildAS) {
                rtUpdateTimer = 0.0f;
                firstFrame = false;
                rtLightingDirty = false;

                if (!world.meshesForRendering.empty()) {
                    rayTracing.rebuildAccelStructures(world);
                }
            }

            if (shouldUpdateLighting && !world.meshesForRendering.empty()) {
                if (rayTracing.accelBuilt) {
                    rayTracing.uploadTorchData(world);

                    float rtSunIntensity;
                    glm::vec3 rtSunColor;
                    float rtAmbientIntensity;
                    glm::vec3 rtSkyColor;
                    getLightParams(dayNight, rtSunIntensity, rtSunColor, rtAmbientIntensity, rtSkyColor);

                    rayTracing.updateLighting(world, dayNight.sunDir,
                                              rtSunIntensity, rtSunColor,
                                              rtAmbientIntensity, rtSkyColor,
                                              useReSTIR, restirSamples, restirBounces);
                }
            }
            uiOverlay.buildConsoleUI(console, totalTime);
            if (showHUD && !console.isOpen()) {
                uiOverlay.buildCrosshair();
                uiOverlay.buildHotbar(player.inventory);
            } else if (showHUD && console.isOpen()) {
                uiOverlay.buildHotbar(player.inventory);
            }
            renderer.drawFrame(world, rayTracing, dayNight, &uiOverlay);
        }

        vkDeviceWaitIdle(engine.device);
        rayTracing.cleanup();
        world.cleanup();
        uiOverlay.cleanup();
        textureLoader.cleanup();
        renderer.cleanup();
        engine.cleanup();

    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
