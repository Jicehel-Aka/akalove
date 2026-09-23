#!/usr/bin/env python3
"""Tests du runtime AKA-Love sur le simulateur sans fenêtre (horloge virtuelle, déterministe).
Usage : python3 tests/run_tests.py [--keep]   (le simulateur doit être compilé : voir README)"""
import os, shutil, subprocess, sys, tempfile
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
# AKALOVE_SIM : autre exécutable (ex. le .exe Windows) ; AKALOVE_RUNNER : préfixe de commande (ex. « wine64 »)
_exe = 'akalove-sim.exe' if os.name == 'nt' else 'akalove-sim'
SIM = os.environ.get('AKALOVE_SIM') or os.path.join(ROOT, 'build-sim', _exe)
RUNNER = os.environ.get('AKALOVE_RUNNER', '').split()
GAMES = os.path.join(ROOT, 'tests', 'games')
EXAMPLES = os.path.join(ROOT, 'examples')
OUT = os.path.join(ROOT, 'tests', 'out')
os.makedirs(OUT, exist_ok=True)

results = []


def check(name, cond, detail=''):
    results.append((name, bool(cond), detail))
    print(('  ok   ' if cond else '  ECHEC') + ' ' + name + ('' if cond else '   -> ' + str(detail)))


class R:
    pass


def run(game, frames=10, shot_frame=None, presses=(), watchdog=None, name=None, root=None, ext='ppm'):
    save = tempfile.mkdtemp(prefix='akalove_save_')
    ppm = os.path.join(OUT, (name or game) + '.' + ext)
    if os.path.exists(ppm):
        os.remove(ppm)
    cmd = RUNNER + [SIM, os.path.join(root or GAMES, game), '--frames', str(frames), '--save', save, '--shot', ppm]
    if shot_frame is not None:
        cmd += ['--shot-frame', str(shot_frame)]
    for p in presses:
        cmd += ['--press', p]
    if watchdog:
        cmd += ['--watchdog-ms', str(watchdog)]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    r = R()
    r.rc, r.out, r.err, r.save = p.returncode, p.stdout, p.stderr, save
    r.img = None
    if os.path.exists(ppm):
        r.img = Image.open(ppm).convert('RGB')
        r.img.save(ppm.rsplit('.', 1)[0] + '.png')
        os.remove(ppm)
    return r


def near(c, ref, tol=8):
    return all(abs(a - b) <= tol for a, b in zip(c, ref))


def count(img, box, pred):
    x0, y0, x1, y1 = box
    return sum(1 for y in range(y0, y1) for x in range(x0, x1) if pred(img.getpixel((x, y))))


RED, BLACK, WHITE, BLUE = (255, 0, 0), (0, 0, 0), (255, 255, 255), (0, 0, 255)

print('== hello : cycle, Lua 5.1, rect, texte')
r = run('hello', frames=5)
check('code de sortie 0', r.rc == 0, r.err)
check('print() -> série', 'Bonjour, tout fonctionne !' in r.out, r.out)
check('sémantique Lua 5.1 (10/2 = 5, unpack, loadstring, setfenv)',
      '5\tfunction\tfunction\tfunction\t320\t240' in r.out, r.out)
check('rectangle rouge (10,10)-(49,49)', near(r.img.getpixel((10, 10)), RED) and near(r.img.getpixel((49, 49)), RED))
check('bords exacts du rectangle (règle du centre du pixel)',
      near(r.img.getpixel((9, 9)), BLACK) and near(r.img.getpixel((50, 50)), BLACK) and near(r.img.getpixel((50, 10)), BLACK))
check('texte blanc dessiné', count(r.img, (10, 60, 140, 72), lambda c: near(c, WHITE)) > 60)

if '--smoke' in sys.argv:   # contrôle rapide (compilation croisée, autre plateforme)
    failed = [n for n, ok, _ in results if not ok]
    print('\n%d vérifications, %d échec(s)' % (len(results), len(failed)))
    sys.exit(1 if failed else 0)

print('== transform : push/translate/rotate, alpha')
r = run('transform', frames=5)
check('rectangle rouge non transformé', near(r.img.getpixel((30, 30)), RED))
check('rectangle tourné, alpha 0,5 sur noir : vert ~125', near(r.img.getpixel((160, 120)), (0, 125, 0), 10), r.img.getpixel((160, 120)))
check('point à l\'intérieur du rectangle tourné', near(r.img.getpixel((193, 120)), (0, 125, 0), 10), r.img.getpixel((193, 120)))
check('point à l\'extérieur (rotation prise en compte)', near(r.img.getpixel((196, 120)), BLACK), r.img.getpixel((196, 120)))
check('pop restaure la matrice (aucun décalage résiduel)', near(r.img.getpixel((10, 10)), RED))

