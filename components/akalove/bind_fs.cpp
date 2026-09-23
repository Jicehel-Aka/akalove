// bind_fs.cpp — love.filesystem : lecture (sauvegarde puis source), écriture (sauvegarde), require
// POSIX (stdio, dirent, stat) : identique sur la console (VFS d'ESP-IDF) et sur le PC.
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>   // _mkdir
#define AKA_MKDIR(p) _mkdir(p)
#else
#define AKA_MKDIR(p) mkdir((p), 0777)
#endif

#include <algorithm>
#include <string>
#include <vector>

#include "bind_common.h"
#include "hal.h"

namespace {

char g_source[256] = ".";
char g_save_root[256] = "./save";
char g_identity[64] = "game";
char g_save[320] = "./save/game";

void copy_str(char* dst, size_t n, const char* src) {   // copie bornée, toujours terminée
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = 0;
}

bool safe_rel(const char* p) {
    if (!p || !*p) return false;
    if (p[0] == '/' || p[0] == '\\') return false;
    for (const char* c = p; *c; ++c) {
        if ((unsigned char)*c < 32 || *c == '\\' || *c == ':') return false;
        if (c[0] == '.' && c[1] == '.' && (c == p || c[-1] == '/') && (c[2] == 0 || c[2] == '/')) return false;
    }
    return true;
}

bool is_file(const char* p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}
bool is_dir(const char* p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

bool mkdir_p(const char* path) {
    char tmp[320];
    strncpy(tmp, path, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = 0;
            AKA_MKDIR(tmp);
            *p = '/';
        }
    }
    if (AKA_MKDIR(tmp) != 0 && errno != EEXIST) return false;
    return true;
}

void join(char* out, size_t n, const char* a, const char* b) { snprintf(out, n, "%s/%s", a, b); }

bool read_all(const char* path, char** data, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        fclose(f);
        return false;
    }
    char* buf = (char*)hal::alloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = 0;
    *data = buf;
    *len = got;
    return true;
}

// Résout un chemin relatif : sauvegarde d'abord, puis source. Renvoie true si trouvé (out = chemin réel).
bool resolve_read(const char* rel, char* out, size_t n) {
    if (!safe_rel(rel)) return false;
    join(out, n, g_save, rel);
    if (is_file(out) || is_dir(out)) return true;
    join(out, n, g_source, rel);
    return is_file(out) || is_dir(out);
}

int fs_error(lua_State* L, const char* what, const char* name) {
    lua_pushnil(L);
    lua_pushfstring(L, "%s '%s'", what, name);
    return 2;
}

// ---------------------------------------------------------------------------------------------------
int f_read(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    char path[600];
    if (!resolve_read(name, path, sizeof path) || !is_file(path)) return fs_error(L, "Could not open file", name);
    char* data;
    size_t len;
    if (!read_all(path, &data, &len)) return fs_error(L, "Could not read file", name);
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        size_t want = (size_t)luaL_checknumber(L, 2);
        if (want < len) len = want;
    }
    lua_pushlstring(L, data, len);
    hal::free_(data);
    lua_pushinteger(L, (lua_Integer)len);
    return 2;
}

// Écrit des octets dans <save>/rel (partagé par love.filesystem.write/append ET l'objet File ci-dessous).
bool write_bytes(const char* rel, const char* data, size_t len, bool append) {
    if (!safe_rel(rel)) return false;
    char path[600];
    join(path, sizeof path, g_save, rel);
    FILE* f = fopen(path, append ? "ab" : "wb");
    if (!f) return false;
    size_t put = fwrite(data, 1, len, f);
    fclose(f);
    return put == len;
}

int write_common(lua_State* L, const char* mode) {
    const char* name = luaL_checkstring(L, 1);
    bool has_size = lua_gettop(L) >= 3 && !lua_isnil(L, 3);
    size_t want = has_size ? (size_t)luaL_checknumber(L, 3) : 0;
    size_t len;
    const char* data = luaL_tolstring(L, 2, &len);   // pousse une valeur : à faire après les lectures d'arguments
    if (has_size && want < len) len = want;
    if (!safe_rel(name)) {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "Could not open file '%s' (invalid path)", name);
        return 2;
    }
    bool ok = write_bytes(name, data, len, !strcmp(mode, "ab"));
    if (!ok) {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "Could not open file '%s' for writing", name);
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}
int f_write(lua_State* L) { return write_common(L, "wb"); }
int f_append(lua_State* L) { return write_common(L, "ab"); }

