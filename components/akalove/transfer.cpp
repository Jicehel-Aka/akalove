// transfer.cpp — voir include/transfer.h. Protocole "AKAT" v1.1 : PING et PUT (déposer un fichier),
// avec une variante interactive qui demande au joueur quoi faire quand le fichier existe déjà.
#include "transfer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>   // _mkdir n'a pas de paramètre de mode (même adaptation que dans bind_fs.cpp)
#define AKA_MKDIR(p) _mkdir(p)
#else
#define AKA_MKDIR(p) mkdir((p), 0777)
#endif

#include "hal.h"
#include "love_gfx.h"

namespace {

constexpr uint8_t MAGIC[4] = {'A', 'K', 'A', 'T'};
constexpr uint8_t VERSION = 1;
constexpr uint8_t CMD_PING = 0x01;
constexpr uint8_t CMD_PUT = 0x02;
// Statuts 0-5 : v1, inchangés (rétrocompatibles). 6-7 : v1.1, seulement possibles en écran interactif.
constexpr uint8_t ST_OK = 0, ST_BAD_LANG = 1, ST_BAD_PATH = 2, ST_CRC_MISMATCH = 3, ST_WRITE_ERROR = 4,
                  ST_TOO_LARGE = 5, ST_OK_RENAMED = 6, ST_CANCELLED = 7;
constexpr uint32_t MAX_PATH = 220, MAX_DATA = 512u * 1024u;   // une piece de jeu, pas un firmware entier
constexpr uint8_t DEVICE_ID = 0;   // 0 = AKA-Love (une seule "device_id" pour l'instant)
constexpr uint32_t DECISION_TIMEOUT_MS = 30000;   // le temps qu'on laisse au joueur pour choisir

bool read_exact(uint8_t* buf, uint32_t n, uint32_t deadline_ms) {
    uint32_t got = 0;
    while (got < n) {
        if (hal::wall_millis() >= deadline_ms) return false;
        int r = hal::serial_read(buf + got, (int)(n - got));
        if (r > 0) got += (uint32_t)r;
        else hal::sleep_ms(1);
    }
    return true;
}
uint16_t rd_u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
void wr_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

uint32_t crc32(const uint8_t* data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return ~crc;
}

void send_reply(uint8_t status) {
    uint8_t out[7] = {MAGIC[0], MAGIC[1], MAGIC[2], MAGIC[3], VERSION, status, DEVICE_ID};
    hal::serial_write(out, (int)sizeof out);
}
void send_reply_renamed(const char* final_rel_path) {
    uint16_t len = (uint16_t)strlen(final_rel_path);
    uint8_t out[7] = {MAGIC[0], MAGIC[1], MAGIC[2], MAGIC[3], VERSION, ST_OK_RENAMED, DEVICE_ID};
    hal::serial_write(out, (int)sizeof out);
    uint8_t lenbuf[2];
    wr_u16(lenbuf, len);
    hal::serial_write(lenbuf, 2);
    hal::serial_write((const uint8_t*)final_rel_path, (int)len);
}

bool safe_rel_path(const char* p, size_t len) {
    if (len == 0 || len >= MAX_PATH) return false;
    if (p[0] == '/' || p[0] == '\\') return false;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (c < 32 || c == '\\' || c == ':') return false;
        if (p[i] == '.' && p[i + 1] == '.' && (i == 0 || p[i - 1] == '/') && (i + 2 == len || p[i + 2] == '/'))
            return false;
    }
    return true;
}

void mkdir_p(char* path) {
    for (char* p = path + 1; *p; ++p)
        if (*p == '/') {
            *p = 0;
            AKA_MKDIR(path);
            *p = '/';
        }
    AKA_MKDIR(path);
}

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool write_file(const char* abs_path, const uint8_t* data, uint32_t len) {
    char dir[790];
    snprintf(dir, sizeof dir, "%s", abs_path);
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        mkdir_p(dir);
    }
    FILE* f = fopen(abs_path, "wb");
    if (!f) return false;
    size_t put = fwrite(data, 1, len, f);
    fclose(f);
    return put == len;
}