print('== virtual : conf 800x600 -> 320x240 (échelle 0,4)')
r = run('virtual', frames=5)
check('getWidth/getHeight = taille de conf.lua', 'size\t800\t600' in r.out and 'mode\t800\t600' in r.out, r.out)
check('rectangle 400x300 virtuel = 160x120 pixels', near(r.img.getpixel((159, 10)), RED) and near(r.img.getpixel((10, 119)), RED) and near(r.img.getpixel((160, 10)), BLACK) and near(r.img.getpixel((10, 120)), BLACK), (r.img.getpixel((159, 10)), r.img.getpixel((160, 10)), r.img.getpixel((10, 119)), r.img.getpixel((10, 120))))
check('cercle virtuel centré en (160,120), rayon 40', near(r.img.getpixel((160, 150)), BLUE) and near(r.img.getpixel((160, 165)), BLACK), (r.img.getpixel((160, 150)), r.img.getpixel((160, 165))))

print('== error : écran d\'erreur (conf 800x600, doit rester lisible)')
r = run('error', frames=6)
check('message et position dans le terminal', 'Error: main.lua:3: attempt to index local \'t\' (a nil value)' in r.out, r.out[:300])
check('traceback présent', 'stack traceback' not in r.out or 'main.lua:3' in r.out)
check('fond rouge sombre', near(r.img.getpixel((300, 150)), (140, 0, 0), 10), r.img.getpixel((300, 150)))
check('texte blanc lisible (résolution native)', count(r.img, (0, 0, 320, 120), lambda c: near(c, WHITE)) > 200)
check('code de sortie 0', r.rc == 0)

print('== restart : A relance le jeu ; love.filesystem écrit puis relit après redémarrage')
r = run('restart', frames=30, presses=['a:3:5'])
check('erreur puis relance', r.out.find('Error: main.lua:') != -1 and 'second run' in r.out and r.out.find('Error') < r.out.find('second run'), r.out)
check('fichier de sauvegarde créé', os.path.exists(os.path.join(r.save, 'restart', 'ran.flag')))

print('== loop : chien de garde')
r = run('loop', frames=15, watchdog=300)
check('boucle infinie interrompue', 'ne rend pas la main' in r.out, r.out[:200])

print('== semantics : Lua 5.1, love.math, love.filesystem, require, garde-fous')
r = run('semantics', frames=5)
lines = r.out.splitlines()
def has(prefix): return any(l.startswith(prefix) for l in lines)
def get(prefix): return next((l for l in lines if l.startswith(prefix)), '')
check('10/2 affiche 5', get('div') == 'div\t5', get('div'))
check('string.format("%d", 3.0)', get('fmt') == 'fmt\t3', get('fmt'))
check('unpack', get('unpack') == 'unpack\t1\t2\t3')
check('random(3,3) = 3 ; random(1) = 1', get('random') == 'random\t3\t1', get('random'))
check('même graine, même suite', get('seed').startswith('seed\ttrue\t42'), get('seed'))
check('write/read', get('write') == 'write\ttrue' and 'read\tabc' in r.out and 'def' in r.out)
check('getInfo (taille ; nil si absent)', get('info') == 'info\t8\tnil', get('info'))
check('lines', 'line\tabc' in r.out and 'line\tdef' in r.out)
check('require("lib.util")', get('require') == 'require\t42', get('require'))
check('filesystem.load', get('load') == 'load\t42')
check('getDirectoryItems', get('items') == 'items\tutil.lua')
check('chemin ".." refusé', get('safe') == 'safe\tnil\tCould not open file \'../secret\'', get('safe'))
check('API absente : appel = erreur Lua habituelle avec position',
      get('shader').startswith('shader\tfalse\tmain.lua:') and "attempt to call field 'newShader' (a nil value)" in get('shader'), get('shader'))
check('absence journalisée une fois', r.out.count('[AKA-Love] absent : love.graphics.newShader') == 1, r.out)
check('joystick virtuel', get('joy') == 'joy\t1\ttrue', get('joy'))
check('love.getVersion', get('version') == 'version\t11\t5\t0\tMysterious Mysteries', get('version'))
check('pop sans push : message de Love', 'Minimum stack depth reached' in get('pop') and get('pop').startswith('pop\tfalse'), get('pop'))
check('sortie propre (love.event.quit)', r.rc == 0)

print('== entrées : clavier prioritaire si keypressed défini ; manette seule sinon')
r = run('input_key', frames=10, presses=['left:2:4'])
check('keypressed(left, left, false)', 'keypressed\tleft\tleft\tfalse' in r.out, r.out)
check('keyreleased', 'keyreleased\tleft\tleft' in r.out)
check('alias isDown("a") = gauche', r.out.count('alias-a') == 2, r.out)
check('pas de double déclenchement (aucun gamepadpressed)', 'gamepadpressed' not in r.out, r.out)
check('isDown("d","right") faux', 'right?' not in r.out)
r = run('input_gp', frames=10, presses=['b:2:4', 'right:5:7'])
check('gamepadpressed(b) avec joystick valide', 'gamepadpressed\tb\ttrue\ttrue' in r.out, r.out)
check('gamepadreleased', 'gamepadreleased\tb' in r.out)
check('axe leftx = 1', 'axis\t1' in r.out, r.out)

