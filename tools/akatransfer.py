#!/usr/bin/env python3
"""Envoie un fichier vers l'AKA (ou le simulateur) avec le protocole AKAT v1.

Sur un vrai port série (nécessite `pip install pyserial`) :
    python tools/akatransfer.py --port /dev/ttyUSB0 --lang lua --dest cassebriques/main.lua main.lua
    python tools/akatransfer.py --port COM5 --lang mpy --dest snake.py snake.py

Sans matériel, pour tester avec le simulateur (écrit la trame dans un fichier, à ré-injecter sur
l'entrée standard du simulateur — voir README, section « Transfert de fichiers ») :
    python tools/akatransfer.py --lang lua --dest cassebriques/main.lua --out trame.bin main.lua
    ./build-sim/akalove-sim --home /chemin/AKA_Love --transfer-window-ms 500 < trame.bin

Voir docs/PROTOCOLE_TRANSFERT.md pour le format exact et le code de statut renvoyé par l'appareil.
"""
import argparse
import struct
import sys
import zlib

MAGIC = b'AKAT'
VERSION = 1
CMD_PUT = 0x02
LANGS = {'lua': 0, 'mpy': 1}
STATUS = {0: 'OK', 1: 'langage inconnu de cet appareil', 2: 'chemin invalide', 3: 'somme de contrôle incorrecte',
          4: "erreur d'écriture (carte SD pleine ou absente ?)", 5: 'fichier trop volumineux',
          6: 'OK (renommé)', 7: "annulé par l'utilisateur sur l'appareil"}
ST_OK_RENAMED = 6


def build_put_frame(lang: str, dest_path: str, data: bytes) -> bytes:
    if lang not in LANGS:
        raise SystemExit("--lang doit valoir 'lua' ou 'mpy'")
    dest = dest_path.replace('\\', '/').encode('utf-8')
    if len(dest) >= 220:
        raise SystemExit('chemin de destination trop long')
    crc = zlib.crc32(data) & 0xFFFFFFFF
    return (MAGIC + bytes([VERSION, CMD_PUT, LANGS[lang]]) + struct.pack('<H', len(dest)) + dest +
            struct.pack('<I', len(data)) + data + struct.pack('<I', crc))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('fichier', help='fichier local à envoyer')
    ap.add_argument('--lang', required=True, choices=['lua', 'mpy'], help='lua = AKA-Love, mpy = MicroPython')
    ap.add_argument('--dest', required=True, help="chemin sur l'appareil, relatif à games/ (lua) ou à la racine (mpy)")
    ap.add_argument('--port', help='port série réel (ex. /dev/ttyUSB0, COM5) — nécessite pyserial')
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('--out', help="au lieu d'un port réel : écrit la trame dans ce fichier (pour le simulateur)")
    args = ap.parse_args()

    data = open(args.fichier, 'rb').read()
    frame = build_put_frame(args.lang, args.dest, data)

    if args.out:
        open(args.out, 'wb').write(frame)
        print('Trame écrite dans %s (%d octets, dont %d de contenu).' % (args.out, len(frame), len(data)))
        return
    if not args.port:
        raise SystemExit('donnez --port (matériel réel) ou --out (test avec le simulateur)')

    import serial   # pyserial : pip install pyserial (import différé pour que --out marche sans)
    # Délai généreux : si l'appareil est sur son écran interactif "Recevoir un code" et que le fichier
    # existe déjà, il attend jusqu'à 30 s que le joueur choisisse (Écraser/Renommer/Annuler) avant de
    # répondre. Voir docs/PROTOCOLE_TRANSFERT.md, section "Délai d'attente côté PC".
    with serial.Serial(args.port, args.baud, timeout=35) as ser:
        ser.write(frame)
        reply = ser.read(7)
        if len(reply) == 7 and reply[5] == ST_OK_RENAMED:
            length = struct.unpack('<H', ser.read(2))[0]
            reply += ser.read(length)
    if len(reply) < 7 or reply[:4] != MAGIC:
        raise SystemExit("Pas de réponse (ou réponse invalide) de l'appareil — vérifiez le port et le câble.")
    status = reply[5]
    if status == 0:
        print('Envoyé : %s -> %s (%d octets).' % (args.fichier, args.dest, len(data)))
    elif status == ST_OK_RENAMED:
        final_name = reply[9:].decode('utf-8', 'replace')
        print('Envoyé : %s -> %s (renommé par l\'utilisateur, %d octets).' % (args.fichier, final_name, len(data)))
    else:
        raise SystemExit('Échec (%s) : %s' % (status, STATUS.get(status, '?')))


if __name__ == '__main__':
    main()