int f_exists(lua_State* L) {
    char path[600];
    lua_pushboolean(L, resolve_read(luaL_checkstring(L, 1), path, sizeof path));
    return 1;
}
int f_isFile(lua_State* L) {
    char path[600];
    lua_pushboolean(L, resolve_read(luaL_checkstring(L, 1), path, sizeof path) && is_file(path));
    return 1;
}
int f_isDirectory(lua_State* L) {
    char path[600];
    lua_pushboolean(L, resolve_read(luaL_checkstring(L, 1), path, sizeof path) && is_dir(path));
    return 1;
}
int f_getInfo(lua_State* L) {
    char path[600];
    const char* name = luaL_checkstring(L, 1);
    struct stat st;
    if (!resolve_read(name, path, sizeof path) || stat(path, &st) != 0) {
        lua_pushnil(L);
        return 1;
    }
    const char* type = S_ISDIR(st.st_mode) ? "directory" : "file";
    if (lua_isstring(L, 2) && strcmp(lua_tostring(L, 2), type) != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushstring(L, type);
    lua_setfield(L, -2, "type");
    if (S_ISREG(st.st_mode)) {
        lua_pushnumber(L, (double)st.st_size);
        lua_setfield(L, -2, "size");
    }
    lua_pushnumber(L, (double)st.st_mtime);
    lua_setfield(L, -2, "modtime");
    return 1;
}
int f_createDirectory(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    char path[600];
    if (!safe_rel(name)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    join(path, sizeof path, g_save, name);
    lua_pushboolean(L, mkdir_p(path));
    return 1;
}
int f_remove(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    char path[600];
    if (!safe_rel(name)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    join(path, sizeof path, g_save, name);
    lua_pushboolean(L, remove(path) == 0);
    return 1;
}

void list_dir(const char* path, std::vector<std::string>& out) {
    DIR* d = opendir(path);
    if (!d) return;
    while (dirent* e = readdir(d)) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (std::find(out.begin(), out.end(), e->d_name) == out.end()) out.push_back(e->d_name);
    }
    closedir(d);
}
int f_getDirectoryItems(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    std::vector<std::string> items;
    if (!strcmp(name, "/") || !strcmp(name, ".") || !*name) {
        list_dir(g_save, items);
        list_dir(g_source, items);
    } else if (safe_rel(name)) {
        char p[600];
        join(p, sizeof p, g_save, name);
        list_dir(p, items);
        join(p, sizeof p, g_source, name);
        list_dir(p, items);
    }
    std::sort(items.begin(), items.end());
    lua_newtable(L);
    for (size_t i = 0; i < items.size(); ++i) {
        lua_pushstring(L, items[i].c_str());
        lua_rawseti(L, -2, (int)i + 1);
    }
    return 1;
}

int load_chunk(lua_State* L, const char* rel, const char* path) {
    char* data;
    size_t len;
    if (!read_all(path, &data, &len)) return -1;
    char chunk[256];
    chunk[0] = '@';
    copy_str(chunk + 1, sizeof chunk - 1, rel);
    int rc = luaL_loadbuffer(L, data, len, chunk);
    hal::free_(data);
    return rc;
}
int f_load(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    char path[600];
    if (!resolve_read(name, path, sizeof path) || !is_file(path)) return fs_error(L, "Could not open file", name);
    int rc = load_chunk(L, name, path);
    if (rc < 0) return fs_error(L, "Could not read file", name);
    if (rc != 0) {
        lua_pushnil(L);
        lua_insert(L, -2);   // nil, message
        return 2;
    }
    return 1;
}

int f_getSaveDirectory(lua_State* L) {
    lua_pushstring(L, g_save);
    return 1;
}
int f_getSource(lua_State* L) {
    lua_pushstring(L, g_source);
    return 1;
}
int f_getIdentity(lua_State* L) {
    lua_pushstring(L, g_identity);
    return 1;
}
int f_setIdentity(lua_State* L) {
    filesystem_configure(g_source, g_save_root, luaL_checkstring(L, 1));
    return 0;
}
int f_false(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}
// ================================================================= objet File (open/read/write/close)
// love.filesystem.newFile("chemin") : API "objet" de Love, en plus de read()/write()/append() déjà
// fonctionnelles ci-dessus. En lecture, le contenu est chargé entièrement en mémoire à open("r") (les
// fichiers visés — sauvegardes JSON, scores — sont petits). En écriture/ajout, les octets s'accumulent
// dans un tampon qui grossit par doublement et n'est réellement écrit sur la carte SD qu'à close() (ou
// au ramasse-miettes si le jeu a oublié de fermer).
constexpr const char* FILE_MT = "aka.File";
enum FileMode { FM_CLOSED, FM_READ, FM_WRITE, FM_APPEND };

struct GfxFile {
    char path[256];
    FileMode mode;
    char* buf;      // lecture : contenu entier ; écriture : accumulateur
    size_t len;     // octets valides dans buf
    size_t cap;     // écriture seulement : taille allouée de buf
    size_t pos;     // lecture seulement : curseur
};

bool file_buf_reserve(GfxFile* f, size_t extra) {
    if (f->len + extra <= f->cap) return true;
    size_t ncap = f->cap ? f->cap : 256;
    while (ncap < f->len + extra) ncap *= 2;
    char* nb = (char*)hal::realloc_(f->buf, ncap);
    if (!nb) return false;
    f->buf = nb;
    f->cap = ncap;
    return true;
}

// Écrit sur la carte SD ce qui a été accumulé (mode écriture/ajout) ; sans effet en lecture ou fermé.
bool file_flush_and_close(GfxFile* f) {
    bool ok = true;
    if (f->mode == FM_WRITE || f->mode == FM_APPEND) ok = write_bytes(f->path, f->buf ? f->buf : "", f->len, f->mode == FM_APPEND);
    if (f->buf) hal::free_(f->buf);
    f->buf = nullptr;
    f->len = f->cap = f->pos = 0;
    f->mode = FM_CLOSED;
    return ok;
}

int file_gc(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    file_flush_and_close(f);   // sécurité : un fichier ouvert en écriture et jamais fermé n'est pas perdu
    return 0;
}
int file_open(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    const char* mode = luaL_checkstring(L, 2);
    if (f->mode != FM_CLOSED) file_flush_and_close(f);   // réouverture : referme proprement l'ancien mode
    if (!strcmp(mode, "r")) {
        char* data;
        size_t len;
        if (!filesystem_read_file(f->path, &data, &len)) {
            lua_pushboolean(L, 0);
            lua_pushfstring(L, "Could not open file '%s'", f->path);
            return 2;
        }
        f->buf = data;
        f->len = len;
        f->pos = 0;
        f->mode = FM_READ;
    } else if (!strcmp(mode, "w")) {
        f->mode = FM_WRITE;
    } else if (!strcmp(mode, "a")) {
        f->mode = FM_APPEND;
    } else {
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "Invalid file open mode: %s", mode);
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}
int file_close(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    bool ok = f->mode == FM_CLOSED || file_flush_and_close(f);
    lua_pushboolean(L, ok);
    return 1;
}
int file_write(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    if (f->mode != FM_WRITE && f->mode != FM_APPEND) return luaL_error(L, "File is not opened for writing");
    bool has_size = lua_gettop(L) >= 3 && !lua_isnil(L, 3);
    size_t want = has_size ? (size_t)luaL_checknumber(L, 3) : 0;
    size_t len;
    const char* data = luaL_tolstring(L, 2, &len);   // pousse une valeur : à faire après les lectures d'arguments
    if (has_size && want < len) len = want;
    if (!file_buf_reserve(f, len)) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Out of memory");
        return 2;
    }
    memcpy(f->buf + f->len, data, len);
    f->len += len;
    lua_pushboolean(L, 1);
    return 1;
}
int file_read(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    if (f->mode != FM_READ) return luaL_error(L, "File is not opened for reading");
    size_t want = f->len - f->pos;
    if (lua_gettop(L) >= 2 && !lua_isnil(L, 2)) {
        size_t n = (size_t)luaL_checknumber(L, 2);
        if (n < want) want = n;
    }
    lua_pushlstring(L, f->buf + f->pos, want);
    f->pos += want;
    lua_pushinteger(L, (lua_Integer)want);
    return 2;
}
int file_close_bool(lua_State* L) {   // isOpen
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    lua_pushboolean(L, f->mode != FM_CLOSED);
    return 1;
}
int file_getSize(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    lua_pushinteger(L, (lua_Integer)f->len);
    return 1;
}
int file_getFilename(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    lua_pushstring(L, f->path);
    return 1;
}
int file_eof(lua_State* L) {
    auto* f = (GfxFile*)luaL_checkudata(L, 1, FILE_MT);
    lua_pushboolean(L, f->mode != FM_READ || f->pos >= f->len);
    return 1;
}
const luaL_Reg file_methods[] = {{"open", file_open},   {"close", file_close},         {"write", file_write},
                                 {"read", file_read},    {"isOpen", file_close_bool},   {"getSize", file_getSize},
                                 {"getFilename", file_getFilename}, {"eof", file_eof},
                                 {nullptr, nullptr}};

int f_newFile(lua_State* L) {
    int nargs = lua_gettop(L);
    const char* name = luaL_checkstring(L, 1);
    auto* f = (GfxFile*)lua_newuserdata(L, sizeof(GfxFile));
    memset(f, 0, sizeof *f);
    copy_str(f->path, sizeof f->path, name);
    if (luaL_newmetatable(L, FILE_MT)) {
        lua_newtable(L);
        for (const luaL_Reg* r = file_methods; r->name; ++r)
            if (r->func) {
                lua_pushcfunction(L, r->func);
                lua_setfield(L, -2, r->name);
            }
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, file_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    int ud = lua_gettop(L);   // index du userdata (déjà créé, métatable posée)
    // mode d'ouverture optionnel : newFile("chemin", "r") ouvre directement, comme Love le permet
    if (nargs >= 2 && !lua_isnil(L, 2)) {
        lua_pushcfunction(L, file_open);
        lua_pushvalue(L, ud);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 2);   // ok, err — ignorés : le jeu peut aussi tester lui-même avec :open()
        lua_pop(L, 2);
    }
    lua_pushvalue(L, ud);
    return 1;
}

const luaL_Reg fs_fns[] = {{"read", f_read},
                           {"write", f_write},
                           {"append", f_append},
                           {"exists", f_exists},
                           {"isFile", f_isFile},
                           {"isDirectory", f_isDirectory},
                           {"getInfo", f_getInfo},
                           {"createDirectory", f_createDirectory},
                           {"remove", f_remove},
                           {"newFile", f_newFile},
                           {"getDirectoryItems", f_getDirectoryItems},
                           {"load", f_load},
                           {"getSaveDirectory", f_getSaveDirectory},
                           {"getSource", f_getSource},
                           {"getWorkingDirectory", f_getSource},
                           {"getIdentity", f_getIdentity},
                           {"setIdentity", f_setIdentity},
                           {"isFused", f_false},
                           {nullptr, nullptr}};

}  // namespace

