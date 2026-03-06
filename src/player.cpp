#include "player.h"
#include "world.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// Inventory
// ============================================================================

void Inventory::init() {
    for (auto& slot : hotbar) {
        slot.type = BlockType::Air;
        slot.count = 0;
    }
    selectedSlot = 0;
}

int Inventory::give(BlockType type, int count) {
    if (type == BlockType::Air || count <= 0) return 0;

    int remaining = count;
    for (auto& slot : hotbar) {
        if (remaining <= 0) break;
        if (slot.type == type && slot.count < 64) {
            if (type == BlockType::Torch) continue;
            int canAdd = std::min(remaining, 64 - slot.count);
            slot.count += canAdd;
            remaining -= canAdd;
        }
    }

    for (auto& slot : hotbar) {
        if (remaining <= 0) break;
        if (slot.type == BlockType::Air || slot.count == 0) {
            slot.type = type;
            int canAdd = std::min(remaining, 64);
            slot.count = canAdd;
            remaining -= canAdd;
        }
    }

    return count - remaining;
}

int Inventory::giveTorch(int count, const glm::vec3& color) {
    if (count <= 0) return 0;

    int remaining = count;

    for (auto& slot : hotbar) {
        if (remaining <= 0) break;
        if (slot.type == BlockType::Torch && slot.count < 64) {
            glm::vec3 diff = slot.torchColor - color;
            if (std::abs(diff.x) < 0.01f && std::abs(diff.y) < 0.01f && std::abs(diff.z) < 0.01f) {
                int canAdd = std::min(remaining, 64 - slot.count);
                slot.count += canAdd;
                remaining -= canAdd;
            }
        }
    }

    for (auto& slot : hotbar) {
        if (remaining <= 0) break;
        if (slot.type == BlockType::Air || slot.count == 0) {
            slot.type = BlockType::Torch;
            slot.torchColor = color;
            int canAdd = std::min(remaining, 64);
            slot.count = canAdd;
            remaining -= canAdd;
        }
    }

    return count - remaining;
}

BlockType Inventory::getSelectedBlock() const {
    const auto& slot = hotbar[selectedSlot];
    if (slot.count > 0) return slot.type;
    return BlockType::Air;
}

glm::vec3 Inventory::getSelectedTorchColor() const {
    const auto& slot = hotbar[selectedSlot];
    return slot.torchColor;
}

BlockType Inventory::consumeSelected() {
    auto& slot = hotbar[selectedSlot];
    if (slot.count <= 0 || slot.type == BlockType::Air) return BlockType::Air;

    BlockType t = slot.type;
    slot.count--;
    if (slot.count <= 0) {
        slot.type = BlockType::Air;
        slot.count = 0;
    }
    return t;
}

// ============================================================================
// Player
// ============================================================================

void Player::init(World* w, Camera* cam) {
    world = w;
    camera = cam;
    onGround = false;
    velocity = glm::vec3(0.0f);

    position = camera->position - glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);

    inventory.init();
}

bool Player::collidesAt(const glm::vec3& feetPos) const {
    float halfW = PLAYER_WIDTH * 0.5f;
    float eps = 0.001f;
    int minX = static_cast<int>(std::floor(feetPos.x - halfW + eps));
    int maxX = static_cast<int>(std::floor(feetPos.x + halfW - eps));
    int minY = static_cast<int>(std::floor(feetPos.y + eps));
    int maxY = static_cast<int>(std::floor(feetPos.y + PLAYER_HEIGHT - eps));
    int minZ = static_cast<int>(std::floor(feetPos.z - halfW + eps));
    int maxZ = static_cast<int>(std::floor(feetPos.z + halfW - eps));

    for (int bx = minX; bx <= maxX; bx++) {
        for (int by = minY; by <= maxY; by++) {
            for (int bz = minZ; bz <= maxZ; bz++) {
                if (by < 0 || by >= CHUNK_SIZE_Y) continue;
                BlockType bt = world->getBlock(bx, by, bz);
                if (bt != BlockType::Air && bt != BlockType::Torch) {
                    return true;
                }
            }
        }
    }
    return false;
}