// Insère " (2)", " (3)"... avant l'extension, dans le même dossier. Renvoie false si 20 essais ne
// suffisent pas.
bool find_free_name(const char* root, const char* rel, char* out_rel, size_t out_n) {
    char base[MAX_PATH], ext[32] = "";
    snprintf(base, sizeof base, "%s", rel);
    char* dot = strrchr(base, '.');
    char* last_slash = strrchr(base, '/');
    if (dot && (!last_slash || dot > last_slash)) {
        snprintf(ext, sizeof ext, "%s", dot);
        *dot = 0;
    }
    for (int n = 2; n <= 20; ++n) {
        char candidate[MAX_PATH + 40];
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"   // candidate/out_rel bornés par MAX_PATH ; jamais réellement tronqué
#endif
        snprintf(candidate, sizeof candidate, "%s (%d)%s", base, n, ext);
        char abs_path[1200];
        snprintf(abs_path, sizeof abs_path, "%s/%s", root, candidate);
        if (!file_exists(abs_path)) {
            snprintf(out_rel, out_n, "%s", candidate);
            return true;
        }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }
    return false;
}


// --- AKA/languages.csv : dossier de scripts par langage, PARTAGÉ entre tous les firmwares AKA (pas
// seulement AKA-Love) et modifiable sans recompiler — voir docs/PROTOCOLE_TRANSFERT.md. Une ligne par
// langage : "lang_id,dossier_relatif_a_la_racine_sd,nom_affiche" (le nom affiché est ignoré ici, il ne
// sert qu'à AKA-IDE). Ce firmware ne s'intéresse qu'à la ligne dont lang_id correspond à SON PROPRE
// langage (0 = Lua) : une ligne pour MicroPython ou un futur langage est lue puis ignorée sans erreur.
constexpr uint32_t CSV_MAX_LINE = 300;

// Lit UNE ligne de fin de fichier `f` (jusqu'à `\n` ou EOF) dans `out` (tronquée si trop longue).
// Renvoie false en fin de fichier sans rien avoir lu.
bool read_line(FILE* f, char* out, size_t out_n) {
    size_t n = 0;
    int c;
    bool got_any = false;
    while ((c = fgetc(f)) != EOF) {
        got_any = true;
        if (c == '\n') break;
        if (c != '\r' && n + 1 < out_n) out[n++] = (char)c;
    }
    out[n] = 0;
    return got_any;
}

// Cherche dans <sd_root>/AKA/languages.csv la ligne pour `want_lang_id` ; copie son dossier (relatif à
// la racine SD) dans `out`. `sd_root` est la racine ABSOLUE de la carte (ex. "/sdcard" sur la console,
// ou --home moins son dernier segment dans le simulateur — voir transfer_load_scripts_root_override()).
// Renvoie false si le fichier est absent, la ligne introuvable, ou le chemin invalide (même règles que
// safe_rel_path : jamais de fichier illisible ne doit pouvoir rediriger une écriture hors de la carte).
bool csv_lookup(const char* sd_root, uint8_t want_lang_id, char* out, size_t out_n) {
    char csv_path[790];
    snprintf(csv_path, sizeof csv_path, "%s/AKA/languages.csv", sd_root);
    FILE* f = fopen(csv_path, "r");
    if (!f) return false;
    char line[CSV_MAX_LINE];
    bool found = false;
    while (!found && read_line(f, line, sizeof line)) {
        if (line[0] == '#' || line[0] == 0) continue;
        char* p = line;
        long id = strtol(p, &p, 10);
        if (*p != ',' || id < 0 || id > 255) continue;
        ++p;
        char* dir_start = p;
        char* comma = strchr(p, ',');
        if (comma) *comma = 0;
        // dépouille les espaces de bord (un CSV édité à la main en a souvent)
        while (*dir_start == ' ') ++dir_start;
        char* end = dir_start + strlen(dir_start);
        while (end > dir_start && end[-1] == ' ') *--end = 0;
        if ((uint8_t)id == want_lang_id && dir_start[0] && safe_rel_path(dir_start, strlen(dir_start))) {
            snprintf(out, out_n, "%s", dir_start);
            found = true;
        }
    }
    fclose(f);
    return found;
}

