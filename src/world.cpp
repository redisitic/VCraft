#include "world.h"
#include <cmath>
#include <cstring>
#include <functional>

// ============================================================================
// Atlas UV helpers
// ============================================================================

BlockFaceUV getBlockFaceUV(int texId) {
    const float atlasSize   = 1024.0f;
    const float tileSize    = 18.0f;
    const int   tilesPerRow = 56;   // 56 * 18 = 1008, last 16px are black padding

    int col = texId % tilesPerRow;
    int row = texId / tilesPerRow;

    // UV coordinates - each tile is 18x18 pixels in a 1024x1024 atlas
    // 56 tiles per row/column, with 16px unused padding at the edges
    float u0 = (col * tileSize) / atlasSize;
    float v0 = (row * tileSize) / atlasSize;
    float u1 = u0 + tileSize / atlasSize;
    float v1 = v0 + tileSize / atlasSize;

    BlockFaceUV result;
    result.uvMin = glm::vec2(u0, v0);
    result.uvMax = glm::vec2(u1, v1);

    return result;
}

// face: 0=+X, 1=-X, 2=+Y(top), 3=-Y(bottom), 4=+Z, 5=-Z
int getBlockFaceTexId(BlockType type, int face) {
    switch (type) {
        case BlockType::Grass:
            if (face == 2) return 0;
            if (face == 3) return 2;
            return 3;
        case BlockType::Stone:
            return 1;
        case BlockType::Dirt:
            return 2;
        case BlockType::WoodPlanks:
            return 4;
        case BlockType::Log:
            if (face == 2 || face == 3) return 21;
            return 20;
        case BlockType::Leaves:
            return 52;
        case BlockType::Torch:
            return 20;
        default:
            return 0;
    }
}

// ============================================================================
// Chunk
// ============================================================================

static uint32_t hashPos(int x, int z) {
    uint32_t h = static_cast<uint32_t>(x * 374761393 + z * 668265263);
    h = (h ^ (h >> 13)) * 1274126177;
    h = h ^ (h >> 16);
    return h;
}

static bool shouldPlaceTree(int wx, int wz) {
    uint32_t h = hashPos(wx, wz);
    return (h % 53) == 0;
}

static int getTerrainHeight(FastNoiseLite& noise, int wx, int wz) {
    float n = noise.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
    int height = 64 + static_cast<int>(n * 20.0f);
    if (height < 1) height = 1;
    if (height >= CHUNK_SIZE_Y) height = CHUNK_SIZE_Y - 1;
    return height;
}

void Chunk::generate(FastNoiseLite& noise) {
    int worldOffsetX = pos.x * CHUNK_SIZE_X;
    int worldOffsetZ = pos.y * CHUNK_SIZE_Z;

    std::memset(blocks, 0, sizeof(blocks));

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            float wx = static_cast<float>(worldOffsetX + x);
            float wz = static_cast<float>(worldOffsetZ + z);

            float n = noise.GetNoise(wx, wz);
            int height = 64 + static_cast<int>(n * 20.0f);

            if (height < 1) height = 1;
            if (height >= CHUNK_SIZE_Y) height = CHUNK_SIZE_Y - 1;

            for (int y = 0; y <= height; y++) {
                if (y < height - 4) {
                    blocks[x][y][z] = BlockType::Stone;
                } else if (y < height) {
                    blocks[x][y][z] = BlockType::Dirt;
                } else {
                    // y == height
                    blocks[x][y][z] = BlockType::Grass;
                }
            }
        }
    }

    // --- Pass 2: trees ---
    const int treeRadius = 2;
    const int trunkMinHeight = 4;
    const int trunkMaxHeight = 6;

    for (int wx = worldOffsetX - treeRadius; wx < worldOffsetX + CHUNK_SIZE_X + treeRadius; wx++) {
        for (int wz = worldOffsetZ - treeRadius; wz < worldOffsetZ + CHUNK_SIZE_Z + treeRadius; wz++) {
            if (!shouldPlaceTree(wx, wz)) continue;

            int groundHeight = getTerrainHeight(noise, wx, wz);

            if (groundHeight < 50 || groundHeight > 100) continue;

            uint32_t h = hashPos(wx + 7, wz + 13);
            int trunkHeight = trunkMinHeight + static_cast<int>(h % (trunkMaxHeight - trunkMinHeight + 1));

            int trunkTop = groundHeight + trunkHeight;

            int lx = wx - worldOffsetX;
            int lz = wz - worldOffsetZ;
            if (lx >= 0 && lx < CHUNK_SIZE_X && lz >= 0 && lz < CHUNK_SIZE_Z) {
                if (groundHeight >= 0 && groundHeight < CHUNK_SIZE_Y) {
                    blocks[lx][groundHeight][lz] = BlockType::Dirt;
                }
                for (int y = groundHeight + 1; y <= trunkTop && y < CHUNK_SIZE_Y; y++) {
                    blocks[lx][y][lz] = BlockType::Log;
                }
            }
            for (int dy = -1; dy <= 2; dy++) {
                int cy = trunkTop + dy;
                if (cy < 0 || cy >= CHUNK_SIZE_Y) continue;

                int r = (dy <= 0) ? treeRadius : 1;

                for (int dx = -r; dx <= r; dx++) {
                    for (int dz = -r; dz <= r; dz++) {
                        if (dx * dx + dz * dz > r * r + 1) continue;

                        int bx = lx + dx;
                        int bz = lz + dz;

                        if (bx < 0 || bx >= CHUNK_SIZE_X ||
                            bz < 0 || bz >= CHUNK_SIZE_Z) continue;

                        if (dx == 0 && dz == 0 && dy <= 0) continue;

                        if (blocks[bx][cy][bz] == BlockType::Air) {
                            blocks[bx][cy][bz] = BlockType::Leaves;
                        }
                    }
                }
            }
        }
    }

    generated = true;
    meshBuilt = false;
}

BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE_X ||
        y < 0 || y >= CHUNK_SIZE_Y ||
        z < 0 || z >= CHUNK_SIZE_Z) {
        return BlockType::Air;
    }
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= CHUNK_SIZE_X ||
        y < 0 || y >= CHUNK_SIZE_Y ||
        z < 0 || z >= CHUNK_SIZE_Z) {
        return;
    }
    blocks[x][y][z] = type;
    meshBuilt = false;
}

// ============================================================================
// World
// ============================================================================

void World::init(Engine* eng) {
    engine = eng;

    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFrequency(0.01f);
    noise.SetSeed(42);
}

void World::cleanup() {
    for (auto& [pos, chunk] : chunks) {
        if (chunk.mesh.uploaded) {
            engine->destroyBuffer(chunk.mesh.vertexBuffer);
            engine->destroyBuffer(chunk.mesh.indexBuffer);
            chunk.mesh.uploaded = false;
        }
        if (chunk.mesh.reservoirBuffer.buffer != VK_NULL_HANDLE) {
            engine->destroyBuffer(chunk.mesh.reservoirBuffer);
            chunk.mesh.reservoirBuffer = {};
            chunk.mesh.reservoirVertexCount = 0;
        }
    }
    chunks.clear();
    meshesForRendering.clear();
    torches.clear();
}

// ============================================================================
// World::update -- generate & mesh chunks within render distance
// ============================================================================

void World::update(const glm::vec3& playerPos) {
    int playerChunkX = static_cast<int>(std::floor(playerPos.x / CHUNK_SIZE_X));
    int playerChunkZ = static_cast<int>(std::floor(playerPos.z / CHUNK_SIZE_Z));

    for (int cx = playerChunkX - renderDistance; cx <= playerChunkX + renderDistance; cx++) {
        for (int cz = playerChunkZ - renderDistance; cz <= playerChunkZ + renderDistance; cz++) {
            glm::ivec2 cpos(cx, cz);

            auto it = chunks.find(cpos);
            if (it == chunks.end()) {
                generateChunk(cpos);
                it = chunks.find(cpos);
            }

            Chunk& chunk = it->second;

            if (!chunk.meshBuilt) {
                buildChunkMesh(chunk);
            }

            if (chunk.mesh.dirty && !chunk.mesh.vertices.empty()) {
                uploadChunkMesh(chunk);
            }
        }
    }

    meshesForRendering.clear();
    for (auto& [pos, chunk] : chunks) {
        int dx = pos.x - playerChunkX;
        int dz = pos.y - playerChunkZ;
        if (dx * dx + dz * dz > renderDistance * renderDistance) continue;

        if (chunk.mesh.uploaded && !chunk.mesh.indices.empty()) {
            meshesForRendering.push_back(&chunk.mesh);
        }
    }
}

void World::generateChunk(glm::ivec2 pos) {
    Chunk& chunk = chunks[pos];
    chunk.pos = pos;
    chunk.mesh.chunkPos = pos;
    chunk.generate(noise);
}

