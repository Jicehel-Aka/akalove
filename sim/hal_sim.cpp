// hal_sim.cpp — HAL du simulateur PC (sans fenêtre) : framebuffer en mémoire, horloge virtuelle,
// boutons scriptés, capture PPM. Sert aux tests de conformité ; une version SDL2 s'ajoutera sans
// toucher au reste du runtime.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#include <stdio.h>

#include "hal.h"
#include "sim.h"

namespace {
void nap_ms(int ms) {   // sans std::thread (absent de certains MinGW)
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, nullptr);
#endif
}
uint16_t g_fb[hal::SCREEN_W * hal::SCREEN_H];
int g_frame = 0;
uint64_t g_vtime_us = 0;
sim::Config g_cfg;
const auto g_t0 = std::chrono::steady_clock::now();

uint64_t real_us() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - g_t0).count();
}

bool ends_with(const char* s, const char* suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !strcmp(s + a - b, suffix);
}

void put_le(FILE* f, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) fputc((int)((v >> (8 * i)) & 0xFF), f);
}

// BMP 24 bits (s'ouvre partout, sans logiciel particulier) : lignes de bas en haut, pixels B G R
void write_bmp(FILE* f) {
    const uint32_t row = hal::SCREEN_W * 3, size = 54 + row * hal::SCREEN_H;   // 960 octets par ligne : déjà multiple de 4
    fputs("BM", f);
    put_le(f, size, 4);
    put_le(f, 0, 4);
    put_le(f, 54, 4);
    put_le(f, 40, 4);
    put_le(f, hal::SCREEN_W, 4);
    put_le(f, hal::SCREEN_H, 4);
    put_le(f, 1, 2);
    put_le(f, 24, 2);
    put_le(f, 0, 4);
    put_le(f, row * hal::SCREEN_H, 4);
    put_le(f, 2835, 4);
    put_le(f, 2835, 4);
    put_le(f, 0, 4);
    put_le(f, 0, 4);
    for (int y = hal::SCREEN_H - 1; y >= 0; --y)
        for (int x = 0; x < hal::SCREEN_W; ++x) {
            uint16_t c = g_fb[y * hal::SCREEN_W + x];
            int r5 = c & 31, g6 = (c >> 5) & 63, b5 = (c >> 11) & 31;
            fputc((b5 << 3) | (b5 >> 2), f);
            fputc((g6 << 2) | (g6 >> 4), f);
            fputc((r5 << 3) | (r5 >> 2), f);
        }
}

void write_ppm(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[sim] impossible d'écrire %s\n", path);
        return;
    }
    if (ends_with(path, ".bmp")) {
        write_bmp(f);
        fclose(f);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", hal::SCREEN_W, hal::SCREEN_H);
    for (int i = 0; i < hal::SCREEN_W * hal::SCREEN_H; ++i) {
        uint16_t c = g_fb[i];   // BGR565 : bleu en poids fort
        int r5 = c & 31, g6 = (c >> 5) & 63, b5 = (c >> 11) & 31;
        unsigned char px[3] = {(unsigned char)((r5 << 3) | (r5 >> 2)), (unsigned char)((g6 << 2) | (g6 >> 4)),
                               (unsigned char)((b5 << 3) | (b5 >> 2))};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}
}  // namespace

namespace sim {
Config& config() { return g_cfg; }
int frame() { return g_frame; }
}  // namespace sim

namespace hal {

// ---------------------------------------------------------------------------------------------------
// Audio (simulateur) : aucune sortie sonore (pas de fenêtre, pas de périphérique audio). On lit
// juste assez l'en-tête WAV pour connaître la durée, afin que isPlaying()/le bouclage restent
// corrects pour les jeux et les tests, sans jamais produire de son.
namespace {
struct Track {
    bool used = false, loop = false;
    char path[512] = {0};
    double duration_s = 0;      // 0 si non déterminable (on considère alors "toujours en cours")
    uint64_t start_us = 0;      // horloge virtuelle (hal::micros()) au dernier (re)démarrage
    float volume = 1.0f;
};
Track g_tracks[hal::AUDIO_TRACKS];
float g_master = 1.0f;

uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Lit juste l'en-tête RIFF/WAVE ('fmt ' + 'data') pour calculer la durée ; renvoie false si le fichier
// n'est pas un WAV lisible. N'exige pas 16 bits (contrairement à la console) : la durée sert seulement
// au simulateur, jamais à décider si le format est jouable sur l'AKA.
bool wav_duration(const char* path, double& out_seconds) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
        fclose(f);
        return false;
    }
    uint32_t rate = 0, byte_rate = 0;
    uint16_t align = 0;
    bool got_fmt = false, got_data = false;
    uint32_t data_size = 0;
    for (;;) {
        uint8_t ck[8];
        if (fread(ck, 1, 8, f) != 8) break;
        uint32_t size = le32(ck + 4);
        long next = ftell(f) + (long)size + (long)(size & 1);
        if (!memcmp(ck, "fmt ", 4) && size >= 16) {
            uint8_t fmt[16];
            if (fread(fmt, 1, 16, f) != 16) break;
            rate = le32(fmt + 4);
            byte_rate = le32(fmt + 8);
            align = le16(fmt + 12);
            got_fmt = true;
        } else if (!memcmp(ck, "data", 4)) {
            data_size = size;
            got_data = true;
        }
        if (got_fmt && got_data) break;
        if (fseek(f, next, SEEK_SET) != 0) break;
    }
    fclose(f);
    if (!got_fmt || !got_data) return false;
    double bps = byte_rate ? byte_rate : (double)rate * align;
    if (bps <= 0) return false;
    out_seconds = data_size / bps;
    return true;
}
}  // namespace

