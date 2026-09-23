// bind_system.cpp — love.timer, love.keyboard, love.joystick, love.window, love.math, love.system, love.aka
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "bind_common.h"
#include "hal.h"
#include "love_gfx.h"
#include "runtime.h"

namespace {

// ================================================================= love.timer
uint32_t g_start_ms = 0, g_last_us = 0;
bool g_first_step = true;
double g_dt = 0, g_acc = 0;
int g_frames = 0, g_fps = 0;

int t_step(lua_State* L) {
    uint32_t now = hal::micros();
    g_dt = g_first_step ? 0.0 : (double)(uint32_t)(now - g_last_us) / 1e6;
    g_first_step = false;
    g_last_us = now;
    g_acc += g_dt;
    ++g_frames;
    if (g_acc >= 1.0) {
        g_fps = (int)(g_frames / g_acc + 0.5);
        g_frames = 0;
        g_acc = 0;
    }
    lua_pushnumber(L, g_dt);
    return 1;
}
int t_getDelta(lua_State* L) {
    lua_pushnumber(L, g_dt);
    return 1;
}
int t_getFPS(lua_State* L) {
    lua_pushinteger(L, g_fps);
    return 1;
}
int t_getAverageDelta(lua_State* L) {
    lua_pushnumber(L, g_fps > 0 ? 1.0 / g_fps : g_dt);
    return 1;
}
int t_getTime(lua_State* L) {
    lua_pushnumber(L, (double)(uint32_t)(hal::millis() - g_start_ms) / 1000.0);
    return 1;
}
int t_sleep(lua_State* L) {
    double s = luaL_checknumber(L, 1);
    if (s < 0) return luaL_error(L, "Time must be a positive number.");
    hal::sleep_ms((uint32_t)(s * 1000.0));
    return 0;
}
const luaL_Reg timer_fns[] = {{"step", t_step},
                              {"getDelta", t_getDelta},
                              {"getFPS", t_getFPS},
                              {"getAverageDelta", t_getAverageDelta},
                              {"getTime", t_getTime},
                              {"sleep", t_sleep},
                              {nullptr, nullptr}};

// ================================================================= entrées : table de correspondance
struct KeyDef {
    hal::Button b;
    const char* key;         // nom émis par keypressed
    const char* alias[3];    // noms supplémentaires reconnus par isDown seulement
    const char* gp;          // bouton de la manette virtuelle
};
const KeyDef KEYS[] = {
    {hal::B_UP, "up", {"w", "z", nullptr}, "dpup"},
    {hal::B_DOWN, "down", {"s", nullptr, nullptr}, "dpdown"},
    {hal::B_LEFT, "left", {"a", "q", nullptr}, "dpleft"},
    {hal::B_RIGHT, "right", {"d", nullptr, nullptr}, "dpright"},
    {hal::B_A, "space", {"return", "kpenter", nullptr}, "a"},
    {hal::B_B, "x", {nullptr, nullptr, nullptr}, "b"},
    {hal::B_C, "c", {nullptr, nullptr, nullptr}, "x"},
    {hal::B_D, "v", {nullptr, nullptr, nullptr}, "y"},
    {hal::B_L1, "lshift", {nullptr, nullptr, nullptr}, "leftshoulder"},
    {hal::B_R1, "rshift", {nullptr, nullptr, nullptr}, "rightshoulder"},
    {hal::B_MENU, "escape", {nullptr, nullptr, nullptr}, "start"},
};

bool pressed(hal::Button b) { return (hal::buttons() >> b) & 1u; }

bool key_matches(const KeyDef& k, const char* name) {
    if (!strcmp(k.key, name)) return true;
    for (const char* a : k.alias)
        if (a && !strcmp(a, name)) return true;
    return false;
}

int k_isDown(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i) {
        const char* name = luaL_checkstring(L, i);
        for (const KeyDef& k : KEYS)
            if (key_matches(k, name) && pressed(k.b)) {
                lua_pushboolean(L, 1);
                return 1;
            }
    }
    lua_pushboolean(L, 0);
    return 1;
}
bool g_key_repeat = false;
int k_setKeyRepeat(lua_State* L) {
    g_key_repeat = lua_toboolean(L, 1) != 0;
    return 0;
}
int k_hasKeyRepeat(lua_State* L) {
    lua_pushboolean(L, g_key_repeat);
    return 1;
}
int k_false(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}
int k_identity(lua_State* L) {
    lua_pushvalue(L, 1);
    return 1;
}
int k_noop(lua_State*) { return 0; }
const luaL_Reg keyboard_fns[] = {{"isDown", k_isDown},
                                 {"isScancodeDown", k_isDown},
                                 {"setKeyRepeat", k_setKeyRepeat},
                                 {"hasKeyRepeat", k_hasKeyRepeat},
                                 {"hasTextInput", k_false},
                                 {"setTextInput", k_noop},
                                 {"getKeyFromScancode", k_identity},
                                 {"getScancodeFromKey", k_identity},
                                 {nullptr, nullptr}};