// ============================================================================
// Block query helpers
// ============================================================================

BlockType World::getBlock(int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return BlockType::Air;

    int cx = static_cast<int>(std::floor(static_cast<float>(wx) / CHUNK_SIZE_X));
    int cz = static_cast<int>(std::floor(static_cast<float>(wz) / CHUNK_SIZE_Z));

    auto it = chunks.find(glm::ivec2(cx, cz));
    if (it == chunks.end()) return BlockType::Air;

    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;

    return it->second.getBlock(lx, wy, lz);
}

bool World::isBlockSolid(int wx, int wy, int wz) {
    BlockType bt = getBlock(wx, wy, wz);
    return ::isBlockSolid(bt);
}

// ============================================================================
// Torch metadata helpers
// ============================================================================

void World::setTorch(int wx, int wy, int wz, const TorchData& data) {
    torches[glm::ivec3(wx, wy, wz)] = data;
}

void World::removeTorch(int wx, int wy, int wz) {
    torches.erase(glm::ivec3(wx, wy, wz));
}

const TorchData* World::getTorch(int wx, int wy, int wz) const {
    auto it = torches.find(glm::ivec3(wx, wy, wz));
    if (it != torches.end()) return &it->second;
    return nullptr;
}

void World::setBlock(int wx, int wy, int wz, BlockType type) {
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;

    int cx = static_cast<int>(std::floor(static_cast<float>(wx) / CHUNK_SIZE_X));
    int cz = static_cast<int>(std::floor(static_cast<float>(wz) / CHUNK_SIZE_Z));

    auto it = chunks.find(glm::ivec2(cx, cz));
    if (it == chunks.end()) return;

    int lx = wx - cx * CHUNK_SIZE_X;
    int lz = wz - cz * CHUNK_SIZE_Z;

    it->second.setBlock(lx, wy, lz, type);

    if (lx == 0) {
        auto adj = chunks.find(glm::ivec2(cx - 1, cz));
        if (adj != chunks.end()) adj->second.meshBuilt = false;
    }
    if (lx == CHUNK_SIZE_X - 1) {
        auto adj = chunks.find(glm::ivec2(cx + 1, cz));
        if (adj != chunks.end()) adj->second.meshBuilt = false;
    }
    if (lz == 0) {
        auto adj = chunks.find(glm::ivec2(cx, cz - 1));
        if (adj != chunks.end()) adj->second.meshBuilt = false;
    }
    if (lz == CHUNK_SIZE_Z - 1) {
        auto adj = chunks.find(glm::ivec2(cx, cz + 1));
        if (adj != chunks.end()) adj->second.meshBuilt = false;
    }
}

bool World::isBlockSolidLocal(Chunk& chunk, int x, int y, int z) {
    if (x >= 0 && x < CHUNK_SIZE_X &&
        y >= 0 && y < CHUNK_SIZE_Y &&
        z >= 0 && z < CHUNK_SIZE_Z) {
        return chunk.blocks[x][y][z] != BlockType::Air;
    }
    if (y < 0 || y >= CHUNK_SIZE_Y) return false;
    return isNeighborSolid(chunk, x, y, z);
}

BlockType World::getNeighborBlock(Chunk& chunk, int x, int y, int z) {
    if (x >= 0 && x < CHUNK_SIZE_X &&
        y >= 0 && y < CHUNK_SIZE_Y &&
        z >= 0 && z < CHUNK_SIZE_Z) {
        return chunk.blocks[x][y][z];
    }
    if (y < 0 || y >= CHUNK_SIZE_Y) return BlockType::Air;
    int wx = chunk.pos.x * CHUNK_SIZE_X + x;
    int wz = chunk.pos.y * CHUNK_SIZE_Z + z;
    return getBlock(wx, y, wz);
}

bool World::shouldShowFace(Chunk& chunk, BlockType block, int nx, int ny, int nz) {
    BlockType neighbor = getNeighborBlock(chunk, nx, ny, nz);

    if (neighbor == BlockType::Air) return true;

    if (isBlockTransparent(block)) {
        return true;
    }

    return isBlockTransparent(neighbor);
}

bool World::isNeighborSolid(Chunk& chunk, int x, int y, int z) {
    int wx = chunk.pos.x * CHUNK_SIZE_X + x;
    int wz = chunk.pos.y * CHUNK_SIZE_Z + z;
    return isBlockSolid(wx, y, wz);
}

