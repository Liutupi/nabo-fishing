#pragma once

#include "fishing_model.h"

void fishing_ui_init(void);
void fishing_ui_draw(const fish_game_t *game, uint32_t now, bool save_error);

void fishing_ui_boot(unsigned elapsed_ms, bool show);
