// =============================================================================
//  hal_sdl.cpp — implémentation SDL2 (fenêtrée + son réel) du HAL AKA-Love.
//  Alternative à hal_sim.cpp (sans fenêtre/PPM) pour JOUER en vrai sur PC.
//
//  Réutilise tout le runtime et main_sim.cpp : seule la couche HAL change.
//  On fournit ici aussi les shims sim::config()/sim::frame() (normalement
//  définis dans hal_sim.cpp) pour que main_sim.cpp édite les liens.
//
//  Fidèle aux limites AKA : 320×240 BGR565 (même pack565), 4 voies audio,
//  boutons D-Pad/A B C D/L1 R1/MENU, stick analogique, RUN réservé.
//
//  Clavier : flèches = D-Pad · I J K L = stick · Espace/Entrée = A · X = B ·
//            C = C · V = D · Maj G = L1 · Maj D = R1 · M = MENU · Échap = quitter.
// =============================================================================
#include "hal.h"
#include "sim.h"

// On garde notre propre main() (dans main_sim.cpp) : SDL ne doit pas le renommer
// en SDL_main ni imposer SDL2main/WinMain sous Windows.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// ── Shims sim:: (fournis par hal_sim.cpp en mode sans fenêtre) ───────────────
namespace sim {
static Config g_cfg;          // main_sim.cpp y écrit les options (ignorées en mode fenêtré)
static int    g_frame = 0;
Config& config() { return g_cfg; }
int frame() { return g_frame; }
}  // namespace sim

namespace hal {

// ── Fenêtre / rendu ──────────────────────────────────────────────────────────
static const int kW = SCREEN_W, kH = SCREEN_H;
static int kScale = 2;                 // 320×240 → 640×480

static SDL_Window*   g_win  = nullptr;
static SDL_Renderer* g_ren  = nullptr;
static SDL_Texture*  g_tex  = nullptr;
static uint16_t      g_fb[kW * kH];    // framebuffer BGR565
static uint32_t      g_rgba[kW * kH];  // tampon de conversion

// ── Horloge ──────────────────────────────────────────────────────────────────
static uint32_t g_vclock_ms = 0;       // horloge « jeu » (un pas par frame)
static const uint32_t kFrameMs = 33;   // ~30 fps, comme la console

// ── Entrées ──────────────────────────────────────────────────────────────────
static uint32_t g_buttons = 0;
static float    g_lx = 0.f, g_ly = 0.f;
static bool     g_quit = false;      // quitter l'appli (fermeture de la fenêtre)
static bool     g_to_menu = false;   // revenir au menu de sélection (touche Échap)

// ── Audio (mixeur logiciel 4 voies, WAV 16 bits) ─────────────────────────────
struct Track {
    Uint8*  buf = nullptr;
    Uint32  len = 0;
    Uint32  pos = 0;
    bool    playing = false;
    bool    loop = false;
    float   vol = 1.f;
};
static Track             g_tracks[AUDIO_TRACKS];
static SDL_AudioDeviceID g_audio = 0;
static SDL_AudioSpec     g_spec;
static float             g_master = 1.f;

// =============================================================================
//  Lifecycle
// =============================================================================
void init()
{
    std::memset(g_fb, 0, sizeof(g_fb));
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        std::exit(1);
    }
    g_win = SDL_CreateWindow("AKA-Love (PC)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kW * kScale, kH * kScale, SDL_WINDOW_SHOWN);
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(g_ren, kW, kH);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_STREAMING, kW, kH);
    audio_init();
}

// =============================================================================
//  Display
// =============================================================================
uint16_t pack565(uint8_t r, uint8_t g, uint8_t b)
{
    // BGR565 (ordre lib AKA), identique à hal_sim/hal_aka : R bas, B haut.
    return (uint16_t)((r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11));
}

uint16_t* framebuffer() { return g_fb; }

static inline uint32_t bgr565_to_rgba(uint16_t v)
{
    int r5 = v & 0x1F, g6 = (v >> 5) & 0x3F, b5 = (v >> 11) & 0x1F;
    uint8_t r = (uint8_t)((r5 << 3) | (r5 >> 2));
    uint8_t g = (uint8_t)((g6 << 2) | (g6 >> 4));
    uint8_t b = (uint8_t)((b5 << 3) | (b5 >> 2));
    // RGBA8888 (SDL) → 0xRRGGBBAA
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFF;
}