// ============================================================================
// Face generation with Minecraft-style vertex AO
// ============================================================================

static int vertexAO(int side1, int side2, int corner) {
    if (side1 && side2) {
    }
    return 3 - (side1 + side2 + corner);
}

static float aoToFloat(int ao) {
    const float table[4] = { 0.2f, 0.45f, 0.7f, 1.0f };
    return table[ao];
}

static bool aoSolid(World* world, Chunk& chunk, int lx, int ly, int lz) {
    BlockType bt = world->getNeighborBlock(chunk, lx, ly, lz);
    return bt != BlockType::Air && bt != BlockType::Torch;
}

void World::addFace(std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
                    const glm::vec3& pos, int face, BlockType type,
                    Chunk& chunk, int bx, int by, int bz) {
    int texId = getBlockFaceTexId(type, face);
    BlockFaceUV uv = getBlockFaceUV(texId);

    uint32_t baseIndex = static_cast<uint32_t>(verts.size());

    glm::vec3 normal;
    glm::vec3 v0, v1, v2, v3;
    float ao0, ao1, ao2, ao3;

    switch (face) {
        case 0: { // +X (right face) — face at x+1, perpendicular axes are Y and Z
            normal = glm::vec3(1, 0, 0);
            v0 = pos + glm::vec3(1, 0, 0);
            v1 = pos + glm::vec3(1, 0, 1);
            v2 = pos + glm::vec3(1, 1, 1);
            v3 = pos + glm::vec3(1, 1, 0);
            // Check neighbors at x+1 plane (already outside block)
            int nx = bx + 1;
            // v0: corner at (y=0, z=0) → edges: y-1, z-1; diag: y-1,z-1
            ao0 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by - 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by - 1, bz - 1) ? 1 : 0));
            // v1: corner at (y=0, z=1) → edges: y-1, z+1; diag: y-1,z+1
            ao1 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by - 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by - 1, bz + 1) ? 1 : 0));
            // v2: corner at (y=1, z=1) → edges: y+1, z+1; diag: y+1,z+1
            ao2 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by + 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by + 1, bz + 1) ? 1 : 0));
            // v3: corner at (y=1, z=0) → edges: y+1, z-1; diag: y+1,z-1
            ao3 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by + 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by + 1, bz - 1) ? 1 : 0));
            break;
        }
        case 1: { // -X (left face) — face at x, perpendicular axes are Y and Z
            normal = glm::vec3(-1, 0, 0);
            v0 = pos + glm::vec3(0, 0, 1);
            v1 = pos + glm::vec3(0, 0, 0);
            v2 = pos + glm::vec3(0, 1, 0);
            v3 = pos + glm::vec3(0, 1, 1);
            int nx = bx - 1;
            // v0: (y=0, z=1)
            ao0 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by - 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by - 1, bz + 1) ? 1 : 0));
            // v1: (y=0, z=0)
            ao1 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by - 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by - 1, bz - 1) ? 1 : 0));
            // v2: (y=1, z=0)
            ao2 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by + 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by + 1, bz - 1) ? 1 : 0));
            // v3: (y=1, z=1)
            ao3 = aoToFloat(vertexAO(
                aoSolid(this, chunk, nx, by + 1, bz) ? 1 : 0,
                aoSolid(this, chunk, nx, by, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, nx, by + 1, bz + 1) ? 1 : 0));
            break;
        }
        case 2: { // +Y (top face) — face at y+1, perpendicular axes are X and Z
            normal = glm::vec3(0, 1, 0);
            v0 = pos + glm::vec3(0, 1, 0);
            v1 = pos + glm::vec3(1, 1, 0);
            v2 = pos + glm::vec3(1, 1, 1);
            v3 = pos + glm::vec3(0, 1, 1);
            int ny = by + 1;
            // v0: (x=0, z=0)
            ao0 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, ny, bz - 1) ? 1 : 0));
            // v1: (x=1, z=0)
            ao1 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, ny, bz - 1) ? 1 : 0));
            // v2: (x=1, z=1)
            ao2 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, ny, bz + 1) ? 1 : 0));
            // v3: (x=0, z=1)
            ao3 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, ny, bz + 1) ? 1 : 0));
            break;
        }
        case 3: { // -Y (bottom face) — face at y, perpendicular axes are X and Z
            normal = glm::vec3(0, -1, 0);
            v0 = pos + glm::vec3(0, 0, 1);
            v1 = pos + glm::vec3(1, 0, 1);
            v2 = pos + glm::vec3(1, 0, 0);
            v3 = pos + glm::vec3(0, 0, 0);
            int ny = by - 1;
            // v0: (x=0, z=1)
            ao0 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, ny, bz + 1) ? 1 : 0));
            // v1: (x=1, z=1)
            ao1 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz + 1) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, ny, bz + 1) ? 1 : 0));
            // v2: (x=1, z=0)
            ao2 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, ny, bz - 1) ? 1 : 0));
            // v3: (x=0, z=0)
            ao3 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, ny, bz) ? 1 : 0,
                aoSolid(this, chunk, bx, ny, bz - 1) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, ny, bz - 1) ? 1 : 0));
            break;
        }
        case 4: { // +Z (front face) — face at z+1, perpendicular axes are X and Y
            normal = glm::vec3(0, 0, 1);
            v0 = pos + glm::vec3(1, 0, 1);
            v1 = pos + glm::vec3(0, 0, 1);
            v2 = pos + glm::vec3(0, 1, 1);
            v3 = pos + glm::vec3(1, 1, 1);
            int nz = bz + 1;
            // v0: (x=1, y=0)
            ao0 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by - 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, by - 1, nz) ? 1 : 0));
            // v1: (x=0, y=0)
            ao1 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by - 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, by - 1, nz) ? 1 : 0));
            // v2: (x=0, y=1)
            ao2 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by + 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, by + 1, nz) ? 1 : 0));
            // v3: (x=1, y=1)
            ao3 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by + 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, by + 1, nz) ? 1 : 0));
            break;
        }
        case 5: { // -Z (back face) — face at z, perpendicular axes are X and Y
            normal = glm::vec3(0, 0, -1);
            v0 = pos + glm::vec3(0, 0, 0);
            v1 = pos + glm::vec3(1, 0, 0);
            v2 = pos + glm::vec3(1, 1, 0);
            v3 = pos + glm::vec3(0, 1, 0);
            int nz = bz - 1;
            // v0: (x=0, y=0)
            ao0 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by - 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, by - 1, nz) ? 1 : 0));
            // v1: (x=1, y=0)
            ao1 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by - 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, by - 1, nz) ? 1 : 0));
            // v2: (x=1, y=1)
            ao2 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx + 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by + 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx + 1, by + 1, nz) ? 1 : 0));
            // v3: (x=0, y=1)
            ao3 = aoToFloat(vertexAO(
                aoSolid(this, chunk, bx - 1, by, nz) ? 1 : 0,
                aoSolid(this, chunk, bx, by + 1, nz) ? 1 : 0,
                aoSolid(this, chunk, bx - 1, by + 1, nz) ? 1 : 0));
            break;
        }
        default:
            return;
    }

    if (type == BlockType::Torch) {
        ao0 = 1.0f; ao1 = 1.0f; ao2 = 1.0f; ao3 = 1.0f;

        auto applyTorchTransform = [&](glm::vec3& v) {
            glm::vec3 local = v - pos;
            local.x = (local.x - 0.5f) * 0.125f + 0.5f;
            local.y = local.y * 0.625f;
            local.z = (local.z - 0.5f) * 0.125f + 0.5f;
            v = pos + local;
        };
        applyTorchTransform(v0);
        applyTorchTransform(v1);
        applyTorchTransform(v2);
        applyTorchTransform(v3);
    }

    glm::vec3 defaultLighting(1.0f, 1.0f, 1.0f);

    verts.push_back({ v0, normal, glm::vec2(uv.uvMin.x, uv.uvMax.y), defaultLighting, ao0 });
    verts.push_back({ v1, normal, glm::vec2(uv.uvMax.x, uv.uvMax.y), defaultLighting, ao1 });
    verts.push_back({ v2, normal, glm::vec2(uv.uvMax.x, uv.uvMin.y), defaultLighting, ao2 });
    verts.push_back({ v3, normal, glm::vec2(uv.uvMin.x, uv.uvMin.y), defaultLighting, ao3 });

    if (ao0 + ao2 > ao1 + ao3) {
        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 3);
    } else {
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 3);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 3);
        indices.push_back(baseIndex + 0);
    }
}

