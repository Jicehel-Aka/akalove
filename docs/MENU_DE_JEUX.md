# Le menu de jeux AKA-Love (lanceur, phase P6)

Au démarrage, AKA-Love affiche un **lanceur** : la liste des jeux présents sur la carte SD. Le joueur
choisit son jeu sans câble ni PC, et sans rien recompiler. Ce document décrit l'écran, l'arborescence
attendue sur la carte, et comment ajouter un jeu.

## À l'écran

```
AKA-Love
Choisis un jeu

  attrape
> hello            <- ligne en surbrillance = sélection
  runner

Haut/Bas: choisir   A: lancer   MENU: quitter
```

- **D-Pad haut / bas** : déplacer la sélection (la liste défile si elle dépasse l'écran ; la sélection
  boucle du bas vers le haut).
- **A** : lancer le jeu sélectionné.
- **MENU** : quitter AKA-Love et revenir au loader de la console.

Quand un jeu se termine (ou fait « revenir au loader » depuis son propre code), on **revient au menu** —
pas besoin de redémarrer la console pour enchaîner un autre jeu.

## Arborescence sur la carte SD

Tout vit dans un dossier `AKA_Love/` à la racine de la carte (comme les autres firmwares du loader) :

```
AKA_Love/
├─ firmware.bin          # le binaire AKA-Love (attendu par le loader)
├─ game.txt              # une ligne : le nom du dernier jeu choisi (présélection)
├─ Picture.png           # (facultatif) image de démarrage
├─ games/
│  ├─ hello/   main.lua  conf.lua  ...
│  ├─ runner/  main.lua  ...
│  └─ attrape/ main.lua  ...
└─ save/                 # sauvegardes, un sous-dossier par jeu (créé automatiquement)
```

Le menu liste simplement les **sous-dossiers de `games/`**, triés par ordre alphabétique (sans tenir
compte de la casse). Un dossier n'apparaît que si son nom ne contient que lettres, chiffres, `_`, `-`, `.`.

## `game.txt` : la mémoire du choix

- Le menu **écrit** le nom du jeu choisi dans `AKA_Love/game.txt` (une seule ligne).
- Au démarrage suivant, ce jeu est **présélectionné** dans la liste (il suffit d'appuyer sur **A**).
- On peut aussi éditer `game.txt` **à la main** depuis un PC (une ligne = le nom exact du dossier) : c'est
  le même fichier, le menu et l'édition manuelle sont interchangeables.
- Si `game.txt` est absent ou contient un nom invalide/inexistant, la présélection retombe sur `hello`.

## Ajouter un jeu

1. Copier le dossier du jeu dans `AKA_Love/games/` (il doit contenir au moins `main.lua`).
2. Au prochain démarrage, il apparaît dans le menu — rien d'autre à faire.

Pour envoyer un jeu **sans retirer la carte SD**, voir `docs/PROTOCOLE_TRANSFERT.md` (l'outil
`tools/akatransfer.py` dépose les fichiers directement dans `AKA_Love/games/`).

## Cas limites

- **Aucun jeu dans `games/`** : un message s'affiche quelques secondes, puis retour au loader (pas de
  plantage).
- **Maintenir D au démarrage** : ouvre l'écran « Recevoir un code » (transfert USB) au lieu du menu.

## Pour les développeurs

- Code : `components/akalove/game_menu.{h,cpp}` — ne dépend que de `hal::` (entrées/temps) et `gfx::`
  (texte), donc le **même code** tourne sur la console (`hal_aka`) et sur les simulateurs PC
  (`hal_sim`, `hal_sdl`).
- Essayer au clavier : `akalove-play --home <dossier AKA_Love> --menu` (flèches + Entrée/Espace, Échap = MENU).
- Tester en scriptant les entrées (simulateur sans fenêtre) :

  ```
  akalove-sim --home <AKA_Love> --no-splash --menu --frames 24 --press up:2:3 --press a:5:6
  ```

  La suite `tests/run_tests.py` couvre la sélection, l'annulation par MENU et le cas `games/` vide.
