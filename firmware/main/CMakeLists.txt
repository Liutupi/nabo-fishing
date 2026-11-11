idf_component_register(SRCS "fishing_main.c" "fishing_model.c" "fishing_ui.c" "fishing_sound.c" "fishing_font_14.c" "fishing_font_20.c"
    INCLUDE_DIRS "." REQUIRES bsp lvgl esp_lvgl_port esp_timer nvs_flash
    EMBED_FILES "lake.rgb565" "fish.rgb565" "welcome.pcm" "cast.pcm" "bite.pcm" "land.pcm" "miss.pcm" "upgrade.pcm")
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
