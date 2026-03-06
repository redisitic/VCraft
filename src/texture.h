#pragma once

#include "engine.h"

class TextureLoader {
public:
    void init(Engine* engine);
    void loadAtlas(const std::string& path);
    void cleanup();

private:
    Engine* engine = nullptr;
};
