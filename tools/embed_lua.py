#!/usr/bin/env python3
"""Régénère components/akalove/embedded_lua.cpp et include/embedded_lua.h à partir de lua/*.lua.
À relancer après toute modification de boot.lua ou errorhandler.lua (le résultat est versionné)."""
import os, sys

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'components', 'akalove')
scripts = [('EMBEDDED_BOOT_LUA', 'boot.lua'), ('EMBEDDED_ERRORHANDLER_LUA', 'errorhandler.lua')]

with open(os.path.join(root, 'include', 'embedded_lua.h'), 'w') as h:
    h.write('// Généré par tools/embed_lua.py — NE PAS ÉDITER\n#pragma once\n')
    for sym, _ in scripts:
        h.write('extern const char* const %s;\n' % sym)

with open(os.path.join(root, 'embedded_lua.cpp'), 'w') as c:
    c.write('// Généré par tools/embed_lua.py — NE PAS ÉDITER (sources : lua/*.lua)\n#include "embedded_lua.h"\n\n')
    for sym, fn in scripts:
        src = open(os.path.join(root, 'lua', fn), encoding='utf-8').read()
        assert ')LUA"' not in src
        c.write('const char* const %s = R"LUA(%s)LUA";\n\n' % (sym, src))
print('embedded_lua.cpp régénéré')
