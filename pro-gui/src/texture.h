#pragma once

#include <string>

// OpenGL texture handle for ImGui::Image
struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
    bool ok() const { return id != 0; }
};

// Load RGBA image (stb_image). Path is filesystem path.
Texture LoadTextureFromFile(const std::string& path);
Texture LoadTextureFromMemory(const void* data, int len);
void DestroyTexture(Texture& t);
