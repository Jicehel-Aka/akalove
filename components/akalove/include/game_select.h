// game_select.h — choix du jeu à lancer, sans recompiler le firmware.
#pragma once
#include "runtime.h"

// Remplit `g` pour <home>/games/<nom> (sauvegardes dans <home>/save). Le nom est la première ligne de
// <home>/game.txt ; sans fichier, ou avec un nom invalide (lettres, chiffres, _ - . seulement), c'est "hello".
void select_game(const char* home, GameInfo& g);