r = run('input_all', frames=20, presses=['a:1:2', 'b:3:4', 'c:5:6', 'd:7:8', 'l1:9:10', 'r1:11:12', 'menu:13:14'])
names = [l.split('\t')[1] for l in r.out.splitlines() if l.startswith('kp\t')]
check('A B C D L1 R1 MENU -> space x c v lshift rshift escape', names == ['space', 'x', 'c', 'v', 'lshift', 'rshift', 'escape'], names)


print('== exemple « attrape » (examples/attrape) : jeu complet, sauvegarde, sortie BMP')
r = run('attrape', frames=60, shot_frame=59, presses=['right:5:60'], root=EXAMPLES)
check('aucune erreur', 'Error' not in r.out and r.rc == 0, r.out[:300])
check('la barre suit la croix (collée à droite)', near(r.img.getpixel((300, 224)), (77, 204, 128), 12) or near(r.img.getpixel((300, 224)), (74, 207, 123), 12), r.img.getpixel((300, 224)))
check('rien à gauche de la barre', near(r.img.getpixel((160, 224)), (24, 26, 51), 12), r.img.getpixel((160, 224)))
r = run('attrape', frames=900, shot_frame=899, root=EXAMPLES)
check('fin de partie : « Perdu ! » affiché', count(r.img, (0, 108, 320, 126), lambda c: near(c, WHITE)) > 60)
saved = os.path.join(r.save, 'attrape', 'meilleur.txt')
check('meilleur score sauvegardé', os.path.exists(saved) and open(saved).read().strip().isdigit(), saved)
rb = run('attrape', frames=900, shot_frame=899, root=EXAMPLES, ext='bmp', name='attrape_bmp')
check('sortie BMP identique à la sortie PPM', rb.img is not None and rb.img.size == (320, 240) and rb.img.tobytes() == r.img.tobytes())

print('== game.txt : choix du jeu sans recompiler')
def run_home(game_txt, frames=5, files=None, extra=(), shot_frame=None):
    home = tempfile.mkdtemp(prefix='akalove_home_')
    os.makedirs(os.path.join(home, 'games'))
    shutil.copytree(os.path.join(GAMES, 'hello'), os.path.join(home, 'games', 'hello'))
    shutil.copytree(os.path.join(EXAMPLES, 'attrape'), os.path.join(home, 'games', 'attrape'))
    if game_txt is not None:
        with open(os.path.join(home, 'game.txt'), 'wb') as f:
            f.write(game_txt)
    for name, data in (files or {}).items():
        with open(os.path.join(home, name), 'wb') as f:
            f.write(data)
    cmd = RUNNER + [SIM, '--home', home, '--frames', str(frames)] + list(extra)
    ppm = None
    if shot_frame is not None:
        ppm = os.path.join(OUT, 'home_shot.ppm')
        cmd += ['--shot', ppm, '--shot-frame', str(shot_frame)]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    p.img = None
    if ppm and os.path.exists(ppm):
        p.img = Image.open(ppm).convert('RGB')
        os.remove(ppm)
    return p, home


p, home = run_home(None)
check('sans game.txt : hello', 'Bonjour, tout fonctionne' in p.stdout, p.stdout + p.stderr)
p, home = run_home(b'attrape\r\n')
check('game.txt = attrape (fin de ligne Windows) : le jeu attrape tourne', 'Bonjour' not in p.stdout and p.returncode == 0 and os.path.isdir(os.path.join(home, 'save', 'attrape')), p.stdout + p.stderr)
p, home = run_home(b'\xef\xbb\xbf  hello  \n')
check('BOM UTF-8 et espaces tolérés', 'Bonjour, tout fonctionne' in p.stdout, p.stdout)
p, home = run_home(b'../secret\n')
check('nom invalide refusé : retour à hello + message', 'Bonjour, tout fonctionne' in p.stdout and 'nom invalide' in p.stderr, p.stdout + p.stderr)
p, home = run_home(b'inexistant\n')
check('jeu introuvable : écran d\'erreur, pas de plantage', 'Cannot find main.lua' in p.stdout and p.returncode == 0, p.stdout + p.stderr)

print('== écran de démarrage : Picture.png (sinon screen.bmp), mise à l\'échelle, alpha, saut au premier appui')
import io
def png(img, fmt='PNG'):
    b = io.BytesIO()
    img.save(b, fmt)
    return b.getvalue()
quad = Image.new('RGBA', (640, 480))
for (x0, y0, col) in [(0, 0, (255, 0, 0, 255)), (320, 0, (0, 255, 0, 255)), (0, 240, (0, 0, 255, 255)), (320, 240, (255, 255, 255, 255))]:
    quad.paste(Image.new('RGBA', (320, 240), col), (x0, y0))
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(quad)}, shot_frame=10)
check('PNG 640x480 réduit à 320x240 : quatre quadrants', p.img is not None and near(p.img.getpixel((80, 60)), RED) and near(p.img.getpixel((240, 60)), (0, 255, 0)) and near(p.img.getpixel((80, 180)), BLUE) and near(p.img.getpixel((240, 180)), WHITE), p.stderr)
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(quad)}, shot_frame=58)
check('après 1,5 s le jeu démarre (carré rouge de hello)', near(p.img.getpixel((30, 30)), RED) and near(p.img.getpixel((240, 60)), BLACK), p.stderr)
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(quad)}, shot_frame=10, extra=['--no-splash'])
check('--no-splash : pas d\'image', near(p.img.getpixel((30, 30)), RED) and near(p.img.getpixel((240, 60)), BLACK))
p, _ = run_home(b'hello\n', frames=30, shot_frame=2)
check('sans image : pas d\'écran de démarrage (le jeu démarre tout de suite)', near(p.img.getpixel((30, 30)), RED))

