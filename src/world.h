#pragma once

#include "engine.h"
#include <FastNoiseLite.h>
#include <unordered_map>
#include <glm/gtx/hash.hpp>

// ============================================================================
// Torch color metadata — stored per world position, not in block array
// ============================================================================
struct TorchData {
    glm::vec3 color = glm::vec3(1.0f, 0.75f, 0.45f); // default ~3000K warm white
    float intensity = 3.5f;
    float radius = 14.0f;
};

enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Stone,
    Dirt,
    WoodPlanks,
    Log,
    Leaves,
    Torch,
    COUNT
};

// Returns true for blocks that have transparency (alpha-tested textures)
// Torches are transparent so adjacent opaque block faces are visible.
inline bool isBlockTransparent(BlockType type) {
    return type == BlockType::Leaves || type == BlockType::Torch;
}

// Returns true for blocks that are not Air and are collidable (occupy space)
// Torches are non-solid — player can walk through them.
inline bool isBlockSolid(BlockType type) {
    return type != BlockType::Air && type != BlockType::Torch;
}

// Returns atlas texture coordinates for a block face
// Atlas: 1024x1024, each block 18x18, 56 blocks per row (last 16px padding)
// blockTexId goes left to right, top to bottom
struct BlockFaceUV {
    glm::vec2 uvMin;
    glm::vec2 uvMax;
};

BlockFaceUV getBlockFaceUV(int texId);

// Get texture ID for each face of a block type
// face: 0=+X, 1=-X, 2=+Y(top), 3=-Y(bottom), 4=+Z, 5=-Z
int getBlockFaceTexId(BlockType type, int face);

constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 128;
constexpr int CHUNK_SIZE_Z = 16;

struct Chunk {
    glm::ivec2 pos; // chunk coords (multiply by 16 for world coords)
    BlockType blocks[CHUNK_SIZE_X][CHUNK_SIZE_Y][CHUNK_SIZE_Z];
    ChunkMesh mesh;
    bool generated = false;
    bool meshBuilt = false;

    void generate(FastNoiseLite& noise);
    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);
};

class World {
public:
    void init(Engine* engine);
    void update(const glm::vec3& playerPos);
    void buildChunkMesh(Chunk& chunk);
    void uploadChunkMesh(Chunk& chunk);
    void cleanup();

    // Get block at world coordinate
    BlockType getBlock(int wx, int wy, int wz);
    bool isBlockSolid(int wx, int wy, int wz);

    // Set block at world coordinate; dirties the chunk and adjacent chunks on boundary
    void setBlock(int wx, int wy, int wz, BlockType type);

    // All chunks
    std::unordered_map<glm::ivec2, Chunk> chunks;
    std::vector<ChunkMesh*> meshesForRendering;

    // Torch metadata (per world position)
    std::unordered_map<glm::ivec3, TorchData> torches;

    // Set a torch with color data at world position
    void setTorch(int wx, int wy, int wz, const TorchData& data);
    // Remove torch metadata at world position
    void removeTorch(int wx, int wy, int wz);
    // Get torch data (returns nullptr if not a torch)
    const TorchData* getTorch(int wx, int wy, int wz) const;

    int renderDistance = 6; // chunks

    // Get neighbor block type (may cross chunk boundary) — public for AO helpers
    BlockType getNeighborBlock(Chunk& chunk, int x, int y, int z);

private:
    Engine* engine = nullptr;
    FastNoiseLite noise;

    void generateChunk(glm::ivec2 pos);
    bool isBlockSolidLocal(Chunk& chunk, int x, int y, int z);
    // Check adjacent chunks for face culling at chunk boundaries
    bool isNeighborSolid(Chunk& chunk, int x, int y, int z);
    // Determine if a face should be visible based on transparency rules
    bool shouldShowFace(Chunk& chunk, BlockType block, int nx, int ny, int nz);

    void addFace(std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
                 const glm::vec3& pos, int face, BlockType type,
                 Chunk& chunk, int bx, int by, int bz);
};
