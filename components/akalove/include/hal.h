// hal.h — HAL v2 d'AKA-Love : framebuffer, entrées, temps, mémoire, série.
// Implémentations : hal_aka.cpp (console, ESP-IDF) et sim/hal_sim.cpp (PC, sans fenêtre).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace hal {

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

void init();

// --- Affichage -------------------------------------------------------------------------------
uint16_t pack565(uint8_t r, uint8_t g, uint8_t b);   // BGR565 (panneau AKA) : seul endroit à adapter
uint16_t* framebuffer();                              // SCREEN_W * SCREEN_H pixels
void present();                                       // pousse l'image, applique la cadence

// --- Temps -----------------------------------------------------------------------------------
uint32_t millis();        // horloge du jeu (l'horloge virtuelle du simulateur avance à chaque present())
uint32_t micros();
uint32_t wall_millis();   // horloge réelle : sert au chien de garde du script
void sleep_ms(uint32_t ms);
void feed_watchdog();

// --- Entrées ---------------------------------------------------------------------------------
// Boutons physiques de l'AKA : croix (ou stick analogique), A B C D, L1 R1, MENU. RUN est réservé par la
// console (extinction / retour au loader) et n'est jamais transmis au jeu.
enum Button : uint8_t { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_C, B_D, B_L1, B_R1, B_MENU, B_COUNT };
void poll_input();        // lit le matériel UNE fois ; appelé par love.event.pump() (une fois par frame)
uint32_t buttons();       // masque de bits (1 << Button) tel que lu par le dernier poll_input()
void axes(float& lx, float& ly);   // stick analogique -1..1 (ly > 0 = vers le bas, comme Love)
bool quit_requested();    // console : RUN+MENU maintenus 500 ms ; simulateur : fin de la session
[[noreturn]] void return_to_loader();   // console : redémarre sur le loader AKA ; simulateur : quitte

// --- Série / journal -------------------------------------------------------------------------
int serial_read(uint8_t* buf, int max);
void serial_write(const uint8_t* buf, int len);
#if defined(__GNUC__)
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void log(const char* fmt, ...);
#endif

// --- Audio : jusqu'à AUDIO_TRACKS pistes WAV 16 bits (mono ou stéréo) mixées matériellement, comme
// gb_audio_player (4 canaux physiques). Un identifiant de piste (0..AUDIO_TRACKS-1) tient lieu de
// "canal" ; love.audio associe chaque Source jouée à une piste libre. Aucun mélange logiciel ici : sur
// la console, gb_audio_player s'en charge ; le simulateur ne produit aucun son (pas de sortie audio),
// seulement l'état (en cours/terminé) pour que les jeux et les tests restent corrects sans matériel.
constexpr int AUDIO_TRACKS = 4;
void audio_init();
// Démarre `path` (chemin ABSOLU, résolu par love.filesystem) sur une piste libre ; renvoie son numéro,
// ou -1 si aucune piste n'est libre ou si le fichier est illisible/invalide (WAV 16 bits attendu).
int audio_play(const char* path, bool loop, float volume);
void audio_stop(int track);
bool audio_is_playing(int track);
void audio_set_volume(int track, float volume);       // 0..1
void audio_set_master_volume(float volume);            // 0..1
// À appeler une fois par frame (déjà fait par hal::present()) : relance les pistes en boucle terminées
// (comme gb_audio_track_wav, qui ne boucle pas nativement) et, sur la console, fait avancer le mixeur.
void audio_poll();

// --- Mémoire : politique d'allocation d'ESP-IDF (petits blocs en RAM interne, gros en PSRAM) ---
void* alloc(size_t n);
void* realloc_(void* p, size_t n);
void free_(void* p);

}  // namespace hal
