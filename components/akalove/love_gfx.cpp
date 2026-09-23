// love_gfx.cpp — rasterizer logiciel : matrice, alpha, scissor, polygones, lignes, texte.
// Règle de couverture unique (comme un GPU) : un pixel est dessiné si son CENTRE est dans la forme.
#include "love_gfx.h"

#include <math.h>
#include <string.h>

#include "aka_font/gb_text_render.h"   // font8x8_basic/extended, utf8_next, glyph_for, fallback_ascii
#include "hal.h"
#include "runtime.h"

namespace gfx {
namespace {

constexpr float PI = 3.14159265358979323846f;

struct Mat {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;   // x' = a·x + c·y + tx ; y' = b·x + d·y + ty
};
struct Vec2 {
    float x, y;
};
struct IRect {
    int x0, y0, x1, y1;   // [x0,x1) x [y0,y1)
};

Mat mul(const Mat& m, const Mat& n) {   // applique n, puis m
    Mat r;
    r.a = m.a * n.a + m.c * n.b;
    r.b = m.b * n.a + m.d * n.b;
    r.c = m.a * n.c + m.c * n.d;
    r.d = m.b * n.c + m.d * n.d;
    r.tx = m.a * n.tx + m.c * n.ty + m.tx;
    r.ty = m.b * n.tx + m.d * n.ty + m.ty;
    return r;
}
Vec2 xf(const Mat& m, float x, float y) { return {m.a * x + m.c * y + m.tx, m.b * x + m.d * y + m.ty}; }

struct Paint {
    uint16_t c;
    uint8_t a32;   // 0..32
};

struct State {
    float color[4] = {1, 1, 1, 1};
    float bg[4] = {0, 0, 0, 1};
    float line_w = 1.0f;
    Mat ctm, base;
    Mat stack[64];
    int sp = 0;
    bool sc_on = false;
    int sc[4] = {0, 0, 0, 0};     // scissor en coordonnées virtuelles (x, y, w, h)
    int ww = hal::SCREEN_W, wh = hal::SCREEN_H;   // taille de la « fenêtre » virtuelle
    ScaleMode mode = SCALE_NONE;
    IRect win{0, 0, hal::SCREEN_W, hal::SCREEN_H};   // zone physique de la fenêtre (hors bandes noires)
    IRect clip{0, 0, hal::SCREEN_W, hal::SCREEN_H};  // fenêtre ∩ scissor
    int font_scale = 1;   // love.graphics.setFont() : agrandissement entier de la police 8x8
};
State S;

inline uint8_t to8(float v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    return (uint8_t)(v * 255.0f + 0.5f);
}
inline int iceil(float v) { return (int)ceilf(v); }
inline int imin(int a, int b) { return a < b ? a : b; }
inline int imax(int a, int b) { return a > b ? a : b; }

Paint current_paint() {
    Paint p;
    p.c = hal::pack565(to8(S.color[0]), to8(S.color[1]), to8(S.color[2]));
    float a = S.color[3];
    p.a32 = (uint8_t)(a <= 0 ? 0 : (a >= 1 ? 32 : (int)(a * 32.0f + 0.5f)));
    return p;
}

// Mélange alpha sur 16 bits, identique en RGB565 et BGR565 (le vert reste au centre). a32 : 0..32
inline uint16_t blend565(uint16_t src, uint16_t dst, uint8_t a32) {
    uint32_t s = (src | ((uint32_t)src << 16)) & 0x07E0F81Fu;
    uint32_t d = (dst | ((uint32_t)dst << 16)) & 0x07E0F81Fu;
    uint32_t r = ((((s - d) * a32) >> 5) + d) & 0x07E0F81Fu;
    return (uint16_t)(r | (r >> 16));
}

void update_clip() {
    IRect c = S.win;
    if (S.sc_on) {
        float x0 = S.base.a * S.sc[0] + S.base.tx, y0 = S.base.d * S.sc[1] + S.base.ty;
        float x1 = S.base.a * (S.sc[0] + S.sc[2]) + S.base.tx, y1 = S.base.d * (S.sc[1] + S.sc[3]) + S.base.ty;
        c.x0 = imax(c.x0, (int)floorf(x0 + 0.5f));
        c.y0 = imax(c.y0, (int)floorf(y0 + 0.5f));
        c.x1 = imin(c.x1, (int)floorf(x1 + 0.5f));
        c.y1 = imin(c.y1, (int)floorf(y1 + 0.5f));
    }
    if (c.x1 < c.x0) c.x1 = c.x0;
    if (c.y1 < c.y0) c.y1 = c.y0;
    S.clip = c;
}

void hspan(int y, int xa, int xb, const Paint& p) {   // [xa, xb)
    if (p.a32 == 0 || y < S.clip.y0 || y >= S.clip.y1) return;
    xa = imax(xa, S.clip.x0);
    xb = imin(xb, S.clip.x1);
    if (xb <= xa) return;
    uint16_t* row = hal::framebuffer() + y * hal::SCREEN_W;
    if (p.a32 >= 32) {
        for (int x = xa; x < xb; ++x) row[x] = p.c;
    } else {
        for (int x = xa; x < xb; ++x) row[x] = blend565(p.c, row[x], p.a32);
    }
}

void fill_rect_px(float x0, float y0, float x1, float y1, const Paint& p) {
    int ix0 = iceil(x0 - 0.5f), ix1 = iceil(x1 - 0.5f);
    int iy0 = iceil(y0 - 0.5f), iy1 = iceil(y1 - 0.5f);
    iy0 = imax(iy0, S.clip.y0);
    iy1 = imin(iy1, S.clip.y1);
    for (int y = iy0; y < iy1; ++y) hspan(y, ix0, ix1, p);
}

// Remplissage par balayage, règle pair/impair
void fill_poly(const Vec2* v, int n, const Paint& p) {
    if (n < 3 || p.a32 == 0) return;
    if (n > 128) n = 128;
    float ymin = v[0].y, ymax = v[0].y;
    for (int i = 1; i < n; ++i) {
        if (v[i].y < ymin) ymin = v[i].y;
        if (v[i].y > ymax) ymax = v[i].y;
    }
    int y0 = imax(iceil(ymin - 0.5f), S.clip.y0);
    int y1 = imin(iceil(ymax - 0.5f), S.clip.y1);
    float xs[128];
    for (int y = y0; y < y1; ++y) {
        float yc = y + 0.5f;
        int cnt = 0;
        for (int i = 0; i < n; ++i) {
            const Vec2& a = v[i];
            const Vec2& b = v[(i + 1) % n];
            if ((a.y <= yc && yc < b.y) || (b.y <= yc && yc < a.y)) {
                float x = a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y);
                int k = cnt++;
                while (k > 0 && xs[k - 1] > x) {
                    xs[k] = xs[k - 1];
                    --k;
                }
                xs[k] = x;
            }
        }
        for (int k = 0; k + 1 < cnt; k += 2) hspan(y, iceil(xs[k] - 0.5f), iceil(xs[k + 1] - 0.5f), p);
    }
}

float ctm_scale() { return sqrtf(fabsf(S.ctm.a * S.ctm.d - S.ctm.b * S.ctm.c)); }

void rect_fill_local(float x, float y, float w, float h, const Mat& m, const Paint& p) {
    if (m.b == 0 && m.c == 0) {   // pas de rotation ni cisaillement : chemin rapide
        float x0 = m.a * x + m.tx, x1 = m.a * (x + w) + m.tx;
        float y0 = m.d * y + m.ty, y1 = m.d * (y + h) + m.ty;
        fill_rect_px(fminf(x0, x1), fminf(y0, y1), fmaxf(x0, x1), fmaxf(y0, y1), p);
    } else {
        Vec2 q[4] = {xf(m, x, y), xf(m, x + w, y), xf(m, x + w, y + h), xf(m, x, y + h)};
        fill_poly(q, 4, p);
    }
}

// Segment épais (quad). ext0/ext1 : prolongement aux extrémités (joints, boucles fermées).
void thick_segment(Vec2 a, Vec2 b, float hw, float ext0, float ext1, const Paint& p) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    dx /= len;
    dy /= len;
    a.x -= dx * ext0;
    a.y -= dy * ext0;
    b.x += dx * ext1;
    b.y += dy * ext1;
    float nx = -dy * hw, ny = dx * hw;
    Vec2 q[4] = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
    fill_poly(q, 4, p);
}

