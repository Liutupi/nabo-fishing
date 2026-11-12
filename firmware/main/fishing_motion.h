#pragma once
#include "lvgl.h"

typedef enum {
    NABO_IDLE, NABO_WINDUP, NABO_CAST, NABO_HOOK, NABO_POSE_COUNT,
} nabo_pose_t;

// Shore backdrop for a season (spring to winter) and period (dawn, day, dusk, night).
const lv_image_dsc_t *fishing_shore(unsigned season, unsigned period);
extern const lv_image_dsc_t fishing_tank_backdrop;  // 320x240 landscape aquarium
const lv_image_dsc_t *fishing_pose(nabo_pose_t pose, bool night);
lv_point_precise_t fishing_grip(nabo_pose_t pose);

// Single scratch sprite shared by the result and catalog under the LVGL lock.
const lv_image_dsc_t *fishing_fish_frame(unsigned species, unsigned frame);

enum { FISHING_TANK_FISH_W = 72, FISHING_TANK_FISH_H = 52 };
// Each aquarium slot owns a sprite. The fish is scaled to `width` pixels wide
// (at most FISHING_TANK_FISH_W) and mirrored when it swims to the left.
const lv_image_dsc_t *fishing_tank_fish(unsigned slot, unsigned species, unsigned width,
                                        bool faces_left, unsigned frame);
