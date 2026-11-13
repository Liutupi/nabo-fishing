#include <assert.h>
#include <stdio.h>
#include "fishing_feedback.h"
#include "fishing_model.h"

static void tap(fish_game_t *game, fish_fx_state_t *fx, fish_button_t button, uint32_t now)
{
    fish_page_t before = game->page;
    fish_press(game, button, now);
    fish_fx_press(fx, button, before, game->page, game->result, now);
}

static uint32_t cast_out(fish_game_t *game, fish_fx_state_t *fx, uint32_t time)
{
    fish_init(game, (fish_save_t){.rod = 4, .line = 4}, 7);
    fish_fx_clear(fx);
    tap(game, fx, FISH_OK, time);
    tap(game, fx, FISH_OK, time + 10);
    tap(game, fx, FISH_OK, time + 660);
    assert(game->page == FISH_WAIT);
    assert(game->bite_at - game->cast_at >= 1800 && game->bite_at - game->cast_at <= 4000);
    return game->bite_at;
}

static uint32_t reach_fight(fish_game_t *game, fish_fx_state_t *fx, uint32_t time)
{
    uint32_t bite = cast_out(game, fx, time);
    fish_tick(game, bite - 1);
    assert(game->page == FISH_WAIT);
    // The press itself ticks into the bite, so the recorded page is still WAIT.
    tap(game, fx, FISH_OK, bite + 650);
    assert(game->page == FISH_FIGHT);
    assert(fish_fx_active(fx, FISH_FIGHT, game->result, bite + 650) == FISH_FX_HOOK);
    return bite + 650;
}

static void geometry(void)
{
    fish_pose_view_t calm, pulling, reel, slack, hook, alert, steady, later;
    fish_fx_state_t fx = {0};
    fish_fight_view(&fx, 0, 0, 1000, &calm);
    fish_fight_view(&fx, 1, 0, 1000, &pulling);
    assert(calm.pose == FISH_POSE_WINDUP && pulling.pose == FISH_POSE_WINDUP);
    assert(pulling.tip_x - calm.tip_x < 0 && calm.tip_x - pulling.tip_x <= 12);
    assert(pulling.sag < calm.sag);

    fx.reel_live = 1;
    fx.reel_at = 1000;
    fish_fight_view(&fx, 1, 0, 1000, &reel);
    assert(reel.pose == FISH_POSE_HOOK);
    assert(reel.tip_x < pulling.tip_x - 15 && reel.bob_y < pulling.bob_y);

    fx.reel_live = 0;
    fx.slack_live = 1;
    fx.slack_at = 1000;
    fish_fight_view(&fx, 1, 0, 1000, &slack);
    assert(slack.pose == FISH_POSE_WINDUP);
    assert(slack.tip_x > pulling.tip_x + 15 && slack.sag > pulling.sag + 8);
    assert(slack.bob_x > pulling.bob_x && slack.bob_y > pulling.bob_y);

    fx.slack_live = 0;
    fx.hook_live = 1;
    fx.hook_at = 1040;
    fx.reel_live = 1;
    fx.reel_at = 1000;
    fish_fight_view(&fx, 0, 0, 1000, &hook);
    assert(hook.pose == reel.pose && hook.tip_x == reel.tip_x);
    fish_fight_view(&fx, 0, 0, 1000 + FISH_FX_ACT_MS, &hook);
    assert(hook.pose == FISH_POSE_HOOK && hook.tip_x != reel.tip_x);

    fish_bite_view(0, &alert);
    fish_bite_view(60, &steady);
    fish_bite_view(FISH_FX_BITE_ALERT_MS, &later);
    fish_pose_view_t held;
    fish_bite_view(1500, &held);
    assert(alert.pose == FISH_POSE_WINDUP && later.pose == FISH_POSE_WINDUP);
    assert(alert.bob_y != steady.bob_y);
    assert(later.bob_y == held.bob_y && later.tip_x == held.tip_x && later.pose == held.pose);

    assert(fish_wait_bob(0) == 0 && fish_wait_bob(225) == 3 && fish_wait_bob(600) == 0);
    for (uint32_t t = 0; t < 600; t++) {
        int bob = fish_wait_bob(t);
        assert(bob >= 0 && bob <= 3);
    }

    fish_pose_view_t miss, whiff, escape, sunk, snap, sprung;
    fish_fail_view(FISH_MISSED, 40, &miss);
    fish_fail_view(FISH_MISSED, 300, &whiff);
    fish_fail_view(FISH_TIMEOUT, 40, &escape);
    fish_fail_view(FISH_TIMEOUT, 300, &sunk);
    fish_fail_view(FISH_SNAPPED, 0, &snap);
    fish_fail_view(FISH_SNAPPED, 240, &sprung);
    assert(miss.pose == FISH_POSE_HOOK && !miss.hide_line && !miss.hide_bobber);
    assert(whiff.pose == FISH_POSE_WINDUP && whiff.bob_y < miss.bob_y);
    assert(escape.pose == FISH_POSE_IDLE && !escape.hide_line && escape.sag > 0);
    assert(sunk.bob_y > escape.bob_y && sunk.sag > escape.sag);
    assert(snap.pose == FISH_POSE_IDLE && snap.hide_line && snap.hide_bobber);
    assert(sprung.tip_x > snap.tip_x && sprung.hide_line);
    assert(fish_landing_ms(FISH_CAUGHT) == FISH_FX_LAND_MS);
    assert(fish_landing_ms(FISH_MISSED) == FISH_FX_MISS_MS);
    assert(fish_landing_ms(FISH_TIMEOUT) == FISH_FX_MISS_MS);
    assert(fish_landing_ms(FISH_SNAPPED) == FISH_FX_MISS_MS);
}

