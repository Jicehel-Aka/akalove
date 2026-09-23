# Protocole de transfert « AKAT » v1.1

Envoyer un fichier depuis un PC (AKA-IDE, un script, `tools/akatransfer.py`…) vers la carte SD d'une
console AKA, par le câble USB déjà utilisé pour flasher/surveiller (série, 115200 bauds — aucun matériel
supplémentaire). Conçu pour être **commun à plusieurs firmwares** (AKA-Love, MicroPython AKA, un futur
BASIC…) : chacun implémente la même trame, chacun décide où il range les fichiers reçus.

Implémentation de référence : `components/akalove/transfer.cpp` (AKA-Love) et `tools/akatransfer.py`
(PC). Statut : **écrit et testé sur le simulateur** (18 contrôles automatiques rien que pour le
transfert, cas d'erreur et écran interactif compris) ; **jamais essayé sur un vrai port série** — voir
`hal_aka.cpp` pour le point précis qui mériterait le premier essai matériel.

Deux façons de recevoir, au choix du firmware :
- **Fenêtre passive** (`transfer_listen`) : une courte écoute silencieuse (1,5 s au démarrage
  d'AKA-Love), qui écrase directement un fichier existant — pensée pour un envoi automatisé (CI, un
  script), sans intervention humaine.
- **Écran interactif** (`transfer_receive_screen`) : un écran « Recevoir un code », entré exprès (sur
  AKA-Love aujourd'hui : bouton **D tenu au démarrage** — un vrai menu suivra en phase P6). Il attend
  indéfiniment, crée les dossiers manquants tout seul, et **si le fichier existe déjà, demande au
  joueur** : A = Écraser, D = Renommer, B = Annuler ce fichier (30 s pour répondre, sinon Annuler par
  sécurité) ; MENU quitte l'écran à tout moment.

## Vue d'ensemble

- Un seul mode : le PC parle, l'appareil répond. Jamais l'inverse.
- L'appareil n'écoute que **par fenêtres** (au démarrage, entre deux jeux…), jamais pendant qu'un jeu
  tourne : pas besoin de séparer un canal « données » d'un canal « texte », le flux série peut aussi
  porter des `print()` de jeu en dehors de ces fenêtres.
- Tous les entiers sont **petit-boutiste** (little-endian).
- Un octet `lang_id` indique à quel langage le fichier appartient ; chaque firmware ne reconnaît que
  ceux qu'il sait ranger (AKA-Love : `0` seulement).

| `lang_id` | Langage | Racine typique sur la carte SD |
|---|---|---|
| `0` | Lua (AKA-Love) | `AKA_Love/games/<chemin envoyé>` |
| `1` | MicroPython (AKA) | `micropython/py/<chemin envoyé>` *(à plat : pas de sous-dossier par jeu)* |

D'autres valeurs pourront être ajoutées (BASIC…) sans casser les firmwares existants : un firmware qui
ne reconnaît pas un `lang_id` répond juste `BAD_LANG`.

## Trame

Chaque commande du PC vers l'appareil commence par l'en-tête commun :

```
octets 0-3   "AKAT"            (magique — 0x41 0x4B 0x41 0x54)
octet  4     version           (= 1)
octet  5     commande          (voir ci-dessous)
```

L'appareil répond toujours par la même forme d'en-tête (magique + version), suivie d'un octet de statut
et, pour l'instant, d'un octet `device_id` (`0` = AKA-Love ; réservé pour distinguer les firmwares) —
soit exactement **7 octets**, sauf pour le statut `OK_RENAMED` (v1.1, écran interactif seulement), qui
en ajoute deux de plus (voir plus bas) :

```
octets 0-3   "AKAT"
octet  4     version
octet  5     statut     (0 = OK, voir la table plus bas)
octet  6     device_id
```

Un client qui ne connaît que la v1 (7 octets fixes) reste compatible : il ne verra simplement jamais le
statut 6, réservé à l'écran interactif qu'il n'utilise pas.

### Commande `PING` (0x01)

Pas de contenu après l'en-tête. Sert à vérifier qu'un appareil AKA est bien à l'écoute (et lequel)
avant d'envoyer un vrai fichier — l'appareil répond `OK` immédiatement, sans toucher à la carte SD.

### Commande `PUT` (0x02) — déposer un fichier

```
octet    0        lang_id           (voir la table plus haut)
octets 1-2        longueur du chemin, u16
octets ...        chemin, UTF-8, SANS le caractère nul final, séparateur "/"
octets ...4        longueur des données, u32
octets ...         données (le contenu du fichier, tel quel)
octets ...4        CRC32 des données, u32 (polynôme 0xEDB88320, le CRC32 « zip » standard)
```

Le chemin est relatif à la racine du langage (voir la table). Il est refusé (`BAD_PATH`) s'il est vide,
absolu (commence par `/`), contient `..`, ou dépasse 219 octets. Un fichier de plus de 512 Ko est refusé
(`TOO_LARGE`) — largement assez pour un script ou un petit jeu, pas pour un firmware entier (ce
protocole ne sert pas à mettre à jour `firmware.bin`). Les dossiers manquants sont créés automatiquement.

### Codes de statut

| Valeur | Nom | Signification |
|---|---|---|
| 0 | `OK` | Fait. |
| 1 | `BAD_LANG` | Ce firmware ne gère pas ce `lang_id`. |
| 2 | `BAD_PATH` | Chemin vide, absolu, `..`, ou trop long. |
| 3 | `CRC_MISMATCH` | Les données reçues ne correspondent pas au CRC32 annoncé — retentez l'envoi. |
| 4 | `WRITE_ERROR` | Échec d'écriture (carte SD absente, pleine, ou en lecture seule). |
| 5 | `TOO_LARGE` | Fichier de plus de 512 Ko. |
| 6 | `OK_RENAMED` | Écrit, mais sous un autre nom (écran interactif, joueur a choisi « Renommer ») — la réponse continue avec le nom final (voir ci-dessous). |
| 7 | `CANCELLED` | Le joueur a choisi « Annuler » (ou n'a pas répondu à temps) sur l'écran interactif. |

Si l'appareil ne reçoit pas une trame complète et valide avant la fin de sa fenêtre d'écoute, il **ne
répond rien** et reprend son fonctionnement normal (lancement du jeu, etc.) : un PC qui n'envoie rien,
ou qui envoie du bruit, ne bloque jamais l'appareil plus longtemps que la fenêtre elle-même.

### Réponse `OK_RENAMED` (statut 6) — nom final

Seulement possible depuis l'écran interactif, quand le joueur choisit « Renommer ». Après les 7 octets
habituels, la réponse continue par le chemin final réellement utilisé (celui que l'IDE doit afficher à
l'utilisateur — par exemple `"cassebriques/main (2).lua"`) :

```
octets 7-8    longueur du chemin final, u16
octets ...    chemin final, UTF-8, sans le caractère nul
```

Le nom est choisi en ajoutant ` (2)`, ` (3)`… avant l'extension, dans le même dossier — premier nom
libre. **Un client qui envoie une trame vers l'écran interactif doit donc lire une réponse de longueur
VARIABLE** (7 octets, puis vérifier le statut, puis éventuellement 2 + N octets de plus) plutôt qu'une
taille fixe — voir `SerialTransferService.cs` côté AKA-IDE pour un exemple de lecture correcte.

### Délai d'attente côté PC

Envoyer vers un appareil en écran interactif peut prendre du temps si le fichier existe déjà (le joueur
doit choisir). Un client qui attend une réponse fixe (quelques secondes) échouera à tort : prévoir un
délai d'au moins **35 secondes** pour un `PUT`, le temps que l'appareil laisse au joueur (30 s) plus une
marge.

### Résistant au bruit

L'appareil resynchronise sur le magique `AKAT` octet par octet : du texte qui traînerait sur la même
ligne série (un `print()` de fin de partie, par exemple) n'empêche pas la trame suivante d'être
reconnue, tant qu'elle commence bien par `AKAT`.

## Utiliser `tools/akatransfer.py`

```sh
# Sur un vrai port série (pip install pyserial) :
python tools/akatransfer.py --port /dev/ttyUSB0 --lang lua --dest cassebriques/main.lua main.lua

# Sans matériel, avec le simulateur (fenêtre d'écoute sur l'entrée standard) :
python tools/akatransfer.py --lang lua --dest cassebriques/main.lua --out trame.bin main.lua
./build-sim/akalove-sim --home /chemin/vers/AKA_Love --transfer-window-ms 800 < trame.bin
```

## Ce qui reste à faire

- **Essai sur un vrai port série** : `hal_aka.cpp` lit `stdin` en non bloquant (`fcntl`/`read`), en
  supposant qu'ESP-IDF y redirige déjà la console UART — à confirmer sur la carte (voir le commentaire
  dans ce fichier).
- **Un vrai menu** pour entrer dans l'écran interactif : aujourd'hui, bouton D tenu au démarrage (le
  temps que la phase P6 — lanceur de jeux — existe et puisse proposer « Recevoir un code » comme une
  option normale).
- **Commande `LIST`** (lister les fichiers déjà présents pour un langage) et **`GET`** (relire un
  fichier), pour un futur panneau « fichiers sur l'appareil » côté AKA-IDE — pas nécessaires pour un
  simple bouton « Envoyer ».
- **MicroPython AKA** : ce dépôt ne fait que réserver `lang_id = 1` ; l'implémenter côté MicroPython
  (écrire dans `micropython/py/`, et son propre écran « Recevoir un code » s'il en a envie) reste à
  faire dans son propre dépôt — le protocole (y compris le dialogue de conflit) est déjà commun.
