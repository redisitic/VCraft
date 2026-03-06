#include "ui_overlay.h"
#include "world.h"
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------------------
// Minimal 8x14 bitmap font data (CP437 style, ASCII 32-126)
// Each character is 8 pixels wide, 14 pixels tall.
// Stored as 14 bytes per character (1 byte per row, MSB = leftmost pixel).
// ---------------------------------------------------------------------------

// clang-format off
static const uint8_t FONT_DATA[][14] = {
    // 32 ' '  (space)
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 33 '!'
    {0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    // 34 '"'
    {0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 35 '#'
    {0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00},
    // 36 '$'
    {0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x06,0x86,0xC6,0x7C,0x18,0x18},
    // 37 '%'
    {0x00,0x00,0x00,0x00,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0x00,0x00},
    // 38 '&'
    {0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0x00,0x00},
    // 39 '''
    {0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 40 '('
    {0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00},
    // 41 ')'
    {0x00,0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00},
    // 42 '*'
    {0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00},
    // 43 '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00},
    // 44 ','
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x30,0x00},
    // 45 '-'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00},
    // 46 '.'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00},
    // 47 '/'
    {0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00},
    // 48 '0'
    {0x00,0x00,0x7C,0xC6,0xC6,0xCE,0xDE,0xF6,0xE6,0xC6,0xC6,0x7C,0x00,0x00},
    // 49 '1'
    {0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00},
    // 50 '2'
    {0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0x00,0x00},
    // 51 '3'
    {0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00},
    // 52 '4'
    {0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0x00,0x00},
    // 53 '5'
    {0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00},
    // 54 '6'
    {0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00},
    // 55 '7'
    {0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00},
    // 56 '8'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00},
    // 57 '9'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0x00,0x00},
    // 58 ':'
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},
    // 59 ';'
    {0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00},
    // 60 '<'
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00},
    // 61 '='
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00},
    // 62 '>'
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00},
    // 63 '?'
    {0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    // 64 '@'
    {0x00,0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0x00,0x00},
    // 65 'A'
    {0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00},
    // 66 'B'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00},
    // 67 'C'
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x00,0x00},
    // 68 'D'
    {0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00},
    // 69 'E'
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0x00,0x00},
    // 70 'F'
    {0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0x00,0x00},
    // 71 'G'
    {0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0x00,0x00},
    // 72 'H'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00},
    // 73 'I'
    {0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00},
    // 74 'J'
    {0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0x00,0x00},
    // 75 'K'
    {0x00,0x00,0xE6,0x66,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0x00,0x00},
    // 76 'L'
    {0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00},
    // 77 'M'
    {0x00,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00},
    // 78 'N'
    {0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0x00,0x00},
    // 79 'O'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00},
    // 80 'P'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0x00,0x00},
    // 81 'Q'
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E},
    // 82 'R'
    {0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0x00,0x00},
    // 83 'S'
    {0x00,0x00,0x7C,0xC6,0xC6,0x60,0x38,0x0C,0x06,0xC6,0xC6,0x7C,0x00,0x00},
    // 84 'T'
    {0x00,0x00,0xFF,0xDB,0x99,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00},
    // 85 'U'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00},
    // 86 'V'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00},
    // 87 'W'
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0xEE,0x6C,0x00,0x00},
    // 88 'X'
    {0x00,0x00,0xC6,0xC6,0x6C,0x7C,0x38,0x38,0x7C,0x6C,0xC6,0xC6,0x00,0x00},
    // 89 'Y'
    {0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x78,0x30,0x30,0x30,0x30,0x78,0x00,0x00},
    // 90 'Z'
    {0x00,0x00,0xFE,0xC6,0x86,0x0C,0x18,0x30,0x60,0xC2,0xC6,0xFE,0x00,0x00},
    // 91 '['
    {0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00},
    // 92 '\'
    {0x00,0x00,0x00,0x80,0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x06,0x02,0x00,0x00},
    // 93 ']'
    {0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00},
    // 94 '^'
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 95 '_'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},
    // 96 '`'
    {0x00,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 97 'a'
    {0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00},
    // 98 'b'
    {0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00},
    // 99 'c'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00},
    // 100 'd'
    {0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00},
    // 101 'e'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00},
    // 102 'f'
    {0x00,0x00,0x1C,0x36,0x32,0x30,0x78,0x30,0x30,0x30,0x30,0x78,0x00,0x00},
    // 103 'g'
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78},
    // 104 'h'
    {0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0x00,0x00},
    // 105 'i'
    {0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00},
    // 106 'j'
    {0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x3C},
    // 107 'k'
    {0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0x00,0x00},
    // 108 'l'
    {0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00},
    // 109 'm'
    {0x00,0x00,0x00,0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0xD6,0xC6,0x00,0x00},
    // 110 'n'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00},
    // 111 'o'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00},
    // 112 'p'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0},
    // 113 'q'
    {0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E},
    // 114 'r'
    {0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0x00,0x00},
    // 115 's'
    {0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00,0x00},
    // 116 't'
    {0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x36,0x1C,0x00,0x00},
    // 117 'u'
    {0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00},
    // 118 'v'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00,0x00},
    // 119 'w'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0x6C,0x00,0x00},
    // 120 'x'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0x00,0x00},
    // 121 'y'
    {0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8},
    // 122 'z'
    {0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00},
    // 123 '{'
    {0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0x00,0x00},
    // 124 '|'
    {0x00,0x00,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x18,0x00,0x00},
    // 125 '}'
    {0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0x00,0x00},
    // 126 '~'
    {0x00,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

static constexpr int FONT_CHAR_COUNT = 95;

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void UIOverlay::init(Engine* eng) {
    engine = eng;
    generateFontAtlas();
}

// ---------------------------------------------------------------------------
// Font atlas generation — blit bitmap font data into an R8 texture
// ---------------------------------------------------------------------------

void UIOverlay::generateFontAtlas() {
    const int atlasW = FONT_ATLAS_SIZE;
    const int atlasH = FONT_ATLAS_SIZE;

    std::vector<uint8_t> pixels(atlasW * atlasH, 0);

    for (int i = 0; i < FONT_CHAR_COUNT; i++) {
        int col = i % GLYPHS_PER_ROW;
        int row = i / GLYPHS_PER_ROW;
        int px = col * GLYPH_W;
        int py = row * GLYPH_H;

        const uint8_t* gdata = FONT_DATA[i];
        for (int y = 0; y < GLYPH_H; y++) {
            if (py + y >= atlasH) break;
            uint8_t rowBits = gdata[y];
            for (int x = 0; x < GLYPH_W; x++) {
                if (px + x >= atlasW) break;
                bool on = (rowBits >> (7 - x)) & 1;
                pixels[(py + y) * atlasW + (px + x)] = on ? 255 : 0;
            }
        }

        char c = (char)(FIRST_CHAR + i);
        glyphs[(int)c].u0 = (float)px / (float)atlasW;
        glyphs[(int)c].v0 = (float)py / (float)atlasH;
        glyphs[(int)c].u1 = (float)(px + GLYPH_W) / (float)atlasW;
        glyphs[(int)c].v1 = (float)(py + GLYPH_H) / (float)atlasH;
        glyphs[(int)c].width = GLYPH_W;
        glyphs[(int)c].height = GLYPH_H;
    }

    VkDeviceSize imageSize = atlasW * atlasH;

    AllocatedBuffer staging = engine->createBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, 0);

    void* data;
    vmaMapMemory(engine->allocator, staging.allocation, &data);
    memcpy(data, pixels.data(), (size_t)imageSize);
    vmaUnmapMemory(engine->allocator, staging.allocation);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = atlasW;
    imageInfo.extent.height = atlasH;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(engine->allocator, &imageInfo, &allocInfo,
                       &engine->fontAtlasImage.image, &engine->fontAtlasImage.allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create font atlas image");
    }

    engine->transitionImageLayout(engine->fontAtlasImage.image, VK_FORMAT_R8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkCommandBuffer cmdBuf = engine->beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {(uint32_t)atlasW, (uint32_t)atlasH, 1};
    vkCmdCopyBufferToImage(cmdBuf, staging.buffer, engine->fontAtlasImage.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    engine->endSingleTimeCommands(cmdBuf);

    engine->transitionImageLayout(engine->fontAtlasImage.image, VK_FORMAT_R8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    engine->fontAtlasImage.view = engine->createImageView(
        engine->fontAtlasImage.image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(engine->device, &samplerInfo, nullptr, &engine->fontSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create font sampler");
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(engine->device, &poolInfo, nullptr, &engine->uiDescPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create UI descriptor pool");
    }

    VkDescriptorSetAllocateInfo descAllocInfo{};
    descAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descAllocInfo.descriptorPool = engine->uiDescPool;
    descAllocInfo.descriptorSetCount = 1;
    descAllocInfo.pSetLayouts = &engine->uiFontDescSetLayout;

    if (vkAllocateDescriptorSets(engine->device, &descAllocInfo, &engine->uiFontDescSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate UI font descriptor set");
    }

    VkDescriptorImageInfo descImageInfo{};
    descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descImageInfo.imageView = engine->fontAtlasImage.view;
    descImageInfo.sampler = engine->fontSampler;

    VkWriteDescriptorSet descWrite{};
    descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet = engine->uiFontDescSet;
    descWrite.dstBinding = 0;
    descWrite.dstArrayElement = 0;
    descWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descWrite.descriptorCount = 1;
    descWrite.pImageInfo = &descImageInfo;

    vkUpdateDescriptorSets(engine->device, 1, &descWrite, 0, nullptr);

    engine->destroyBuffer(staging);
}

// ---------------------------------------------------------------------------
// Glyph lookup
// ---------------------------------------------------------------------------

const GlyphInfo& UIOverlay::getGlyph(char c) const {
    int idx = (int)c;
    if (idx < FIRST_CHAR || idx > LAST_CHAR) idx = FIRST_CHAR;
    return glyphs[idx];
}

// ---------------------------------------------------------------------------
// Quad / text helpers
// ---------------------------------------------------------------------------

void UIOverlay::addQuad(float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1,
                         const glm::vec4& color) {
    uint32_t base = (uint32_t)vertices.size();

    vertices.push_back({{x0, y0}, {u0, v0}, color});
    vertices.push_back({{x1, y0}, {u1, v0}, color});
    vertices.push_back({{x1, y1}, {u1, v1}, color});
    vertices.push_back({{x0, y1}, {u0, v1}, color});

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

void UIOverlay::addText(const std::string& text, float x, float y, float scale,
                         const glm::vec4& color) {
    float cx = x;
    for (char c : text) {
        if (c < FIRST_CHAR || c > LAST_CHAR) {
            cx += GLYPH_W * scale;
            continue;
        }
        const GlyphInfo& g = getGlyph(c);
        float w = g.width * scale;
        float h = g.height * scale;

        addQuad(cx, y, cx + w, y + h, g.u0, g.v0, g.u1, g.v1, color);
        cx += w;
    }
}

// ---------------------------------------------------------------------------
// Build console UI — Minecraft style
// ---------------------------------------------------------------------------

void UIOverlay::buildConsoleUI(const Console& console, float totalTime) {
    vertices.clear();
    indices.clear();

    if (!console.isOpen()) return;

    float screenW = (float)engine->swapChainExtent.width;
    float screenH = (float)engine->swapChainExtent.height;

    float scale = 2.0f;
    float lineH = GLYPH_H * scale;
    float padding = 4.0f * scale;

    float barHeight = lineH + padding * 2.0f;
    float barY = screenH - barHeight;

    const GlyphInfo& spaceG = getGlyph(' ');
    const GlyphInfo& solidG = getGlyph('M');
    float solidU = (solidG.u0 + solidG.u1) * 0.5f;
    float solidV = (solidG.v0 + solidG.v1) * 0.5f;
    float solidEps = 0.001f;

    addQuad(0, barY, screenW, screenH,
            solidU - solidEps, solidV - solidEps,
            solidU + solidEps, solidV + solidEps,
            glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));

    std::string inputText = "/" + console.getInput();

    bool showCursor = (fmod(totalTime, 1.0f) < 0.5f);
    if (showCursor) {
        inputText += "_";
    }

    addText(inputText, padding, barY + padding, scale,
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    const auto& messages = console.getMessages();
    float msgY = barY;
    for (int i = (int)messages.size() - 1; i >= 0; i--) {
        msgY -= (lineH + 2.0f);
        if (msgY < 0) break;

        addQuad(0, msgY, screenW, msgY + lineH + 2.0f,
                solidU - solidEps, solidV - solidEps,
                solidU + solidEps, solidV + solidEps,
                glm::vec4(0.0f, 0.0f, 0.0f, 0.35f));

        addText(messages[i], padding, msgY + 1.0f, scale,
                glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    }
}

// ---------------------------------------------------------------------------
// Build crosshair (small + in center of screen)
// ---------------------------------------------------------------------------

void UIOverlay::buildCrosshair() {
    float screenW = (float)engine->swapChainExtent.width;
    float screenH = (float)engine->swapChainExtent.height;
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;

    float scale = 2.0f;
    const GlyphInfo& g = getGlyph('+');
    float w = g.width * scale;
    float h = g.height * scale;

    addQuad(cx - w * 0.5f, cy - h * 0.5f,
            cx + w * 0.5f, cy + h * 0.5f,
            g.u0, g.v0, g.u1, g.v1,
            glm::vec4(1.0f, 1.0f, 1.0f, 0.8f));
}

// ---------------------------------------------------------------------------
// Build hotbar — 9 slots at bottom center, Minecraft style
// ---------------------------------------------------------------------------

static const char* blockShortName(BlockType type) {
    switch (type) {
        case BlockType::Grass:      return "Grass";
        case BlockType::Stone:      return "Stone";
        case BlockType::Dirt:       return "Dirt";
        case BlockType::WoodPlanks: return "Plank";
        case BlockType::Log:        return "Log";
        case BlockType::Leaves:     return "Leaf";
        case BlockType::Torch:      return "Torch";
        default:                    return "";
    }
}

void UIOverlay::buildHotbar(const Inventory& inventory) {
    float screenW = (float)engine->swapChainExtent.width;
    float screenH = (float)engine->swapChainExtent.height;

    float slotSize = 40.0f;
    float slotGap  = 4.0f;
    float barPad   = 4.0f;

    float totalW = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotGap + barPad * 2.0f;
    float totalH = slotSize + barPad * 2.0f;
    float barX = (screenW - totalW) * 0.5f;
    float barY = screenH - totalH - 4.0f;

    const GlyphInfo& solidG = getGlyph('M');
    float solidU = (solidG.u0 + solidG.u1) * 0.5f;
    float solidV = (solidG.v0 + solidG.v1) * 0.5f;
    float solidEps = 0.001f;

    addQuad(barX, barY, barX + totalW, barY + totalH,
            solidU - solidEps, solidV - solidEps,
            solidU + solidEps, solidV + solidEps,
            glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));

    float textScale = 1.0f;

    for (int i = 0; i < HOTBAR_SLOTS; i++) {
        float sx = barX + barPad + i * (slotSize + slotGap);
        float sy = barY + barPad;

        glm::vec4 slotBg = (i == inventory.selectedSlot)
            ? glm::vec4(1.0f, 1.0f, 1.0f, 0.35f)
            : glm::vec4(0.3f, 0.3f, 0.3f, 0.5f);

        addQuad(sx, sy, sx + slotSize, sy + slotSize,
                solidU - solidEps, solidV - solidEps,
                solidU + solidEps, solidV + solidEps,
                slotBg);

        if (i == inventory.selectedSlot) {
            float bw = 2.0f;
            glm::vec4 borderCol(1.0f, 1.0f, 1.0f, 0.8f);
            // Top
            addQuad(sx - bw, sy - bw, sx + slotSize + bw, sy,
                    solidU - solidEps, solidV - solidEps,
                    solidU + solidEps, solidV + solidEps, borderCol);
            // Bottom
            addQuad(sx - bw, sy + slotSize, sx + slotSize + bw, sy + slotSize + bw,
                    solidU - solidEps, solidV - solidEps,
                    solidU + solidEps, solidV + solidEps, borderCol);
            // Left
            addQuad(sx - bw, sy, sx, sy + slotSize,
                    solidU - solidEps, solidV - solidEps,
                    solidU + solidEps, solidV + solidEps, borderCol);
            // Right
            addQuad(sx + slotSize, sy, sx + slotSize + bw, sy + slotSize,
                    solidU - solidEps, solidV - solidEps,
                    solidU + solidEps, solidV + solidEps, borderCol);
        }

        const auto& slot = inventory.hotbar[i];
        if (slot.type != BlockType::Air && slot.count > 0) {
            const char* name = blockShortName(slot.type);
            float nameW = (float)strlen(name) * GLYPH_W * textScale;
            float nameX = sx + (slotSize - nameW) * 0.5f;
            float nameY = sy + (slotSize - GLYPH_H * textScale) * 0.5f - 4.0f;
            addText(name, nameX, nameY, textScale,
                    glm::vec4(1.0f, 1.0f, 1.0f, 0.95f));

            if (slot.count > 1) {
                std::string countStr = std::to_string(slot.count);
                float countW = (float)countStr.size() * GLYPH_W * textScale;
                float countX = sx + slotSize - countW - 2.0f;
                float countY = sy + slotSize - GLYPH_H * textScale - 1.0f;
                addText(countStr, countX, countY, textScale,
                        glm::vec4(1.0f, 1.0f, 0.5f, 1.0f));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Draw — upload vertices and record draw commands
// ---------------------------------------------------------------------------

void UIOverlay::draw(VkCommandBuffer cmdBuf) {
    if (vertices.empty()) return;

    if (buffersAllocated) {
        engine->destroyBuffer(vertexBuffer);
        engine->destroyBuffer(indexBuffer);
        buffersAllocated = false;
    }

    VkDeviceSize vtxSize = vertices.size() * sizeof(UIVertex);
    VkDeviceSize idxSize = indices.size() * sizeof(uint32_t);

    vertexBuffer = engine->createBuffer(vtxSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    indexBuffer = engine->createBuffer(idxSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    buffersAllocated = true;

    void* vtxData;
    vmaMapMemory(engine->allocator, vertexBuffer.allocation, &vtxData);
    memcpy(vtxData, vertices.data(), vtxSize);
    vmaUnmapMemory(engine->allocator, vertexBuffer.allocation);

    void* idxData;
    vmaMapMemory(engine->allocator, indexBuffer.allocation, &idxData);
    memcpy(idxData, indices.data(), idxSize);
    vmaUnmapMemory(engine->allocator, indexBuffer.allocation);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->uiPipeline);

    UIPushConstants pc{};
    pc.screenWidth = (float)engine->swapChainExtent.width;
    pc.screenHeight = (float)engine->swapChainExtent.height;
    vkCmdPushConstants(cmdBuf, engine->uiPipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstants), &pc);

    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine->uiPipelineLayout, 0, 1, &engine->uiFontDescSet, 0, nullptr);

    VkBuffer buffers[] = {vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(cmdBuf, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmdBuf, (uint32_t)indices.size(), 1, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void UIOverlay::cleanup() {
    if (buffersAllocated) {
        engine->destroyBuffer(vertexBuffer);
        engine->destroyBuffer(indexBuffer);
        buffersAllocated = false;
    }
}