const char* resolve_root(uint8_t lang, const char* home, const char* mpy_root, char* sub, size_t sub_n) {
    if (lang == 0 && home) {
        // AKA/languages.csv (racine de la carte SD, dérivée de `home` en retirant son dernier segment,
        // ex. "/sdcard/AKA_Love" -> "/sdcard") : remplace le dossier par défaut si présent et valide.
        char sd_root[500];
        snprintf(sd_root, sizeof sd_root, "%s", home);
        char* last_slash = strrchr(sd_root, '/');
        if (last_slash && last_slash != sd_root) *last_slash = 0;
        char csv_dir[MAX_PATH];
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"   // sub_n est toujours assez grand en pratique (voir les appelants)
#endif
        if (csv_lookup(sd_root, /*want_lang_id=*/0, csv_dir, sizeof csv_dir)) {
            snprintf(sub, sub_n, "%s/%s", sd_root, csv_dir);
        } else {
            snprintf(sub, sub_n, "%s/games", home);   // repli : pas de CSV, ou langage 0 absent du fichier
        }
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        return sub;
    }
    if (lang == 1 && mpy_root) return mpy_root;
    return nullptr;
}


void draw_waiting(const char* status_line) {
    gfx::clear(0.06f, 0.08f, 0.14f, 1);
    gfx::set_color(1, 1, 1, 1);
    gfx::print("Recevoir un code", 8, 8, 0, 1, 1, 0, 0);
    gfx::print(status_line, 8, 28, 0, 1, 1, 0, 0);
    gfx::print("MENU : quitter", 8, 224, 0, 1, 1, 0, 0);
    gfx::present();
}
void draw_conflict(const char* rel_path) {
    gfx::clear(0.35f, 0.12f, 0.05f, 1);
    gfx::set_color(1, 1, 1, 1);
    gfx::print("Ce fichier existe deja :", 8, 8, 0, 1, 1, 0, 0);
    gfx::print(rel_path, 8, 24, 0, 1, 1, 0, 0);
    gfx::print("A : Ecraser", 8, 90, 0, 1, 1, 0, 0);
    gfx::print("D : Renommer", 8, 106, 0, 1, 1, 0, 0);
    gfx::print("B : Annuler ce fichier", 8, 122, 0, 1, 1, 0, 0);
    gfx::present();
}

enum class Decision { Pending, Overwrite, Rename, Cancel };

Decision wait_for_decision(const char* rel_path, uint32_t deadline_ms) {
    hal::poll_input();
    uint32_t held = hal::buttons();
    while (hal::wall_millis() < deadline_ms) {
        draw_conflict(rel_path);
        hal::poll_input();
        uint32_t now = hal::buttons();
        uint32_t pressed = now & ~held;
        held = now;
        if (pressed & (1u << hal::B_A)) return Decision::Overwrite;
        if (pressed & (1u << hal::B_D)) return Decision::Rename;
        if (pressed & (1u << hal::B_B)) return Decision::Cancel;
        hal::sleep_ms(10);
    }
    return Decision::Cancel;
}

