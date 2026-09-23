// transfer.h — réception de fichiers depuis un PC (protocole "AKAT", voir docs/PROTOCOLE_TRANSFERT.md).
// Deux façons de recevoir :
//   - transfer_listen()         : fenêtre passive et silencieuse (au démarrage) — écrase sans demander,
//                                 comme avant ; pratique pour un envoi automatisé (AKA-IDE, script CI).
//   - transfer_receive_screen() : écran interactif ("Recevoir un code" au menu) — dessine un message
//                                 d'attente, et si le fichier envoyé existe déjà, demande au joueur quoi
//                                 faire (Écraser / Renommer / Annuler) avant d'écrire quoi que ce soit.
// Utilisé aussi bien par la console (hal_aka.cpp) que par le simulateur (hal_sim.cpp) : la logique ne
// dépend que de hal:: (et, pour l'écran interactif, de gfx:: pour le texte — comme splash.cpp).
#pragma once
#include <stdint.h>

void transfer_listen(uint32_t window_ms, const char* home, const char* mpy_root);

// Écran interactif, jusqu'à ce que le joueur appuie sur MENU (ou que --frames s'épuise en simulation).
// `home` : dossier AKA_Love (racine "lua") ; `mpy_root` : racine MicroPython (nullptr si non géré ici).
void transfer_receive_screen(const char* home, const char* mpy_root);
