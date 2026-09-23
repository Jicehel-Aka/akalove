// splash.cpp — écran de démarrage
#include "splash.h"

#include <stdio.h>
#include <string.h>

#include "hal.h"
#include "image_decode.h"

namespace {

constexpr size_t MAX_FILE = 8u * 1024 * 1024;

void join(char* dst, size_t n, const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la >= n) la = n - 1;
    if (la + lb >= n) lb = n - 1 - la;
    memcpy(dst, a, la);
    memcpy(dst + la, b, lb);
    dst[la + lb] = 0;
}

bool read_file(const char* path, uint8_t** data, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (size_t)sz > MAX_FILE) {
        fclose(f);
        return false;
    }
    uint8_t* buf = (uint8_t*)hal::alloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        hal::free_(buf);
        return false;
    }
    *data = buf;
    *len = got;
    return true;
}

// Charge <home>/<name> avec le décodeur demandé
bool load(const char* home, const char* name, bool png, Image& im) {
    char path[300];
    join(path, sizeof path, home, name);
    uint8_t* data = nullptr;
    size_t len = 0;
    if (!read_file(path, &data, &len)) return false;   // absent ou trop gros : silencieux
    const char* why = nullptr;
    bool ok = png ? decode_png(data, len, im, &why) : decode_bmp(data, len, im, &why);
    hal::free_(data);
    if (!ok) hal::log("[AKA-Love] splash : %s illisible (%s)\n", name, why ? why : "?");
    return ok;
}

// Réduction par moyenne de zone (image plus grande que la cible), échantillonnage au plus proche sinon ;
// fondu sur noir selon l'alpha. Image centrée, bandes noires.
void blit_fit(const Image& im) {
    uint16_t* fb = hal::framebuffer();
    const uint16_t black = hal::pack565(0, 0, 0);
    for (int i = 0; i < hal::SCREEN_W * hal::SCREEN_H; ++i) fb[i] = black;

    float sw = (float)hal::SCREEN_W / im.w, sh = (float)hal::SCREEN_H / im.h;
    float s = sw < sh ? sw : sh;
    int dw = (int)(im.w * s + 0.5f), dh = (int)(im.h * s + 0.5f);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    if (dw > hal::SCREEN_W) dw = hal::SCREEN_W;
    if (dh > hal::SCREEN_H) dh = hal::SCREEN_H;
    int ox = (hal::SCREEN_W - dw) / 2, oy = (hal::SCREEN_H - dh) / 2;

    for (int dy = 0; dy < dh; ++dy) {
        int sy0 = (int)((int64_t)dy * im.h / dh), sy1 = (int)((int64_t)(dy + 1) * im.h / dh);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sy1 > im.h) sy1 = im.h;
        for (int dx = 0; dx < dw; ++dx) {
            int sx0 = (int)((int64_t)dx * im.w / dw), sx1 = (int)((int64_t)(dx + 1) * im.w / dw);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sx1 > im.w) sx1 = im.w;
            uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int y = sy0; y < sy1; ++y) {
                const uint8_t* p = im.rgba + ((size_t)y * im.w + sx0) * 4;
                for (int x = sx0; x < sx1; ++x, p += 4) {
                    r += p[0];
                    g += p[1];
                    b += p[2];
                    a += p[3];
                    ++n;
                }
            }
            r /= n;
            g /= n;
            b /= n;
            a /= n;
            fb[(oy + dy) * hal::SCREEN_W + ox + dx] =
                hal::pack565((uint8_t)(r * a / 255), (uint8_t)(g * a / 255), (uint8_t)(b * a / 255));
        }
    }
}

}  // namespace

bool show_splash(const char* home, uint32_t ms) {
    Image im;
    if (!load(home, "/Picture.png", true, im) && !load(home, "/screen.bmp", false, im)) return false;
    blit_fit(im);
    image_free(im);

    hal::poll_input();
    uint32_t held = hal::buttons();   // déjà tenus au lancement (le A du loader) : ne font pas sauter l'image
    uint32_t t0 = hal::millis();
    while (hal::millis() - t0 < ms) {
        hal::present();
        hal::poll_input();
        if (hal::quit_requested()) break;
        uint32_t now = hal::buttons();
        if (now & ~held) break;
        held &= now;
    }
    uint16_t* fb = hal::framebuffer();   // écran propre pour le jeu
    for (int i = 0; i < hal::SCREEN_W * hal::SCREEN_H; ++i) fb[i] = hal::pack565(0, 0, 0);
    return true;
}
