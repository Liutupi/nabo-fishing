#pragma once
#include "lvgl.h"

void fishing_intro_init(lv_obj_t *screen);
void fishing_intro_draw(unsigned elapsed_ms, bool show);

// The welcome animation's frame buffer, free for other sprites once the intro is hidden.
enum { FISHING_INTRO_SCRATCH_BYTES = 136 * 220 * 2 };
void *fishing_intro_scratch(void);
