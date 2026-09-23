// main.cpp — point d'entrée console AKA-Love (NON COMPILÉ par ESP-IDF : voir README).
// P1 : lance un jeu fixe. Le lanceur de jeux (dossier games/) est la phase P6.
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gb_core.h"   // gb_core, MOUNT_POINT
#include "game_select.h"
#include "hal.h"
#include "runtime.h"
#include "splash.h"
#include "transfer.h"

gb_core g_core;   // objet global attendu par la bibliothèque et par hal_aka.cpp (comme dans les autres jeux AKA)

// Arborescence sur la carte SD : dossier propre à AKA-Love à la racine, comme les autres jeux du loader.
// Noms longs activés (CONFIG_FATFS_LFN_HEAP). Dossiers de jeux et fichiers : minuscules (FAT ignore la casse,
// Linux et Love sur PC non). Contenu exact attendu par le loader (firmware.bin, meta.json...) : à confirmer.
#define AKALOVE_HOME MOUNT_POINT "/AKA_Love"
// Le jeu lancé est nommé dans AKA_Love/game.txt (défaut : hello) : changer de jeu = éditer ce fichier sur la SD.

static void game_task(void*) {
    hal::init();
    // Fenêtre de transfert (protocole AKAT, voir docs/PROTOCOLE_TRANSFERT.md) : 1,5 s au démarrage,
    // avant même l'image de démarrage, pour qu'un transfert depuis AKA-IDE ne soit jamais retardé par
    // l'attente d'un appui bouton. Un PC qui n'envoie rien ne coûte que ces 1,5 s.
    transfer_listen(1500, AKALOVE_HOME, nullptr);   // fenêtre passive, silencieuse (v1, sans menu)
    hal::poll_input();
    // Bouton D tenu au démarrage : écran interactif "Recevoir un code" au lieu du jeu — en attendant
    // un vrai lanceur/menu (phase P6), c'est le point d'entrée le plus simple à ce stade.
    if (hal::buttons() & (1u << hal::B_D)) transfer_receive_screen(AKALOVE_HOME, nullptr);
    show_splash(AKALOVE_HOME);   // AKA_Love/Picture.png (sinon screen.bmp), 1,5 s ou premier appui
    GameInfo g = {};
    select_game(AKALOVE_HOME, g);
    printf("[AKA-Love] jeu : %s\n", g.dir);
    Exit e;
    do {
        e = runtime_play(g);
    } while (e == Exit::Restart);
    // Pile réellement utilisée (à surveiller pendant la mise au point)
    printf("[AKA-Love] pile GameTask : %u octets libres au minimum\n",
           (unsigned)(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
    hal::return_to_loader();
}

extern "C" void app_main(void) {
    g_core.init();   // écran, bus, expander, SD (montée sur /sdcard), audio
    // Toute la VM Lua vit dans cette tâche (une seule VM, un seul fil), sur CPU1 comme les autres jeux AKA.
    // La pile est en RAM interne, déjà chargée par le framebuffer statique (150 Ko) : 24 Ko, à ajuster.
    xTaskCreatePinnedToCore(game_task, "GameTask", 24 * 1024, nullptr, 5, nullptr, 1);
}
