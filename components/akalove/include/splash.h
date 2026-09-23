// splash.h — image de démarrage : <home>/Picture.png (sinon <home>/screen.bmp).
#pragma once
#include <stdint.h>

// Affiche l'image, mise à l'échelle pour tenir dans 320x240 (bandes noires si le format diffère), pendant
// `ms` millisecondes ou jusqu'à un NOUVEL appui de bouton (un bouton déjà tenu au lancement ne compte pas).
// Sans image lisible : ne fait rien. Renvoie true si une image a été affichée.
bool show_splash(const char* home, uint32_t ms = 1500);
