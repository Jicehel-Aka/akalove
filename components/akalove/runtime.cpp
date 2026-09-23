// runtime.cpp — cycle de vie d'un jeu : VM, conf.lua, main.lua, love.run, écran d'erreur.
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bind_common.h"
#include "embedded_lua.h"
#include "hal.h"
#include "love_gfx.h"

volatile uint32_t g_frame_t0 = 0;
static uint32_t g_watchdog_ms = 4000;
static const char* ERRLOOP_KEY = "akalove.errloop";

void runtime_set_watchdog_ms(uint32_t ms) { g_watchdog_ms = ms; }

// ---------------------------------------------------------------------------------------------------
// Allocateur (PSRAM sur la console) : passé à lua_newstate, jamais luaL_newstate
static void* aka_alloc(void*, void* ptr, size_t, size_t nsize) {
    if (nsize == 0) {
        hal::free_(ptr);
        return nullptr;
    }
    return hal::realloc_(ptr, nsize);
}

// Chien de garde : interrompt un script qui ne rend jamais la main
static void watchdog_hook(lua_State* L, lua_Debug*) {
    hal::feed_watchdog();
    if (hal::wall_millis() - g_frame_t0 > g_watchdog_ms)
        luaL_error(L, "le script ne rend pas la main (boucle infinie ?)");
}

// print() -> série (terminal du PC) ; séparateur tabulation comme Lua
static int l_print(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        size_t len;
        const char* s = luaL_tolstring(L, i, &len);
        hal::serial_write((const uint8_t*)s, (int)len);
        lua_pop(L, 1);
        if (i < n) hal::serial_write((const uint8_t*)"\t", 1);
    }
    hal::serial_write((const uint8_t*)"\n", 1);
    return 0;
}

