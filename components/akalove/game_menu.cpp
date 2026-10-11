// game_menu.cpp — lanceur de jeux (phase P6). Voir game_menu.h.
#include "game_menu.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>   // strcasecmp
#include <sys/stat.h>

#include "hal.h"
#include "love_gfx.h"

namespace {

constexpr int MAX_GAMES = 128;
constexpr int kNameMax = 64;

// Concaténation bornée (même esprit que game_select.cpp).
void put(char* dst, size_t n, const char* a, const char* b = "", const char* c = "") {
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

// Noms de dossiers acceptés : lettres/chiffres/_-. (comme game_select), ni "." ni "..".
bool valid_name(const char* s) {
    size_t l = strlen(s);
    if (l == 0 || l >= kNameMax || !strcmp(s, ".") || !strcmp(s, "..")) return false;
    for (; *s; ++s) {
        char c = *s;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

bool is_dir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Liste les sous-dossiers de <home>/games/ (triés alphabétiquement). Renvoie le nombre trouvé.
int list_games(const char* home, char names[][kNameMax]) {
    char games_dir[300];
    put(games_dir, sizeof games_dir, home, "/games");
    DIR* d = opendir(games_dir);
    if (!d) return 0;
    int n = 0;
    while (dirent* e = readdir(d)) {
        if (n >= MAX_GAMES) break;
        if (!valid_name(e->d_name)) continue;
        char full[400];
        put(full, sizeof full, games_dir, "/", e->d_name);
        if (!is_dir(full)) continue;   // on ignore les fichiers (ex. game.txt n'est pas ici de toute façon)
        put(names[n], kNameMax, e->d_name);
        ++n;
    }
    closedir(d);
    // Tri par insertion, sans casse (l'ordre de readdir n'est pas garanti).
    for (int i = 1; i < n; ++i) {
        char key[kNameMax];
        put(key, kNameMax, names[i]);
        int j = i - 1;
        while (j >= 0 && strcasecmp(names[j], key) > 0) {
            put(names[j + 1], kNameMax, names[j]);
            --j;
        }
        put(names[j + 1], kNameMax, key);
    }
    return n;
}

// Lit le nom courant dans <home>/game.txt (pour présélectionner). "" si absent/invalide.
void read_current(const char* home, char out[kNameMax]) {
    out[0] = 0;
    char path[300];
    put(path, sizeof path, home, "/game.txt");
    FILE* f = fopen(path, "rb");
    if (!f) return;
    char line[80];
    size_t n = fread(line, 1, sizeof line - 1, f);
    fclose(f);
    line[n] = 0;
    char* s = line;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
    while (*s == ' ' || *s == '\t') ++s;
    char* e = s;
    while (*e && *e != '\r' && *e != '\n') ++e;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
    *e = 0;
    if (valid_name(s)) put(out, kNameMax, s);
}

// Écrit le choix dans <home>/game.txt (une ligne) pour que le prochain démarrage le retienne.
void write_choice(const char* home, const char* name) {
    char path[300];
    put(path, sizeof path, home, "/game.txt");
    if (FILE* f = fopen(path, "wb")) {
        fputs(name, f);
        fputc('\n', f);
        fclose(f);
    } else {
        hal::log("[AKA-Love] menu : impossible d'ecrire game.txt\n");
    }
}

void fill_game(const char* home, const char* name, GameInfo& g) {
    memset(&g, 0, sizeof g);
    put(g.dir, sizeof g.dir, home, "/games/", name);
    put(g.save_root, sizeof g.save_root, home, "/save");
    put(g.name, sizeof g.name, name);
}

void draw_menu(char names[][kNameMax], int n, int sel, int top, int visible) {
    gfx::clear(0.06f, 0.08f, 0.14f, 1);
    gfx::set_color(0.6f, 0.8f, 1, 1);
    gfx::print("AKA-Love", 8, 8, 0, 1, 1, 0, 0);
    gfx::set_color(0.7f, 0.7f, 0.7f, 1);
    gfx::print("Choisis un jeu", 8, 22, 0, 1, 1, 0, 0);

    const int y0 = 44, lh = 14;
    for (int i = 0; i < visible && top + i < n; ++i) {
        int idx = top + i;
        int y = y0 + i * lh;
        if (idx == sel) {
            gfx::set_color(0.20f, 0.35f, 0.65f, 1);
            gfx::rect(true, 4, (float)(y - 2), 312, (float)lh);
            gfx::set_color(1, 1, 1, 1);
        } else {
            gfx::set_color(0.85f, 0.85f, 0.85f, 1);
        }
        gfx::print(names[idx], 12, (float)y, 0, 1, 1, 0, 0);
    }

    gfx::set_color(0.55f, 0.55f, 0.55f, 1);
    gfx::print("Haut/Bas: choisir   A: lancer   MENU: quitter", 8, 224, 0, 1, 1, 0, 0);
}

}  // namespace

bool choose_game(const char* home, GameInfo& g) {
    static char names[MAX_GAMES][kNameMax];
    int n = list_games(home, names);

    gfx::init();

    if (n == 0) {
        // Aucun jeu : on l'affiche brièvement puis on rend la main au loader.
        uint32_t t0 = hal::millis();
        while (hal::millis() - t0 < 2500 && !hal::quit_requested()) {
            gfx::clear(0.06f, 0.08f, 0.14f, 1);
            gfx::set_color(1, 1, 1, 1);
            gfx::print("Aucun jeu dans AKA_Love/games", 8, 100, 0, 1, 1, 0, 0);
            gfx::present();
            hal::poll_input();
        }
        return false;
    }

    // Présélection sur le jeu courant (game.txt), sinon le premier.
    char current[kNameMax];
    read_current(home, current);
    int sel = 0;
    for (int i = 0; i < n; ++i)
        if (!strcmp(names[i], current)) { sel = i; break; }

    const int visible = 12;
    int top = 0;

    hal::poll_input();
    uint32_t held = hal::buttons();
    while (!hal::quit_requested()) {
        if (sel < top) top = sel;
        if (sel >= top + visible) top = sel - visible + 1;
        draw_menu(names, n, sel, top, visible);
        gfx::present();

        hal::poll_input();
        uint32_t now = hal::buttons();
        uint32_t pressed = now & ~held;
        held = now;

        if (pressed & (1u << hal::B_MENU)) return false;
        if (pressed & (1u << hal::B_UP)) sel = (sel - 1 + n) % n;
        if (pressed & (1u << hal::B_DOWN)) sel = (sel + 1) % n;
        if (pressed & (1u << hal::B_A)) {
            write_choice(home, names[sel]);
            fill_game(home, names[sel], g);
            return true;
        }
        hal::sleep_ms(10);
    }
    return false;   // simulation : --frames épuisées sans choix
}