void present()
{
    for (int i = 0; i < kW * kH; ++i) g_rgba[i] = bgr565_to_rgba(g_fb[i]);
    SDL_UpdateTexture(g_tex, nullptr, g_rgba, kW * sizeof(uint32_t));
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, nullptr, nullptr);
    SDL_RenderPresent(g_ren);

    static uint64_t last = 0;
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t now = SDL_GetPerformanceCounter();
    if (last != 0) {
        double elapsed_ms = (double)(now - last) * 1000.0 / (double)freq;
        if (elapsed_ms < kFrameMs) SDL_Delay((Uint32)(kFrameMs - elapsed_ms));
    }
    last = SDL_GetPerformanceCounter();

    g_vclock_ms += kFrameMs;
    ++sim::g_frame;
    audio_poll();
}

// =============================================================================
//  Time
// =============================================================================
uint32_t millis()      { return g_vclock_ms; }
uint32_t micros()      { return g_vclock_ms * 1000u; }
uint32_t wall_millis() { return SDL_GetTicks(); }
void     sleep_ms(uint32_t ms) { SDL_Delay(ms); }
void     feed_watchdog() {}

// =============================================================================
//  Input
// =============================================================================
static inline void set_btn(uint32_t& mask, Button b, bool down)
{ if (down) mask |= (1u << b); }

void poll_input()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) g_quit = true;                                      // fermer la fenêtre = quitter
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) g_to_menu = true;  // Échap = retour au menu
    }

    const Uint8* k = SDL_GetKeyboardState(nullptr);
    uint32_t m = 0;
    set_btn(m, B_UP,    k[SDL_SCANCODE_UP]);
    set_btn(m, B_DOWN,  k[SDL_SCANCODE_DOWN]);
    set_btn(m, B_LEFT,  k[SDL_SCANCODE_LEFT]);
    set_btn(m, B_RIGHT, k[SDL_SCANCODE_RIGHT]);
    set_btn(m, B_A,     k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN]);
    set_btn(m, B_B,     k[SDL_SCANCODE_X]);
    set_btn(m, B_C,     k[SDL_SCANCODE_C]);
    set_btn(m, B_D,     k[SDL_SCANCODE_V]);
    set_btn(m, B_L1,    k[SDL_SCANCODE_LSHIFT]);
    set_btn(m, B_R1,    k[SDL_SCANCODE_RSHIFT]);
    // Le « bouton MENU » n'est PAS mappé sur une lettre (SDL lit la POSITION physique : le M QWERTY
    // tombe sur « , » en AZERTY, source de confusion). Le retour au menu se fait par Échap, intercepté
    // par le runtime (quit_to_menu), donc indépendamment du code Lua du jeu.
    g_buttons = m;

    g_lx = (k[SDL_SCANCODE_L] ? 1.f : 0.f) - (k[SDL_SCANCODE_J] ? 1.f : 0.f);
    g_ly = (k[SDL_SCANCODE_K] ? 1.f : 0.f) - (k[SDL_SCANCODE_I] ? 1.f : 0.f);
}

uint32_t buttons() { return g_buttons; }
void axes(float& lx, float& ly) { lx = g_lx; ly = g_ly; }
// Le runtime s'arrête si on quitte l'appli OU si on demande le menu — ainsi le retour au menu ne
// dépend pas du code du jeu (max_frames ignoré en mode fenêtré).
bool quit_requested() { return g_quit || g_to_menu; }
bool quit_to_menu() { return g_to_menu && !g_quit; }   // la session s'est terminée pour revenir au menu
void clear_quit_to_menu() { g_to_menu = false; }

[[noreturn]] void return_to_loader()
{
    if (g_audio) SDL_CloseAudioDevice(g_audio);
    if (g_tex) SDL_DestroyTexture(g_tex);
    if (g_ren) SDL_DestroyRenderer(g_ren);
    if (g_win) SDL_DestroyWindow(g_win);
    SDL_Quit();
    std::exit(0);
}

// =============================================================================
//  Série / journal
// =============================================================================
int  serial_read(uint8_t*, int) { return 0; }
void serial_write(const uint8_t* buf, int len) { if (buf && len > 0) std::fwrite(buf, 1, (size_t)len, stderr); }
void log(const char* fmt, ...) { va_list ap; va_start(ap, fmt); std::vfprintf(stderr, fmt, ap); va_end(ap); std::fputc('\n', stderr); }

