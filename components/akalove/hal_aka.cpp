// hal_aka.cpp — HAL de la console Gamebuino AKA (ESP32-S3, ESP-IDF 5.4), écrite contre la bibliothèque
// `gamebuino` fournie avec wargame_aka (gb_core, gb_graphics, gb_ll_*).
//
// STATUT : non compilé par ESP-IDF (pas de chaîne ici). Les appels gb_* ont été vérifiés contre les vrais
// en-têtes de la bibliothèque (voir tools/check_hal_aka.sh) ; le comportement sur le matériel reste à valider.
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "gb_audio_player.h"
#include "gb_audio_track_wav.h"
#include "gb_core.h"
#include "gb_graphics.h"
#include "gb_ll_i2c.h"   // gb_ll_expander_read()
#include "hal.h"

static_assert(sizeof(gb_pixel) == 2, "AKA-Love exige un framebuffer 16 bits (pas USE_VIDEO_256_INDEXED)");
static_assert(SCREEN_WIDTH == hal::SCREEN_W && SCREEN_HEIGHT == hal::SCREEN_H, "écran 320x240");

extern gb_core g_core;   // défini dans main/main.cpp, comme dans les autres jeux AKA

namespace {

gb_graphics g_gfx;
uint32_t g_mask = 0;               // boutons du jeu (1 << hal::Button)
float g_lx = 0, g_ly = 0;          // stick analogique
bool g_quit = false;
uint32_t g_combo_t0 = 0, g_run_t0 = 0;

// Audio : un seul lecteur, AUDIO_TRACKS pistes WAV (même schéma que Dark & Under : gb_audio_player +
// gb_audio_track_wav, add_track() une fois au démarrage). gb_audio_player::pool() doit tourner
// régulièrement ; ici depuis present() plutôt que depuis une tâche FreeRTOS séparée comme Dark & Under
// — GameTask appelle déjà present() à chaque frame, et ça évite de synchroniser deux tâches sur les
// mêmes objets gb_audio_track_wav (jamais touchés que par GameTask).
gb_audio_player g_audio;
gb_audio_track_wav g_tracks[hal::AUDIO_TRACKS];
bool g_loop[hal::AUDIO_TRACKS] = {false, false, false, false};
char g_track_path[hal::AUDIO_TRACKS][300];

constexpr uint32_t QUIT_COMBO_MS = 500;    // RUN+MENU : retour au loader (convention des jeux AKA)
constexpr uint32_t POWER_OFF_MS = 1500;    // RUN seul, maintenu : extinction

struct KeyMap {
    uint16_t key;      // bit expander (gb_buttons::KEY_xxx)
    hal::Button b;
};
const KeyMap KEYMAP[] = {
    {gb_buttons::KEY_UP, hal::B_UP},   {gb_buttons::KEY_DOWN, hal::B_DOWN}, {gb_buttons::KEY_LEFT, hal::B_LEFT},
    {gb_buttons::KEY_RIGHT, hal::B_RIGHT}, {gb_buttons::KEY_A, hal::B_A},   {gb_buttons::KEY_B, hal::B_B},
    {gb_buttons::KEY_C, hal::B_C},     {gb_buttons::KEY_D, hal::B_D},       {gb_buttons::KEY_L1, hal::B_L1},
    {gb_buttons::KEY_R1, hal::B_R1},   {gb_buttons::KEY_MENU, hal::B_MENU},
};

float clamp1(float v) { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); }

}  // namespace

