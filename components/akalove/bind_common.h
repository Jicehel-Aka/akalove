// bind_common.h — utilitaires communs aux bindings love.*
#pragma once
#include "lua_compat.h"

// love.<name> = { fns }  (la table love doit exister)
static inline void set_module(lua_State* L, const char* name, const luaL_Reg* fns) {
    lua_getglobal(L, "love");
    lua_newtable(L);
    luaL_setfuncs(L, fns, 0);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

// Déclarations des modules (un fichier bind_*.cpp par famille)
void open_graphics(lua_State* L);
void open_system(lua_State* L);      // timer, keyboard, joystick, window, math, system, aka
void open_filesystem(lua_State* L);
void open_audio(lua_State* L);

// Utilisés par le runtime
void system_set_title(const char* t);
void system_set_input_mode(const char* mode);
void filesystem_configure(const char* source_dir, const char* save_root, const char* identity);
int filesystem_loader(lua_State* L);   // package.loaders[2]
bool filesystem_read_file(const char* rel, char** data, size_t* len);   // sauvegarde puis source ; malloc hal::alloc
// Chemin ABSOLU réel d'un fichier existant (sauvegarde puis source), sans le lire — sert à l'audio
// (gb_audio_track_wav::play_wav() lit directement depuis la carte SD, pas depuis un tampon mémoire).
// out doit faire au moins 600 octets (comme les tampons internes de bind_fs.cpp).
bool filesystem_resolve_path(const char* rel, char* out, size_t out_n);