void audio_init() {
    for (auto& t : g_tracks) t = Track{};
    g_master = 1.0f;
}

int audio_play(const char* path, bool loop, float volume) {
    int slot = -1;
    for (int i = 0; i < AUDIO_TRACKS; ++i)
        if (!g_tracks[i].used) {
            slot = i;
            break;
        }
    if (slot < 0) return -1;
    Track& t = g_tracks[slot];
    double dur = 0;
    bool ok = wav_duration(path, dur);
    if (!ok) return -1;   // fichier illisible ou pas un WAV : même échec que la console
    t.used = true;
    t.loop = loop;
    t.duration_s = dur;
    t.volume = volume;
    snprintf(t.path, sizeof t.path, "%s", path);
    t.start_us = micros();
    return slot;
}
void audio_stop(int track) {
    if (track < 0 || track >= AUDIO_TRACKS) return;
    g_tracks[track] = Track{};
}
bool audio_is_playing(int track) {
    if (track < 0 || track >= AUDIO_TRACKS || !g_tracks[track].used) return false;
    Track& t = g_tracks[track];
    if (t.duration_s <= 0) return true;   // durée indéterminée : considérée en cours jusqu'à stop()
    double elapsed = (double)(uint64_t)(micros() - t.start_us) / 1e6;
    return elapsed < t.duration_s;
}
void audio_set_volume(int track, float volume) {
    if (track >= 0 && track < AUDIO_TRACKS) g_tracks[track].volume = volume;
}
void audio_set_master_volume(float volume) { g_master = volume; }
void audio_poll() {
    for (auto& t : g_tracks) {
        if (!t.used || !t.loop || t.duration_s <= 0) continue;
        double elapsed = (double)(uint64_t)(micros() - t.start_us) / 1e6;
        if (elapsed >= t.duration_s) t.start_us = micros();   // relance la boucle (comme gb_audio_track_wav)
    }
}



void init() {
    memset(g_fb, 0, sizeof g_fb);
    audio_init();
}

uint16_t pack565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3));
}
uint16_t* framebuffer() { return g_fb; }

void present() {
    if (g_frame == g_cfg.shot_frame && !g_cfg.shot_path.empty()) write_ppm(g_cfg.shot_path.c_str());
    ++g_frame;
    g_vtime_us += 33333;   // 30 fps virtuels
    if (g_cfg.realtime) nap_ms(33);
    audio_poll();
}

uint32_t micros() { return (uint32_t)(g_cfg.realtime ? real_us() : g_vtime_us); }
uint32_t millis() { return micros() / 1000u; }
uint32_t wall_millis() { return (uint32_t)(real_us() / 1000u); }
void sleep_ms(uint32_t ms) {
    if (g_cfg.realtime) nap_ms((int)ms);
}
void feed_watchdog() {}

uint32_t buttons() {
    uint32_t m = 0;
    for (const sim::Press& p : g_cfg.presses)
        if (g_frame >= p.from && g_frame < p.to) m |= 1u << p.b;
    return m;
}
void poll_input() {}
void axes(float& lx, float& ly) {   // le simulateur n'a pas de stick : la croix tient lieu d'axes
    uint32_t b = buttons();
    lx = (float)(((b >> B_RIGHT) & 1) - ((b >> B_LEFT) & 1));
    ly = (float)(((b >> B_DOWN) & 1) - ((b >> B_UP) & 1));
}
bool quit_requested() { return g_cfg.max_frames >= 0 && g_frame >= g_cfg.max_frames; }
void return_to_loader() {
    fflush(stdout);
    exit(0);
}

// Entrée "série" du simulateur = l'entrée standard, en non bloquant. Sert à tester le protocole de
// transfert (transfer.cpp) sans matériel : voir tools/akatransfer.py et --transfer-window-ms.
// Sous Windows, pas d'équivalent simple à fcntl(O_NONBLOCK) sur stdin : le transfert via simulateur
// reste pour l'instant un outil de test Linux (la console elle-même n'est pas concernée, voir hal_aka.cpp).
#ifdef _WIN32
int serial_read(uint8_t*, int) { return 0; }
#else
int serial_read(uint8_t* buf, int max) {
    static bool nonblock_set = false;
    if (!nonblock_set) {
        int flags = fcntl(0, F_GETFL, 0);
        if (flags >= 0) fcntl(0, F_SETFL, flags | O_NONBLOCK);
        nonblock_set = true;
    }
    int r = (int)read(0, buf, (size_t)max);
    return r > 0 ? r : 0;
}
#endif
void serial_write(const uint8_t* buf, int len) {
    fwrite(buf, 1, (size_t)len, stdout);
    fflush(stdout);
}
void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void* alloc(size_t n) { return malloc(n); }
void* realloc_(void* p, size_t n) { return realloc(p, n); }
void free_(void* p) { free(p); }

}  // namespace hal
