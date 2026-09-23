#!/usr/bin/env python3
"""Génère components/akalove/font6x12.h (police bitmap 6x12, ASCII 32..126)
à partir de DejaVu Sans Mono (licence libre Bitstream Vera/DejaVu).

C'est une police PROVISOIRE de la phase P1. Elle sera remplacée par la police 8x8 accentuée de
gb_graphics (gen_font.py du projet AKA) puis par Vera via stb_truetype (voir dossier §9.4).
"""
import sys
from PIL import Image, ImageDraw, ImageFont

TTF = '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf'
W, H, SIZE, THRESH = 6, 12, 10, 80

font = ImageFont.truetype(TTF, SIZE, layout_engine=ImageFont.Layout.BASIC)
rows = []
for code in range(32, 127):
    img = Image.new('L', (W, H), 0)
    ImageDraw.Draw(img).text((0, -2), chr(code), fill=255, font=font)
    px = img.load()
    glyph = []
    for y in range(H):
        bits = 0
        for x in range(W):
            if px[x, y] >= THRESH:
                bits |= 1 << (W - 1 - x)          # bit 5 = colonne 0
        glyph.append(bits)
    rows.append(glyph)

out = sys.argv[1] if len(sys.argv) > 1 else 'components/akalove/font6x12.h'
with open(out, 'w') as f:
    f.write('// Généré par tools/gen_font.py — NE PAS ÉDITER\n#pragma once\n#include <stdint.h>\n')
    f.write('namespace font6x12 {\nconstexpr int W = %d, H = %d, FIRST = 32, LAST = 126;\n' % (W, H))
    f.write('// une ligne = un octet, bit 5 = colonne de gauche\n')
    f.write('static const uint8_t data[%d][%d] = {\n' % (len(rows), H))
    for code, g in zip(range(32, 127), rows):
        name = chr(code) if chr(code) not in '\\/' else 'car.%d' % code
        f.write('    {%s},  // %s\n' % (','.join('0x%02X' % b for b in g), name))
    f.write('};\n}\n')
print('écrit', out)
