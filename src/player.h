#pragma once

#include "camera.h"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// Forward declarations
class World;
enum class BlockType : uint8_t;

// ============================================================================
// Inventory
// ============================================================================

struct ItemStack {
    BlockType type;     // BlockType::Air means empty slot
    int count = 0;
    glm::vec3 torchColor = glm::vec3(1.0f, 0.75f, 0.45f); // only used for Torch type
};

constexpr int HOTBAR_SLOTS = 9;

struct Inventory {
    std::array<ItemStack, HOTBAR_SLOTS> hotbar;
    int selectedSlot = 0;   // 0-8

    void init();
    // Add items, returns number actually added
    int give(BlockType type, int count);
    // Add torch with specific color, returns number added
    int giveTorch(int count, const glm::vec3& color);
    // Get the block type in the selected slot (Air if empty)
    BlockType getSelectedBlock() const;
    // Get the torch color of the selected slot
    glm::vec3 getSelectedTorchColor() const;
    // Consume one item from selected slot; returns the type consumed (Air if nothing)
    BlockType consumeSelected();
};

// ============================================================================
// Raycast result
// ============================================================================

struct RaycastHit {
    bool hit = false;
    glm::ivec3 blockPos;    // the block that was hit
    glm::ivec3 faceNormal;  // outward normal of the face hit (for placement)
};

// ============================================================================
// Player
// ============================================================================

// Player hitbox: 0.6 wide (±0.3 from center), 1.8 tall
// Eyes at +1.62 from feet
constexpr float PLAYER_WIDTH  = 0.6f;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_EYE_HEIGHT = 1.62f;

constexpr float GRAVITY      = 28.0f;   // blocks/s^2 (Minecraft-ish)
constexpr float JUMP_VELOCITY = 8.5f;   // blocks/s (slightly > sqrt(2*G*1.25) for 1.25 block jump)
constexpr float WALK_SPEED   = 4.317f;  // blocks/s (Minecraft walk speed)
constexpr float TERMINAL_VEL = 78.4f;   // blocks/s max fall speed

constexpr float REACH_DISTANCE = 5.0f;  // max block interaction distance (blocks)

class Player {
public:
    void init(World* world, Camera* camera);

    // Call every frame; handles gravity, collision, camera sync
    void update(float deltaTime);

    // Movement inputs (call from InputHandler::update)
    void moveForward(float deltaTime);
    void moveBack(float deltaTime);
    void moveLeft(float deltaTime);
    void moveRight(float deltaTime);
    void jump();

    // Block interaction
    void breakBlock();      // destroy the block we're looking at
    void placeBlock();      // place selected hotbar block on the face we're looking at

    // DDA voxel raycast from eye along camera.front
    RaycastHit raycast(float maxDist) const;

    // Returns true if a block modification happened this frame (for RT rebuild)
    bool blockModified = false;

    // Position of player feet (bottom-center of hitbox)
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    bool onGround = false;

    Inventory inventory;

private:
    World* world = nullptr;
    Camera* camera = nullptr;

    // Check if the player AABB at a given feet position collides with any solid block
    bool collidesAt(const glm::vec3& feetPos) const;

    // Move along one axis with collision response; returns actual displacement
    float moveAxis(int axis, float delta);
};
