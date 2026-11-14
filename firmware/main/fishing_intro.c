#include <stdint.h>
#include <string.h>
#include "fishing_intro.h"
#include "fishing_motion.h"

LV_FONT_DECLARE(fishing_font_14);
LV_FONT_DECLARE(fishing_font_20);
extern const uint8_t welcome_cover_start[] asm("_binary_welcome_cover_rgb565_start");
extern const uint8_t intro_poses_start[] asm("_binary_intro_poses_pal_start");

#define INTRO_W 136
#define INTRO_H 220
#define INTRO_X 6
#define INTRO_Y 42
#define LEAP_AT 1020
#define LEAP_MS 880
#define WATER_Y 200

typedef struct {
    uint32_t offset, length;
    int grip_x, grip_y;
} pose_t;

static const pose_t poses[] = {
#include "fishing_intro_poses.inc"
};
static const lv_image_dsc_t cover = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 240, .h = 320, .stride = 480},
    .data_size = 240 * 320 * 2, .data = welcome_cover_start,
};
static _Alignas(4) uint16_t pose_pixels[FISHING_INTRO_SCRATCH_BYTES / 2];
static const lv_image_dsc_t actor = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = INTRO_W, .h = INTRO_H, .stride = INTRO_W * 2},
    .data_size = INTRO_W * INTRO_H * 2, .data = (const uint8_t *)pose_pixels,
};
_Static_assert(sizeof pose_pixels >= INTRO_W * INTRO_H * 2, "scratch must also fit the intro frame");
void *fishing_intro_scratch(void) { return pose_pixels; }
static lv_obj_t *intro, *actor_image, *caption, *rod, *fish, *splash[6];
static lv_point_precise_t rod_points[3];

static void visible(lv_obj_t *obj, bool show)
{
    if (show) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *plain(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    return obj;
}

static lv_obj_t *text(lv_obj_t *parent, int y, const char *value, unsigned rgb, const lv_font_t *font)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, 14, y);
    lv_obj_set_width(obj, 212);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(obj, value);
    return obj;
}

void fishing_intro_init(lv_obj_t *screen)
{
    intro = plain(screen, 0, 0, 240, 320);
    lv_obj_t *backdrop = lv_image_create(intro);
    lv_image_set_src(backdrop, &cover);
    actor_image = lv_image_create(intro);
    lv_obj_set_pos(actor_image, INTRO_X, INTRO_Y);
    lv_image_set_src(actor_image, &actor);
    rod = lv_line_create(intro);
    lv_obj_set_style_line_width(rod, 2, 0);
    lv_obj_set_style_line_rounded(rod, true, 0);
    lv_obj_set_style_line_color(rod, lv_color_hex(0xc89153), 0);
    lv_obj_t *above_water = plain(intro, 0, 0, 240, WATER_Y);
    fish = lv_image_create(above_water);
    for (unsigned i = 0; i < 6; i++) {
        splash[i] = plain(intro, 0, 0, i % 2 ? 3 : 4, i % 2 ? 3 : 4);
        lv_obj_set_style_bg_color(splash[i], lv_color_hex(0xe4ffe7), 0);
        lv_obj_set_style_bg_opa(splash[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(splash[i], 2, 0);
    }
    text(intro, 18, "NABO 钓鱼小岛", 0xffefd3, &fishing_font_20);
    caption = text(intro, 267, "NABO，陪你钓鱼", 0xffc85e, &fishing_font_20);
    text(intro, 295, "任意键跳过", 0xa6c3bc, &fishing_font_14);
    visible(intro, false);
}

static unsigned pose_at(unsigned elapsed)
{
    if (elapsed < 240 || elapsed >= 2220) return 0;
    if (elapsed < 440 || elapsed >= 2000) return 1;
    return (elapsed - 440) / 195 % 2 ? 3 : 2;
}

static void draw_pose(unsigned index)
{
    static unsigned shown = UINT32_MAX;
    if (shown == index) return;
    shown = index;
    const pose_t *pose = &poses[index];
    const uint8_t *palette = intro_poses_start + pose->offset;
    const uint8_t *span = palette + 512;
    const uint8_t *end = palette + pose->length;
    for (unsigned y = 0; y < INTRO_H; y++) {
        uint16_t *row = pose_pixels + y * INTRO_W;
        memcpy(row, welcome_cover_start + ((INTRO_Y + y) * 240 + INTRO_X) * 2, INTRO_W * 2);
        if (end - span < 2) break;
        unsigned first = *span++, count = *span++;
        if (first + count > INTRO_W || end - span < count) break;
        for (unsigned i = 0; i < count; i++) {
            unsigned color = *span++;
            if (color) row[first + i] = palette[color * 2] | ((uint16_t)palette[color * 2 + 1] << 8);
        }
    }
    lv_obj_invalidate(actor_image);
    int x = pose->grip_x, y = pose->grip_y;
    lv_obj_invalidate(rod);
    rod_points[0] = (lv_point_precise_t){0, y - 54};
    rod_points[1] = (lv_point_precise_t){14, (y - 54) / 3};
    rod_points[2] = (lv_point_precise_t){32, 0};
    lv_obj_set_pos(rod, x, 54);
    lv_line_set_points(rod, rod_points, 3);
}

static int parabola(unsigned t, unsigned duration, int height)
{
    return (int)(4 * height * t * (duration - t) / (duration * duration));
}

static void draw_leap(unsigned elapsed)
{
    unsigned age = elapsed - LEAP_AT;
    bool rising = age < LEAP_MS;
    visible(fish, rising);
    if (rising) {
        static unsigned last_phase = UINT32_MAX;
        unsigned phase = age / 42;
        if (phase != last_phase) {
            last_phase = phase;
            const lv_image_dsc_t *image = fishing_fish_frame(5, phase);
            if (lv_image_get_src(fish) != image) lv_image_set_src(fish, image);
            lv_obj_invalidate(fish);
        }
        int x = 170 + (int)(age * 27 / LEAP_MS);
        int y = WATER_Y + 30 - parabola(age, LEAP_MS, 116);
        lv_obj_set_pos(fish, x - 42, y - 30);
    }
    unsigned spray = age >= LEAP_MS ? age - LEAP_MS : age;
    bool spraying = spray < 235;
    int origin = age >= LEAP_MS ? 197 : 170;
    for (unsigned i = 0; i < 6; i++) {
        visible(splash[i], spraying);
        if (spraying) {
            int x = origin + ((int)i * 2 - 5) * (int)(2 + spray / 22);
            int y = WATER_Y - parabola(spray, 235, 14 + (int)(i % 3) * 8);
            lv_obj_set_pos(splash[i], x, y);
        }
    }
}

void fishing_intro_draw(unsigned elapsed_ms, bool show)
{
    visible(intro, show);
    if (!show) return;
    draw_pose(pose_at(elapsed_ms));
    draw_leap(elapsed_ms);
    unsigned reveal = elapsed_ms > 360 ? elapsed_ms - 360 : 0;
    lv_obj_set_style_text_opa(caption, reveal < 280 ? reveal * 255 / 280 : 255, 0);
}