// ================================================================= love.joystick (manette virtuelle)
int g_joy_ref = LUA_NOREF;

bool gp_down(const char* name) {
    for (const KeyDef& k : KEYS)
        if (!strcmp(k.gp, name) && pressed(k.b)) return true;
    return false;
}
int j_isGamepad(lua_State* L) {
    lua_pushboolean(L, 1);
    return 1;
}
int j_isGamepadDown(lua_State* L) {
    int n = lua_gettop(L);
    for (int i = 2; i <= n; ++i)
        if (gp_down(luaL_checkstring(L, i))) {
            lua_pushboolean(L, 1);
            return 1;
        }
    lua_pushboolean(L, 0);
    return 1;
}
int j_getGamepadAxis(lua_State* L) {
    const char* a = luaL_checkstring(L, 2);
    float lx, ly;
    hal::axes(lx, ly);
    auto dead = [](float v) { return (v > -0.1f && v < 0.1f) ? 0.0f : v; };   // zone morte 10 %
    double v = 0;
    if (!strcmp(a, "leftx")) v = dead(lx);
    else if (!strcmp(a, "lefty")) v = dead(ly);
    lua_pushnumber(L, v);
    return 1;
}
int j_getName(lua_State* L) {
    lua_pushstring(L, "Gamebuino AKA");
    return 1;
}
int j_getID(lua_State* L) {
    lua_pushinteger(L, 1);
    lua_pushinteger(L, 1);
    return 2;
}
int j_true(lua_State* L) {
    lua_pushboolean(L, 1);
    return 1;
}
const luaL_Reg joystick_methods[] = {{"isGamepad", j_isGamepad},
                                     {"isGamepadDown", j_isGamepadDown},
                                     {"getGamepadAxis", j_getGamepadAxis},
                                     {"getName", j_getName},
                                     {"getID", j_getID},
                                     {"isConnected", j_true},
                                     {nullptr, nullptr}};
int js_getJoysticks(lua_State* L) {
    lua_newtable(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, g_joy_ref);
    lua_rawseti(L, -2, 1);
    return 1;
}
int js_getJoystickCount(lua_State* L) {
    lua_pushinteger(L, 1);
    return 1;
}
const luaL_Reg joystick_fns[] = {{"getJoysticks", js_getJoysticks},
                                 {"getJoystickCount", js_getJoystickCount},
                                 {"loadGamepadMappings", k_noop},
                                 {nullptr, nullptr}};

// ================================================================= sondage des entrées -> love.event.push
char g_input_mode[16] = "auto";   // auto | keyboard | gamepad | both
uint32_t g_prev_buttons = 0;

