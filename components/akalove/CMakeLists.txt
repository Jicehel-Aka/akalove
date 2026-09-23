# Composant AKA-Love (console). Le simulateur PC compile les mêmes sources avec sim/CMakeLists.txt.
idf_component_register(
    SRCS "hal_aka.cpp" "runtime.cpp" "love_gfx.cpp" "bind_graphics.cpp" "bind_system.cpp" "bind_fs.cpp" "game_select.cpp" "splash.cpp" "image_decode.cpp" "stb_impl.cpp" "bind_audio.cpp" "transfer.cpp"
         "embedded_lua.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES lua51 aka_font
    PRIV_REQUIRES gamebuino app_update esp_partition esp_system freertos
)
# Code tiers (stb_image) : compilé sans avertissements, le reste du composant garde ses exigences.
set_source_files_properties("stb_impl.cpp" PROPERTIES COMPILE_OPTIONS "-w")
