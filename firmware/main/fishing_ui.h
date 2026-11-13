#pragma once

#include "fishing_model.h"

void fishing_ui_init(void);
void fishing_ui_reset(void);
// Record a press after fish_press. Visual only: never drops or delays the press.
void fishing_ui_press(fish_button_t button, fish_page_t before, const fish_game_t *game, uint32_t now);
void fishing_ui_draw(const fish_game_t *game, uint32_t now, bool save_error);

void fishing_ui_boot(unsigned elapsed_ms, bool show);
