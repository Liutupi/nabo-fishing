#pragma once

#include "fishing_model.h"

// Visual feedback only. These timers never gate input, tension, or the save.

enum {
    FISH_POSE_IDLE = 0,
    FISH_POSE_WINDUP = 1,
    FISH_POSE_CAST = 2,
    FISH_POSE_HOOK = 3,
    FISH_FX_ACT_MS = 160,
    FISH_FX_HOOK_MS = 150,
    FISH_FX_BITE_ALERT_MS = 240,
    FISH_FX_MISS_MS = 420,
    FISH_FX_LAND_MS = 660,
};

typedef enum { FISH_FX_NONE, FISH_FX_REEL, FISH_FX_SLACK, FISH_FX_HOOK } fish_fx_t;

typedef struct {
    uint32_t reel_at, slack_at, hook_at;
    uint8_t reel_live, slack_live, hook_live;
} fish_fx_state_t;

// pose is a FISH_POSE_* value. ripple: 0 = caller default, -1 = hide, >0 = width.
typedef struct {
    int pose;
    int tip_x, tip_y;
    int bob_x, bob_y;
    int sag;
    int hide_line, hide_bobber;
    int ripple;
} fish_pose_view_t;

void fish_fx_clear(fish_fx_state_t *fx);

// `before` is the page before fish_press. fish_press ticks first, so a bite
// that arrives on the same press still has before == FISH_WAIT.
void fish_fx_press(fish_fx_state_t *fx, fish_button_t button, fish_page_t before,
                   fish_page_t after, fish_result_t result, uint32_t now);

fish_fx_t fish_fx_active(const fish_fx_state_t *fx, fish_page_t page,
                         fish_result_t result, uint32_t now);

int fish_wait_bob(uint32_t now);
int fish_landing_ms(fish_result_t result);
void fish_bite_view(unsigned age, fish_pose_view_t *view);
void fish_fight_view(const fish_fx_state_t *fx, int pulling, int landed, uint32_t now,
                     fish_pose_view_t *view);
void fish_fail_view(fish_result_t result, unsigned age, fish_pose_view_t *view);