float Player::moveAxis(int axis, float delta) {
    if (delta == 0.0f) return 0.0f;

    glm::vec3 newPos = position;
    newPos[axis] += delta;

    if (!collidesAt(newPos)) {
        position[axis] = newPos[axis];
        return delta;
    }
    float sign = (delta > 0.0f) ? 1.0f : -1.0f;
    float halfW = PLAYER_WIDTH * 0.5f;
    float step = 0.01f;
    float moved = 0.0f;
    float absDelta = std::abs(delta);

    while (moved + step <= absDelta) {
        glm::vec3 testPos = position;
        testPos[axis] += sign * (moved + step);
        if (collidesAt(testPos)) {
            break;
        }
        moved += step;
    }

    position[axis] += sign * moved;
    return sign * moved;
}

void Player::moveForward(float deltaTime) {
    glm::vec3 forward = camera->front;
    forward.y = 0.0f;
    if (glm::length(forward) > 0.001f) {
        forward = glm::normalize(forward);
    }
    velocity.x += forward.x * WALK_SPEED * 20.0f * deltaTime;
    velocity.z += forward.z * WALK_SPEED * 20.0f * deltaTime;
}

void Player::moveBack(float deltaTime) {
    glm::vec3 forward = camera->front;
    forward.y = 0.0f;
    if (glm::length(forward) > 0.001f) {
        forward = glm::normalize(forward);
    }
    velocity.x -= forward.x * WALK_SPEED * 20.0f * deltaTime;
    velocity.z -= forward.z * WALK_SPEED * 20.0f * deltaTime;
}

void Player::moveLeft(float deltaTime) {
    glm::vec3 right = camera->right;
    right.y = 0.0f;
    if (glm::length(right) > 0.001f) {
        right = glm::normalize(right);
    }
    velocity.x -= right.x * WALK_SPEED * 20.0f * deltaTime;
    velocity.z -= right.z * WALK_SPEED * 20.0f * deltaTime;
}

void Player::moveRight(float deltaTime) {
    glm::vec3 right = camera->right;
    right.y = 0.0f;
    if (glm::length(right) > 0.001f) {
        right = glm::normalize(right);
    }
    velocity.x += right.x * WALK_SPEED * 20.0f * deltaTime;
    velocity.z += right.z * WALK_SPEED * 20.0f * deltaTime;
}

void Player::jump() {
    if (onGround) {
        velocity.y = JUMP_VELOCITY;
        onGround = false;
    }
}

void Player::update(float deltaTime) {
    velocity.y -= GRAVITY * deltaTime;
    if (velocity.y < -TERMINAL_VEL) velocity.y = -TERMINAL_VEL;
    float damping = std::pow(0.001f, deltaTime);
    velocity.x *= damping;
    velocity.z *= damping;
    if (std::abs(velocity.x) < 0.01f) velocity.x = 0.0f;
    if (std::abs(velocity.z) < 0.01f) velocity.z = 0.0f;
    float dx = velocity.x * deltaTime;
    float dy = velocity.y * deltaTime;
    float dz = velocity.z * deltaTime;
    moveAxis(0, dx);
    moveAxis(2, dz);
    float actualDy = moveAxis(1, dy);
    if (velocity.y <= 0.0f && std::abs(actualDy) < std::abs(dy) - 0.0001f) {
        onGround = true;
        velocity.y = 0.0f;
    } else if (velocity.y > 0.0f && std::abs(actualDy) < std::abs(dy) - 0.0001f) {
        velocity.y = 0.0f;
    } else {
        onGround = false;
    }
    if (position.y < -10.0f) {
        position.y = 100.0f;
        velocity.y = 0.0f;
    }
    camera->position = position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
}

// ============================================================================
// DDA voxel raycast (Amanatides & Woo)
// ============================================================================

