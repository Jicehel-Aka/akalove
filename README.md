# AKA-Love — runtime (phases P0 + P1)

Runtime **Love2D 11.5 sur Gamebuino AKA** : Lua 5.1.5 + API `love.*` en C++, projet ESP-IDF autonome.
Ce dépôt suit le dossier de conception *AKA-Love v0.3* (PDF).

## Prochaine étape : ce qui manque encore

D'après un vrai passage en revue de code de cours (52 fichiers Lua, jeux GameCodeur : le sous-marin, un
fermier avec tuiles/animations, un char avec compte à rebours sonore, des DLC pathfinding/JSON…) — testés
un par un, sans modification, sur le simulateur :

1. **Canvas** (`love.graphics.newCanvas`, rendu hors écran) — aucun usage vu dans ces sources, mais utile pour des effets plus avancés (post-traitement, mini-cartes).
2. **`love.physics`** (Box2D) — pas rencontré non plus, gros morceau si jamais demandé.
3. Musique en vrai flux (le "stream" de `love.audio` est aujourd'hui traité comme "static" — voir la section Son ci-dessous) pour de longues pistes sans tout garder en mémoire.
4. Côté transfert (voir ci-dessous) : essai sur un vrai port série, commandes `LIST`/`GET`, MicroPython.

## Transférer un fichier sans retirer la carte SD

Un protocole simple (`docs/PROTOCOLE_TRANSFERT.md`, v1.1) envoie un fichier vers la console par le câble
USB déjà utilisé pour flasher : `tools/akatransfer.py --port /dev/ttyUSB0 --lang lua --dest cassebriques/main.lua main.lua`.
Pensé pour être commun à plusieurs firmwares AKA (AKA-Love, MicroPython AKA...) : un octet `lang_id`
dit à quel langage le fichier appartient, chaque firmware range à son idée (AKA-Love : `AKA_Love/games/`).

Deux façons de recevoir :
- **Fenêtre passive** (1,5 s au démarrage) : silencieuse, écrase directement — pour un envoi automatisé.
- **Écran interactif « Recevoir un code »** (bouton **D tenu au démarrage**, en attendant un vrai menu —
  phase P6) : attend indéfiniment, crée les dossiers manquants, et **si le fichier existe déjà, demande
  au joueur** — A : Écraser, D : Renommer, B : Annuler (30 s pour répondre, sinon annulé par sécurité).

**Écrit et testé** sur le simulateur (18 contrôles rien que pour le transfert : dépôt, erreurs de somme
de contrôle/chemin/langage, bruit sur la ligne, et les trois choix de l'écran interactif avec de vraies
pressions de boutons simulées) ; **jamais essayé sur un vrai port série** — voir le document pour le
point précis à valider en premier sur la carte.

## Police, son, fichiers, souris : détails et limites connues

- **Police** : une seule police de glyphes (celle d'`aka_font`, avec accents FR/ES/DE), mais toute taille
  fonctionne via `love.graphics.newFont(taille)` — elle est arrondie au multiple de 8 le plus proche et
  affichée en agrandissant les pixels (pas de police vectorielle). `newFont(chemin, taille)` ignore le
  chemin (police personnalisée non supportée) et le journalise une fois.
- **Son** : jusqu'à **4 sons simultanés** (comme `gb_audio_player`, 4 canaux matériels) ; au-delà,
  `Source:play()` échoue silencieusement (comme une console qui manque de voix). `Source:pause()`
  équivaut à `stop()` (pas de vraie reprise, `gb_audio_track_wav` n'en a pas). WAV **16 bits** exigé par
  la console (mono ou stéréo) ; le simulateur ne vérifie pas ce point (il ne produit aucun son de toute
  façon, seulement l'état de lecture, pour que les jeux et les tests restent corrects sans matériel).
- **Fichiers** : `love.filesystem.newFile` complète les fonctions déjà là (`read/write/append`) ; en
  écriture, le contenu est accumulé en mémoire et n'atteint la carte SD qu'à `:close()`.
- **Souris** : le stick déplace un curseur virtuel (`love.mouse.getPosition()`), le bouton A envoie
  `mousepressed`/`mousereleased` à cette position. Il n'y a pas de curseur système affiché — un jeu conçu
  pour la souris reste jouable seulement s'il dessine lui-même un sprite de curseur (ce que font la
  plupart des jeux GameCodeur à la souris).

## Premiers pas

Guide pas à pas (PDF `AKA-Love_Guide_de_demarrage`) : mise en place, création du jeu `examples/attrape`, test sur PC,
exécution sur l'AKA. En bref : le jeu lancé est celui nommé dans `AKA_Love/game.txt` sur la carte SD (défaut :
`hello`) ; ses fichiers sont dans `AKA_Love/games/<nom>/`. Changer de jeu = éditer `game.txt`, sans recompiler.

**Image de démarrage** : `AKA_Love/Picture.png` (l'image du catalogue web) est affichée 1,5 s au lancement, ou jusqu'au
premier appui ; sans elle, `screen.bmp` (l'image du loader) ; sans aucune des deux, le jeu démarre tout de suite.
Taille libre (réduite ou agrandie pour tenir dans 320×240, bandes noires si besoin), transparence fondue sur noir.
`python tools/make_screen_bmp.py Picture.png screen.bmp [--crop]` fabrique `screen.bmp` (BMP 24 bits 320×240, comme
les captures de la console) à partir de `Picture.png`. Les deux fichiers sont fournis dans `sdcard_files/AKA_Love/`
(`Picture.png` 640×480, `screen.bmp` 320×240) et le workflow les ajoute au zip carte SD ; ajoutez-y `meta.json` si votre loader l'exige.

## Où en est-on

| Élément | État |
|---|---|
| Noyau : VM Lua 5.1.5, `conf.lua`, `main.lua`, `love.run`, écran d'erreur, chien de garde | **écrit et testé** (simulateur) |
| `love.graphics` : couleur, transformations, scissor, rectangle/cercle/polygone/ligne/points, `print`/`printf` | **écrit et testé** (simulateur) |
| `love.timer`, `love.event`, `love.keyboard`, `love.joystick` (manette virtuelle), `love.window`, `love.math`, `love.system`, `love.filesystem` (+ `require`) | **écrit et testé** (simulateur) |
| Résolution virtuelle (conf 800×600 → 320×240) | **écrit et testé** |
| HAL console `hal_aka.cpp`, `main/main.cpp`, CMake ESP-IDF | **écrits contre la vraie bibliothèque `gamebuino`** (noms et signatures vérifiés sur ses en-têtes, `tools/check_hal_aka.sh`, qui applique d'abord la correction de casse sur une copie) ; **jamais compilés par ESP-IDF ni essayés sur la console** |
| `love.graphics.newImage` (PNG/BMP), `newQuad`, `draw` (rotation/échelle/cisaillement/teinte/alpha/repère, Quad, image comme sw/sh Love 11) | **écrit et testé** (simulateur ; validé sur un vrai jeu GameCodeur non modifié, sprite + tuiles) |
| `love.graphics.newFont(taille)`/`setFont` : police accentuée (`aka_font`) agrandie par un facteur entier (8, 16, 24…) | **écrit et testé** |
| `love.audio` : `newSource` (WAV, "static"/"stream" traités pareil), `play/stop/pause`, volume, boucle, `clone`, volume maître | **écrit et testé** (simulateur, sans son réel — juste l'état ; console : `gb_audio_player`/`gb_audio_track_wav`, 4 canaux, vérifié contre les vrais en-têtes) |
| `love.filesystem.newFile` (API objet `open/read/write/close/eof/getSize`) | **écrit et testé** |
| Souris virtuelle (`love.mouse`, `mousepressed/released`) : curseur piloté par le **stick analogique**, clic = bouton A | **écrit et testé** — approximation : pas de curseur système, le jeu doit dessiner lui-même un sprite à `love.mouse.getPosition()` |
| Canvas | à faire |
| Audio (`love.audio`, mixeur) | à faire — P5 |
| Lanceur de jeux, USB (`akalove` PC), simulateur SDL2 | à faire — P6/P3/P7 |

Vérifié : **61 contrôles automatiques passent** (`tests/run_tests.py`), rendus vérifiés pixel par pixel
(règle « centre du pixel », alpha, rotation, échelle virtuelle, scissor, texte). Le jeu *Infinite Runner*
du dossier tourne **sans modification** (ses images/sons absents sont absorbés par ses `pcall`).
Le noyau se compile en `-Wall -Wextra -Werror -fno-exceptions -fno-rtti` (comme sous ESP-IDF).

**Pas vérifié** : compilation ESP-IDF, comportement sur la console (affichage, boutons, RUN+MENU), vitesse réelle sur ESP32-S3, comparaison avec un vrai Love2D
(pas de Love installé ici : les « golden images » du dossier §19 restent à produire côté PC).

## Compiler et tester sur PC (Linux ou Windows/MinGW ; aucune fenêtre, horloge virtuelle déterministe)

```sh
cmake -S sim -B build-sim && cmake --build build-sim -j
python3 tests/run_tests.py            # nécessite Python 3 + Pillow (--smoke : contrôle rapide)
./build-sim/akalove-sim tests/games/hello --frames 60 --shot hello.ppm
```

Options du simulateur : `--frames N`, `--shot f.ppm --shot-frame K`, `--press bouton:de:a`
(`up down left right a b c d l1 r1 menu`), `--save dir`, `--watchdog-ms N`, `--realtime`, `--home <dossier AKA_Love>` (lit `game.txt` comme la console).
`--shot` accepte `.bmp` (s'ouvre partout) ou `.ppm`. `--no-splash` : avec `--home`, sans image de démarrage.

## CI et releases (GitHub Actions)

Un seul workflow, `.github/workflows/release.yml`, calqué sur celui de Dark & Under. À chaque mise à jour de
`main`/`master` (hors documentation seule) il compile les trois versions puis publie une Release en
incrémentant la **version mineure** (`v0.1.0`, `v0.2.0`, ...). Rien n'est publié si une compilation ou un test échoue.

| Fichier de la Release | Contenu |
|---|---|
| `firmware.bin` | Binaire AKA seul (nom attendu par le loader) |
| `aka-love-sdcard-vX.Y.0.zip` | Dossier `AKA_Love/` à copier à la racine de la carte SD : `firmware.bin` + `game.txt` + `games/hello`, `games/runner`, `games/attrape` (+ le contenu de `sdcard_files/AKA_Love/` s'il existe : `meta.json`, `Picture.png`...) |
| `aka-love-sim-windows-x64-vX.Y.0.zip` | Simulateur PC Windows (MinGW, exécutable autonome) + jeux d'exemple |
| `aka-love-sim-linux-x64-vX.Y.0.zip` | Simulateur PC Linux + jeux d'exemple |

- **Prérequis** : la bibliothèque `gamebuino` doit être dans `components/gamebuino` du dépôt (comme dans les autres
  projets AKA). Le workflow applique lui-même `tools/fix_gamebuino_case.py` avant de compiler.
- **Le simulateur PC est sans fenêtre** pour l'instant (il écrit des captures PPM) ; la version SDL2 reste à faire.
- **Windows** : le simulateur compile avec MinGW-w64 ; le `.exe` compilé en croisé passe les 62 contrôles sous Wine.
  Sur GitHub, la version Windows fait un contrôle rapide (le jeu `hello` doit s'exécuter) ; la suite complète tourne sous Linux.
- **Non vérifié** : le job `aka` (ESP-IDF) n'a jamais tourné. Sa première exécution est le premier vrai test de
  compilation du firmware : lisez son journal, c'est là que les erreurs éventuelles apparaîtront.
- Le bootloader et la table de partitions ne sont pas publiés : les flasher écraserait le loader.

## Police et texte : le composant `aka_font`

Le rendu de texte utilise maintenant `components/aka_font` (fourni pour ce projet, réutilisé ailleurs dans
vos jeux AKA) au lieu de l'ancienne police maison 6×12 ASCII seul. Glyphes 8×8, accents français
(à â ç è é ê ë î ï ô ù û ü + majuscules, «, »), espagnols (á í ñ ó ú + majuscules, ¿, ¡) et allemands
(ä ö ü ß + majuscules). Décodage UTF-8 réel (1 à 3 octets) : `love.graphics.print`/`printf` comptent les
**caractères**, pas les octets — un mot accentué n'est jamais coupé en deux au retour à la ligne, et son
avancement (`text_width`) est correct pour l'alignement centré/droite. Un caractère hors de ce
répertoire (emoji, CJK, °...) s'affiche `?` sans planter ; `œ`/`Œ` se replient sur `oe`/`OE`.

**Deux corrections apportées au composant fourni** (détaillées dans `components/aka_font/README.md`,
section « Correctifs (v1.1) », et dans `aka_font_corrige.zip` à part si vous le réutilisez ailleurs) :
- `font8x8_extended.h` (fichier *généré*) n'avait pas les 4 glyphes allemands ä/ö/Ä/Ö, malgré le README du
  composant qui les annonce — `generate_extended_font.py` avait juste oublié ces 4 lignes (même technique
  que ë/ï/ü, déjà présents). Corrigé directement dans le générateur ; 45 glyphes au lieu de 41.
- `font8x8_basic.h` déclarait son tableau sans `static` : l'inclure depuis plus d'un `.cpp` (ce que fait
  `aka_font/gb_text_render.h`) aurait provoqué une erreur d'édition de liens dès qu'un 2ᵉ fichier en
  aurait eu besoin. Passé en `static const unsigned char` (supprime aussi un avertissement de narrowing
  sous `-Werror`, sans changer les octets stockés). Un seul point d'inclusion suffit maintenant à éviter
  tout risque, mais ce n'est plus une obligation stricte.

## Passer sur la console

**Première compilation ESP-IDF réussie** (confirmée par l'utilisateur, ESP-IDF 5.5.1/Windows) : `akalove.bin`
fait 583 Ko, posé dans la partition `factory` (3 Mo, 18 % occupés). ESP-IDF avertit aussi que le binaire ne
tiendrait pas dans la partition `loader` (512 Ko) — normal et sans conséquence : cette partition est celle du
loader/menu de la console (OTA_1), jamais celle où AKA-Love est flashé ; l'avertissement dit juste « si vous
flashiez ici par erreur, ça ne rentrerait pas », ce qui protège plutôt le loader. La commande `idf.py flash`
suggérée par ESP-IDF cible bien `0x20000` (`factory`), pas `0x320000` (`loader`).

Base : la bibliothèque et le `sdkconfig` de `wargame_aka` (ESP-IDF 5.4.1, ESP32-S3, 8 Mo de flash, PSRAM octale).

1. Copier `wargame_aka/components/gamebuino` dans `components/` (la bibliothèque n'est pas dupliquée ici), puis
   la rendre compatible Linux : soit recopier `patches/gamebuino/include_lib/gb_graphics.h` par-dessus, soit lancer
   `python tools/fix_gamebuino_case.py components/gamebuino`. Le seul écart trouvé dans toute l'arborescence de
   `wargame_aka` : `gb_graphics.h` incluait `"gb_ll_LCD.h"` alors que le fichier s'appelle `gb_ll_lcd.h`
   (une ligne changée, fins de ligne CRLF conservées). Les fichiers à majuscules (`WAV_SYSTEM.h`, `TAS2505_regs.h`,
   `Arduino.h`) sont inclus avec la même casse : ils fonctionnent sous Linux et n'ont pas été renommés.
2. `sdkconfig.defaults` et `partitions.csv` reprennent ceux de `wargame_aka` (la partition `loader`/`ota_1` doit
   rester celle de votre loader). **Une différence voulue : `-O2` au lieu de `-Og`**, indispensable à la VM Lua.
3. Dossier SD : `/sdcard/AKA_Love` (`AKALOVE_HOME` dans `main/main.cpp`) ; copier `tests/games/hello` dans
   `AKA_Love/games/hello`. Noms longs **activés** (`CONFIG_FATFS_LFN_HEAP`). Le contenu attendu par le loader
   (`firmware.bin`, `meta.json`...) reste à confirmer.
4. `idf.py build flash monitor`. Ce code n'a jamais vu ESP-IDF : attendez-vous à de petits ajustements.
   Au démarrage, `main.cpp` affiche la pile libre de la GameTask (24 Ko réservés, à ajuster).

Comportement de `hal_aka.cpp` à connaître :
- **Framebuffer** = le tableau global `framebuffer` de la bibliothèque ; `present()` = `gb_graphics::update()`
  (attend le DMA et la synchro verticale, `set_refresh_rate(60)`).
- **Boutons** : lus par `gb_ll_expander_read()` **une fois par frame** (`love.event.pump`). `gb_buttons::update()`
  n'est pas utilisé car il éteint la console dès que RUN est vu. **RUN n'est jamais un bouton de jeu** :
  RUN+MENU 500 ms → `love.quit` puis retour au loader ; **RUN seul 1,5 s → extinction** (ajout de ma part,
  à valider). Le stick analogique alimente les axes `leftx`/`lefty` et la croix.
- **Mémoire** : allocations Lua par `malloc` (politique ESP-IDF : ≤ 16 Ko en RAM interne, sinon PSRAM).

## Organisation

```
components/lua51/      Lua 5.1.5 d'origine (MIT), sans lua.c/luac.c/print.c
components/akalove/    splash.cpp, image_decode.cpp, third_party/stb_image.h (PNG/BMP, écran de démarrage)
                       runtime.cpp (VM, boucle, erreurs) · love_gfx.cpp (rasterizer) · bind_*.cpp (API love.*)
                       lua/boot.lua, lua/errorhandler.lua  →  embedded_lua.cpp (généré : tools/embed_lua.py)
                       font6x12.h (généré : tools/gen_font.py — police PROVISOIRE, ASCII seulement)
                       hal_aka.cpp (console, squelette) · include/hal.h (HAL v2)
sim/                   hal_sim.cpp + main_sim.cpp : simulateur sans fenêtre (PPM), tests
examples/attrape/      jeu d'exemple complet (utilisé par le guide et les tests)
tests/                 games/* (petits jeux de test) · run_tests.py · screenshots.png
```

Après avoir modifié `boot.lua` ou `errorhandler.lua` : `python3 tools/embed_lua.py`.

## Écarts avec le dossier v0.3 (découverts en écrivant le code)

1. **Entrées — risque de double déclenchement.** Une même pression émettait `keypressed` *et*
   `gamepadpressed` ; un jeu qui gère les deux (courant sur PC) aurait agi deux fois.
   Mode `t.aka.input = "auto"` (défaut) : `keypressed` seul si le jeu le définit ; `gamepadpressed`
   seulement s'il est défini **sans** `keypressed`. Autres valeurs : `keyboard`, `gamepad`, `both`.
   `isDown` / `isGamepadDown` fonctionnent toujours.
2. **Table des touches, d'après les vrais boutons** (croix, A B C D, L1 R1, MENU ; RUN réservé) :
   A `space` (+ `return`), B `x`, C `c`, D `v`, L1 `lshift`, R1 `rshift`, MENU `escape` ; manette `a b x y
   leftshoulder rightshoulder start`. Le `z` reste alias de « haut » (AZERTY) et non de A.
3. **Fonction `love.*` absente = `nil` + journal (une fois), pas une erreur à l'accès.** Une erreur à
   l'accès cassait les jeux qui passent la fonction en paramètre (le jeu du dossier lui-même) et la
   détection `if love.x.y then`. L'appel donne l'erreur Lua habituelle avec fichier:ligne.
4. **Écran d'erreur.** Comme Love, `love.errorhandler(msg)` est appelé *au point de l'erreur* (pile intacte
   pour `debug.traceback`) ; les scripts sont chargés avec le nom `@main.lua` pour avoir de vrais
   `main.lua:12:` dans les messages.
5. **Redémarrage.** Un bouton déjà maintenu au démarrage n'est pas un « nouvel appui » (sinon un A
   maintenu relançait un jeu en erreur sans fin).
6. **Piège d'API C Lua 5.1** : `luaL_tolstring` pousse une valeur sur la pile ; lire *d'abord* les
   arguments optionnels (deux bugs trouvés et corrigés).

7. **Casse des noms de fichiers.** La carte SD (FAT) ignore la casse ; Love sur PC/Linux non. Un jeu qui écrit
   `require("Lib.Util")` ou `newImage("Images/Fond.png")` avec la mauvaise casse marchera sur l'AKA mais pas
   ailleurs. À faire respecter : minuscules pour les fichiers et dossiers des jeux (et `akalove check` doit
   signaler les écarts de casse, prévu au dossier).

## Limites connues de la phase P1

`rectangle` : coins arrondis ignorés (journalisé) · joints de lignes en angle vif approximés (jonction
carrée) · segments de cercle choisis selon le rayon (≠ Love pixel pour pixel) · pas d'antialiasing ·
`love.audio`, `love.image`, `love.data`, `love.thread`, `love.physics`… absents (renvoient `nil`) ·
`bit` (LuaBitOp) pas encore fourni · texte ASCII seulement (les accents s'affichent `?`).
Mémoire : ~95 Ko de tas Lua après le démarrage ; vitesse à mesurer sur la console.
