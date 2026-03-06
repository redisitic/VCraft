#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void TextureLoader::init(Engine* engine) {
    this->engine = engine;
}

void TextureLoader::loadAtlas(const std::string& path) {
    int texWidth, texHeight, texChannels;
    std::string actualPath = path;
    stbi_uc* pixels = stbi_load(actualPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        actualPath = "build/" + path;
        pixels = stbi_load(actualPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    }
    if (!pixels) {
        actualPath = "../" + path;
        pixels = stbi_load(actualPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    }
    if (!pixels) {
        throw std::runtime_error("Failed to load texture atlas: " + path);
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    AllocatedBuffer staging = engine->createBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY, 0);

    void* data;
    vmaMapMemory(engine->allocator, staging.allocation, &data);
    memcpy(data, pixels, (size_t)imageSize);
    vmaUnmapMemory(engine->allocator, staging.allocation);
    stbi_image_free(pixels);

    engine->textureAtlas = engine->createImage(texWidth, texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    engine->transitionImageLayout(engine->textureAtlas.image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    engine->copyBufferToImage(staging.buffer, engine->textureAtlas.image, texWidth, texHeight);
    engine->transitionImageLayout(engine->textureAtlas.image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    engine->textureAtlas.view = engine->createImageView(engine->textureAtlas.image,
        VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

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

    if (vkCreateSampler(engine->device, &samplerInfo, nullptr, &engine->textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler!");
    }

    engine->destroyBuffer(staging);
}

void TextureLoader::cleanup() {
}
