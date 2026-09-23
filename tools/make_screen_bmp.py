#!/usr/bin/env python3
"""Fabrique screen.bmp (image du loader AKA) à partir de Picture.png.

Sortie : BMP 24 bits 320x240, le même format que les captures d'écran de la console (MENU long).
Par défaut l'image est réduite pour tenir entièrement dans 320x240 (bandes noires si le rapport diffère) ;
--crop la recadre au centre pour remplir tout l'écran.

Usage : make_screen_bmp.py Picture.png [screen.bmp] [--crop]      (nécessite Pillow : pip install pillow)
"""
import sys

from PIL import Image

W, H = 320, 240


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    crop = '--crop' in sys.argv
    if not args or len(args) > 2:
        print(__doc__)
        return 2
    src, dst = args[0], (args[1] if len(args) > 1 else 'screen.bmp')
    im = Image.open(src).convert('RGBA')
    bg = Image.new('RGBA', im.size, (0, 0, 0, 255))
    im = Image.alpha_composite(bg, im).convert('RGB')          # transparence fondue sur noir, comme le splash
    s = max(W / im.width, H / im.height) if crop else min(W / im.width, H / im.height)
    size = (max(1, round(im.width * s)), max(1, round(im.height * s)))
    im = im.resize(size, Image.LANCZOS)
    out = Image.new('RGB', (W, H), (0, 0, 0))
    out.paste(im, ((W - size[0]) // 2, (H - size[1]) // 2))     # centrage (recadrage si --crop : coller hors cadre)
    out.save(dst, 'BMP')
    print('écrit %s (%dx%d, 24 bits)' % (dst, W, H))
    return 0


if __name__ == '__main__':
    sys.exit(main())
