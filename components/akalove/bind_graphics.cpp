// bind_graphics.cpp — love.graphics : primitives, couleurs, transformations, texte, images (P2)
#include <string.h>

#include "bind_common.h"
#include "hal.h"
#include "image_decode.h"
#include "love_gfx.h"

namespace {

// Tampon partagé (une seule VM, un seul fil) : jusqu'à 1024 points
float g_pts[2048];
constexpr int MAX_POINTS = 1024;

// Décodage UTF-8 minimal (1 à 3 octets), copié de aka_font/gb_text_render.h (gb_text::utf8_next) :
// seule la découpe des lignes en a besoin ici, pas les tables de glyphes (voir la note de licence plus
// bas sur l'unique point d'inclusion de gb_text_render.h, dans love_gfx.cpp).
uint32_t utf8_next(const char*& p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) {
        p += 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && p[1]) {
        uint32_t cp = ((c & 0x1Fu) << 6) | ((unsigned char)p[1] & 0x3Fu);
        p += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
        uint32_t cp = ((c & 0x0Fu) << 12) | (((unsigned char)p[1] & 0x3Fu) << 6) | ((unsigned char)p[2] & 0x3Fu);
        p += 3;
        return cp;
    }
    p += 1;
    return '?';   // octet invalide isolé : on avance quand même
}

bool draw_mode(lua_State* L, int i) {   // true = fill
    const char* m = luaL_checkstring(L, i);
    if (!strcmp(m, "fill")) return true;
    if (!strcmp(m, "line")) return false;
    return luaL_argerror(L, i, "Invalid draw mode (expected 'fill' or 'line')") != 0;
}

// Lit des coordonnées : x1,y1,x2,y2,... ou une table plate {x1,y1,...} ou {{x,y},{x,y}}
int read_xy(lua_State* L, int first, float* out, int max_pts) {
    int n = 0;
    if (lua_istable(L, first)) {
        int len = (int)lua_objlen(L, first);
        if (len > 0) {
            lua_rawgeti(L, first, 1);
            bool nested = lua_istable(L, -1);
            lua_pop(L, 1);
            if (nested) {
                for (int i = 1; i <= len && n < max_pts; ++i) {
                    lua_rawgeti(L, first, i);
                    lua_rawgeti(L, -1, 1);
                    lua_rawgeti(L, -2, 2);
                    out[2 * n] = (float)lua_tonumber(L, -2);
                    out[2 * n + 1] = (float)lua_tonumber(L, -1);
                    lua_pop(L, 3);
                    ++n;
                }
                return n;
            }
            for (int i = 1; i + 1 <= len && n < max_pts; i += 2) {
                lua_rawgeti(L, first, i);
                lua_rawgeti(L, first, i + 1);
                out[2 * n] = (float)lua_tonumber(L, -2);
                out[2 * n + 1] = (float)lua_tonumber(L, -1);
                lua_pop(L, 2);
                ++n;
            }
        }
        return n;
    }
    int top = lua_gettop(L);
    for (int i = first; i + 1 <= top && n < max_pts; i += 2) {
        out[2 * n] = (float)luaL_checknumber(L, i);
        out[2 * n + 1] = (float)luaL_checknumber(L, i + 1);
        ++n;
    }
    return n;
}

void read_color(lua_State* L, int first, float c[4]) {
    if (lua_istable(L, first)) {
        for (int i = 0; i < 4; ++i) {
            lua_rawgeti(L, first, i + 1);
            c[i] = lua_isnil(L, -1) ? (i == 3 ? 1.0f : 0.0f) : (float)lua_tonumber(L, -1);
            lua_pop(L, 1);
        }
    } else {
        c[0] = (float)luaL_checknumber(L, first);
        c[1] = (float)luaL_checknumber(L, first + 1);
        c[2] = (float)luaL_checknumber(L, first + 2);
        c[3] = (float)luaL_optnumber(L, first + 3, 1.0);
    }
}