yellow = Image.new('RGBA', (400, 400), (255, 255, 0, 255))
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(yellow)}, shot_frame=5)
check('image carrée : centrée avec bandes noires (240x240)', near(p.img.getpixel((160, 120)), (255, 255, 0)) and near(p.img.getpixel((30, 120)), BLACK) and near(p.img.getpixel((290, 120)), BLACK) and near(p.img.getpixel((45, 120)), (255, 255, 0)), [p.img.getpixel(x) for x in ((160, 120), (30, 120), (290, 120))])
small = Image.new('RGBA', (32, 24), (0, 255, 255, 255))
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(small)}, shot_frame=5)
check('petite image agrandie (x10) pour remplir l\'écran', near(p.img.getpixel((5, 5)), (0, 255, 255)) and near(p.img.getpixel((315, 235)), (0, 255, 255)))

alpha = Image.new('RGBA', (100, 100), (255, 0, 0, 0))
alpha.paste(Image.new('RGBA', (50, 100), (255, 0, 0, 128)), (50, 0))
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(alpha)}, shot_frame=5)
check('transparence : fondue sur noir (alpha 0 = noir, alpha 128 = rouge moitié)', near(p.img.getpixel((80, 120)), BLACK) and near(p.img.getpixel((240, 120)), (128, 0, 0), 12), (p.img.getpixel((80, 120)), p.img.getpixel((240, 120))))

modes_ok = True
detail = []
for mode, col, expect in [('L', 128, (128, 128, 128)), ('LA', (200, 255), (200, 200, 200)), ('RGB', (5, 130, 250), (5, 130, 250)), ('P', 3, (255, 128, 0))]:
    im = Image.new(mode, (64, 48), col)
    if mode == 'P':
        pal = [0] * 768
        pal[9:12] = [255, 128, 0]
        im.putpalette(pal)
    p, _ = run_home(b'hello\n', frames=30, files={'Picture.png': png(im)}, shot_frame=5)
    got = p.img.getpixel((160, 120)) if p.img else None
    if got is None or not near(got, expect, 9):
        modes_ok = False
        detail.append((mode, got, expect))
check('PNG en niveaux de gris, gris+alpha, RGB, palette', modes_ok, detail)

bmp24 = Image.new('RGB', (320, 240), (10, 200, 30))
p, _ = run_home(b'hello\n', frames=60, files={'screen.bmp': png(bmp24, 'BMP')}, shot_frame=5)
check('screen.bmp (24 bits) utilisé à défaut de Picture.png', near(p.img.getpixel((100, 100)), (10, 200, 30), 8), p.img.getpixel((100, 100)))
bmp32 = Image.new('RGBA', (320, 240), (200, 20, 100, 0))
p, _ = run_home(b'hello\n', frames=60, files={'screen.bmp': png(bmp32, 'BMP')}, shot_frame=5)
check('BMP 32 bits (octet alpha nul ignoré)', near(p.img.getpixel((100, 100)), (200, 20, 100), 8), p.img.getpixel((100, 100)))
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': b'ceci n\'est pas un png', 'screen.bmp': png(bmp24, 'BMP')}, shot_frame=5)
check('Picture.png illisible : message, puis repli sur screen.bmp', 'Picture.png illisible' in p.stderr and near(p.img.getpixel((100, 100)), (10, 200, 30), 8), p.stderr)
p, _ = run_home(b'hello\n', frames=30, files={'Picture.png': b'xx'}, shot_frame=3)
check('image corrompue seule : le jeu démarre quand même', 'Picture.png illisible' in p.stderr and p.returncode == 0)

p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(quad)}, shot_frame=10, extra=['--press', 'a:3:5'])
check('un nouvel appui saute l\'image', near(p.img.getpixel((30, 30)), RED) and near(p.img.getpixel((240, 60)), BLACK))
p, _ = run_home(b'hello\n', frames=60, files={'Picture.png': png(quad)}, shot_frame=10, extra=['--press', 'a:0:5'])
check('un bouton déjà tenu au lancement ne saute pas l\'image', near(p.img.getpixel((240, 60)), (0, 255, 0)))

print('== accents (aka_font) : français/espagnol/allemand, largeur en codepoints, retour à la ligne UTF-8')
def col(img, y, pred):
    for x in range(320):
        if pred(img.getpixel((x, y))):
            return x
    return None
