// bind_audio.cpp — love.audio : Source (newSource, play/stop/volume/looping), volume maître.
//
// Une Source Lua est un descripteur léger (chemin résolu, type déclaré, réglages) ; la lecture réelle
// est déléguée à hal::audio_* (voir hal.h) qui gère les AUDIO_TRACKS canaux matériels (4, comme
// gb_audio_player). Simplification assumée : "static" et "stream" sont traités de façon identique — sur
// la console, gb_audio_track_wav::play_wav() lit déjà directement depuis la carte SD dans les deux cas,
// il n'y a donc rien de plus à faire pour "stream" ; c'est documenté comme limitation dans le README.
#include <string.h>

#include "bind_common.h"
#include "hal.h"

namespace {

constexpr const char* SOURCE_MT = "aka.Source";

struct GfxSource {
    char path[400];   // chemin ABSOLU résolu à la création (filesystem_resolve_path)
    bool is_static;    // "static" vs "stream" : juste mémorisé pour type()/clone(), aucune autre différence
    bool looping;
    float volume;
    int track;         // canal hal:: en cours (0..AUDIO_TRACKS-1), -1 si arrêtée
};

void start(GfxSource* s) {
    if (s->track >= 0) hal::audio_stop(s->track);   // play() sur une source déjà lancée : redémarre du début (comme Love)
    s->track = hal::audio_play(s->path, s->looping, s->volume);
}

int source_play(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    start(s);
    lua_pushboolean(L, s->track >= 0);
    return 1;
}
int source_stop(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    if (s->track >= 0) hal::audio_stop(s->track);
    s->track = -1;
    return 0;
}
// pas de vraie pause/reprise côté HAL (gb_audio_track_wav n'en a pas) : pause() arrête, comme si la
// lecture était terminée ; journalisé une fois pour que ce ne soit pas une surprise silencieuse.
int source_pause(lua_State* L) {
    static bool warned = false;
    if (!warned) {
        warned = true;
        hal::log("[AKA-Love] Source:pause() n'est pas une vraie pause (pas de reprise) : équivaut à stop()\n");
    }
    return source_stop(L);
}
int source_isPlaying(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    bool playing = s->track >= 0 && hal::audio_is_playing(s->track);
    if (!playing) s->track = -1;   // terminée toute seule (durée écoulée, non bouclée) : on l'oublie
    lua_pushboolean(L, playing);
    return 1;
}
int source_setVolume(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    s->volume = (float)luaL_checknumber(L, 2);
    if (s->track >= 0) hal::audio_set_volume(s->track, s->volume);
    return 0;
}
int source_getVolume(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    lua_pushnumber(L, s->volume);
    return 1;
}
int source_setLooping(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    s->looping = lua_toboolean(L, 2) != 0;
    return 0;
}
int source_isLooping(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    lua_pushboolean(L, s->looping);
    return 1;
}
int source_type(lua_State* L) {
    lua_pushstring(L, "Source");
    return 1;
}
int source_getType(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    lua_pushstring(L, s->is_static ? "static" : "stream");
    return 1;
}
int source_clone(lua_State* L);   // déclaration (utilise push_source, définie plus bas)

const luaL_Reg source_methods[] = {{"play", source_play},
                                   {"stop", source_stop},
                                   {"pause", source_pause},
                                   {"isPlaying", source_isPlaying},
                                   {"setVolume", source_setVolume},
                                   {"getVolume", source_getVolume},
                                   {"setLooping", source_setLooping},
                                   {"isLooping", source_isLooping},
                                   {"clone", source_clone},
                                   {"type", source_type},
                                   {"getType", source_getType},
                                   {nullptr, nullptr}};

GfxSource* push_source(lua_State* L, const char* path, bool is_static) {
    auto* s = (GfxSource*)lua_newuserdata(L, sizeof(GfxSource));
    memset(s, 0, sizeof *s);
    strncpy(s->path, path, sizeof s->path - 1);
    s->is_static = is_static;
    s->volume = 1.0f;
    s->track = -1;
    if (luaL_newmetatable(L, SOURCE_MT)) {
        lua_newtable(L);
        luaL_setfuncs(L, source_methods, 0);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
    return s;
}

int source_clone(lua_State* L) {
    auto* s = (GfxSource*)luaL_checkudata(L, 1, SOURCE_MT);
    push_source(L, s->path, s->is_static);   // nouvelle Source indépendante (pas encore lancée)
    return 1;
}

// love.audio.newSource(chemin [, "static"|"stream"]) : vérifie juste que le fichier existe (résolution
// sauvegarde puis source, comme love.graphics.newImage) ; le contenu n'est ni lu ni décodé ici — c'est
// hal::audio_play() qui le fera, au premier :play(), et qui peut donc encore échouer plus tard si le
// fichier n'est finalement pas un WAV 16 bits valide.
int g_newSource(lua_State* L) {
    const char* rel = luaL_checkstring(L, 1);
    const char* kind = luaL_optstring(L, 2, "static");
    char abs_path[600];
    if (!filesystem_resolve_path(rel, abs_path, sizeof abs_path)) return luaL_error(L, "Could not open file %s", rel);
    push_source(L, abs_path, strcmp(kind, "stream") != 0);
    return 1;
}
int g_setVolume(lua_State* L) {
    hal::audio_set_master_volume((float)luaL_checknumber(L, 1));
    return 0;
}
int g_getVolume(lua_State* L) {
    lua_pushnumber(L, 1.0);   // pas de lecture côté hal:: (volume maître écrit seulement) ; approximation
    return 1;
}
int g_stop(lua_State*) {
    for (int i = 0; i < hal::AUDIO_TRACKS; ++i) hal::audio_stop(i);
    return 0;
}

const luaL_Reg audio_fns[] = {
    {"newSource", g_newSource}, {"setVolume", g_setVolume}, {"getVolume", g_getVolume}, {"stop", g_stop}, {nullptr, nullptr}};

}  // namespace

void open_audio(lua_State* L) { set_module(L, "audio", audio_fns); }