void push_event(lua_State* L, const char* name, int nargs_after_name, int joystick_first) {
    // pile : ... arg1 ... argN (déjà poussés) ; on appelle love.event.push(name, args...)
    int base = lua_gettop(L) - nargs_after_name;
    lua_getglobal(L, "love");
    lua_getfield(L, -1, "event");
    lua_getfield(L, -1, "push");
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_pushstring(L, name);
    if (joystick_first) lua_rawgeti(L, LUA_REGISTRYINDEX, g_joy_ref);
    for (int i = 1; i <= nargs_after_name; ++i) lua_pushvalue(L, base + i);
    lua_call(L, 1 + (joystick_first ? 1 : 0) + nargs_after_name, 0);
    lua_settop(L, base);
}

bool love_defines(lua_State* L, const char* fn) {
    lua_getglobal(L, "love");
    lua_getfield(L, -1, fn);
    bool ok = lua_isfunction(L, -1);
    lua_pop(L, 2);
    return ok;
}

// ================================================================= love.mouse (souris virtuelle)
// L'AKA n'a pas de pointeur physique : le stick analogique déplace un curseur virtuel en coordonnées
// de fenêtre (résolution virtuelle du jeu), et le bouton A tient lieu de clic gauche (bouton 1). Un jeu
// écrit pour la souris (menus PC classiques) devient ainsi jouable, à condition qu'il dessine lui-même
// son curseur à love.mouse.getPosition() — il n'y a pas de curseur système à l'écran.
float g_mouse_x = 0, g_mouse_y = 0;
bool g_mouse_inited = false, g_mouse_visible = true;
uint32_t g_mouse_last_ms = 0;
constexpr float MOUSE_SPEED = 260.0f;   // pixels virtuels / s, stick à fond

// L'horloge JEU (hal::millis(), celle de love.timer), pas l'horloge murale (g_frame_t0 : chien de
// garde uniquement) — en simulation "instantanée" (sans --realtime), l'horloge murale n'avance presque
// pas d'une frame à l'autre et le curseur ne bougerait jamais.
void ensure_mouse_inited() {
    if (g_mouse_inited) return;
    g_mouse_x = gfx::win_w() / 2.0f;
    g_mouse_y = gfx::win_h() / 2.0f;
    g_mouse_last_ms = hal::millis();
    g_mouse_inited = true;
}
void mouse_move() {
    ensure_mouse_inited();
    uint32_t now_ms = hal::millis();
    float dt = (float)(now_ms - g_mouse_last_ms) / 1000.0f;
    g_mouse_last_ms = now_ms;
    float lx, ly;
    hal::axes(lx, ly);
    g_mouse_x += lx * MOUSE_SPEED * dt;
    g_mouse_y += ly * MOUSE_SPEED * dt;
    if (g_mouse_x < 0) g_mouse_x = 0;
    if (g_mouse_y < 0) g_mouse_y = 0;
    if (g_mouse_x > gfx::win_w() - 1) g_mouse_x = (float)(gfx::win_w() - 1);
    if (g_mouse_y > gfx::win_h() - 1) g_mouse_y = (float)(gfx::win_h() - 1);
}
// Émet mousepressed/mousereleased(x, y, bouton=1) quand A change d'état — indépendant du mode clavier
// / manette (`t.aka.input`) : la souris virtuelle est toujours active en plus.
void mouse_emit_click(lua_State* L, uint32_t changed, uint32_t now) {
    if (!(changed & (1u << hal::B_A))) return;
    bool down = (now & (1u << hal::B_A)) != 0;
    lua_pushnumber(L, g_mouse_x);
    lua_pushnumber(L, g_mouse_y);
    lua_pushinteger(L, 1);
    if (down) {
        lua_pushboolean(L, 0);   // "istouch" (Love 11) : toujours faux ici
        push_event(L, "mousepressed", 4, 0);
    } else {
        push_event(L, "mousereleased", 3, 0);
    }
}