void polyline(const float* xy, int n, bool closed) {
    if (n < 1) return;
    Paint p = current_paint();
    float hw = 0.5f * S.line_w * ctm_scale();
    if (hw < 0.5f) hw = 0.5f;
    Vec2 pts[130];
    if (n > 130) n = 130;
    for (int i = 0; i < n; ++i) pts[i] = xf(S.ctm, xy[2 * i], xy[2 * i + 1]);
    if (n == 1) {
        fill_rect_px(pts[0].x - hw, pts[0].y - hw, pts[0].x + hw, pts[0].y + hw, p);
        return;
    }
    int segs = closed ? n : n - 1;
    for (int i = 0; i < segs; ++i) {
        float e0 = (closed || i > 0) ? hw : 0.0f;
        float e1 = (closed || i < segs - 1) ? hw : 0.0f;
        thick_segment(pts[i], pts[(i + 1) % n], hw, e0, e1, p);
    }
}

// Un codepoint dessine 1 glyphe (8x8, gb_text::glyph_for), ou un repli ASCII de 1 ou 2 caractères
// (gb_text::fallback_ascii, ex. œ -> "oe") quand aucun glyphe dédié n'existe. slots_for() renvoie le
// nombre de cases de 8px consommées ; slot_bits() la trame LSB-premier (bit 0 = colonne de gauche) de
// la case n. Utilisé à la fois par print() (dessin) et text_width()/printf() (mise en page), pour que
// largeur mesurée et largeur dessinée restent toujours identiques.
int slots_for(uint32_t cp, const uint8_t* slot_bits[2]) {
    if (const unsigned char* g = gb_text::glyph_for(cp)) {
        slot_bits[0] = (const uint8_t*)g;
        return 1;
    }
    char fb[2];
    int n = gb_text::fallback_ascii(cp, fb);
    for (int i = 0; i < n; ++i) slot_bits[i] = (const uint8_t*)font8x8_basic[(unsigned char)fb[i]];
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------------------------

void reset() {
    S.font_scale = 1;
    S.color[0] = S.color[1] = S.color[2] = S.color[3] = 1.0f;
    S.bg[0] = S.bg[1] = S.bg[2] = 0.0f;
    S.bg[3] = 1.0f;
    S.line_w = 1.0f;
    S.sp = 0;
    S.sc_on = false;
    S.ctm = S.base;
    update_clip();
}

void set_virtual(int W, int H, ScaleMode m) {
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    float sx = (float)hal::SCREEN_W / W, sy = (float)hal::SCREEN_H / H;
    if (m == SCALE_NONE) {
        sx = sy = 1.0f;
    } else if (m == SCALE_FIT) {
        sx = sy = fminf(sx, sy);
    }
    float ox = (hal::SCREEN_W - W * sx) * 0.5f, oy = (hal::SCREEN_H - H * sy) * 0.5f;
    S.base = Mat();
    S.base.a = sx;
    S.base.d = sy;
    S.base.tx = ox;
    S.base.ty = oy;
    S.ctm = S.base;
    S.sp = 0;
    S.ww = W;
    S.wh = H;
    S.mode = m;
    S.win.x0 = imax(0, (int)floorf(ox + 0.5f));
    S.win.y0 = imax(0, (int)floorf(oy + 0.5f));
    S.win.x1 = imin(hal::SCREEN_W, (int)floorf(ox + W * sx + 0.5f));
    S.win.y1 = imin(hal::SCREEN_H, (int)floorf(oy + H * sy + 0.5f));
    update_clip();
}

void init() {
    S = State();
    set_virtual(hal::SCREEN_W, hal::SCREEN_H, SCALE_NONE);
    reset();
}

void use_native() {
    S.sc_on = false;
    set_virtual(hal::SCREEN_W, hal::SCREEN_H, SCALE_NONE);
}

int win_w() { return S.ww; }
int win_h() { return S.wh; }
ScaleMode scale_mode() { return S.mode; }

void present() {
    hal::present();
    g_frame_t0 = hal::wall_millis();
}

void set_color(float r, float g, float b, float a) {
    S.color[0] = r;
    S.color[1] = g;
    S.color[2] = b;
    S.color[3] = a;
}
void get_color(float out[4]) { memcpy(out, S.color, sizeof S.color); }
void set_bg(float r, float g, float b, float a) {
    S.bg[0] = r;
    S.bg[1] = g;
    S.bg[2] = b;
    S.bg[3] = a;
}
void get_bg(float out[4]) { memcpy(out, S.bg, sizeof S.bg); }
void set_line_width(float w) { S.line_w = w; }
float get_line_width() { return S.line_w; }

void clear(float r, float g, float b, float a) {
    (void)a;   // pas de canal alpha dans le framebuffer
    uint16_t c = hal::pack565(to8(r), to8(g), to8(b));
    uint16_t* fb = hal::framebuffer();
    bool letterbox = S.win.x0 > 0 || S.win.y0 > 0 || S.win.x1 < hal::SCREEN_W || S.win.y1 < hal::SCREEN_H;
    if (letterbox && !S.sc_on) {   // bandes noires autour de la fenêtre virtuelle
        uint16_t black = hal::pack565(0, 0, 0);
        for (int i = 0; i < hal::SCREEN_W * hal::SCREEN_H; ++i) fb[i] = black;
    }
    for (int y = S.clip.y0; y < S.clip.y1; ++y)
        for (int x = S.clip.x0; x < S.clip.x1; ++x) fb[y * hal::SCREEN_W + x] = c;
}

bool push() {
    if (S.sp >= 64) return false;
    S.stack[S.sp++] = S.ctm;
    return true;
}
bool pop() {
    if (S.sp <= 0) return false;
    S.ctm = S.stack[--S.sp];
    return true;
}
void origin() { S.ctm = S.base; }
void translate(float dx, float dy) {
    Mat t;
    t.tx = dx;
    t.ty = dy;
    S.ctm = mul(S.ctm, t);
}
void rotate(float rad) {
    Mat t;
    t.a = cosf(rad);
    t.b = sinf(rad);
    t.c = -sinf(rad);
    t.d = cosf(rad);
    S.ctm = mul(S.ctm, t);
}
void scale(float sx, float sy) {
    Mat t;
    t.a = sx;
    t.d = sy;
    S.ctm = mul(S.ctm, t);
}
void shear(float kx, float ky) {
    Mat t;
    t.c = kx;
    t.b = ky;
    S.ctm = mul(S.ctm, t);
}

void set_scissor(int x, int y, int w, int h) {
    S.sc_on = true;
    S.sc[0] = x;
    S.sc[1] = y;
    S.sc[2] = w;
    S.sc[3] = h;
    update_clip();
}
void clear_scissor() {
    S.sc_on = false;
    update_clip();
}
bool get_scissor(int out[4]) {
    if (!S.sc_on) return false;
    for (int i = 0; i < 4; ++i) out[i] = S.sc[i];
    return true;
}

void rect(bool fill, float x, float y, float w, float h) {
    if (fill) {
        rect_fill_local(x, y, w, h, S.ctm, current_paint());
    } else {
        float pts[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
        polyline(pts, 4, true);
    }
}

void circle(bool fill, float cx, float cy, float r, int segments) {
    if (r <= 0) return;
    if (segments <= 0) {
        float rs = r * ctm_scale();
        segments = (int)(rs * 1.2f) + 8;
        if (segments < 12) segments = 12;
        if (segments > 120) segments = 120;
    }
    if (segments > 120) segments = 120;
    float pts[240];
    for (int i = 0; i < segments; ++i) {
        float t = 2.0f * PI * i / segments;
        pts[2 * i] = cx + r * cosf(t);
        pts[2 * i + 1] = cy + r * sinf(t);
    }
    if (fill) polygon(true, pts, segments);
    else polyline(pts, segments, true);
}

void polygon(bool fill, const float* xy, int n) {
    if (!fill) {
        polyline(xy, n, true);
        return;
    }
    Vec2 q[128];
    if (n > 128) n = 128;
    for (int i = 0; i < n; ++i) q[i] = xf(S.ctm, xy[2 * i], xy[2 * i + 1]);
    fill_poly(q, n, current_paint());
}

void line(const float* xy, int n) { polyline(xy, n, false); }

void points(const float* xy, int n) {
    Paint p = current_paint();
    float half = 0.5f * (S.line_w > 1 ? S.line_w : 1.0f) * ctm_scale();
    for (int i = 0; i < n; ++i) {
        Vec2 v = xf(S.ctm, xy[2 * i], xy[2 * i + 1]);
        fill_rect_px(v.x - half, v.y - half, v.x + half, v.y + half, p);
    }
}

int font_height() { return 8 * S.font_scale; }
void set_font_scale(int scale) { S.font_scale = scale > 0 ? scale : 1; }
int get_font_scale() { return S.font_scale; }

int text_width(const char* s) {
    int best = 0, cur = 0;
    const char* p = s;
    while (*p) {
        if (*p == '\n') {
            best = imax(best, cur);
            cur = 0;
            ++p;
            continue;
        }
        if (*p == '\t') {
            cur += 4;
            ++p;
            continue;
        }
        const uint8_t* slots[2];
        uint32_t cp = gb_text::utf8_next(p);
        cur += slots_for(cp, slots);
    }
    return imax(best, cur) * 8 * S.font_scale;
}

void print(const char* s, float x, float y, float rot, float sx, float sy, float ox, float oy) {
    Mat m = S.ctm;
    Mat t;
    t.tx = x;
    t.ty = y;
    m = mul(m, t);
    if (rot != 0) {
        Mat r;
        r.a = cosf(rot);
        r.b = sinf(rot);
        r.c = -sinf(rot);
        r.d = cosf(rot);
        m = mul(m, r);
    }
    if (sx != 1 || sy != 1) {
        Mat k;
        k.a = sx;
        k.d = sy;
        m = mul(m, k);
    }
    if (ox != 0 || oy != 0) {
        Mat o;
        o.tx = -ox;
        o.ty = -oy;
        m = mul(m, o);
    }
    Paint p = current_paint();
    int col = 0, row = 0;
    const char* c = s;
    while (*c) {
        if (*c == '\n') {
            col = 0;
            ++row;
            ++c;
            continue;
        }
        if (*c == '\t') {
            col += 4;
            ++c;
            continue;
        }
        const uint8_t* slots[2];
        uint32_t cp = gb_text::utf8_next(c);   // avance c d'1 à 3 octets
        int n = slots_for(cp, slots);
        int fs = S.font_scale;
        for (int i = 0; i < n; ++i, ++col) {
            const uint8_t* g = slots[i];
            for (int gy = 0; gy < 8; ++gy) {
                uint8_t bits = g[gy];   // LSB = colonne de gauche (convention aka_font/font8x8)
                int gx = 0;
                while (gx < 8) {   // regroupe les pixels consécutifs en un rectangle (agrandi x fs)
                    if (bits & (1u << gx)) {
                        int start = gx;
                        while (gx < 8 && (bits & (1u << gx))) ++gx;
                        rect_fill_local((float)(col * 8 + start) * fs, (float)(row * 8 + gy) * fs,
                                        (float)(gx - start) * fs, (float)fs, m, p);
                    } else {
                        ++gx;
                    }
                }
            }
        }
    }
}

void draw_image(const ImageView& img, float qx, float qy, float qw, float qh, float x, float y, float r,
                float sx, float sy, float ox, float oy, float kx, float ky) {
    if (img.w <= 0 || img.h <= 0 || qw <= 0 || qh <= 0) return;
    // Bornes du Quad à l'intérieur de la texture source (un Quad légèrement hors image est toléré,
    // comme sous Love : les texels hors bornes sont simplement absents).
    if (qx < 0) qx = 0;
    if (qy < 0) qy = 0;
    if (qx + qw > img.w) qw = img.w - qx;
    if (qy + qh > img.h) qh = img.h - qy;
    if (qw <= 0 || qh <= 0) return;

    // Matrice locale->écran : ctm * translate(x,y) * rotate(r) * scale(sx,sy) * shear(kx,ky) *
    // translate(-ox,-oy) — même ordre que Love (et que print() plus haut). L'espace local va de
    // (0,0) à (qw,qh) : le Quad, pas l'image entière.
    Mat m = S.ctm;
    Mat t;
    t.tx = x;
    t.ty = y;
    m = mul(m, t);
    if (r != 0) {
        Mat rot;
        rot.a = cosf(r);
        rot.b = sinf(r);
        rot.c = -sinf(r);
        rot.d = cosf(r);
        m = mul(m, rot);
    }
    if (sx != 1 || sy != 1) {
        Mat k;
        k.a = sx;
        k.d = sy;
        m = mul(m, k);
    }
    if (kx != 0 || ky != 0) {
        Mat sh;
        sh.c = kx;
        sh.b = ky;
        m = mul(m, sh);
    }
    if (ox != 0 || oy != 0) {
        Mat o;
        o.tx = -ox;
        o.ty = -oy;
        m = mul(m, o);
    }

    // Inverse de la partie 2x3 de m (même convention que xf() : x' = a·x + c·y + tx)
    float det = m.a * m.d - m.b * m.c;
    if (fabsf(det) < 1e-9f) return;   // matrice dégénérée (échelle nulle) : rien à dessiner
    float ia = m.d / det, ib = -m.b / det, ic = -m.c / det, id = m.a / det;
    float itx = -(ia * m.tx + ic * m.ty), ity = -(ib * m.tx + id * m.ty);

    // Boîte englobante à l'écran (les 4 coins du Quad transformé), rognée par le scissor courant
    Vec2 c0 = xf(m, 0, 0), c1 = xf(m, qw, 0), c2 = xf(m, qw, qh), c3 = xf(m, 0, qh);
    float minx = fminf(fminf(c0.x, c1.x), fminf(c2.x, c3.x));
    float maxx = fmaxf(fmaxf(c0.x, c1.x), fmaxf(c2.x, c3.x));
    float miny = fminf(fminf(c0.y, c1.y), fminf(c2.y, c3.y));
    float maxy = fmaxf(fmaxf(c0.y, c1.y), fmaxf(c2.y, c3.y));
    int x0 = imax((int)floorf(minx), S.clip.x0), x1 = imin((int)ceilf(maxx), S.clip.x1);
    int y0 = imax((int)floorf(miny), S.clip.y0), y1 = imin((int)ceilf(maxy), S.clip.y1);
    if (x1 <= x0 || y1 <= y0) return;

    uint8_t tr = to8(S.color[0]), tg = to8(S.color[1]), tb = to8(S.color[2]);
    float talpha = S.color[3];
    uint16_t* fb = hal::framebuffer();

    for (int py = y0; py < y1; ++py) {
        float sy0 = py + 0.5f;
        for (int px = x0; px < x1; ++px) {
            float sx0 = px + 0.5f;
            float lx = ia * sx0 + ic * sy0 + itx, ly = ib * sx0 + id * sy0 + ity;
            if (lx < 0 || lx >= qw || ly < 0 || ly >= qh) continue;
            int tx_ = (int)(qx + lx), ty_ = (int)(qy + ly);
            if (tx_ >= img.w) tx_ = img.w - 1;   // garde-fou (arrondi flottant à la limite exacte)
            if (ty_ >= img.h) ty_ = img.h - 1;
            const unsigned char* texel = img.rgba + ((size_t)ty_ * img.w + tx_) * 4;
            uint8_t a8 = texel[3];
            if (a8 == 0) continue;
            uint8_t a32 = (uint8_t)((a8 * (int)(talpha * 32.0f + 0.5f) + 127) / 255);
            if (a32 == 0) continue;
            uint16_t src = hal::pack565((uint8_t)((texel[0] * tr + 127) / 255), (uint8_t)((texel[1] * tg + 127) / 255),
                                        (uint8_t)((texel[2] * tb + 127) / 255));
            uint16_t& dst = fb[py * hal::SCREEN_W + px];
            dst = a32 >= 32 ? src : blend565(src, dst, a32);
        }
    }
}

}  // namespace gfx