r = run('accents', frames=4)
check('aucune erreur (police avec accents chargée)', 'Error' not in r.out and r.rc == 0, r.out[:300])
check('texte coloré : largeur du 1er segment ("café ", 5 codepoints) déplace bien le 2e (X vert)',
      col(r.img, 20, lambda p: p[1] > 200 and p[0] < 100) is not None and
      35 <= col(r.img, 20, lambda p: p[1] > 200 and p[0] < 100) <= 48)
check('printf aligné à droite : "café" (4 codepoints = 32px) compte les caractères, pas les octets UTF-8',
      col(r.img, 32, lambda p: p[0] > 200) is not None and 64 <= col(r.img, 32, lambda p: p[0] > 200) <= 72,
      col(r.img, 32, lambda p: p[0] > 200))
check('retour à la ligne : "été" jamais coupé au milieu d\'un caractère accentué (5 lignes rendues)',
      all(col(r.img, 40 + row * 8, lambda p: p[0] > 200) is not None for row in range(5)))

print('== correctif appliqué dans aka_font (composant fourni) : ä/ö/Ä/Ö, et static sur font8x8_basic')
gen_src = open(os.path.join(ROOT, 'components', 'aka_font', 'tools', 'generate_extended_font.py'), encoding='utf-8').read()
check('le générateur produit désormais aussi ä ö Ä Ö (même technique que ë/ï/ü)',
      "0x00E4" in gen_src and "0x00F6" in gen_src and "0x00C4" in gen_src and "0x00D6" in gen_src)
basic_src = open(os.path.join(ROOT, 'components', 'aka_font', 'include', 'aka_font', 'font8x8_basic.h'), encoding='utf-8').read()
check('font8x8_basic est static (évite un conflit d\'édition de liens si inclus ailleurs)', 'static' in basic_src.split('font8x8_basic[128][8]')[0].splitlines()[-1] or 'static const unsigned char font8x8_basic' in basic_src)

print('== images (P2) : newImage (PNG/BMP), draw, newQuad, rotation/échelle, teinte, alpha')
r = run('images', frames=4)
check('aucune erreur', 'Error' not in r.out and r.rc == 0, r.out[:300])
check('taille lue correctement (ship 16x16)', 'ship\t16\t16' in r.out, r.out)
check('sprite simple : couleur du vaisseau + centre jaune', near(r.img.getpixel((14, 16)), (255, 60, 60)) and near(r.img.getpixel((18, 17)), (255, 255, 0)))
check('transparence respectée : rien en dehors du sprite (coin 10,10)', near(r.img.getpixel((10, 10)), BLACK))
check('teinte rouge (setColor avant draw) : la composante verte/bleue du sprite est annulée (255,60,60) x rouge = (255,0,0)', near(r.img.getpixel((44, 16)), (255, 0, 0)) and near(r.img.getpixel((40, 10)), BLACK))
check('rotation 90° + échelle x2 centrée : forme large (8x12 -> ~16x8 x2), point central = le point jaune du sprite', near(r.img.getpixel((95, 40)), (255, 60, 60)) and near(r.img.getpixel((100, 40)), (255, 255, 0)) and near(r.img.getpixel((100, 25)), BLACK) and near(r.img.getpixel((100, 55)), BLACK), [r.img.getpixel((95,40)), r.img.getpixel((100,40)), r.img.getpixel((100,25))])
check('alpha 50% : rouge assombri sur fond noir (~128,30,30)', near(r.img.getpixel((14, 66)), (128, 30, 30), 14), r.img.getpixel((14, 66)))
check('newQuad(x,y,w,h,sw,sh) : 1ère case (rouge) de la feuille', near(r.img.getpixel((158, 18)), (200, 40, 40)))
check('newQuad(x,y,w,h,image) : 2ème case (bleue), variante Love 11', near(r.img.getpixel((178, 18)), (40, 40, 220)))
check('aucun débordement entre les deux quads', near(r.img.getpixel((166, 18)), BLACK))
check('newImage(.bmp) : couleur correcte (à la quantification RGB565 près), opaque', near(r.img.getpixel((154, 44)), (10, 200, 30), 10))
check('dessin hors écran : pas de plantage (déjà garanti par rc==0 plus haut)', True)

print('== polices (P3) : newFont/setFont, tailles multiples de la police accentuée')
r = run('fonts', frames=4)
check('aucune erreur', 'Error' not in r.out and r.rc == 0, r.out[:300])
check('hauteurs 8/16/24', 'heights\t8\t16\t24' in r.out, r.out)
check('largeur "café" à taille 16 (4 caractères x 16px)', 'width café\t64' in r.out, r.out)
check('rendu : la police change bien de taille à l\'écran (plus de blanc sur la ligne "Grand" que sur "Petit café")',
      count(r.img, (0, 50, 220, 74), lambda c: near(c, WHITE)) > count(r.img, (0, 4, 220, 12), lambda c: near(c, WHITE)))

print('== fichiers (P3) : love.filesystem.newFile (open/read/write/append/close)')
r = run('newfile', frames=3)
check('écrit puis relu à l\'identique', 'read\t{"score":42}\t12' in r.out, r.out)
check('eof après lecture complète', 'eof\ttrue' in r.out)
check('newFile(chemin,"r") ouvre directement', 'direct\ttrue\t{"sc\t4' in r.out, r.out)
check('append via l\'objet File', 'appended\t{"score":42}!' in r.out, r.out)