int m_getPosition(lua_State* L) {
    ensure_mouse_inited();
    lua_pushnumber(L, g_mouse_x);
    lua_pushnumber(L, g_mouse_y);
    return 2;
}
int m_getX(lua_State* L) {
    ensure_mouse_inited();
    lua_pushnumber(L, g_mouse_x);
    return 1;
}
int m_getY(lua_State* L) {
    ensure_mouse_inited();
    lua_pushnumber(L, g_mouse_y);
    return 1;
}
int m_setPosition(lua_State* L) {
    g_mouse_x = (float)luaL_checknumber(L, 1);
    g_mouse_y = (float)luaL_checknumber(L, 2);
    return 0;
}
int m_isDown(lua_State* L) {
    int n = lua_gettop(L);
    bool a_down = (hal::buttons() & (1u << hal::B_A)) != 0;
    for (int i = 1; i <= n; ++i)
        if ((int)luaL_checknumber(L, i) == 1 && a_down) {
            lua_pushboolean(L, 1);
            return 1;
        }
    lua_pushboolean(L, 0);
    return 1;
}
int m_setVisible(lua_State* L) {
    g_mouse_visible = lua_toboolean(L, 1) != 0;
    return 0;
}
int m_isVisible(lua_State* L) {
    lua_pushboolean(L, g_mouse_visible);
    return 1;
}
const luaL_Reg mouse_fns[] = {{"getPosition", m_getPosition},
                              {"getX", m_getX},
                              {"getY", m_getY},
                              {"setPosition", m_setPosition},
                              {"isDown", m_isDown},
                              {"setVisible", m_setVisible},
                              {"isVisible", m_isVisible},
                              {"isGrabbed", k_false},
                              {"setGrabbed", k_noop},
                              {"isCursorSupported", k_false},
                              {nullptr, nullptr}};

int a_poll(lua_State* L) {
    g_frame_t0 = hal::wall_millis();
    hal::poll_input();
    if (hal::quit_requested()) {
        lua_pushinteger(L, 0);
        push_event(L, "quit", 1, 0);
    }
    uint32_t now = hal::buttons();
    uint32_t changed = now ^ g_prev_buttons;
    g_prev_buttons = now;

    mouse_move();                     // déplacement continu (stick), même sans changement de bouton
    mouse_emit_click(L, changed, now);

    if (!changed) return 0;

    bool has_kp = love_defines(L, "keypressed");
    bool has_gp = love_defines(L, "gamepadpressed");
    bool emit_key = !strcmp(g_input_mode, "keyboard") || !strcmp(g_input_mode, "both") ||
                    (!strcmp(g_input_mode, "auto") && has_kp);
    bool emit_gp = !strcmp(g_input_mode, "gamepad") || !strcmp(g_input_mode, "both") ||
                   (!strcmp(g_input_mode, "auto") && has_gp && !has_kp);
    for (const KeyDef& k : KEYS) {
        uint32_t bit = 1u << k.b;
        if (!(changed & bit)) continue;
        bool down = (now & bit) != 0;
        if (emit_key) {
            lua_pushstring(L, k.key);
            lua_pushstring(L, k.key);
            if (down) {
                lua_pushboolean(L, 0);
                push_event(L, "keypressed", 3, 0);
            } else {
                push_event(L, "keyreleased", 2, 0);
            }
        }
        if (emit_gp) {
            lua_pushstring(L, k.gp);
            push_event(L, down ? "gamepadpressed" : "gamepadreleased", 1, 1);
        }
    }
    return 0;
}

int a_setInputMode(lua_State* L) {
    const char* m = luaL_checkstring(L, 1);
    strncpy(g_input_mode, m, sizeof g_input_mode - 1);
    g_input_mode[sizeof g_input_mode - 1] = 0;
    return 0;
}
int a_useNativeResolution(lua_State*) {
    gfx::use_native();
    return 0;
}
int a_setScale(lua_State* L) {   // love.aka.setScaleMode("fit"|"stretch"|"none") : appelé par le runtime après conf.lua
    const char* m = luaL_checkstring(L, 1);
    gfx::set_virtual(gfx::win_w(), gfx::win_h(),
                     !strcmp(m, "none") ? gfx::SCALE_NONE : (!strcmp(m, "stretch") ? gfx::SCALE_STRETCH : gfx::SCALE_FIT));
    return 0;
}
const luaL_Reg aka_fns[] = {{"_poll", a_poll},
                            {"setInputMode", a_setInputMode},
                            {"useNativeResolution", a_useNativeResolution},
                            {"setScaleMode", a_setScale},
                            {nullptr, nullptr}};

