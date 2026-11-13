#include "fishing_feedback.h"

static int clamp_i(int value, int low, int high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int within(uint32_t now, uint32_t at, int live, unsigned window)
{
    return live && (now - at) < window;
}

void fish_fx_clear(fish_fx_state_t *fx) { *fx = (fish_fx_state_t){0}; }

void fish_fx_press(fish_fx_state_t *fx, fish_button_t button, fish_page_t before,
                   fish_page_t after, fish_result_t result, uint32_t now)
{
    int entered_fight = button == FISH_OK && after == FISH_FIGHT && before != FISH_FIGHT;
    int missed = button == FISH_OK && after == FISH_RESULT && result == FISH_MISSED;
    if (entered_fight || missed) {
        fx->hook_at = now;
        fx->hook_live = 1;
    }
    if (before == FISH_FIGHT && after == FISH_FIGHT) {
        if (button == FISH_OK) {
            fx->reel_at = now;
            fx->reel_live = 1;
        } else if (button == FISH_DOWN) {
            fx->slack_at = now;
            fx->slack_live = 1;
        }
    }
    int keep = after == FISH_FIGHT || after == FISH_BITE ||
               (after == FISH_RESULT && result == FISH_MISSED);
    if (!keep) fx->reel_live = fx->slack_live = fx->hook_live = 0;
}

fish_fx_t fish_fx_active(const fish_fx_state_t *fx, fish_page_t page,
                         fish_result_t result, uint32_t now)
{
    if (page == FISH_FIGHT) {
        int reel = within(now, fx->reel_at, fx->reel_live, FISH_FX_ACT_MS);
        int slack = within(now, fx->slack_at, fx->slack_live, FISH_FX_ACT_MS);
        if (reel && slack) {
            return (now - fx->reel_at) <= (now - fx->slack_at) ? FISH_FX_REEL : FISH_FX_SLACK;
        }
        if (reel) return FISH_FX_REEL;
        if (slack) return FISH_FX_SLACK;
        if (within(now, fx->hook_at, fx->hook_live, FISH_FX_HOOK_MS)) return FISH_FX_HOOK;
    }
    if (page == FISH_RESULT && result == FISH_MISSED &&
        within(now, fx->hook_at, fx->hook_live, FISH_FX_HOOK_MS)) {
        return FISH_FX_HOOK;
    }
    return FISH_FX_NONE;
}

int fish_wait_bob(uint32_t now)
{
    static const int8_t table[8] = {0, 1, 2, 3, 3, 2, 1, 0};
    return table[(now / 75u) % 8u];
}

int fish_landing_ms(fish_result_t result)
{
    return result == FISH_CAUGHT ? FISH_FX_LAND_MS : FISH_FX_MISS_MS;
}

static void place(fish_pose_view_t *view, int pose, int tip_x, int tip_y,
                  int bob_x, int bob_y, int sag, int ripple)
{
    view->pose = pose;
    view->tip_x = clamp_i(tip_x, 16, 220);
    view->tip_y = clamp_i(tip_y, 8, 96);
    view->bob_x = clamp_i(bob_x, 36, 214);
    view->bob_y = clamp_i(bob_y, 64, 146);
    view->sag = sag < 0 ? 0 : sag;
    view->hide_line = 0;
    view->hide_bobber = 0;
    view->ripple = ripple;
}

void fish_bite_view(unsigned age, fish_pose_view_t *view)
{
    if (age < FISH_FX_BITE_ALERT_MS) {
        int dip = (age / 60u) % 2u;
        place(view, FISH_POSE_WINDUP, 154, 36 + dip * 5, 181, 110 + dip * 8, 0, dip ? 22 : 14);
    } else {
        place(view, FISH_POSE_WINDUP, 158, 42, 181, 118, 0, 16);
    }
}

void fish_fight_view(const fish_fx_state_t *fx, int pulling, int landed, uint32_t now,
                     fish_pose_view_t *view)
{
    int bob_x = 181 - 4 * landed;
    fish_fx_t action = fish_fx_active(fx, FISH_FIGHT, FISH_CAUGHT, now);
    if (action == FISH_FX_REEL) {
        place(view, FISH_POSE_HOOK, 122, 22, bob_x - 14, 106, 0, 22);
    } else if (action == FISH_FX_SLACK) {
        place(view, FISH_POSE_WINDUP, 198, 66, bob_x + 16, 126, 20, 12);
    } else if (action == FISH_FX_HOOK) {
        place(view, FISH_POSE_HOOK, 126, 24, bob_x - 6, 108, 0, 20);
    } else if (pulling) {
        place(view, FISH_POSE_WINDUP, 156, 40, bob_x, 116, 1, 16);
    } else {
        place(view, FISH_POSE_WINDUP, 164, 46, bob_x, 114, 3, 14);
    }
}

void fish_fail_view(fish_result_t result, unsigned age, fish_pose_view_t *view)
{
    if (result == FISH_SNAPPED) {
        int tip_x = 132 + (int)age / 6;
        int tip_y = 34 + (int)age / 14;
        place(view, FISH_POSE_IDLE, tip_x, tip_y, 181, 114, 0, -1);
        view->hide_line = 1;
        view->hide_bobber = 1;
        return;
    }
    if (result == FISH_TIMEOUT) {
        place(view, FISH_POSE_IDLE, 172, 50, 188, 118 + (int)age / 18, 8 + (int)age / 24,
              age > 300 ? -1 : 18 - (int)age / 24);
        return;
    }
    if (age < FISH_FX_HOOK_MS) {
        place(view, FISH_POSE_HOOK, 126, 24, 176, 108, 0, 18);
        return;
    }
    unsigned rise = age - FISH_FX_HOOK_MS;
    place(view, FISH_POSE_WINDUP, 148 - (int)rise / 10, 30 - (int)rise / 18,
          170 - (int)rise / 8, 100 - (int)rise / 5, 0, 12);
}
