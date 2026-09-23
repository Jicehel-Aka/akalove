// main_sim.cpp — akalove-sim <dossier-du-jeu> [options]
// ou : akalove-sim --home <dossier AKA_Love> [options]   (lit game.txt comme la console)
//   --frames N          fin de session après N frames (défaut 300 ; -1 = illimité)
//   --shot fichier.ppm  écrit le framebuffer après la frame --shot-frame (défaut : dernière)
//   --shot-frame K
//   --press bouton:de:a bouton maintenu pendant les frames [de, a)  (up down left right a b c d l1 r1 menu)
//   --save dossier      racine des sauvegardes (défaut : ./_save)
//   --watchdog-ms N     délai du chien de garde (défaut 4000)
//   --no-splash         avec --home : ne pas afficher Picture.png / screen.bmp au démarrage
//   --transfer-window-ms N   écoute le protocole de transfert (AKAT) sur l'entrée standard
//                            pendant N ms avant de lancer le jeu (0 = désactivé, défaut)
//   --receive-screen         lance l'écran interactif "Recevoir un code" (--home requis) au lieu
//                            du jeu ; se termine sur --frames écoulées ou bouton menu simulé
//   --realtime          horloge réelle, 30 fps (sinon horloge virtuelle déterministe)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_select.h"
#include "hal.h"
#include "runtime.h"
#include "splash.h"
#include "transfer.h"
#include "sim.h"

static bool button_from_name(const char* n, hal::Button& b) {
    static const struct {
        const char* name;
        hal::Button b;
    } T[] = {{"up", hal::B_UP},   {"down", hal::B_DOWN}, {"left", hal::B_LEFT}, {"right", hal::B_RIGHT}, {"a", hal::B_A},
             {"b", hal::B_B},     {"c", hal::B_C},       {"d", hal::B_D},       {"l1", hal::B_L1},       {"r1", hal::B_R1},
             {"menu", hal::B_MENU}};
    for (const auto& e : T)
        if (!strcmp(e.name, n)) {
            b = e.b;
            return true;
        }
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage : %s <dossier-du-jeu> [--frames N] [--shot f.ppm] [--shot-frame K] [--press b:de:a] "
                        "[--save dir] [--watchdog-ms N] [--realtime]\n", argv[0]);
        return 2;
    }
    sim::Config& cfg = sim::config();
    GameInfo g;
    memset(&g, 0, sizeof g);
    int first_opt = 2;
    const char* home = nullptr;
    bool no_splash = false, receive_screen = false;
    uint32_t transfer_ms = 0;
    if (!strcmp(argv[1], "--home") && argc >= 3) {
        home = argv[2];
        select_game(home, g);
        first_opt = 3;
    } else {
        snprintf(g.dir, sizeof g.dir, "%s", argv[1]);
        snprintf(g.save_root, sizeof g.save_root, "./_save");
        const char* base = argv[1];   // dernier composant du chemin (separateurs slash et antislash)
        for (const char* c = argv[1]; *c; ++c)
            if ((*c == '/' || *c == '\\') && c[1]) base = c + 1;
        snprintf(g.name, sizeof g.name, "%s", base);
    }

    for (int i = first_opt; i < argc; ++i) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) cfg.max_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) cfg.shot_path = argv[++i];
        else if (!strcmp(argv[i], "--shot-frame") && i + 1 < argc) cfg.shot_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) snprintf(g.save_root, sizeof g.save_root, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--watchdog-ms") && i + 1 < argc) runtime_set_watchdog_ms((uint32_t)atoi(argv[++i]));
        else if (!strcmp(argv[i], "--realtime")) cfg.realtime = true;
        else if (!strcmp(argv[i], "--no-splash")) no_splash = true;
        else if (!strcmp(argv[i], "--transfer-window-ms") && i + 1 < argc) transfer_ms = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--receive-screen")) receive_screen = true;
        else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
            char name[16];
            int from, to;
            if (sscanf(argv[++i], "%15[a-z0-9]:%d:%d", name, &from, &to) != 3) {
                fprintf(stderr, "--press : format bouton:de:a\n");
                return 2;
            }
            hal::Button b;
            if (!button_from_name(name, b)) {
                fprintf(stderr, "--press : bouton inconnu '%s'\n", name);
                return 2;
            }
            cfg.presses.push_back({b, from, to});
        } else {
            fprintf(stderr, "option inconnue : %s\n", argv[i]);
            return 2;
        }
    }
    if (cfg.shot_path.size() && cfg.shot_frame < 0) cfg.shot_frame = cfg.max_frames > 0 ? cfg.max_frames - 1 : 2;

    hal::init();
    if (receive_screen && home) {
        transfer_receive_screen(home, nullptr);
        return 0;
    }
    if (transfer_ms && home) transfer_listen(transfer_ms, home, nullptr);   // --home requis (racine "games/")
    if (home && !no_splash) show_splash(home);   // comme la console : Picture.png (ou screen.bmp) avant le jeu
    Exit e;
    do {
        e = runtime_play(g);
    } while (e == Exit::Restart);
    return 0;
}
