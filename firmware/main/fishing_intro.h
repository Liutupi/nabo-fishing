#pragma once
#include "lvgl.h"
#include "fishing_motion.h"

void fishing_intro_init(lv_obj_t *screen);
void fishing_intro_draw(unsigned elapsed_ms, bool show);

// The welcome animation's frame buffer, free for other sprites once the intro is hidden.
enum { FISHING_INTRO_SCRATCH_BYTES = (FISH_TANK_SLOTS - 1) * FISHING_TANK_FISH_W * FISHING_TANK_FISH_H * 3 };
void *fishing_intro_scratch(void);