// ============================================================================
// Mesh building -- greedy-ish per-block face culling
// ============================================================================

void World::buildChunkMesh(Chunk& chunk) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(4096);
    indices.reserve(8192);

    int worldOffsetX = chunk.pos.x * CHUNK_SIZE_X;
    int worldOffsetZ = chunk.pos.y * CHUNK_SIZE_Z;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int z = 0; z < CHUNK_SIZE_Z; z++) {
                BlockType block = chunk.blocks[x][y][z];
                if (block == BlockType::Air) continue;

                glm::vec3 worldPos(
                    static_cast<float>(worldOffsetX + x),
                    static_cast<float>(y),
                    static_cast<float>(worldOffsetZ + z)
                );

                // Face 0: +X
                if (shouldShowFace(chunk, block, x + 1, y, z))
                    addFace(vertices, indices, worldPos, 0, block, chunk, x, y, z);
                // Face 1: -X
                if (shouldShowFace(chunk, block, x - 1, y, z))
                    addFace(vertices, indices, worldPos, 1, block, chunk, x, y, z);
                // Face 2: +Y
                if (shouldShowFace(chunk, block, x, y + 1, z))
                    addFace(vertices, indices, worldPos, 2, block, chunk, x, y, z);
                // Face 3: -Y
                if (shouldShowFace(chunk, block, x, y - 1, z))
                    addFace(vertices, indices, worldPos, 3, block, chunk, x, y, z);
                // Face 4: +Z
                if (shouldShowFace(chunk, block, x, y, z + 1))
                    addFace(vertices, indices, worldPos, 4, block, chunk, x, y, z);
                // Face 5: -Z
                if (shouldShowFace(chunk, block, x, y, z - 1))
                    addFace(vertices, indices, worldPos, 5, block, chunk, x, y, z);
            }
        }
    }

    chunk.mesh.vertices = std::move(vertices);
    chunk.mesh.indices  = std::move(indices);
    chunk.mesh.dirty    = true;
    chunk.meshBuilt     = true;
}