// =============================================================================
//  Audio
// =============================================================================
static void mix_callback(void*, Uint8* stream, int bytes)
{
    std::memset(stream, 0, (size_t)bytes);
    int16_t* out = (int16_t*)stream;
    int samples = bytes / (int)sizeof(int16_t);

    for (int t = 0; t < AUDIO_TRACKS; ++t) {
        Track& tr = g_tracks[t];
        if (!tr.playing || !tr.buf) continue;
        const int16_t* src = (const int16_t*)tr.buf;
        Uint32 total = tr.len / (Uint32)sizeof(int16_t);
        if (total == 0) { tr.playing = false; continue; }
        float gain = tr.vol * g_master;
        Uint32 cur = tr.pos / (Uint32)sizeof(int16_t);
        for (int i = 0; i < samples; ++i) {
            if (cur >= total) {
                if (tr.loop) cur = 0;
                else { tr.playing = false; break; }
            }
            int v = out[i] + (int)(src[cur] * gain);
            if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
            out[i] = (int16_t)v;
            ++cur;
        }
        tr.pos = cur * (Uint32)sizeof(int16_t);
    }
}

void audio_init()
{
    for (auto& t : g_tracks) { if (t.buf) { std::free(t.buf); } t = Track{}; }
    g_master = 1.f;
    if (g_audio) return;   // déjà ouvert
    SDL_AudioSpec want; SDL_zero(want);
    want.freq = 44100; want.format = AUDIO_S16SYS; want.channels = 2;
    want.samples = 1024; want.callback = mix_callback;
    g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, &g_spec, 0);
    if (g_audio) SDL_PauseAudioDevice(g_audio, 0);
}

int audio_play(const char* path, bool loop, float volume)
{
    int t = -1;
    for (int i = 0; i < AUDIO_TRACKS; ++i) if (!g_tracks[i].playing) { t = i; break; }
    if (t < 0 || !path) return -1;

    SDL_AudioSpec wav; Uint8* wbuf = nullptr; Uint32 wlen = 0;
    if (!SDL_LoadWAV(path, &wav, &wbuf, &wlen)) return -1;

    SDL_AudioCVT cvt;
    int need = SDL_BuildAudioCVT(&cvt, wav.format, wav.channels, wav.freq,
                                 g_spec.format, g_spec.channels, g_spec.freq);
    if (need < 0) { SDL_FreeWAV(wbuf); return -1; }

    Uint8* data; Uint32 dlen;
    if (need == 0) {
        data = (Uint8*)std::malloc(wlen); std::memcpy(data, wbuf, wlen); dlen = wlen;
        SDL_FreeWAV(wbuf);
    } else {
        cvt.len = (int)wlen;
        cvt.buf = (Uint8*)std::malloc((size_t)(cvt.len * cvt.len_mult));
        std::memcpy(cvt.buf, wbuf, wlen);
        SDL_FreeWAV(wbuf);
        if (SDL_ConvertAudio(&cvt) != 0) { std::free(cvt.buf); return -1; }
        data = cvt.buf; dlen = (Uint32)cvt.len_cvt;
    }

    SDL_LockAudioDevice(g_audio);
    Track& tr = g_tracks[t];
    if (tr.buf) std::free(tr.buf);
    tr.buf = data; tr.len = dlen; tr.pos = 0;
    tr.loop = loop; tr.vol = volume; tr.playing = true;
    SDL_UnlockAudioDevice(g_audio);
    return t;
}

void audio_stop(int track)
{
    if (track < 0 || track >= AUDIO_TRACKS) return;
    SDL_LockAudioDevice(g_audio);
    g_tracks[track].playing = false; g_tracks[track].pos = 0;
    SDL_UnlockAudioDevice(g_audio);
}

bool audio_is_playing(int track)
{ return (track >= 0 && track < AUDIO_TRACKS) && g_tracks[track].playing; }

void audio_set_volume(int track, float volume)
{ if (track >= 0 && track < AUDIO_TRACKS) g_tracks[track].vol = volume; }

void audio_set_master_volume(float volume) { g_master = volume; }

void audio_poll() { /* bouclage géré dans le callback */ }

// =============================================================================
//  Mémoire
// =============================================================================
void* alloc(size_t n)              { return std::malloc(n); }
void* realloc_(void* p, size_t n)  { return std::realloc(p, n); }
void  free_(void* p)               { std::free(p); }

}  // namespace hal
