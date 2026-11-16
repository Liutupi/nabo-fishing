#pragma once

#include <stdbool.h>

typedef enum {
    SOUND_STOP, SOUND_WELCOME, SOUND_CAST, SOUND_BITE,
    SOUND_LAND, SOUND_MISS, SOUND_UPGRADE,
} fishing_sound_t;

#include <stdint.h>
bool fishing_sound_init(uint8_t volume);
void fishing_sound_volume(uint8_t volume);
void fishing_sound_play(fishing_sound_t sound);
void fishing_sound_sleep(void);
