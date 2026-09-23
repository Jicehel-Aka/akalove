// game_select.cpp — lit <home>/game.txt : première ligne = nom du dossier du jeu dans <home>/games.
#include "game_select.h"

#include <stdio.h>
#include <string.h>

#include "hal.h"

namespace {

void put(char* dst, size_t n, const char* a, const char* b = "", const char* c = "") {   // concaténation bornée
    size_t pos = 0;
    const char* parts[3] = {a, b, c};
    for (const char* part : parts) {
        size_t l = strlen(part);
        if (pos + l >= n) l = n - 1 - pos;
        memcpy(dst + pos, part, l);
        pos += l;
    }
    dst[pos] = 0;
}

bool valid_name(const char* s) {
    size_t l = strlen(s);
    if (l == 0 || l > 60 || !strcmp(s, ".") || !strcmp(s, "..")) return false;
    for (; *s; ++s) {
        char c = *s;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

}  // namespace

void select_game(const char* home, GameInfo& g) {
    char name[64];
    put(name, sizeof name, "hello");

    char path[300];
    put(path, sizeof path, home, "/game.txt");
    if (FILE* f = fopen(path, "rb")) {
        char line[80];
        size_t n = fread(line, 1, sizeof line - 1, f);
        fclose(f);
        line[n] = 0;
        char* s = line;
        if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;   // BOM UTF-8
        while (*s == ' ' || *s == '\t') ++s;
        char* e = s;
        while (*e && *e != '\r' && *e != '\n') ++e;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
        *e = 0;
        if (valid_name(s)) put(name, sizeof name, s);
        else hal::log("[AKA-Love] game.txt : nom invalide, utilisation de hello\n");
    }

    memset(&g, 0, sizeof g);
    put(g.dir, sizeof g.dir, home, "/games/", name);
    put(g.save_root, sizeof g.save_root, home, "/save");
    put(g.name, sizeof g.name, name);
}
