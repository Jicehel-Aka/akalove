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