// --- état ---------------------------------------------------------------------------------------
int g_setColor(lua_State* L) {
    float c[4];
    read_color(L, 1, c);
    gfx::set_color(c[0], c[1], c[2], c[3]);
    return 0;
}
int g_getColor(lua_State* L) {
    float c[4];
    gfx::get_color(c);
    for (int i = 0; i < 4; ++i) lua_pushnumber(L, c[i]);
    return 4;
}
int g_setBackgroundColor(lua_State* L) {
    float c[4];
    read_color(L, 1, c);
    gfx::set_bg(c[0], c[1], c[2], c[3]);
    return 0;
}
int g_getBackgroundColor(lua_State* L) {
    float c[4];
    gfx::get_bg(c);
    for (int i = 0; i < 4; ++i) lua_pushnumber(L, c[i]);
    return 4;
}
int g_clear(lua_State* L) {
    float c[4] = {0, 0, 0, 0};
    if (lua_gettop(L) > 0 && !lua_isnil(L, 1)) {
        if (lua_istable(L, 1) || lua_isnumber(L, 1)) read_color(L, 1, c);
    }
    gfx::clear(c[0], c[1], c[2], c[3]);
    return 0;
}
int g_present(lua_State*) {
    gfx::present();
    return 0;
}
int g_setLineWidth(lua_State* L) {
    gfx::set_line_width((float)luaL_checknumber(L, 1));
    return 0;
}
int g_getLineWidth(lua_State* L) {
    lua_pushnumber(L, gfx::get_line_width());
    return 1;
}
int g_noop(lua_State*) { return 0; }   // setLineStyle, setLineJoin, setDefaultFilter : sans effet ici
int g_reset(lua_State*) {
    gfx::reset();
    return 0;
}
int g_isActive(lua_State* L) {
    lua_pushboolean(L, 1);
    return 1;
}
int g_getWidth(lua_State* L) {
    lua_pushinteger(L, gfx::win_w());
    return 1;
}
int g_getHeight(lua_State* L) {
    lua_pushinteger(L, gfx::win_h());
    return 1;
}
int g_getDimensions(lua_State* L) {
    lua_pushinteger(L, gfx::win_w());
    lua_pushinteger(L, gfx::win_h());
    return 2;
}

// --- transformations ---------------------------------------------------------------------------------
int g_push(lua_State* L) {
    if (!gfx::push()) return luaL_error(L, "Maximum stack depth reached (more pushes than pops?)");
    return 0;
}
int g_pop(lua_State* L) {
    if (!gfx::pop()) return luaL_error(L, "Minimum stack depth reached (more pops than pushes?)");
    return 0;
}
int g_origin(lua_State*) {
    gfx::origin();
    return 0;
}
int g_translate(lua_State* L) {
    gfx::translate((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2));
    return 0;
}
int g_rotate(lua_State* L) {
    gfx::rotate((float)luaL_checknumber(L, 1));
    return 0;
}
int g_scale(lua_State* L) {
    float sx = (float)luaL_checknumber(L, 1);
    gfx::scale(sx, (float)luaL_optnumber(L, 2, sx));
    return 0;
}
int g_shear(lua_State* L) {
    gfx::shear((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2));
    return 0;
}
int g_setScissor(lua_State* L) {
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        gfx::clear_scissor();
    } else {
        int w = (int)luaL_checknumber(L, 3), h = (int)luaL_checknumber(L, 4);
        if (w < 0 || h < 0) return luaL_error(L, "Can't set scissor with negative width and/or height.");
        gfx::set_scissor((int)luaL_checknumber(L, 1), (int)luaL_checknumber(L, 2), w, h);
    }
    return 0;
}
int g_getScissor(lua_State* L) {
    int r[4];
    if (!gfx::get_scissor(r)) return 0;
    for (int i = 0; i < 4; ++i) lua_pushinteger(L, r[i]);
    return 4;
}