// ---------------------------------------------------------------------------------------------------
void filesystem_configure(const char* source_dir, const char* save_root, const char* identity) {
    char src[256], root[256], ident[64];   // copies : les arguments peuvent pointer vers nos propres tampons
    copy_str(src, sizeof src, source_dir);
    copy_str(root, sizeof root, save_root);
    copy_str(ident, sizeof ident, identity);
    copy_str(g_source, sizeof g_source, src);
    copy_str(g_save_root, sizeof g_save_root, root);
    copy_str(g_identity, sizeof g_identity, ident);
    snprintf(g_save, sizeof g_save, "%s/%s", g_save_root, g_identity);
    mkdir_p(g_save);
}

bool filesystem_read_file(const char* rel, char** data, size_t* len) {
    char path[600];
    if (!resolve_read(rel, path, sizeof path) || !is_file(path)) return false;
    return read_all(path, data, len);
}

bool filesystem_resolve_path(const char* rel, char* out, size_t out_n) {
    char path[600];
    if (!resolve_read(rel, path, sizeof path) || !is_file(path)) return false;
    copy_str(out, out_n, path);
    return true;
}

// package.loaders[2] : require("a.b") -> a/b.lua puis a/b/init.lua
int filesystem_loader(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    char base[200];
    strncpy(base, name, sizeof base - 1);
    base[sizeof base - 1] = 0;
    for (char* p = base; *p; ++p)
        if (*p == '.') *p = '/';
    char cand[2][230];
    snprintf(cand[0], sizeof cand[0], "%s.lua", base);
    snprintf(cand[1], sizeof cand[1], "%s/init.lua", base);
    for (int i = 0; i < 2; ++i) {
        char path[600];
        if (!resolve_read(cand[i], path, sizeof path) || !is_file(path)) continue;
        int rc = load_chunk(L, cand[i], path);
        if (rc < 0) continue;
        if (rc != 0)
            return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s", name, cand[i], lua_tostring(L, -1));
        return 1;
    }
    lua_pushfstring(L, "\n\tno file '%s' or '%s' in the game source or save directory", cand[0], cand[1]);
    return 1;
}

void open_filesystem(lua_State* L) { set_module(L, "filesystem", fs_fns); }