// Handler de message des appels protégés : comme Love, appelle love.errorhandler(msg) AU POINT DE
// L'ERREUR (la pile est intacte pour debug.traceback) et met de côté la fonction qu'il renvoie.
static int msg_handler(lua_State* L) {
    lua_getglobal(L, "love");
    lua_getfield(L, -1, "errorhandler");
    lua_remove(L, -2);
    if (lua_isfunction(L, -1)) {
        lua_pushvalue(L, 1);
        if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
            lua_setfield(L, LUA_REGISTRYINDEX, ERRLOOP_KEY);
        } else {
            hal::log("[AKA-Love] love.errorhandler a échoué : %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
    lua_pushvalue(L, 1);
    return 1;
}

static int call_with(lua_State* L, lua_CFunction handler, int nargs, int nres) {
    int f = lua_gettop(L) - nargs;   // index de la fonction
    lua_pushcfunction(L, handler);
    lua_insert(L, f);
    int rc = lua_pcall(L, nargs, nres, f);
    lua_remove(L, f);
    return rc;
}
static int call_protected(lua_State* L, int nargs, int nres) { return call_with(L, msg_handler, nargs, nres); }
static int call_plain(lua_State* L, int nargs, int nres) { return call_with(L, aka_traceback, nargs, nres); }

static bool run_embedded(lua_State* L, const char* name, const char* src) {
    if (luaL_loadbuffer(L, src, strlen(src), name) != LUA_OK || call_plain(L, 0, 0) != LUA_OK) {
        hal::log("[AKA-Love] script embarqué %s : %s\n", name, lua_tostring(L, -1));
        return false;
    }
    return true;
}

static lua_State* new_vm() {
    lua_State* L = lua_newstate(aka_alloc, nullptr);
    if (!L) return nullptr;
    luaL_openlibs(L);

    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");

    // require : uniquement via love.filesystem (pas de chargeurs C ni de chemins système)
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaders");
    lua_pushcfunction(L, filesystem_loader);
    lua_rawseti(L, -2, 2);
    lua_pushnil(L);
    lua_rawseti(L, -2, 3);
    lua_pushnil(L);
    lua_rawseti(L, -2, 4);
    lua_pop(L, 2);

    lua_newtable(L);
    lua_setglobal(L, "love");
    open_graphics(L);
    open_system(L);
    open_filesystem(L);
    open_audio(L);

    if (!run_embedded(L, "=boot.lua", EMBEDDED_BOOT_LUA) ||
        !run_embedded(L, "=errorhandler.lua", EMBEDDED_ERRORHANDLER_LUA)) {
        lua_close(L);
        return nullptr;
    }
    lua_sethook(L, watchdog_hook, LUA_MASKCOUNT, 10000);
    return L;
}

// ---------------------------------------------------------------------------------------------------
static double field_num(lua_State* L, int t, const char* k, double def) {
    lua_getfield(L, t, k);
    double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
    lua_pop(L, 1);
    return v;
}
static bool field_str(lua_State* L, int t, const char* k, char* out, size_t n) {
    lua_getfield(L, t, k);
    bool ok = lua_type(L, -1) == LUA_TSTRING;
    if (ok) {
        strncpy(out, lua_tostring(L, -1), n - 1);
        out[n - 1] = 0;
    }
    lua_pop(L, 1);
    return ok;
}

// Lit la table t renvoyée par love.aka._load_conf() (au sommet) et configure fenêtre, identité, entrées
static void apply_conf(lua_State* L, const GameInfo& g) {
    int t = lua_gettop(L);
    int w = 800, h = 600;
    char title[64] = "Untitled", identity[64], scale[16] = "fit", input[16] = "auto";
    bool has_identity = field_str(L, t, "identity", identity, sizeof identity);

    lua_getfield(L, t, "window");
    if (lua_istable(L, -1)) {
        int win = lua_gettop(L);
        w = (int)field_num(L, win, "width", 800);
        h = (int)field_num(L, win, "height", 600);
        field_str(L, win, "title", title, sizeof title);
    }
    lua_pop(L, 1);

    lua_getfield(L, t, "aka");
    if (lua_istable(L, -1)) {
        int aka = lua_gettop(L);
        field_str(L, aka, "scale", scale, sizeof scale);
        field_str(L, aka, "input", input, sizeof input);
    }
    lua_pop(L, 1);

    if (has_identity) filesystem_configure(g.dir, g.save_root, identity);
    gfx::ScaleMode m = !strcmp(scale, "none") ? gfx::SCALE_NONE : (!strcmp(scale, "stretch") ? gfx::SCALE_STRETCH : gfx::SCALE_FIT);
    gfx::set_virtual(w, h, m);
    system_set_title(title);
    system_set_input_mode(input);
    lua_settop(L, t - 1);   // retire la table
}

static Exit exit_from_value(lua_State* L, int idx) {
    if (lua_type(L, idx) == LUA_TSTRING && !strcmp(lua_tostring(L, idx), "restart")) return Exit::Restart;
    return Exit::Quit;
}

// Après une erreur : exécute la boucle renvoyée par love.errorhandler (si elle existe)
static Exit handle_error(lua_State* L) {
    Exit result = Exit::Quit;
    const char* msg = lua_tostring(L, -1);
    lua_getfield(L, LUA_REGISTRYINDEX, ERRLOOP_KEY);
    if (lua_isfunction(L, -1)) {
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        for (;;) {
            g_frame_t0 = hal::wall_millis();
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (call_plain(L, 0, 1) != LUA_OK) {
                hal::log("[AKA-Love] écran d'erreur : %s\n", lua_tostring(L, -1));
                break;
            }
            if (!lua_isnil(L, -1)) {
                result = exit_from_value(L, -1);
                break;
            }
            lua_pop(L, 1);
        }
    } else {
        hal::log("[AKA-Love] ERREUR\n%s\n", msg ? msg : "?");
    }
    return result;
}

static int l_raise(lua_State* L) { return lua_error(L); }

// Signale une erreur détectée hors d'un appel protégé (fichier manquant, erreur de syntaxe)
static Exit fail_with(lua_State* L, const char* msg) {
    lua_pushcfunction(L, l_raise);
    lua_pushstring(L, msg);
    call_protected(L, 1, 0);   // lève l'erreur : msg_handler prépare l'écran d'erreur
    Exit r = handle_error(L);
    lua_close(L);
    return r;
}

Exit runtime_play(const GameInfo& g) {
    gfx::init();
    lua_State* L = new_vm();
    if (!L) return Exit::Quit;
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, ERRLOOP_KEY);
    filesystem_configure(g.dir, g.save_root, g.name);
    g_frame_t0 = hal::wall_millis();
    Exit result = Exit::Quit;

    // 1. conf.lua / love.conf
    lua_getglobal(L, "love");
    lua_getfield(L, -1, "aka");
    lua_getfield(L, -1, "_load_conf");
    lua_remove(L, -2);
    lua_remove(L, -2);
    if (call_protected(L, 0, 1) != LUA_OK) {
        result = handle_error(L);
        lua_close(L);
        return result;
    }
    apply_conf(L, g);

    // 2. main.lua
    char* src = nullptr;
    size_t len = 0;
    if (!filesystem_read_file("main.lua", &src, &len)) return fail_with(L, "Cannot find main.lua in the game source");
    int rc = luaL_loadbuffer(L, src, len, "@main.lua");
    hal::free_(src);
    if (rc != LUA_OK) {   // erreur de syntaxe : même traitement qu'une erreur d'exécution
        char msg[512];
        snprintf(msg, sizeof msg, "%s", lua_tostring(L, -1));
        return fail_with(L, msg);
    }
    if (call_protected(L, 0, 0) != LUA_OK) {
        result = handle_error(L);
        lua_close(L);
        return result;
    }

    // 3. love.run() : normalement une closure appelée une fois par frame
    lua_getglobal(L, "love");
    lua_getfield(L, -1, "run");
    lua_remove(L, -2);
    if (call_protected(L, 0, 1) != LUA_OK) {
        result = handle_error(L);
        lua_close(L);
        return result;
    }
    if (lua_isfunction(L, -1)) {
        int step = luaL_ref(L, LUA_REGISTRYINDEX);
        for (;;) {
            g_frame_t0 = hal::wall_millis();
            lua_rawgeti(L, LUA_REGISTRYINDEX, step);
            if (call_protected(L, 0, 1) != LUA_OK) {
                result = handle_error(L);
                break;
            }
            if (!lua_isnil(L, -1)) {
                result = exit_from_value(L, -1);
                break;
            }
            lua_pop(L, 1);
        }
    } else {
        result = exit_from_value(L, -1);   // love.run surchargé : a déjà tourné jusqu'à la sortie
    }
    lua_close(L);
    return result;
}