// --- primitives --------------------------------------------------------------------------------------------
int g_rectangle(lua_State* L) {
    bool fill = draw_mode(L, 1);
    if (lua_gettop(L) >= 6 && lua_tonumber(L, 6) != 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            hal::log("[AKA-Love] rectangle : coins arrondis ignorés (phase P4)\n");
        }
    }
    gfx::rect(fill, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
              (float)luaL_checknumber(L, 5));
    return 0;
}
int g_circle(lua_State* L) {
    bool fill = draw_mode(L, 1);
    gfx::circle(fill, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                (int)luaL_optinteger(L, 5, 0));
    return 0;
}
int g_polygon(lua_State* L) {
    bool fill = draw_mode(L, 1);
    int n = read_xy(L, 2, g_pts, 128);
    if (n < 3) return luaL_error(L, "Need at least three vertices to draw a polygon.");
    gfx::polygon(fill, g_pts, n);
    return 0;
}
int g_line(lua_State* L) {
    int n = read_xy(L, 1, g_pts, 128);
    if (n < 2) return luaL_error(L, "Need at least two vertices to draw a line.");
    gfx::line(g_pts, n);
    return 0;
}
int g_points(lua_State* L) {
    int n = read_xy(L, 1, g_pts, MAX_POINTS);
    gfx::points(g_pts, n);
    return 0;
}

// --- texte ------------------------------------------------------------------------------------------------------
// print(text | {couleur, texte, ...}, x, y, r, sx, sy, ox, oy)
int g_print(lua_State* L) {
    float x = (float)luaL_optnumber(L, 2, 0), y = (float)luaL_optnumber(L, 3, 0);
    float r = (float)luaL_optnumber(L, 4, 0);
    float sx = (float)luaL_optnumber(L, 5, 1), sy = (float)luaL_optnumber(L, 6, sx);
    float ox = (float)luaL_optnumber(L, 7, 0), oy = (float)luaL_optnumber(L, 8, 0);
    if (lua_istable(L, 1)) {
        float saved[4];
        gfx::get_color(saved);
        float advance = 0;
        int len = (int)lua_objlen(L, 1);
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 1, i);
            if (lua_istable(L, -1)) {
                float c[4];
                read_color(L, lua_gettop(L), c);
                gfx::set_color(c[0], c[1], c[2], c[3]);
            } else if (lua_isstring(L, -1)) {
                const char* s = lua_tostring(L, -1);
                gfx::print(s, x, y, r, sx, sy, ox - advance, oy);
                advance += (float)gfx::text_width(s);
            }
            lua_pop(L, 1);
        }
        gfx::set_color(saved[0], saved[1], saved[2], saved[3]);
        return 0;
    }
    const char* s = luaL_tolstring(L, 1, nullptr);
    gfx::print(s, x, y, r, sx, sy, ox, oy);
    return 0;
}

// printf(text, x, y, limit, align, r, sx, sy, ox, oy) : retour à la ligne aux espaces
int g_printf(lua_State* L) {
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3);
    float limit = (float)luaL_checknumber(L, 4);
    const char* align = luaL_optstring(L, 5, "left");
    float r = (float)luaL_optnumber(L, 6, 0);
    float sx = (float)luaL_optnumber(L, 7, 1), sy = (float)luaL_optnumber(L, 8, sx);
    float ox = (float)luaL_optnumber(L, 9, 0), oy = (float)luaL_optnumber(L, 10, 0);
    const char* text = luaL_tolstring(L, 1, nullptr);   // en dernier : pousse une valeur sur la pile
    int cw = 8 * gfx::get_font_scale();   // largeur d'un caractère à la taille de police courante
    int cols = (int)(limit / (float)cw);  // approx pour les rares caractères à repli double comme œ
    if (cols < 1) cols = 1;               // (comptent ici pour 1 colonne)

    char line[400];
    int row = 0;
    const char* p = text;
    while (*p) {
        // Découpe au mot : avance par CODEPOINTS (jamais au milieu d'un caractère accentué sur 2-3 octets),
        // en mémorisant le dernier espace rencontré (position octet + colonne) pour y couper en cas de
        // débordement. line_end/next_p délimitent la ligne à afficher / le début de la suivante.
        const char* line_start = p;
        const char* last_space = nullptr;
        const char* last_space_end = nullptr;
        int col = 0;
        bool forced_break = false;
        const char* q = p;
        while (*q) {
            if (*q == '\n') {
                forced_break = true;
                break;
            }
            if (col >= cols) break;
            const char* before = q;
            uint32_t cp = utf8_next(q);
            if (cp == ' ') {
                last_space = before;
                last_space_end = q;
            }
            ++col;
        }
        const char* line_end = q;
        const char* next_p = q;
        if (!forced_break && *q && last_space) {   // débordement réel (pas fin de texte/ligne) : coupe à l'espace
            line_end = last_space;
            next_p = last_space_end;
        }
        size_t len = (size_t)(line_end - line_start);
        if (len >= sizeof line) len = sizeof line - 1;   // ligne anormalement longue (pas d'espace) : coupe dure
        memcpy(line, line_start, len);
        line[len] = 0;

        int w = gfx::text_width(line);
        float off = 0;
        if (!strcmp(align, "center")) off = (limit - w) * 0.5f;
        else if (!strcmp(align, "right")) off = limit - w;
        gfx::print(line, x + off, y + row * (float)gfx::font_height(), r, sx, sy, ox, oy);
        ++row;
        p = next_p;
        if (*p == '\n') ++p;
    }
    return 0;
}

