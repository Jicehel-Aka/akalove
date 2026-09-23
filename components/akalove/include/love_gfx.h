// love_gfx.h — rasterizer logiciel fidèle à l'état graphique de Love2D.
#pragma once
#include <stddef.h>

namespace gfx {

enum ScaleMode { SCALE_FIT, SCALE_STRETCH, SCALE_NONE };

void init();                                  // démarrage d'un jeu : état par défaut, fenêtre = écran
void reset();                                 // love.graphics.reset() : état graphique par défaut
void set_virtual(int W, int H, ScaleMode m);  // résolution « fenêtre » du jeu (love.window)
void use_native();                            // 320x240 sans mise à l'échelle (écran d'erreur)
int win_w();
int win_h();
ScaleMode scale_mode();

void present();                               // hal::present() + chien de garde

// Couleur, fond, style
void set_color(float r, float g, float b, float a);
void get_color(float out[4]);
void set_bg(float r, float g, float b, float a);
void get_bg(float out[4]);
void set_line_width(float w);
float get_line_width();
void clear(float r, float g, float b, float a);

// Transformations
bool push();
bool pop();
void origin();
void translate(float dx, float dy);
void rotate(float rad);
void scale(float sx, float sy);
void shear(float kx, float ky);

// Scissor (coordonnées virtuelles)
void set_scissor(int x, int y, int w, int h);
void clear_scissor();
bool get_scissor(int out[4]);

// Primitives (coordonnées virtuelles, soumises à la matrice courante)
void rect(bool fill, float x, float y, float w, float h);
void circle(bool fill, float cx, float cy, float r, int segments);
void polygon(bool fill, const float* xy, int n);        // n sommets
void line(const float* xy, int n);                      // polyligne de n points
void points(const float* xy, int n);
void print(const char* s, float x, float y, float rot, float sx, float sy, float ox, float oy);
int text_width(const char* s);                          // largeur de la plus longue ligne, police courante
int font_height();                                       // hauteur d'une ligne, police courante

// Police : un seul jeu de glyphes (aka_font, 8x8, accents FR/ES/DE), mais une TAILLE réglable, comme
// des love.graphics.newFont(taille) : chaque glyphe est agrandi par un facteur entier (échantillonnage
// au plus proche, dans l'esprit du reste du rendu) — pas de police vectorielle, mais aucune taille de
// texte n'est bloquée. set_font_scale(0) revient à la taille par défaut (1).
void set_font_scale(int scale);
int get_font_scale();

// Image : décodée par image_decode.h (RGBA), pixels lus tels quels. draw_image() reproduit
// love.graphics.draw(image [, quad], x, y, r, sx, sy, ox, oy, kx, ky) : la matrice courante (ctm),
// la couleur/alpha courants (qui multiplient la texture) et le scissor s'appliquent comme pour les
// autres primitives. (qx,qy,qw,qh) : rectangle source dans l'image (le Quad ; par défaut l'image
// entière). Échantillonnage au plus proche (pas de filtrage), cohérent avec setDefaultFilter ignoré
// ailleurs dans le runtime.
struct ImageView {
    const unsigned char* rgba;   // w*h*4 octets
    int w, h;
};
void draw_image(const ImageView& img, float qx, float qy, float qw, float qh, float x, float y, float r,
                float sx, float sy, float ox, float oy, float kx, float ky);

}  // namespace gfx
