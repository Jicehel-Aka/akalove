// sim.h — configuration du simulateur (sans fenêtre) : horloge virtuelle, boutons scriptés, captures
#pragma once
#include <string>
#include <vector>

#include "hal.h"

namespace sim {

struct Press {
    hal::Button b;
    int from, to;   // appuyé pendant les frames [from, to)
};

struct Config {
    int max_frames = 300;        // fin de session après N présentations
    int shot_frame = -1;         // frame après laquelle le framebuffer est écrit (PPM)
    std::string shot_path;
    bool realtime = false;       // false : horloge virtuelle, 1/30 s par frame (déterministe)
    std::vector<Press> presses;
};

Config& config();
int frame();

}  // namespace sim