void handle_put(const char* home, const char* mpy_root, uint32_t deadline_ms, bool interactive) {
    uint8_t hdr[3];
    if (!read_exact(hdr, sizeof hdr, deadline_ms)) return;
    uint8_t lang = hdr[0];
    uint16_t path_len = rd_u16(hdr + 1);
    if (path_len == 0 || path_len >= MAX_PATH) {
        send_reply(ST_BAD_PATH);
        return;
    }
    char path[MAX_PATH];
    if (!read_exact((uint8_t*)path, path_len, deadline_ms)) return;
    path[path_len] = 0;
    if (!safe_rel_path(path, path_len)) {
        send_reply(ST_BAD_PATH);
        return;
    }

    char sub[550];
    const char* root = resolve_root(lang, home, mpy_root, sub, sizeof sub);
    if (!root) {
        send_reply(ST_BAD_LANG);
        return;
    }

    uint8_t lenbuf[4];
    if (!read_exact(lenbuf, sizeof lenbuf, deadline_ms)) return;
    uint32_t data_len = rd_u32(lenbuf);
    if (data_len > MAX_DATA) {
        send_reply(ST_TOO_LARGE);
        return;
    }
    uint8_t* data = (uint8_t*)hal::alloc(data_len ? data_len : 1);
    if (!data) {
        send_reply(ST_WRITE_ERROR);
        return;
    }
    bool ok = data_len == 0 || read_exact(data, data_len, deadline_ms);
    uint8_t crcbuf[4];
    ok = ok && read_exact(crcbuf, sizeof crcbuf, deadline_ms);
    if (!ok) {
        hal::free_(data);
        return;
    }
    if (crc32(data, data_len) != rd_u32(crcbuf)) {
        hal::free_(data);
        send_reply(ST_CRC_MISMATCH);
        return;
    }

    char final_rel[MAX_PATH];
    snprintf(final_rel, sizeof final_rel, "%s", path);
    char abs_path[790];
    snprintf(abs_path, sizeof abs_path, "%s/%s", root, final_rel);

    if (interactive && file_exists(abs_path)) {
        Decision d = wait_for_decision(final_rel, hal::wall_millis() + DECISION_TIMEOUT_MS);
        if (d == Decision::Cancel) {
            hal::free_(data);
            send_reply(ST_CANCELLED);
            return;
        }
        if (d == Decision::Rename) {
            char renamed[MAX_PATH];
            if (!find_free_name(root, path, renamed, sizeof renamed)) {
                hal::free_(data);
                send_reply(ST_WRITE_ERROR);
                return;
            }
            snprintf(final_rel, sizeof final_rel, "%s", renamed);
            snprintf(abs_path, sizeof abs_path, "%s/%s", root, final_rel);
        }
    }

    bool wrote = write_file(abs_path, data, data_len);
    hal::free_(data);
    if (!wrote) send_reply(ST_WRITE_ERROR);
    else if (strcmp(final_rel, path) != 0) send_reply_renamed(final_rel);
    else send_reply(ST_OK);
}

void run(uint32_t end_ms, const char* home, const char* mpy_root, bool interactive,
        void (*draw_idle)(), bool* cancelled) {
    uint8_t sync = 0;
    while (hal::wall_millis() < end_ms && !hal::quit_requested()) {
        if (cancelled) {
            hal::poll_input();
            if (hal::buttons() & (1u << hal::B_MENU)) {
                *cancelled = true;
                return;
            }
        }
        if (draw_idle) draw_idle();

        uint8_t b;
        int r = hal::serial_read(&b, 1);
        if (r <= 0) {
            hal::sleep_ms(draw_idle ? 16 : 1);
            continue;
        }
        if (b != MAGIC[sync]) {
            sync = (b == MAGIC[0]) ? 1 : 0;
            continue;
        }
        if (++sync < 4) continue;
        sync = 0;

        uint8_t hdr[2];
        if (!read_exact(hdr, sizeof hdr, end_ms)) return;
        if (hdr[0] != VERSION) continue;
        if (hdr[1] == CMD_PING) send_reply(ST_OK);
        else if (hdr[1] == CMD_PUT) handle_put(home, mpy_root, end_ms, interactive);
    }
}

void draw_idle_screen() { draw_waiting("En attente d'un transfert USB..."); }

}  // namespace

void transfer_listen(uint32_t window_ms, const char* home, const char* mpy_root) {
    run(hal::wall_millis() + window_ms, home, mpy_root, /*interactive=*/false, nullptr, nullptr);
}

void transfer_receive_screen(const char* home, const char* mpy_root) {
    gfx::init();
    bool cancelled = false;
    // Fenêtre très longue (24 h) : la vraie sortie se fait par MENU (cancelled=true), pas par le temps.
    run(hal::wall_millis() + 24u * 3600u * 1000u, home, mpy_root, /*interactive=*/true, draw_idle_screen,
        &cancelled);
}
