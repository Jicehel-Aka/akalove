// game_menu.h — lanceur de jeux (phase P6).
// Affiche la liste des dossiers de <home>/games/, laisse le joueur en choisir un
// (D-Pad haut/bas + A pour lancer, MENU pour revenir au loader), écrit le choix
// dans <home>/game.txt (compatible avec game_select) puis remplit `g`.
// Renvoie true si un jeu a été choisi, false si annulé (MENU) ou si aucun jeu.
// Ne dépend que de hal:: (entrées/temps) et gfx:: (texte) — comme splash/transfer :
// même code sur la console (hal_aka) et sur les simulateurs PC (hal_sim/hal_sdl).
#pragma once
#include "runtime.h"

bool choose_game(const char* home, GameInfo& g);