// ================================================================= Image et Quad (love.graphics)
// Deux userdata Lua pleins (pas de table) : GfxImage porte les pixels décodés (RGBA, libérés par
// __gc), GfxQuad son rectangle source. Un seul type de "drawable" pour l'instant (les Canvas/Text de
// Love restent une phase ultérieure) : love.graphics.draw() ne connaît que les Image.
constexpr const char* IMAGE_MT = "aka.Image";
constexpr const char* QUAD_MT = "aka.Quad";

struct GfxImage {
    Image img;   // image_decode.h : w, h, rgba (hal::alloc, libéré par __gc)
};
struct GfxQuad {
    float x, y, w, h;      // rectangle dans la texture source, en pixels
    float sw, sh;          // dimensions de référence au moment de la création (utilisées par getViewport)
};

// Équivalent de luaL_testudata (absent de Lua 5.1) : renvoie le pointeur si idx a la bonne métatable,
// nullptr sinon (sans lever d'erreur — sert à distinguer "Quad fourni ou pas" dans g_draw).
void* test_udata(lua_State* L, int idx, const char* tname) {
    void* p = lua_touserdata(L, idx);
    if (!p || !lua_getmetatable(L, idx)) return nullptr;
    luaL_getmetatable(L, tname);
    bool ok = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return ok ? p : nullptr;
}

int image_gc(lua_State* L) {
    auto* gi = (GfxImage*)luaL_checkudata(L, 1, IMAGE_MT);
    image_free(gi->img);
    return 0;
}
int image_getWidth(lua_State* L) {
    auto* gi = (GfxImage*)luaL_checkudata(L, 1, IMAGE_MT);
    lua_pushinteger(L, gi->img.w);
    return 1;
}
int image_getHeight(lua_State* L) {
    auto* gi = (GfxImage*)luaL_checkudata(L, 1, IMAGE_MT);
    lua_pushinteger(L, gi->img.h);
    return 1;
}
int image_getDimensions(lua_State* L) {
    auto* gi = (GfxImage*)luaL_checkudata(L, 1, IMAGE_MT);
    lua_pushinteger(L, gi->img.w);
    lua_pushinteger(L, gi->img.h);
    return 2;
}
int image_type(lua_State* L) {
    lua_pushstring(L, "Image");
    return 1;
}
const luaL_Reg image_methods[] = {{"getWidth", image_getWidth},
                                  {"getHeight", image_getHeight},
                                  {"getDimensions", image_getDimensions},
                                  {"getPixelDimensions", image_getDimensions},
                                  {"type", image_type},
                                  {"setFilter", g_noop},   // pas de filtrage (échantillonnage au plus proche partout)
                                  {"setWrap", g_noop},
                                  {nullptr, nullptr}};

