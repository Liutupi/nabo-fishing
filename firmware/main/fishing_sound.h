#pragma once

#include <stdbool.h>

typedef enum {
    SOUND_STOP, SOUND_WELCOME, SOUND_CAST, SOUND_BITE,
    SOUND_LAND, SOUND_MISS, SOUND_UPGRADE,
} fishing_sound_t;

bool fishing_sound_init(void);
void fishing_sound_play(fishing_sound_t sound);
void fishing_sound_sleep(void);