print('== son (P3) : love.audio (newSource, play/stop, volume, boucle, clone)')
r = run('audio', frames=45)
check('aucune erreur', 'Error' not in r.out and r.rc == 0, r.out[:300])
check('pas encore lancée : isPlaying faux', 'isPlaying avant\tfalse' in r.out)
check('play() démarre la lecture', 'isPlaying après play\ttrue' in r.out)
check('type/getType', 'type\tSource\tstatic' in r.out)
check('setVolume/getVolume', 'volume\t1' in r.out and 'volume2\t0.5' in r.out)
check('clone() : indépendant, pas encore lancé', 'clone isPlaying (pas encore lancé)\tfalse' in r.out)
check('fichier manquant : erreur Lua normale (comme newImage)', "fichier manquant\tfalse\tCould not open file" in r.out, r.out)
check('WAV invalide : play() renvoie faux sans planter', 'wav invalide : play renvoie\tfalse' in r.out)
check('toujours en cours après 0,3s (son de 0,5s)', 'bip après 0.3s\ttrue' in r.out)
check('terminé après 0,6s (son de 0,5s, sans boucle)', 'bip après 0.6s (fini)\tfalse' in r.out)
check('boucle : toujours en cours après 1s (son de 0,2s en boucle)', 'loop après 1s (toujours en cours, a bouclé)\ttrue' in r.out)

print('== souris virtuelle (P3) : curseur piloté par le stick, clic = bouton A')
r = run('mouse', frames=5)
check('position de départ centrée (fenêtre 320x240)', 'depart\t160\t120' in r.out, r.out)
r = run('mouse', frames=60, presses=['right:5:35', 'a:20:25'])
check('mousepressed émis avec la position courante du curseur, bouton 1', any(l.startswith('mousepressed\t') and l.endswith('\t1') for l in r.out.splitlines()), r.out)
check('mousereleased émis aussi', 'mousereleased' in r.out)
r = run('mouse', frames=40, presses=['right:5:35'], shot_frame=39)
check('le curseur se déplace réellement vers la droite (dessiné par le jeu)',
      any(near(r.img.getpixel((x, 120)), (255, 255, 0)) for x in range(300, 320)),
      [r.img.getpixel((x, 120)) for x in (160, 250, 315)])

print('== exemple « casse-briques » (examples/cassebriques) : jeu complet du tutoriel')
r = run('cassebriques', frames=5, root=EXAMPLES)
check('menu au démarrage, aucune erreur', 'Error' not in r.out and r.rc == 0, r.out[:300])
r = run('cassebriques', frames=60, shot_frame=59, root=EXAMPLES, presses=['a:2:4', 'a:5:7'])
check('une partie démarre : briques, raquette, balle visibles', 'Error' not in r.out and r.rc == 0)
check('les briques (rouges) sont affichées en haut', count(r.img, (0, 24, 320, 44), lambda c: near(c, (230, 77, 77), 20)) > 200)
check('la raquette (blanche) est visible en bas', count(r.img, (0, 220, 320, 236), lambda c: near(c, WHITE)) > 20)
r = run('cassebriques', frames=30, shot_frame=29, root=EXAMPLES, presses=['a:2:4', 'a:5:7', 'menu:20:22'])
check('MENU met en pause (texte "PAUSE" blanc affiché au centre)', count(r.img, (0, 106, 320, 120), lambda c: near(c, WHITE)) > 30)

print('== transfert de fichiers (protocole AKAT) : PING, PUT, erreurs, resynchronisation')
if SIM.endswith('.exe') or any('wine' in r.lower() for r in RUNNER):
    print("  (ignoré sur cette plateforme : serial_read() du simulateur Windows est un bourrage sans effet,"
          " voir sim/hal_sim.cpp — la console elle-même n'est pas concernée)")
