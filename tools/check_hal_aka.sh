#!/bin/sh
# Vérifie hal_aka.cpp et main.cpp contre les VRAIS en-têtes de la bibliothèque `gamebuino`
# (les en-têtes ESP-IDF sont remplacés par de faux en-têtes minimaux : tools/fake_idf).
# La bibliothèque est copiée dans un dossier temporaire et mise en minuscules (fix_gamebuino_case.py) :
# votre arborescence n'est jamais modifiée.
# Usage : tools/check_hal_aka.sh <dossier components/gamebuino>
set -e
GB="${1:?usage: $0 <dossier components/gamebuino>}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cp -r "$GB/include_lib" "$GB/include_ll" "$TMP/"
python3 "$HERE/tools/fix_gamebuino_case.py" "$TMP"
for f in "$HERE/components/akalove/hal_aka.cpp" "$HERE/main/main.cpp"; do
  g++ -std=gnu++17 -fsyntax-only -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
      -I"$HERE/tools/fake_idf" -I"$TMP/include_lib" -I"$TMP/include_ll" \
      -I"$HERE/components/akalove/include" "$f"
  echo "OK : $f"
done