namespace hal {

void init() {
    g_gfx.set_backlight_percent(80);
    g_gfx.set_refresh_rate(60);   // synchro verticale : present() ne dépasse pas 60 images/s
    memset(&::framebuffer[0], 0, sizeof(gb_pixel) * SCREEN_WIDTH * SCREEN_HEIGHT);
    g_mask = 0;
    g_quit = false;
    g_combo_t0 = g_run_t0 = 0;
    for (int i = 0; i < AUDIO_TRACKS; ++i) {
        g_audio.add_track(&g_tracks[i]);
        g_loop[i] = false;
        g_track_path[i][0] = 0;
    }
    poll_input();
}

// La bibliothèque fournit le framebuffer (statique en SRAM, ou PSRAM selon gb_config.h) : on dessine dedans
uint16_t* framebuffer() { return (uint16_t*)&::framebuffer[0]; }
uint16_t pack565(uint8_t r, uint8_t g, uint8_t b) { return lcd_color_rgb(r, g, b); }   // BGR565 du panneau

// Attend la fin du transfert DMA précédent et la synchro verticale, puis lance le suivant (ZERO_BUFFER)
void present() {
    g_gfx.update();
    audio_poll();
}

uint32_t micros() { return (uint32_t)g_core.get_micros(); }
uint32_t millis() { return g_core.get_millis(); }
uint32_t wall_millis() { return g_core.get_millis(); }
void sleep_ms(uint32_t ms) { g_core.delay_ms(ms); }
void feed_watchdog() { esp_task_wdt_reset(); }

// Une seule lecture I2C par frame. On n'appelle PAS gb_buttons::update() : il coupe l'alimentation dès que
// RUN est vu, ce qui interdirait la combinaison RUN+MENU.
void poll_input() {
    uint16_t raw = gb_ll_expander_read() & EXPANDER_KEY;   // 1 = appuyé
    g_core.joystick.update();
    raw |= g_core.joystick.state();                         // croix émulée par le stick (seuil 50 %)
    uint32_t now = g_core.get_millis();

    bool run = (raw & EXPANDER_KEY_RUN) != 0, menu = (raw & EXPANDER_KEY_MENU) != 0;
    if (run && menu) {
        if (!g_combo_t0) g_combo_t0 = now ? now : 1;
        else if (now - g_combo_t0 >= QUIT_COMBO_MS) g_quit = true;
    } else {
        g_combo_t0 = 0;
    }
    if (run && !menu) {
        if (!g_run_t0) g_run_t0 = now ? now : 1;
        else if (now - g_run_t0 >= POWER_OFF_MS) g_core.power_down();
    } else {
        g_run_t0 = 0;
    }
    if (run) raw &= (uint16_t)~EXPANDER_KEY_MENU;   // MENU tenu avec RUN = commande système, pas un appui du jeu

    uint32_t m = 0;
    for (const KeyMap& k : KEYMAP)
        if (raw & k.key) m |= 1u << k.b;
    g_mask = m;

    float lx = clamp1(g_core.joystick.get_x() / 1000.0f);
    float ly = clamp1(-g_core.joystick.get_y() / 1000.0f);   // le stick renvoie y > 0 vers le haut ; Love : bas positif
    if (lx > -0.1f && lx < 0.1f) lx = (float)(((raw & EXPANDER_KEY_RIGHT) ? 1 : 0) - ((raw & EXPANDER_KEY_LEFT) ? 1 : 0));
    if (ly > -0.1f && ly < 0.1f) ly = (float)(((raw & EXPANDER_KEY_DOWN) ? 1 : 0) - ((raw & EXPANDER_KEY_UP) ? 1 : 0));
    g_lx = lx;
    g_ly = ly;
}

uint32_t buttons() { return g_mask; }
void axes(float& lx, float& ly) {
    lx = g_lx;
    ly = g_ly;
}
bool quit_requested() { return g_quit; }

// Même mécanisme que le retour loader des autres jeux AKA : la partition « loader » (ota_1) redevient
// la partition de démarrage.
[[noreturn]] void return_to_loader() {
    const esp_partition_t* loader =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (loader) esp_ota_set_boot_partition(loader);
    else printf("[AKA-Love] partition loader (ota_1) introuvable : simple redémarrage\n");
    esp_restart();
}

// Console = UART par défaut + USB-Serial-JTAG en secondaire : printf/fwrite atteignent le PC.
// TODO (phase P3, chargeur USB) : lecture des commandes du PC (usb_serial_jtag_read_bytes ou stdin).
// Console = UART par défaut + USB-Serial-JTAG en secondaire : printf/fwrite atteignent le PC.
// serial_read() lit ce même flux, en non bloquant, pour le protocole de transfert (transfer.cpp) —
// NON VÉRIFIÉ SUR MATÉRIEL (comme le reste de ce fichier) : ESP-IDF redirige déjà stdin/stdout vers
// l'UART console par défaut (driver UART installé par le démarrage standard), donc `read(0, ...)`
// DEVRAIT fonctionner tel quel, mais ce point précis (lecture non bloquante sur la console standard,
// sans piloter le driver UART directement) est celui qui mérite le premier test sur la vraie carte.
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
void serial_write(const uint8_t* buf, int len) { fwrite(buf, 1, (size_t)len, stdout); }
void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

// Politique d'ESP-IDF (CONFIG_SPIRAM_USE_MALLOC + ALWAYSINTERNAL=16 Ko) : les petits blocs de la VM Lua
// restent en RAM interne (rapide), les gros (images, tampons) vont en PSRAM.
void* alloc(size_t n) { return malloc(n); }
void* realloc_(void* p, size_t n) { return realloc(p, n); }
void free_(void* p) { free(p); }

// Un canal = un objet gb_audio_track_wav permanent (ajouté une fois au lecteur dans init()) ;
// play_wav(chemin) lit directement depuis la carte SD, aucun décodage ni tampon mémoire ici.
// L'ajout des pistes au lecteur se fait dans hal::init() (une seule fois, avant le premier jeu) ; rien
// de plus à faire ici. Fonction fournie pour respecter le même contrat hal:: que le simulateur.
void audio_init() {}

int audio_play(const char* path, bool loop, float volume) {
    int slot = -1;
    for (int i = 0; i < AUDIO_TRACKS; ++i)
        if (!g_tracks[i].is_playing()) {
            slot = i;
            break;
        }
    if (slot < 0) return -1;
    g_tracks[slot].set_track_volume(volume);
    if (g_tracks[slot].play_wav(path) != 0) return -1;   // fichier illisible ou format non géré (voir
                                                           // gb_audio_track_wav::check_file_format : 16
                                                           // bits obligatoire)
    g_loop[slot] = loop;
    strncpy(g_track_path[slot], path, sizeof g_track_path[slot] - 1);
    return slot;
}
void audio_stop(int track) {
    if (track < 0 || track >= AUDIO_TRACKS) return;
    g_tracks[track].stop_playing();
    g_loop[track] = false;
}
bool audio_is_playing(int track) { return track >= 0 && track < AUDIO_TRACKS && g_tracks[track].is_playing(); }
void audio_set_volume(int track, float volume) {
    if (track >= 0 && track < AUDIO_TRACKS) g_tracks[track].set_track_volume(volume);
}
void audio_set_master_volume(float volume) {
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    g_audio.set_master_volume((uint8_t)(volume * 255.0f + 0.5f));
}
// gb_audio_track_wav ne boucle pas nativement (comme dans Dark & Under) : on relance nous-mêmes dès
// qu'une piste bouclée s'arrête. Appelé depuis present(), donc une fois par frame — un peu plus large
// que les 5 ms de la tâche dédiée de Dark & Under, mais largement suffisant pour un jeu.
void audio_poll() {
    for (int i = 0; i < AUDIO_TRACKS; ++i)
        if (g_loop[i] && !g_tracks[i].is_playing()) g_tracks[i].play_wav(g_track_path[i]);
}

}  // namespace hal
