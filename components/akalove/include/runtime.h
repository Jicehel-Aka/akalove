// runtime.h — cycle de vie d'un jeu : conf.lua, main.lua, love.run, erreurs.
#pragma once
#include <stdint.h>

struct GameInfo {
    char dir[192];        // dossier source du jeu (main.lua, conf.lua, assets)
    char save_root[192];  // racine des sauvegardes ; le dossier du jeu est save_root/<identité>
    char name[64];        // nom par défaut (identité si conf.lua n'en donne pas)
};

enum class Exit { Quit, Restart };

// Exécute un jeu du début à la sortie (à appeler depuis la GameTask). Bloquant.
Exit runtime_play(const GameInfo& g);

// Délai (ms réelles) avant qu'un script qui ne rend pas la main soit interrompu. Défaut : 4000.
void runtime_set_watchdog_ms(uint32_t ms);

// Posé au début de chaque frame ; remis à jour par love.event.pump() et love.graphics.present().
extern volatile uint32_t g_frame_t0;