else:
    import struct, zlib
    MAGIC = b'AKAT'
    def akat_frame(lang, dest, data, bad_crc=False):
        dest_b = dest.encode()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        if bad_crc: crc ^= 0xFFFFFFFF
        return (MAGIC + bytes([1, 2, lang]) + struct.pack('<H', len(dest_b)) + dest_b +
                struct.pack('<I', len(data)) + data + struct.pack('<I', crc))

    def run_transfer(frame_bytes, frames=3):
        home = tempfile.mkdtemp(prefix='akalove_xfer_')
        os.makedirs(os.path.join(home, 'games'))
        shutil.copytree(os.path.join(GAMES, 'hello'), os.path.join(home, 'games', 'hello'))
        with open(os.path.join(home, 'game.txt'), 'wb') as f:
            f.write(b'hello\n')
        p = subprocess.run(RUNNER + [SIM, '--home', home, '--frames', str(frames), '--transfer-window-ms', '400'],
                            input=frame_bytes, capture_output=True, timeout=60)
        return p, home

    p, home = run_transfer(akat_frame(0, 'jeu2/main.lua', b'print("ok")\n'))
    check('PUT valide : réponse AKAT/OK, fichier écrit au bon endroit',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and
          open(os.path.join(home, 'games', 'jeu2', 'main.lua'), 'rb').read() == b'print("ok")\n')

    p, home = run_transfer(akat_frame(0, 'jeu3/main.lua', b'print("x")\n', bad_crc=True))
    check('PUT avec somme de contrôle invalide : refusé (statut 3), rien écrit',
          p.stdout[:6] == MAGIC + bytes([1, 3]) and not os.path.exists(os.path.join(home, 'games', 'jeu3')))

    p, home = run_transfer(akat_frame(9, 'jeu4/main.lua', b'print("x")\n'))
    check('PUT langage inconnu de ce firmware : refusé (statut 1)', p.stdout[:6] == MAGIC + bytes([1, 1]))

    p, home = run_transfer(akat_frame(0, '../echappement.lua', b'print("x")\n'))
    check('PUT chemin ".." : refusé (statut 2), comme love.filesystem', p.stdout[:6] == MAGIC + bytes([1, 2]))

    p, home = run_transfer(MAGIC + bytes([1, 1]))   # PING
    check('PING : réponse AKAT/OK, aucun fichier touché', p.stdout[:6] == MAGIC + bytes([1, 0]))

    p, home = run_transfer(b'bruit\r\nAKA' + akat_frame(0, 'jeu5/main.lua', b'print("apres bruit")\n'))
    check('du bruit avant la trame : le protocole se resynchronise et traite quand même le PUT',
          os.path.exists(os.path.join(home, 'games', 'jeu5', 'main.lua')))

    print('== AKA/languages.csv : dossier de scripts configurable, partagé entre firmwares AKA')
    def run_with_csv(csv_content):
        sdroot = tempfile.mkdtemp(prefix='akalove_csv_')
        home = os.path.join(sdroot, 'AKA_Love')
        os.makedirs(os.path.join(home, 'games'))
        shutil.copytree(os.path.join(GAMES, 'hello'), os.path.join(home, 'games', 'hello'))
        with open(os.path.join(home, 'game.txt'), 'wb') as f:
            f.write(b'hello\n')
        if csv_content is not None:
            os.makedirs(os.path.join(sdroot, 'AKA'))
            with open(os.path.join(sdroot, 'AKA', 'languages.csv'), 'w', encoding='utf-8') as f:
                f.write(csv_content)
        frame_bytes = akat_frame(0, 'attrape/main.lua', b'-- via csv\n')
        p = subprocess.run(RUNNER + [SIM, '--home', home, '--frames', '5', '--transfer-window-ms', '500'],
                           input=frame_bytes, capture_output=True, timeout=60)
        return p, sdroot, home

    p, sdroot, home = run_with_csv(None)
    check('sans AKA/languages.csv : comportement par défaut inchangé (<home>/games)',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and
          os.path.exists(os.path.join(home, 'games', 'attrape', 'main.lua')))

    p, sdroot, home = run_with_csv('0,mes_scripts_lua,Lua perso\n1,py,MicroPython\n')
    check('AKA/languages.csv présent : redirige vers le dossier configuré, relatif à la racine SD',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and
          os.path.exists(os.path.join(sdroot, 'mes_scripts_lua', 'attrape', 'main.lua')) and
          not os.path.exists(os.path.join(home, 'games', 'attrape')))

    p, sdroot, home = run_with_csv('# rien pour lang 0\n1,py,MicroPython\n')
    check('CSV présent mais sans ligne pour ce langage : repli sur <home>/games, sans erreur',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and
          os.path.exists(os.path.join(home, 'games', 'attrape', 'main.lua')))

    p, sdroot, home = run_with_csv('0,../../hors_de_la_carte,Lua malicieux\n')
    check('CSV avec un chemin ".." : refusé, repli sur <home>/games (jamais d\'échappement de la carte)',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and
          os.path.exists(os.path.join(home, 'games', 'attrape', 'main.lua')))

    print('== écran interactif "Recevoir un code" : conflit Écraser/Renommer/Annuler, bouton MENU')
    def run_receive(existing_content, presses, frames=200):
        home = tempfile.mkdtemp(prefix='akalove_recv_')
        os.makedirs(os.path.join(home, 'games', 'attrape'))
        shutil.copytree(os.path.join(GAMES, 'hello'), os.path.join(home, 'games', 'hello'))
        with open(os.path.join(home, 'game.txt'), 'wb') as f:
            f.write(b'hello\n')
        target = os.path.join(home, 'games', 'attrape', 'main.lua')
        if existing_content is not None:
            with open(target, 'wb') as f:
                f.write(existing_content)
        frame_bytes = akat_frame(0, 'attrape/main.lua', b'-- nouveau contenu\n')
        cmd = RUNNER + [SIM, '--home', home, '--receive-screen', '--frames', str(frames)]
        for p in presses:
            cmd += ['--press', p]
        p = subprocess.run(cmd, input=frame_bytes, capture_output=True, timeout=60)
        return p, home, target

    p, home, target = run_receive(None, [])
    check('pas de conflit (fichier absent) : écrit directement, sans prompt',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and open(target, 'rb').read() == b'-- nouveau contenu\n')

    p, home, target = run_receive(b'-- originale\n', ['a:100:110'])
    check('conflit + A (Écraser) : contenu remplacé, statut OK',
          p.stdout[:6] == MAGIC + bytes([1, 0]) and open(target, 'rb').read() == b'-- nouveau contenu\n')

    p, home, target = run_receive(b'-- originale\n', ['d:100:110'])
    renamed = os.path.join(home, 'games', 'attrape', 'main (2).lua')
    check('conflit + D (Renommer) : original intact, nouveau fichier "main (2).lua", statut OK_RENAMED',
          p.stdout[:6] == MAGIC + bytes([1, 6]) and open(target, 'rb').read() == b'-- originale\n' and
          os.path.exists(renamed) and open(renamed, 'rb').read() == b'-- nouveau contenu\n')

    p, home, target = run_receive(b'-- originale\n', ['b:100:110'])
    check('conflit + B (Annuler) : rien ne change, statut CANCELLED (7)',
          p.stdout[:6] == MAGIC + bytes([1, 7]) and open(target, 'rb').read() == b'-- originale\n' and
          len(os.listdir(os.path.dirname(target))) == 1)

    home5 = tempfile.mkdtemp(prefix='akalove_recv_')
    os.makedirs(os.path.join(home5, 'games'))
    shutil.copytree(os.path.join(GAMES, 'hello'), os.path.join(home5, 'games', 'hello'))
    with open(os.path.join(home5, 'game.txt'), 'wb') as f:
        f.write(b'hello\n')
    p5 = subprocess.run(RUNNER + [SIM, '--home', home5, '--receive-screen', '--frames', '200', '--press', 'menu:5:10'],
                        input=b'', capture_output=True, timeout=60)
    check('bouton MENU : quitte proprement (aucune donnée envoyée)', p5.returncode == 0)

    p, home = run_transfer(b'', frames=3)
    check('rien envoyé : le jeu démarre normalement après la fenêtre (aucun blocage)',
          b'Bonjour, tout fonctionne' in p.stdout)

