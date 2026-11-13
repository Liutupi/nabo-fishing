// Renders stills with the firmware's own UI code: the shore in several seasons
// and periods, the catalog, and the landscape aquarium with its care screens.
// Usage: render_scenes <output directory>. Each file is raw RGB565 named
// NN-name-WIDTHxHEIGHT.rgb565; scenes.py turns them into one PNG sheet.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "fishing_model.h"
#include "fishing_ui.h"

static uint16_t screen[320 * 320];
static uint16_t strip[240 * 40];
static const char *directory;
static lv_display_t *display;
static fish_game_t game;
static uint32_t now = 5000;
static unsigned saved;

static void flush(lv_display_t *target, const lv_area_t *area, uint8_t *data)
{
    unsigned w = area->x2 - area->x1 + 1;
    unsigned stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    assert(area->x2 < lv_display_get_horizontal_resolution(target) &&
           area->y2 < lv_display_get_vertical_resolution(target));
    for (int y = area->y1; y <= area->y2; y++)
        memcpy(screen + y * 320 + area->x1, data + (y - area->y1) * stride, w * 2);
    lv_display_flush_ready(target);
}

static void run(unsigned ms)
{
    for (uint32_t end = now + ms; now < end; now += 40) {
        fish_tick(&game, now);
        fishing_ui_draw(&game, now, false);
        lv_refr_now(display);
    }
}

static void press(fish_button_t button)
{
    fish_page_t before = game.page;
    fish_press(&game, button, now);
    fishing_ui_press(button, before, &game, now);
    run(40);
}

static void save(const char *name)
{
    char path[512];
    int w = lv_display_get_horizontal_resolution(display), h = lv_display_get_vertical_resolution(display);
    snprintf(path, sizeof path, "%s/%02u-%s-%dx%d.rgb565", directory, saved++, name, w, h);
    FILE *output = fopen(path, "wb");
    if (!output) abort();
    for (int y = 0; y < h; y++)
        if (fwrite(screen + y * 320, 2, w, output) != (size_t)w) abort();
    if (fclose(output)) abort();
}

static void start(fish_save_t progress)
{
    fish_init(&game, progress, 42);
    run(400);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    directory = argv[1];
    lv_init();
    display = lv_display_create(240, 320);
    lv_display_set_buffers(display, strip, NULL, sizeof strip, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    fishing_ui_init();
    fishing_ui_boot(0, false);

    static const struct { unsigned season, period; const char *name; } shores[] = {
        {0, 0, "spring-dawn"}, {1, 1, "summer-day"}, {2, 2, "autumn-dusk"},
        {3, 3, "winter-night"}, {1, 3, "summer-night"},
    };
    for (unsigned i = 0; i < sizeof shores / sizeof shores[0]; i++) {
        start((fish_save_t){.rod = 2, .line = 1, .coins = 37,
                            .clock = shores[i].season * 36 + shores[i].period * 3});
        save(shores[i].name);
    }

    // Resting moves the shore on by one period.
    start((fish_save_t){.rod = 1, .line = 1, .clock = 5});
    press(FISH_OK);
    for (unsigned i = 0; i < 5; i++) press(FISH_DOWN);
    save("prep-rest");
    press(FISH_OK);
    assert(game.save.clock == 6 && fish_period(game.save.clock) == 2);
    for (unsigned i = 0; i < 2; i++) press(FISH_UP);    // back to bait, then cast and wait
    press(FISH_OK);
    press(FISH_OK);
    run(600);
    assert(game.page == FISH_WAIT && game.save.clock == 7);
    save("waiting-at-dusk");

    fish_save_t full = {.rod = 4, .line = 2, .coins = 12, .clock = 40,
                        .seen = {UINT32_MAX, UINT32_MAX, UINT32_MAX, 15}};
    start(full);
    press(FISH_DOWN); press(FISH_DOWN); press(FISH_OK);  // catalog
    press(FISH_OK);
    for (unsigned i = 0; i < 4; i++) press(FISH_DOWN);
    run(200);
    save("catalog-night-fish");

    full.tank[0] = (fish_pet_t){.species = 4, .growth = 3};
    full.tank[1] = (fish_pet_t){.species = 27, .growth = 0};
    full.tank[2] = (fish_pet_t){.species = 83, .growth = 2};
    full.tank[3] = (fish_pet_t){.species = 58, .growth = 1};
    full.tank[4] = (fish_pet_t){.species = 100, .growth = 3};
    full.tank[5] = (fish_pet_t){.species = 51, .growth = 2};
    start(full);
    press(FISH_UP);
    press(FISH_OK);
    assert(game.page == FISH_TANK && lv_display_get_horizontal_resolution(display) == 320);
    run(3000);
    save("tank");
    run(5000);
    save("tank-later");
    press(FISH_DOWN); press(FISH_DOWN);
    press(FISH_OK);
    assert(game.page == FISH_CARE);
    press(FISH_OK);
    assert(game.save.tank[2].fed && game.save.coins == 10);
    run(480);
    save("tank-feeding");
    press(FISH_DOWN);
    save("tank-sell");
    press(FISH_OK);
    assert(!game.save.tank[2].species);
    run(400);
    save("tank-empty-slot");
    for (unsigned i = 0; i < 4; i++) press(FISH_DOWN);
    save("tank-exit");
    press(FISH_OK);
    assert(game.page == FISH_HOME && lv_display_get_horizontal_resolution(display) == 240);
    run(400);
    save("home-after-tank");
    // The catalog sprite lent a buffer to the aquarium; it must draw cleanly again.
    press(FISH_UP); press(FISH_OK); press(FISH_OK);
    run(200);
    save("catalog-after-tank");

    start((fish_save_t){.rod = 1, .line = 1});
    press(FISH_UP); press(FISH_OK);
    run(400);
    save("tank-empty");
    printf("%u scenes rendered\n", saved);
    return 0;
}
