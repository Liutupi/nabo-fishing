idf_component_register(SRCS "fishing_main.c" "fishing_model.c" "fishing_ui.c" "fishing_tank.c" "fishing_motion.c" "fishing_intro.c" "fishing_sound.c" "fishing_font_14.c" "fishing_font_20.c" "fishing_shop_font_14.c" "fishing_shop_font_20.c"
    INCLUDE_DIRS "." REQUIRES bsp lvgl esp_lvgl_port esp_timer nvs_flash
    EMBED_FILES "aquarium.rgb565a" "shore.rgb565" "tank.rgb565" "welcome_cover.rgb565" "intro_poses.pal" "nabo_anim.rgb565a" "nabo_night.rgb565a" "fish_anim.pal" "welcome.pcm" "cast.pcm" "bite.pcm" "land.pcm" "miss.pcm" "upgrade.pcm")
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