RaycastHit Player::raycast(float maxDist) const {
    RaycastHit result;
    result.hit = false;

    glm::vec3 origin = camera->position;
    glm::vec3 dir = glm::normalize(camera->front);
    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));
    int stepX = (dir.x >= 0.0f) ? 1 : -1;
    int stepY = (dir.y >= 0.0f) ? 1 : -1;
    int stepZ = (dir.z >= 0.0f) ? 1 : -1;
    float tDeltaX = (dir.x != 0.0f) ? std::abs(1.0f / dir.x) : 1e30f;
    float tDeltaY = (dir.y != 0.0f) ? std::abs(1.0f / dir.y) : 1e30f;
    float tDeltaZ = (dir.z != 0.0f) ? std::abs(1.0f / dir.z) : 1e30f;
    float tMaxX, tMaxY, tMaxZ;
    if (dir.x > 0.0f)
        tMaxX = (static_cast<float>(x + 1) - origin.x) / dir.x;
    else if (dir.x < 0.0f)
        tMaxX = (static_cast<float>(x) - origin.x) / dir.x;
    else
        tMaxX = 1e30f;

    if (dir.y > 0.0f)
        tMaxY = (static_cast<float>(y + 1) - origin.y) / dir.y;
    else if (dir.y < 0.0f)
        tMaxY = (static_cast<float>(y) - origin.y) / dir.y;
    else
        tMaxY = 1e30f;

    if (dir.z > 0.0f)
        tMaxZ = (static_cast<float>(z + 1) - origin.z) / dir.z;
    else if (dir.z < 0.0f)
        tMaxZ = (static_cast<float>(z) - origin.z) / dir.z;
    else
        tMaxZ = 1e30f;
    float t = 0.0f;
    glm::ivec3 normal(0);

    while (t < maxDist) {
        if (tMaxX < tMaxY) {
            if (tMaxX < tMaxZ) {
                x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                normal = glm::ivec3(-stepX, 0, 0);
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                normal = glm::ivec3(0, 0, -stepZ);
            }
        } else {
            if (tMaxY < tMaxZ) {
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                normal = glm::ivec3(0, -stepY, 0);
            } else {
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                normal = glm::ivec3(0, 0, -stepZ);
            }
        }

        if (t > maxDist) break;
        BlockType bt = world->getBlock(x, y, z);
        if (bt != BlockType::Air) {
            result.hit = true;
            result.blockPos = glm::ivec3(x, y, z);
            result.faceNormal = normal;
            return result;
        }
    }

    return result;
}

// ============================================================================
// Block interaction
// ============================================================================

void Player::breakBlock() {
    RaycastHit hit = raycast(REACH_DISTANCE);
    if (!hit.hit) return;

    BlockType broken = world->getBlock(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
    if (broken == BlockType::Air) return;
    if (broken == BlockType::Torch) {
        const TorchData* td = world->getTorch(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
        glm::vec3 color = td ? td->color : glm::vec3(1.0f, 0.75f, 0.45f);
        world->removeTorch(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
        world->setBlock(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z, BlockType::Air);
        inventory.giveTorch(1, color);
    } else {
        world->setBlock(hit.blockPos.x, hit.blockPos.y, hit.blockPos.z, BlockType::Air);
        inventory.give(broken, 1);
    }

    blockModified = true;
}

void Player::placeBlock() {
    RaycastHit hit = raycast(REACH_DISTANCE);
    if (!hit.hit) return;
    glm::ivec3 placePos = hit.blockPos + hit.faceNormal;
    if (placePos.y < 0 || placePos.y >= 128) return;
    BlockType existing = world->getBlock(placePos.x, placePos.y, placePos.z);
    if (existing != BlockType::Air && existing != BlockType::Torch) return;
    BlockType toPlace = inventory.getSelectedBlock();
    if (toPlace == BlockType::Air) return;
    world->setBlock(placePos.x, placePos.y, placePos.z, toPlace);
    if (toPlace != BlockType::Torch && collidesAt(position)) {
        world->setBlock(placePos.x, placePos.y, placePos.z, BlockType::Air);
        return;
    }
    if (toPlace == BlockType::Torch) {
        glm::vec3 color = inventory.getSelectedTorchColor();
        TorchData td;
        td.color = color;
        world->setTorch(placePos.x, placePos.y, placePos.z, td);
    }
    inventory.consumeSelected();

    blockModified = true;
}