static void lifetimes(void)
{
    fish_game_t game;
    fish_fx_state_t fx;
    uint32_t hooked = reach_fight(&game, &fx, 1000);
    uint8_t landed = game.landed;
    uint8_t tension = game.tension;
    assert(!fish_fx_active(&fx, FISH_FIGHT, game.result, hooked + FISH_FX_HOOK_MS));

    tap(&game, &fx, FISH_OK, hooked + 400);
    assert(game.page == FISH_FIGHT);
    assert(game.landed > landed);
    assert(game.tension > tension);
    assert(fish_fx_active(&fx, FISH_FIGHT, game.result, hooked + 400) == FISH_FX_REEL);
    assert(fish_fx_active(&fx, FISH_FIGHT, game.result, hooked + 400 + FISH_FX_ACT_MS - 1) == FISH_FX_REEL);
    uint32_t again = hooked + 400 + FISH_FX_ACT_MS - 1;
    tap(&game, &fx, FISH_OK, again);
    assert(game.landed >= 2);
    assert(fish_fx_active(&fx, FISH_FIGHT, game.result, again + FISH_FX_ACT_MS - 1) == FISH_FX_REEL);
    assert(!fish_fx_active(&fx, FISH_FIGHT, game.result, again + FISH_FX_ACT_MS));

    landed = game.landed;
    tension = game.tension;
    uint32_t slack = again + FISH_FX_ACT_MS + 10;
    tap(&game, &fx, FISH_DOWN, slack);
    assert(game.page == FISH_FIGHT && game.landed == landed && game.tension < tension);
    assert(fish_fx_active(&fx, FISH_FIGHT, game.result, slack) == FISH_FX_SLACK);

    fish_fx_state_t overlap = {.reel_live = 1, .slack_live = 1, .reel_at = 1000, .slack_at = 1030};
    assert(fish_fx_active(&overlap, FISH_FIGHT, FISH_CAUGHT, 1030) == FISH_FX_SLACK);
    assert(fish_fx_active(&overlap, FISH_FIGHT, FISH_CAUGHT, 1170) == FISH_FX_SLACK);
    assert(fish_fx_active(&overlap, FISH_FIGHT, FISH_CAUGHT, 1030 + FISH_FX_ACT_MS) == FISH_FX_NONE);

    tap(&game, &fx, FISH_UP, slack + 40);
    assert(game.page == FISH_HOME);
    assert(fish_fx_active(&fx, FISH_HOME, game.result, slack + 40) == FISH_FX_NONE);
    assert(fish_fx_active(&fx, FISH_FIGHT, game.result, slack + 40) == FISH_FX_NONE);

    hooked = reach_fight(&game, &fx, 50000);
    assert(fish_fx_active(&fx, FISH_FIGHT, game.result, hooked) == FISH_FX_HOOK);

    fish_fx_state_t wrapped = {0};
    wrapped.reel_live = 1;
    wrapped.reel_at = 0xfffffff0u;
    assert(fish_fx_active(&wrapped, FISH_FIGHT, FISH_CAUGHT, 0x10) == FISH_FX_REEL);
    assert(fish_fx_active(&wrapped, FISH_FIGHT, FISH_CAUGHT, 0xfffffff0u + FISH_FX_ACT_MS) == FISH_FX_NONE);

    uint32_t bite = cast_out(&game, &fx, 80000);
    tap(&game, &fx, FISH_OK, bite);
    assert(game.page == FISH_RESULT && game.result == FISH_MISSED);
    assert(fish_fx_active(&fx, FISH_RESULT, FISH_MISSED, bite) == FISH_FX_HOOK);
    assert(fish_fx_active(&fx, FISH_RESULT, FISH_MISSED, bite + FISH_FX_HOOK_MS) == FISH_FX_NONE);

    bite = cast_out(&game, &fx, 90000);
    fish_tick(&game, bite + 5200);
    assert(game.page == FISH_RESULT && game.result == FISH_TIMEOUT);
    assert(fish_fx_active(&fx, FISH_RESULT, FISH_TIMEOUT, bite + 5200) == FISH_FX_NONE);

    reach_fight(&game, &fx, 100000);
    uint32_t now = game.fight_at;
    while (!fish_pulling(&game, now)) now += 50;
    while (game.page == FISH_FIGHT) tap(&game, &fx, FISH_OK, ++now);
    assert(game.result == FISH_SNAPPED && !game.save.pending);
    assert(fish_fx_active(&fx, FISH_RESULT, FISH_SNAPPED, now) == FISH_FX_NONE);
}

int main(void)
{
    geometry();
    lifetimes();
    puts("PASS: feedback lifetimes, poses, repeated input, exit and restart");
    return 0;
}
