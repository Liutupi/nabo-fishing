#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fishing_model.h"
#include "fishing_motion.h"
#include "fishing_ui.h"

enum { WIDTH = 240, HEIGHT = 320, FRAME_BYTES = 84 * 60 * 3 };
static uint16_t screen[WIDTH * HEIGHT];
static uint16_t strip[WIDTH * 40];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    unsigned w = area->x2 - area->x1 + 1;
    unsigned stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    for (int y = area->y1; y <= area->y2; y++)
        memcpy(screen + y * WIDTH + area->x1, data + (y - area->y1) * stride, w * 2);
    lv_display_flush_ready(display);
}

static void frame(FILE *output, lv_display_t *display)
{
    lv_refr_now(display);
    if (fwrite(screen, sizeof screen, 1, output) != 1) abort();
}

static bool press(fish_game_t *game, fish_button_t button, uint32_t now)
{
    fish_page_t before = game->page;
    bool changed = fish_press(game, button, now);
    fishing_ui_press(button, before, game, now);
    return changed;
}

static void verify_fish(void)
{
    uint8_t base[FRAME_BYTES];
    for (unsigned id = 0; id < FISH_COUNT; id++) {
        const lv_image_dsc_t *sprite = fishing_fish_frame(id, 0);
        assert(sprite && sprite->data_size == sizeof base);
        memcpy(base, sprite->data, sizeof base);
        unsigned painted = 0;
        for (unsigned p = 84 * 60 * 2; p < sizeof base; p++) painted += !!base[p];
        assert(painted > 100 && painted < 84 * 60);
        for (unsigned phase = 1; phase < 12; phase++) {
            sprite = fishing_fish_frame(id, phase);
            unsigned bent_pixels = 0;
            for (unsigned p = 84 * 60 * 2; p < sizeof base; p++) bent_pixels += !!sprite->data[p];
            assert(bent_pixels == painted);
            if (phase == 3) assert(memcmp(base, sprite->data, sizeof base) != 0);
        }
    }
    assert(fishing_fish_frame(FISH_COUNT, 0) == NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *output = fopen(argv[1], "wb");
    if (!output) return 2;
    lv_init();
    verify_fish();
    lv_display_t *display = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_buffers(display, strip, NULL, sizeof strip, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    fishing_ui_init();
    fish_game_t game;
    fish_init(&game, (fish_save_t){.rod = 4}, 42);

    for (unsigned t = 0; t < 3400; t += 40) {
        fishing_ui_boot(t, true);
        frame(output, display);
    }
    fishing_ui_boot(0, false);
    fishing_ui_draw(&game, 3400, false);
    press(&game, FISH_OK, 3440);
    for (unsigned t = 3440; t < 4000; t += 40) {
        fishing_ui_draw(&game, t, false);
        frame(output, display);
    }
    press(&game, FISH_OK, 4000);
    for (unsigned t = 4000; t < 4650; t += 40) {
        fishing_ui_draw(&game, t, false);
        frame(output, display);
    }
    press(&game, FISH_OK, 4650);
    unsigned end = game.bite_at + 640;
    for (unsigned t = 4650; t <= end; t += 40) {
        fish_tick(&game, t);
        fishing_ui_draw(&game, t, false);
        frame(output, display);
    }
    assert(!press(&game, FISH_OK, end) && game.page == FISH_FIGHT);
    while (game.page == FISH_FIGHT) {
        end += 320;
        if (fish_pulling(&game, end) || game.tension > 42) press(&game, FISH_DOWN, end);
        else press(&game, FISH_OK, end);
        for (unsigned t = end; t < end + 320; t += 40) {
            fishing_ui_draw(&game, t, false);
            frame(output, display);
        }
    }
    assert(game.result == FISH_CAUGHT && game.save.pending);
    end += 320;
    for (unsigned t = end; t < end + 1280; t += 40) {
        fishing_ui_draw(&game, t, false);
        frame(output, display);
    }
    end += 1280;
    assert(press(&game, FISH_UP, end));
    printf("All %u fish have an opaque body, transparent background and distinct swim frames.\n", FISH_COUNT);
    if (fclose(output)) return 1;
    return 0;
}
