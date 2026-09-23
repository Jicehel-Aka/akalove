# Lua 5.1.5 (PUC-Rio, licence MIT — voir COPYRIGHT). Sources d'origine non modifiées,
# sans lua.c, luac.c ni print.c (pas d'interpréteur ni de compilateur en ligne de commande).
set(LUA_FILES
    lapi.c lauxlib.c lbaselib.c lcode.c ldblib.c ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c
    llex.c lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c lstate.c lstring.c
    lstrlib.c ltable.c ltablib.c ltm.c lundump.c lvm.c lzio.c)
list(TRANSFORM LUA_FILES PREPEND "src/")

idf_component_register(SRCS ${LUA_FILES} INCLUDE_DIRS "src")
# Le code de Lua n'est pas écrit pour -Werror : on n'y touche pas, on masque ses avertissements.
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
