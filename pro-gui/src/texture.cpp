#include "texture.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

// OpenGL — use glad-less core via GLFW headers
#include <GLFW/glfw3.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#include "stb_image.h"

// Meme thumbs stay small. Local assets (world map is 4800x2515) must still load.
static const int kMaxTexDim = 4096;

static unsigned char* DownsampleToMax(unsigned char* src, int& w, int& h, int maxDim)
{
    if (!src || w <= 0 || h <= 0)
        return src;
    if (w <= maxDim && h <= maxDim)
        return src;
    const float s = (w > h) ? (float)maxDim / (float)w : (float)maxDim / (float)h;
    int nw = (int)(w * s);
    int nh = (int)(h * s);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    unsigned char* dst = (unsigned char*)std::malloc((size_t)nw * (size_t)nh * 4);
    if (!dst) {
        stbi_image_free(src);
        w = h = 0;
        return nullptr;
    }
    for (int y = 0; y < nh; ++y) {
        const int sy = y * h / nh;
        for (int x = 0; x < nw; ++x) {
            const int sx = x * w / nw;
            const unsigned char* p = src + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
            unsigned char* q = dst + ((size_t)y * (size_t)nw + (size_t)x) * 4;
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
            q[3] = p[3];
        }
    }
    stbi_image_free(src);
    w = nw;
    h = nh;
    return dst;
}

static Texture UploadRgba(unsigned char* data, int w, int h)
{
    Texture t;
    data = DownsampleToMax(data, w, h, kMaxTexDim);
    if (!data || w <= 0 || h <= 0)
        return t;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(GL_UNPACK_ROW_LENGTH)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    std::free(data);
    t.id = id;
    t.width = w;
    t.height = h;
    return t;
}

Texture LoadTextureFromFile(const std::string& path)
{
    int w = 0, h = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) {
        std::fprintf(stderr, "texture: failed to load %s\n", path.c_str());
        return Texture();
    }
    return UploadRgba(data, w, h);
}

Texture LoadTextureFromMemory(const void* data, int len)
{
    if (!data || len <= 0)
        return Texture();
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory((const stbi_uc*)data, len, &w, &h, &n, 4);
    if (!px)
        return Texture();
    return UploadRgba(px, w, h);
}

void DestroyTexture(Texture& t)
{
    if (t.id) {
        GLuint id = t.id;
        glDeleteTextures(1, &id);
        t.id = 0;
    }
}
