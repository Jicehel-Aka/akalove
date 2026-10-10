# Simulateur PC sans fenêtre : cmake -S sim -B build-sim && cmake --build build-sim
cmake_minimum_required(VERSION 3.16)
project(akalove_sim C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(AK ${ROOT}/components/akalove)

file(GLOB LUA_SRC ${ROOT}/components/lua51/src/*.c)
add_library(lua51 STATIC ${LUA_SRC})
target_include_directories(lua51 PUBLIC ${ROOT}/components/lua51/src)
target_compile_options(lua51 PRIVATE -w)

add_executable(akalove-sim
  main_sim.cpp hal_sim.cpp
  ${AK}/runtime.cpp ${AK}/love_gfx.cpp ${AK}/bind_graphics.cpp ${AK}/bind_system.cpp ${AK}/bind_fs.cpp ${AK}/game_select.cpp ${AK}/splash.cpp ${AK}/image_decode.cpp ${AK}/stb_impl.cpp ${AK}/bind_audio.cpp ${AK}/transfer.cpp
  ${AK}/embedded_lua.cpp)
target_include_directories(akalove-sim PRIVATE ${AK} ${AK}/include ${CMAKE_CURRENT_LIST_DIR}
                                                  ${ROOT}/components/aka_font/include)
# Même exigence que la console (-Werror) sur le code du runtime ; embedded_lua.cpp est généré.
target_compile_options(akalove-sim PRIVATE -Wall -Wextra -Werror -fno-exceptions -fno-rtti)  # comme ESP-IDF
set_source_files_properties(${AK}/stb_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")   # code tiers
target_link_libraries(akalove-sim PRIVATE lua51 m)
if(MINGW)   # exécutable autonome : aucune DLL du compilateur à livrer
  target_link_options(akalove-sim PRIVATE -static)
endif()

# ---------------------------------------------------------------------------
# Simulateur PC FENÊTRÉ + son réel (SDL2) : pour JOUER en vrai sur PC.
#   cmake -S sim -B build-sim && cmake --build build-sim   (cible auto si SDL2 présent)
# Binaire : aka-love-sim[.exe]  — lancé par AKA-IDE : aka-love-sim "<dossier du jeu>"
# Mêmes sources/runtime que akalove-sim ; seule la couche HAL change (hal_sdl.cpp au
# lieu de hal_sim.cpp). hal_sdl.cpp fournit aussi les shims sim::config()/sim::frame().
# Si SDL2 est introuvable, la cible est simplement ignorée (le sim sans fenêtre reste bâti).
# ---------------------------------------------------------------------------
find_package(SDL2 QUIET)
if(SDL2_FOUND)
  add_executable(aka-love-sim
    main_sim.cpp hal_sdl.cpp
    ${AK}/runtime.cpp ${AK}/love_gfx.cpp ${AK}/bind_graphics.cpp ${AK}/bind_system.cpp ${AK}/bind_fs.cpp ${AK}/game_select.cpp ${AK}/splash.cpp ${AK}/image_decode.cpp ${AK}/stb_impl.cpp ${AK}/bind_audio.cpp ${AK}/transfer.cpp
    ${AK}/embedded_lua.cpp)
  target_include_directories(aka-love-sim PRIVATE ${AK} ${AK}/include ${CMAKE_CURRENT_LIST_DIR}
                                                    ${ROOT}/components/aka_font/include)
  # -Werror retiré ici : les en-têtes SDL2 déclenchent des avertissements hors de notre contrôle.
  target_compile_options(aka-love-sim PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
  # On lie UNIQUEMENT la cible moderne SDL2::SDL2 (le coeur). On évite la variable
  # ${SDL2_LIBRARIES} qui, sous MinGW, tire SDL2main : son WinMain appelle SDL_main,
  # qu'on n'a pas (notre main() est dans main_sim.cpp, SDL_MAIN_HANDLED dans hal_sdl.cpp)
  # -> sinon « undefined reference to SDL_main » à l'édition de liens Windows.
  if(TARGET SDL2::SDL2)
    target_link_libraries(aka-love-sim PRIVATE lua51 m SDL2::SDL2)
  else()
    target_include_directories(aka-love-sim PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(aka-love-sim PRIVATE lua51 m ${SDL2_LIBRARIES})
  endif()
  if(MINGW)   # SDL2 livré en DLL à côté ; on fige seulement le runtime GCC
    target_link_options(aka-love-sim PRIVATE -static-libgcc -static-libstdc++)
  endif()
  message(STATUS "aka-love-sim (SDL2, fenêtré + son) : ACTIVÉ")
else()
  message(STATUS "aka-love-sim (SDL2, fenêtré + son) : SDL2 introuvable -> cible ignorée")
endif()
