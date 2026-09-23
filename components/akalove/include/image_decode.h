// image_decode.h — décodage d'images en mémoire : PNG (stb_image) et BMP 24/32 bits.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct Image {
    int w = 0, h = 0;
    uint8_t* rgba = nullptr;   // w*h*4 octets (R,G,B,A), alloué par hal::alloc ; libérer avec image_free()
};

// Renvoient false et laissent `out` vide en cas d'échec ; `why` (facultatif) reçoit un court motif.
bool decode_png(const uint8_t* data, size_t len, Image& out, const char** why = nullptr);
bool decode_bmp(const uint8_t* data, size_t len, Image& out, const char** why = nullptr);
void image_free(Image& im);
