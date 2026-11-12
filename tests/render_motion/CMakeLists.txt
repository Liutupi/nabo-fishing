cmake_minimum_required(VERSION 3.16)
project(fishing_motion_preview C CXX ASM)
set(LV_BUILD_CONF_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CONFIG_LV_BUILD_DEMOS OFF)
set(CONFIG_LV_BUILD_EXAMPLES OFF)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF)
set(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware")
set(GAME "${FIRMWARE}/main")
add_subdirectory("${FIRMWARE}/managed_components/lvgl__lvgl" lvgl)
configure_file(assets.S.in assets.S @ONLY)
set(GAME_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/assets.S"
    "${GAME}/fishing_motion.c" "${GAME}/fishing_intro.c" "${GAME}/fishing_ui.c" "${GAME}/fishing_tank.c"
    "${GAME}/fishing_model.c" "${GAME}/fishing_font_14.c" "${GAME}/fishing_font_20.c")
# render_motion streams the portrait fishing loop; render_scenes saves stills of
# the shore by season and time, and of the landscape aquarium.
add_executable(render_motion render.c ${GAME_SOURCES})
add_executable(render_scenes render_scenes.c ${GAME_SOURCES})
foreach(tool render_motion render_scenes)
    target_include_directories(${tool} PRIVATE "${GAME}")
    target_link_libraries(${tool} PRIVATE lvgl)
endforeach()