// love.graphics.newImage("chemin.png"|"chemin.bmp") : cherche via love.filesystem (sauvegarde puis
// source, comme les autres lectures), décode selon l'extension. Erreur Lua si absent/illisible, comme
// sous Love.
int g_newImage(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    uint8_t* data = nullptr;
    size_t len = 0;
    if (!filesystem_read_file(path, (char**)&data, &len)) return luaL_error(L, "Could not open file %s", path);
    const char* dot = strrchr(path, '.');
    bool is_bmp = dot && (!strcasecmp(dot, ".bmp"));
    Image img;
    const char* why = nullptr;
    bool ok = is_bmp ? decode_bmp(data, len, img, &why) : decode_png(data, len, img, &why);
    hal::free_(data);
    if (!ok) return luaL_error(L, "Could not decode image %s (%s)", path, why ? why : "?");
    auto* gi = (GfxImage*)lua_newuserdata(L, sizeof(GfxImage));
    gi->img = img;
    if (luaL_newmetatable(L, IMAGE_MT)) {
        lua_newtable(L);
        luaL_setfuncs(L, image_methods, 0);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, image_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    return 1;
}

// love.graphics.newQuad(x, y, w, h, sw, sh) ou newQuad(x, y, w, h, image) (Love 11 : le Texture
// remplace sw/sh, dont on lit alors juste getWidth/getHeight — mêmes valeurs, on ne garde qu'elles).
int g_newQuad(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3), h = (float)luaL_checknumber(L, 4);
    float sw, sh;
    if (auto* gi = (GfxImage*)test_udata(L, 5, IMAGE_MT)) {
        sw = (float)gi->img.w;
        sh = (float)gi->img.h;
    } else {
        sw = (float)luaL_checknumber(L, 5);
        sh = (float)luaL_checknumber(L, 6);
    }
    auto* q = (GfxQuad*)lua_newuserdata(L, sizeof(GfxQuad));
    *q = {x, y, w, h, sw, sh};
    luaL_getmetatable(L, QUAD_MT);
    if (lua_isnil(L, -1)) {   // 1ère utilisation : crée la métatable (aucune méthode pour l'instant)
        lua_pop(L, 1);
        luaL_newmetatable(L, QUAD_MT);
    }
    lua_setmetatable(L, -2);
    return 1;
}

// love.graphics.draw(image [, quad], x, y, r, sx, sy, ox, oy, kx, ky)
int g_draw(lua_State* L) {
    auto* gi = (GfxImage*)luaL_checkudata(L, 1, IMAGE_MT);
    int i = 2;
    float qx = 0, qy = 0, qw = (float)gi->img.w, qh = (float)gi->img.h;
    if (auto* q = (GfxQuad*)test_udata(L, 2, QUAD_MT)) {
        qx = q->x;
        qy = q->y;
        qw = q->w;
        qh = q->h;
        i = 3;
    }
    float x = (float)luaL_optnumber(L, i, 0), y = (float)luaL_optnumber(L, i + 1, 0);
    float r = (float)luaL_optnumber(L, i + 2, 0);
    float sx = (float)luaL_optnumber(L, i + 3, 1), sy = (float)luaL_optnumber(L, i + 4, sx);
    float ox = (float)luaL_optnumber(L, i + 5, 0), oy = (float)luaL_optnumber(L, i + 6, 0);
    float kx = (float)luaL_optnumber(L, i + 7, 0), ky = (float)luaL_optnumber(L, i + 8, 0);
    gfx::ImageView view{gi->img.rgba, gi->img.w, gi->img.h};
    gfx::draw_image(view, qx, qy, qw, qh, x, y, r, sx, sy, ox, oy, kx, ky);
    return 0;
}

// ================================================================= Font (love.graphics)
// Une seule police de glyphes (aka_font, 8x8 avec accents), mais une TAILLE réglable par un facteur
// entier (love_gfx::set_font_scale). newFont(chemin, taille) : le chemin est ignoré (pas de police
// vectorielle) et journalisé une fois, comme les autres API absentes — le jeu reçoit quand même un
// objet Font utilisable, à la taille demandée.
constexpr const char* FONT_MT = "aka.Font";
struct GfxFont {
    int scale;
};