// ================================================================= love.window
char g_title[64] = "Untitled";

int w_getMode(lua_State* L) {
    lua_pushinteger(L, gfx::win_w());
    lua_pushinteger(L, gfx::win_h());
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "fullscreen");
    lua_pushstring(L, "desktop");
    lua_setfield(L, -2, "fullscreentype");
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "vsync");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "msaa");
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "resizable");
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "borderless");
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "display");
    return 3;
}
int w_setMode(lua_State* L) {
    int w = (int)luaL_checknumber(L, 1), h = (int)luaL_checknumber(L, 2);
    gfx::set_virtual(w, h, gfx::scale_mode() == gfx::SCALE_NONE ? gfx::SCALE_FIT : gfx::scale_mode());
    lua_pushboolean(L, 1);
    return 1;
}
int w_getTitle(lua_State* L) {
    lua_pushstring(L, g_title);
    return 1;
}
int w_setTitle(lua_State* L) {
    system_set_title(luaL_checkstring(L, 1));
    return 0;
}
int w_getDesktopDimensions(lua_State* L) {
    lua_pushinteger(L, hal::SCREEN_W);
    lua_pushinteger(L, hal::SCREEN_H);
    return 2;
}
int w_one(lua_State* L) {
    lua_pushnumber(L, 1.0);
    return 1;
}
int w_getFullscreen(lua_State* L) {
    lua_pushboolean(L, 1);
    lua_pushstring(L, "desktop");
    return 2;
}
int w_true(lua_State* L) {
    lua_pushboolean(L, 1);
    return 1;
}
const luaL_Reg window_fns[] = {{"getMode", w_getMode},
                               {"setMode", w_setMode},
                               {"getTitle", w_getTitle},
                               {"setTitle", w_setTitle},
                               {"getDesktopDimensions", w_getDesktopDimensions},
                               {"getDPIScale", w_one},
                               {"getPixelScale", w_one},
                               {"getFullscreen", w_getFullscreen},
                               {"setFullscreen", w_true},
                               {"isVisible", w_true},
                               {"isOpen", w_true},
                               {"hasFocus", w_true},
                               {"setIcon", w_true},
                               {nullptr, nullptr}};

// ================================================================= love.math (xorshift128+)
uint64_t g_rs[2] = {0x9E3779B97F4A7C15ull, 0xBF58476D1CE4E5B9ull};
uint64_t g_seed_lo = 0, g_seed_hi = 0;