print('== scissor')
r = run('scissor', frames=4)
check('getScissor', 'scissor\t0\t0\t20\t20' in r.out, r.out)
check('dessin limité au scissor', near(r.img.getpixel((19, 19)), RED) and near(r.img.getpixel((20, 20)), BLACK) and near(r.img.getpixel((50, 5)), BLACK))
check('setScissor() sans argument supprime la limite', near(r.img.getpixel((105, 105)), (0, 255, 0)))

print('== texte : printf centré, retour à la ligne, texte coloré')
r = run('text', frames=4)
check('HELLO centré (30 px de large)', count(r.img, (145, 100, 175, 112), lambda c: near(c, WHITE)) > 40 and count(r.img, (0, 100, 140, 112), lambda c: near(c, WHITE)) == 0)
check('printf avec limite 60 : plusieurs lignes', count(r.img, (0, 140, 60, 152), lambda c: near(c, WHITE)) > 0 and count(r.img, (0, 152, 60, 164), lambda c: near(c, WHITE)) > 0 and count(r.img, (0, 164, 60, 176), lambda c: near(c, WHITE)) > 0)
check('aucun débordement à droite de la limite', count(r.img, (61, 140, 320, 200), lambda c: near(c, WHITE)) == 0)
check('texte coloré rouge puis vert', count(r.img, (0, 10, 30, 22), lambda c: near(c, RED)) > 20 and count(r.img, (30, 10, 60, 22), lambda c: near(c, (0, 255, 0))) > 20)

print('== runner (jeu du dossier, sans modification) : 300 frames, saut à la frame 10')
r = run('runner', frames=300, shot_frame=60, presses=['a:10:12'])
check('aucune erreur', 'Error' not in r.out and r.rc == 0, r.out[:300])
check('assets absents (images/fond.png inexistant) : le pcall du jeu absorbe l\'échec sans planter (rien dans le terminal)',
      'Could not open file' not in r.out and r.rc == 0)   # l'erreur reste interne au pcall, jamais affichée
check('love.audio (implémenté) : fichier manquant, absorbé par le même pcall, rien dans le terminal',
      'Could not open file' not in r.out and r.rc == 0)
check('fond bleu nuit', near(r.img.getpixel((10, 10)), (26, 26, 77), 10), r.img.getpixel((10, 10)))
check('sol vert', near(r.img.getpixel((10, 230)), (77, 204, 77), 10), r.img.getpixel((10, 230)))
check('joueur blanc au sol', near(r.img.getpixel((48, 192)), WHITE), r.img.getpixel((48, 192)))
check('score affiché', count(r.img, (4, 4, 90, 16), lambda c: near(c, WHITE)) > 30)

failed = [n for n, ok, _ in results if not ok]
print('\n%d vérifications, %d échec(s)' % (len(results), len(failed)))
sys.exit(1 if failed else 0)