int font_getHeight(lua_State* L) {
    auto* f = (GfxFont*)luaL_checkudata(L, 1, FONT_MT);
    lua_pushinteger(L, 8 * f->scale);
    return 1;
}
int font_getWidth(lua_State* L) {
    auto* f = (GfxFont*)luaL_checkudata(L, 1, FONT_MT);
    const char* s = luaL_checkstring(L, 2);
    int saved = gfx::get_font_scale();
    gfx::set_font_scale(f->scale);
    lua_pushinteger(L, gfx::text_width(s));
    gfx::set_font_scale(saved);
    return 1;
}
int font_type(lua_State* L) {
    lua_pushstring(L, "Font");
    return 1;
}
const luaL_Reg font_methods[] = {
    {"getHeight", font_getHeight}, {"getWidth", font_getWidth}, {"type", font_type}, {nullptr, nullptr}};

GfxFont* push_font(lua_State* L, int scale) {
    auto* f = (GfxFont*)lua_newuserdata(L, sizeof(GfxFont));
    f->scale = scale;
    if (luaL_newmetatable(L, FONT_MT)) {
        lua_newtable(L);
        luaL_setfuncs(L, font_methods, 0);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
    return f;
}

// love.graphics.newFont([chemin,] taille) : arrondit à un multiple de 8 (granularité de la police
// embarquée), minimum 8. Sans argument : taille par défaut (8, comme au démarrage).
int g_newFont(lua_State* L) {
    int n = lua_gettop(L);
    double size = 8;
    if (n >= 2) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            hal::log("[AKA-Love] newFont : polices personnalisées non supportées, utilisation de la police "
                     "accentuée intégrée à la taille demandée\n");
        }
        size = luaL_checknumber(L, 2);
    } else if (n == 1 && lua_isnumber(L, 1)) {
        size = lua_tonumber(L, 1);
    }
    int scale = (int)(size / 8.0 + 0.5);
    if (scale < 1) scale = 1;
    push_font(L, scale);
    return 1;
}
int g_setFont(lua_State* L) {
    if (lua_gettop(L) == 0 || lua_isnil(L, 1)) {
        gfx::set_font_scale(1);
        return 0;
    }
    auto* f = (GfxFont*)luaL_checkudata(L, 1, FONT_MT);
    gfx::set_font_scale(f->scale);
    return 0;
}
int g_getFont(lua_State* L) {
    push_font(L, gfx::get_font_scale());
    return 1;
}

const luaL_Reg graphics_fns[] = {
    {"getWidth", g_getWidth},
    {"getHeight", g_getHeight},
    {"getDimensions", g_getDimensions},
    {"isActive", g_isActive},
    {"setColor", g_setColor},
    {"getColor", g_getColor},
    {"setBackgroundColor", g_setBackgroundColor},
    {"getBackgroundColor", g_getBackgroundColor},
    {"clear", g_clear},
    {"present", g_present},
    {"reset", g_reset},
    {"setLineWidth", g_setLineWidth},
    {"getLineWidth", g_getLineWidth},
    {"setLineStyle", g_noop},
    {"setLineJoin", g_noop},
    {"setDefaultFilter", g_noop},
    {"push", g_push},
    {"pop", g_pop},
    {"origin", g_origin},
    {"translate", g_translate},
    {"rotate", g_rotate},
    {"scale", g_scale},
    {"shear", g_shear},
    {"setScissor", g_setScissor},
    {"getScissor", g_getScissor},
    {"rectangle", g_rectangle},
    {"circle", g_circle},
    {"polygon", g_polygon},
    {"line", g_line},
    {"points", g_points},
    {"print", g_print},
    {"printf", g_printf},
    {"newImage", g_newImage},
    {"newQuad", g_newQuad},
    {"draw", g_draw},
    {"newFont", g_newFont},
    {"setFont", g_setFont},
    {"getFont", g_getFont},
    {nullptr, nullptr}};

}  // namespace

void open_graphics(lua_State* L) { set_module(L, "graphics", graphics_fns); }
