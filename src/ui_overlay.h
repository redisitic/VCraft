#pragma once

#include "engine.h"
#include "console.h"
#include "player.h"
#include <vector>
#include <string>

// Glyph info for each character in the font atlas
struct GlyphInfo {
    float u0, v0, u1, v1;   // UV coordinates in atlas
    int width;               // glyph width in pixels
    int height;              // glyph height in pixels
};

class UIOverlay {
public:
    void init(Engine* engine);
    void cleanup();

    // Build UI vertex data for the current frame
    // Call each frame before recording command buffer
    void buildConsoleUI(const Console& console, float totalTime);
    void buildCrosshair();
    void buildHotbar(const Inventory& inventory);

    // Upload vertex data to GPU and draw
    void draw(VkCommandBuffer cmdBuf);

    // Glyph lookup
    const GlyphInfo& getGlyph(char c) const;

private:
    Engine* engine = nullptr;

    // Font atlas (R8 format, generated at init time)
    static constexpr int FONT_ATLAS_SIZE = 256;    // 256x256 atlas
    static constexpr int GLYPH_W = 8;              // each glyph cell is 8px wide
    static constexpr int GLYPH_H = 14;             // each glyph cell is 14px tall
    static constexpr int GLYPHS_PER_ROW = FONT_ATLAS_SIZE / GLYPH_W; // 32
    static constexpr int FIRST_CHAR = 32;           // ASCII space
    static constexpr int LAST_CHAR = 126;           // ASCII tilde

    GlyphInfo glyphs[128];

    void generateFontAtlas();
    void renderGlyph(uint8_t* atlas, int atlasW, char c, int cellX, int cellY);

    // Vertex data built each frame
    std::vector<UIVertex> vertices;
    std::vector<uint32_t> indices;

    // Dynamic vertex/index buffers (recreated each frame)
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    bool buffersAllocated = false;

    // Helper: add a colored quad
    void addQuad(float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1,
                 const glm::vec4& color);

    // Helper: add a text string
    void addText(const std::string& text, float x, float y, float scale,
                 const glm::vec4& color);
};