// ============================================================================
// GPU upload via staging buffers
// ============================================================================

void World::uploadChunkMesh(Chunk& chunk) {
    ChunkMesh& mesh = chunk.mesh;

    if (mesh.uploaded) {
        engine->destroyBuffer(mesh.vertexBuffer);
        engine->destroyBuffer(mesh.indexBuffer);
        mesh.uploaded = false;
    }

    if (mesh.reservoirBuffer.buffer != VK_NULL_HANDLE) {
        engine->destroyBuffer(mesh.reservoirBuffer);
        mesh.reservoirBuffer = {};
        mesh.reservoirVertexCount = 0;
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) {
        mesh.dirty = false;
        return;
    }

    VkDeviceSize vertexDataSize = sizeof(Vertex) * mesh.vertices.size();
    VkDeviceSize indexDataSize  = sizeof(uint32_t) * mesh.indices.size();

    // --- Vertex buffer ---
    AllocatedBuffer vertexStaging = engine->createBuffer(
        vertexDataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        0
    );
    void* mappedData = nullptr;
    vmaMapMemory(engine->allocator, vertexStaging.allocation, &mappedData);
    std::memcpy(mappedData, mesh.vertices.data(), static_cast<size_t>(vertexDataSize));
    vmaUnmapMemory(engine->allocator, vertexStaging.allocation);

    mesh.vertexBuffer = engine->createBuffer(
        vertexDataSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    );

    // --- Index buffer ---
    AllocatedBuffer indexStaging = engine->createBuffer(
        indexDataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        0
    );

    vmaMapMemory(engine->allocator, indexStaging.allocation, &mappedData);
    std::memcpy(mappedData, mesh.indices.data(), static_cast<size_t>(indexDataSize));
    vmaUnmapMemory(engine->allocator, indexStaging.allocation);

    mesh.indexBuffer = engine->createBuffer(
        indexDataSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    );

    VkCommandBuffer cmd = engine->beginSingleTimeCommands();

    VkBufferCopy vertexCopy{};
    vertexCopy.size = vertexDataSize;
    vkCmdCopyBuffer(cmd, vertexStaging.buffer, mesh.vertexBuffer.buffer, 1, &vertexCopy);

    VkBufferCopy indexCopy{};
    indexCopy.size = indexDataSize;
    vkCmdCopyBuffer(cmd, indexStaging.buffer, mesh.indexBuffer.buffer, 1, &indexCopy);

    engine->endSingleTimeCommands(cmd);

    engine->destroyBuffer(vertexStaging);
    engine->destroyBuffer(indexStaging);

    mesh.vertexBuffer.size = vertexDataSize;
    mesh.indexBuffer.size  = indexDataSize;
    mesh.uploaded = true;
    mesh.dirty    = false;
}