uint64_t splitmix(uint64_t& x) {
    uint64_t z = (x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
void seed_rng(uint32_t lo, uint32_t hi) {
    g_seed_lo = lo;
    g_seed_hi = hi;
    uint64_t x = ((uint64_t)hi << 32) | lo;
    g_rs[0] = splitmix(x);
    g_rs[1] = splitmix(x);
    if (!g_rs[0] && !g_rs[1]) g_rs[1] = 1;
}
uint64_t next_u64() {
    uint64_t s1 = g_rs[0];
    const uint64_t s0 = g_rs[1];
    g_rs[0] = s0;
    s1 ^= s1 << 23;
    g_rs[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);
    return g_rs[1] + s0;
}
double next_double() { return (double)(next_u64() >> 11) * (1.0 / 9007199254740992.0); }   // [0,1)

int m_random(lua_State* L) {
    int n = lua_gettop(L);
    double r = next_double();
    if (n == 0) {
        lua_pushnumber(L, r);
    } else if (n == 1) {
        double hi = floor(luaL_checknumber(L, 1));
        if (hi < 1) return luaL_error(L, "Invalid random range: 1 > %d", (int)hi);
        lua_pushnumber(L, floor(r * hi) + 1);
    } else {
        double lo = floor(luaL_checknumber(L, 1)), hi = floor(luaL_checknumber(L, 2));
        if (lo > hi) return luaL_error(L, "Invalid random range: %d > %d", (int)lo, (int)hi);
        lua_pushnumber(L, floor(r * (hi - lo + 1)) + lo);
    }
    return 1;
}
int m_setRandomSeed(lua_State* L) {
    uint32_t lo = (uint32_t)(uint64_t)luaL_checknumber(L, 1);
    uint32_t hi = (uint32_t)(uint64_t)luaL_optnumber(L, 2, 0);
    seed_rng(lo, hi);
    return 0;
}
int m_getRandomSeed(lua_State* L) {
    lua_pushnumber(L, (double)g_seed_lo);
    lua_pushnumber(L, (double)g_seed_hi);
    return 2;
}
int m_randomNormal(lua_State* L) {
    double stddev = luaL_optnumber(L, 1, 1.0), mean = luaL_optnumber(L, 2, 0.0);
    double u1 = next_double(), u2 = next_double();
    if (u1 < 1e-12) u1 = 1e-12;
    lua_pushnumber(L, mean + stddev * sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979323846 * u2));
    return 1;
}
const luaL_Reg math_fns[] = {{"random", m_random},
                             {"setRandomSeed", m_setRandomSeed},
                             {"getRandomSeed", m_getRandomSeed},
                             {"randomNormal", m_randomNormal},
                             {nullptr, nullptr}};

// ================================================================= love.system
int s_getOS(lua_State* L) {
    lua_pushstring(L, "Other");
    return 1;
}
int s_getProcessorCount(lua_State* L) {
    lua_pushinteger(L, 2);
    return 1;
}
int s_getPowerInfo(lua_State* L) {
    lua_pushstring(L, "unknown");
    lua_pushnil(L);
    lua_pushnil(L);
    return 3;
}
int s_getClipboardText(lua_State* L) {
    lua_pushstring(L, "");
    return 1;
}
int s_false(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}
const luaL_Reg system_fns[] = {{"getOS", s_getOS},
                               {"getProcessorCount", s_getProcessorCount},
                               {"getPowerInfo", s_getPowerInfo},
                               {"getClipboardText", s_getClipboardText},
                               {"setClipboardText", k_noop},
                               {"openURL", s_false},
                               {"vibrate", k_noop},
                               {nullptr, nullptr}};

}  // namespace

// ---------------------------------------------------------------------------------------------------
void system_set_title(const char* t) {
    strncpy(g_title, t ? t : "", sizeof g_title - 1);
    g_title[sizeof g_title - 1] = 0;
}
void system_set_input_mode(const char* mode) {
    strncpy(g_input_mode, mode ? mode : "auto", sizeof g_input_mode - 1);
    g_input_mode[sizeof g_input_mode - 1] = 0;
}

void open_system(lua_State* L) {
    g_start_ms = hal::millis();
    g_first_step = true;
    g_dt = g_acc = 0;
    g_frames = g_fps = 0;
    g_prev_buttons = hal::buttons();   // un bouton déjà maintenu au démarrage n'est pas un « nouvel appui »
    g_mouse_inited = false;            // recentré à la première utilisation (fenêtre pas encore connue ici)
    g_mouse_visible = true;
    seed_rng((uint32_t)hal::micros() ^ 0x5EEDu, 0);

    // objet joystick virtuel (table + métatable de méthodes)
    lua_newtable(L);
    lua_newtable(L);
    lua_newtable(L);
    luaL_setfuncs(L, joystick_methods, 0);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    g_joy_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    set_module(L, "timer", timer_fns);
    set_module(L, "keyboard", keyboard_fns);
    set_module(L, "joystick", joystick_fns);
    set_module(L, "window", window_fns);
    set_module(L, "mouse", mouse_fns);
    set_module(L, "math", math_fns);
    set_module(L, "system", system_fns);
    set_module(L, "aka", aka_fns);
}
