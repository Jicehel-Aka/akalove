// image_decode.cpp — PNG via stb_image (domaine public / MIT, third_party/), BMP écrit ici.
#include "image_decode.h"

#include <string.h>

#include "hal.h"

#include "third_party/stb_image.h"   // déclarations seulement ; l'implémentation est dans stb_impl.cpp

namespace {
constexpr int MAX_SIDE = 4096;   // au-delà : refusé (mémoire)

uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
bool fail(const char** why, const char* msg) {
    if (why) *why = msg;
    return false;
}
}  // namespace

bool decode_png(const uint8_t* data, size_t len, Image& out, const char** why) {
    image_free(out);
    int w = 0, h = 0, n = 0;
    uint8_t* px = stbi_load_from_memory(data, (int)len, &w, &h, &n, 4);
    if (!px) return fail(why, stbi_failure_reason() ? stbi_failure_reason() : "PNG invalide");
    if (w > MAX_SIDE || h > MAX_SIDE) {
        hal::free_(px);
        return fail(why, "image trop grande (4096 px maximum)");
    }
    out.w = w;
    out.h = h;
    out.rgba = px;
    return true;
}

bool decode_bmp(const uint8_t* d, size_t len, Image& out, const char** why) {
    image_free(out);
    if (len < 54 || d[0] != 'B' || d[1] != 'M') return fail(why, "pas un BMP");
    uint32_t off = le32(d + 10), dib = le32(d + 14);
    int32_t w = (int32_t)le32(d + 18), h = (int32_t)le32(d + 22);
    uint16_t planes = le16(d + 26), bpp = le16(d + 28);
    uint32_t comp = le32(d + 30);
    if (dib < 40 || planes != 1) return fail(why, "en-tête BMP non géré");
    if (bpp != 24 && bpp != 32) return fail(why, "BMP : seulement 24 ou 32 bits");
    if (comp != 0 && !(comp == 3 && bpp == 32)) return fail(why, "BMP compressé non géré");
    bool top_down = h < 0;
    if (top_down) h = -h;
    if (w <= 0 || h <= 0 || w > MAX_SIDE || h > MAX_SIDE) return fail(why, "dimensions BMP invalides");
    uint32_t stride = (((uint32_t)w * bpp + 31) / 32) * 4;
    if ((uint64_t)off + (uint64_t)stride * (uint32_t)h > len) return fail(why, "BMP tronqué");

    uint8_t* px = (uint8_t*)hal::alloc((size_t)w * h * 4);
    if (!px) return fail(why, "mémoire insuffisante");
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = d + off + (size_t)stride * (top_down ? y : (h - 1 - y));
        uint8_t* dst = px + (size_t)y * w * 4;
        for (int x = 0; x < w; ++x) {
            dst[4 * x + 0] = src[x * (bpp / 8) + 2];   // BGR(A) -> RGBA
            dst[4 * x + 1] = src[x * (bpp / 8) + 1];
            dst[4 * x + 2] = src[x * (bpp / 8) + 0];
            dst[4 * x + 3] = 255;                      // l'octet alpha des BMP 32 bits est presque toujours 0 : ignoré
        }
    }
    out.w = w;
    out.h = h;
    out.rgba = px;
    return true;
}

void image_free(Image& im) {
    if (im.rgba) hal::free_(im.rgba);
    im = Image();
}
