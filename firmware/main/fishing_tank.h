#pragma once

#include "lvgl.h"
#include "fishing_model.h"

// Landscape aquarium, 320x240. The badge is held with its keys along the bottom.
void fishing_tank_init(lv_obj_t *screen);
void fishing_tank_show(bool show);
void fishing_tank_draw(const fish_game_t *game, uint32_t now);
void fishing_tank_animate(const fish_game_t *game, uint32_t now);
