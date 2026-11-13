// Host capture of the firmware UI at 240x320. Real LVGL frames, no invented tweens.
// Usage: render_feedback <directory> [--dump]
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "fishing_model.h"
#include "fishing_ui.h"

enum { WIDTH = 240, HEIGHT = 320 };
static uint16_t screen[WIDTH * HEIGHT];
static uint16_t strip[WIDTH * 40];
static uint16_t wait_a[WIDTH * HEIGHT], wait_b[WIDTH * HEIGHT];
static uint16_t alert[WIDTH * HEIGHT], steady[WIDTH * HEIGHT], steady_later[WIDTH * HEIGHT];
static uint16_t hook[WIDTH * HEIGHT], struggle[WIDTH * HEIGHT];
static uint16_t reel[WIDTH * HEIGHT], reel_later[WIDTH * HEIGHT];
static uint16_t slack[WIDTH * HEIGHT], slack_later[WIDTH * HEIGHT];
static uint16_t miss[WIDTH * HEIGHT], whiff[WIDTH * HEIGHT];
static uint16_t escape[WIDTH * HEIGHT], snap[WIDTH * HEIGHT];
static fish_game_t game;
static int dumping;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    unsigned w = area->x2 - area->x1 + 1;
    unsigned stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    for (int y = area->y1; y <= area->y2; y++)
        memcpy(screen + y * WIDTH + area->x1, data + (y - area->y1) * stride, w * 2);
    lv_display_flush_ready(display);
}

static void tap(fish_button_t button, uint32_t now)
{
    fish_page_t before = game.page;
    fish_press(&game, button, now);
    fishing_ui_press(button, before, &game, now);
}

static void draw_at(lv_display_t *display, uint32_t now)
{
    fish_tick(&game, now);
    fishing_ui_draw(&game, now, false);
    lv_refr_now(display);
}

static void save(const char *directory, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.rgb565", directory, name);
    FILE *output = fopen(path, "wb");
    if (!output || fwrite(screen, sizeof screen, 1, output) != 1 || fclose(output)) abort();
}

static void shot(lv_display_t *display, const char *directory, const char *name, uint32_t now,
                 uint16_t *copy)
{
    draw_at(display, now);
    if (copy) memcpy(copy, screen, sizeof screen);
    save(directory, name);
}

static unsigned lake_diff(const uint16_t *a, const uint16_t *b)
{
    unsigned changed = 0;
    for (int y = 0; y < 148; y++)
        for (int x = 0; x < WIDTH; x++)
            changed += a[y * WIDTH + x] != b[y * WIDTH + x];
    return changed;
}

static int bobber_y(void)
{
    int count = 0, sum = 0;
    for (int y = 40; y < 150; y++) {
        for (int x = 160; x < 230; x++) {
            uint16_t pixel = screen[y * WIDTH + x];
            int red = pixel >> 11, green = (pixel >> 5) & 63, blue = pixel & 31;
            if (red >= 26 && green >= 18 && green <= 40 && blue <= 14) {
                count++;
                sum += y;
            }
        }
    }
    return count > 8 ? sum / count : -1;
}

static uint32_t cast_out(uint32_t time)
{
    fishing_ui_reset();
    fish_init(&game, (fish_save_t){.rod = 4, .line = 1}, 7);
    tap(FISH_OK, time);
    tap(FISH_OK, time + 10);
    tap(FISH_OK, time + 660);
    assert(game.page == FISH_WAIT);
    return game.bite_at;
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3) return 2;
    dumping = argc == 3;
    lv_init();
    lv_display_t *display = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_buffers(display, strip, NULL, sizeof strip, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    fishing_ui_init();
    fishing_ui_boot(0, false);

    uint32_t bite = cast_out(10000);
    shot(display, argv[1], "wait-a", game.cast_at + 450, wait_a);
    int wait_high = bobber_y();
    shot(display, argv[1], "wait-b", game.cast_at + 750, wait_b);
    int wait_low = bobber_y();
    shot(display, argv[1], "bite-alert", bite + 40, alert);
    shot(display, argv[1], "bite-steady", bite + 500, steady);
    shot(display, argv[1], "bite-steady-later", bite + 800, steady_later);
    assert(game.page == FISH_BITE);
    tap(FISH_OK, bite + 800);
    assert(game.page == FISH_FIGHT);
    shot(display, argv[1], "hook", bite + 800, hook);
    shot(display, argv[1], "struggle", bite + 1000, struggle);
    tap(FISH_OK, bite + 1100);
    assert(game.page == FISH_FIGHT && game.landed > 0);
    shot(display, argv[1], "reel", bite + 1100, reel);
    shot(display, argv[1], "reel-later", bite + 1300, reel_later);
    uint8_t landed = game.landed;
    tap(FISH_DOWN, bite + 1500);
    assert(game.page == FISH_FIGHT && game.landed == landed);
    shot(display, argv[1], "slack", bite + 1500, slack);
    shot(display, argv[1], "slack-later", bite + 1700, slack_later);

    bite = cast_out(40000);
    tap(FISH_OK, bite);
    assert(game.result == FISH_MISSED);
    shot(display, argv[1], "miss-set", bite, miss);
    shot(display, argv[1], "miss-whiff", bite + 280, whiff);

    bite = cast_out(60000);
    draw_at(display, bite + 40);
    assert(game.page == FISH_BITE);
    shot(display, argv[1], "escape", bite + 5200, escape);
    assert(game.result == FISH_TIMEOUT);

    bite = cast_out(80000);
    tap(FISH_OK, bite + 650);
    assert(game.page == FISH_FIGHT);
    draw_at(display, bite + 650);
    uint32_t now = bite + 650;
    while (!fish_pulling(&game, now)) now += 50;
    while (game.page == FISH_FIGHT) tap(FISH_OK, ++now);
    assert(game.result == FISH_SNAPPED);
    shot(display, argv[1], "snap", now, snap);

    if (!dumping) {
        assert(wait_high >= 0 && wait_low >= 0 && wait_high != wait_low);
        assert(lake_diff(steady, steady_later) == 0);
        assert(lake_diff(alert, steady) > 20);
        assert(lake_diff(hook, struggle) > 50);
        assert(lake_diff(reel, reel_later) > 50);
        assert(lake_diff(slack, slack_later) > 50);
        assert(lake_diff(reel, slack) > 50);
        assert(lake_diff(miss, whiff) > 20);
        assert(lake_diff(miss, escape) > 20);
        assert(lake_diff(escape, snap) > 20);
        assert(lake_diff(miss, snap) > 20);
        printf("PASS: firmware UI feedback frames in %s\n", argv[1]);
    } else {
        printf("dumped firmware UI frames in %s\n", argv[1]);
    }
    return 0;
}
