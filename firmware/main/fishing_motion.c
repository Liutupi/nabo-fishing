#include <stdint.h>
#include "fishing_model.h"
#include "fishing_motion.h"
#include "fishing_intro.h"

#define ACTOR_W 140
#define ACTOR_H 148
#define ACTOR_BYTES (ACTOR_W * ACTOR_H * 3)
#define FISH_W 84
#define FISH_H 60
#define FISH_PIXELS (FISH_W * FISH_H)
#define FISH_RECORD (512 + FISH_PIXELS)

extern const uint8_t shore_start[] asm("_binary_shore_rgb565_start");
extern const uint8_t tank_start[] asm("_binary_tank_rgb565_start");
extern const uint8_t nabo_start[] asm("_binary_nabo_anim_rgb565a_start");
extern const uint8_t nabo_night_start[] asm("_binary_nabo_night_rgb565a_start");
extern const uint8_t aquarium_start[] asm("_binary_aquarium_rgb565a_start");
extern const uint8_t fish_start[] asm("_binary_fish_anim_pal_start");

#define SHORE_BYTES (240 * 160 * 2)
#define SHORE_FRAME(i) { \
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, \
               .w = 240, .h = 160, .stride = 480}, \
    .data_size = SHORE_BYTES, .data = shore_start + (i) * SHORE_BYTES, \
}
static const lv_image_dsc_t shores[16] = {
    SHORE_FRAME(0), SHORE_FRAME(1), SHORE_FRAME(2), SHORE_FRAME(3),
    SHORE_FRAME(4), SHORE_FRAME(5), SHORE_FRAME(6), SHORE_FRAME(7),
    SHORE_FRAME(8), SHORE_FRAME(9), SHORE_FRAME(10), SHORE_FRAME(11),
    SHORE_FRAME(12), SHORE_FRAME(13), SHORE_FRAME(14), SHORE_FRAME(15),
};
const lv_image_dsc_t *fishing_shore(unsigned season, unsigned period)
{
    return &shores[(season % 4) * 4 + period % 4];
}

const lv_image_dsc_t fishing_tank_backdrop = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 320, .h = 240, .stride = 640},
    .data_size = 320 * 240 * 2, .data = tank_start,
};

#define AQUARIUM_IMAGE(width, height, offset, bytes, format) { \
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_##format, \
               .w = (width), .h = (height), .stride = (width) * 2}, \
    .data_size = (bytes), .data = aquarium_start + (offset), \
}
static const lv_image_dsc_t aquarium_images[] = {
#include "fishing_aquarium_assets.inc"
};
_Static_assert(sizeof aquarium_images / sizeof aquarium_images[0] ==
               FISH_TANK_TYPES - 1 + FISH_DECOR_COUNT, "aquarium image tables must agree");

const lv_image_dsc_t *fishing_tank_theme(unsigned type)
{
    return type && type < FISH_TANK_TYPES ? &aquarium_images[type - 1] : &fishing_tank_backdrop;
}

const lv_image_dsc_t *fishing_ornament(unsigned id)
{
    return id < FISH_DECOR_COUNT ? &aquarium_images[FISH_TANK_TYPES - 1 + id] : NULL;
}

#define ACTOR_FRAME(start, i) { \
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8, \
               .w = ACTOR_W, .h = ACTOR_H, .stride = ACTOR_W * 2}, \
    .data_size = ACTOR_BYTES, .data = start + (i) * ACTOR_BYTES, \
}
// The second row is the same four poses in moonlight.
static const lv_image_dsc_t poses[2][NABO_POSE_COUNT] = {
    {ACTOR_FRAME(nabo_start, 0), ACTOR_FRAME(nabo_start, 1),
     ACTOR_FRAME(nabo_start, 2), ACTOR_FRAME(nabo_start, 3)},
    {ACTOR_FRAME(nabo_night_start, 0), ACTOR_FRAME(nabo_night_start, 1),
     ACTOR_FRAME(nabo_night_start, 2), ACTOR_FRAME(nabo_night_start, 3)},
};
static const lv_point_precise_t grips[NABO_POSE_COUNT] = {
#include "fishing_anim_anchors.inc"
};

const lv_image_dsc_t *fishing_pose(nabo_pose_t pose, bool night) { return &poses[night][pose]; }
lv_point_precise_t fishing_grip(nabo_pose_t pose) { return grips[pose]; }

static _Alignas(4) uint16_t fish_buffer[FISH_PIXELS * 3 / 2];
static const lv_image_dsc_t fish_sprite = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8,
               .w = FISH_W, .h = FISH_H, .stride = FISH_W * 2},
    .data_size = sizeof fish_buffer,
    .data = (const uint8_t *)fish_buffer,
};

#define WAVE_FRAMES 12
static unsigned catalog_species = FISH_COUNT, catalog_frame, catalog_variant;  // what fish_buffer holds

// Vertical shift of one source column for a tail-wag frame. The art faces right.
static int column_bend(const uint8_t *indices, int x, unsigned frame)
{
    static const int8_t wave[WAVE_FRAMES] = {0, 2, 4, 5, 4, 2, 0, -2, -4, -5, -4, -2};
    const int from_head = FISH_W - 1 - x;
    int bend = wave[frame] * from_head * from_head / ((FISH_W - 1) * (FISH_W - 1));
    // Reduce the bend near an edge when a tall fin fills the card.
    if (bend > 0) {
        for (int y = FISH_H - 1; y >= FISH_H - bend; y--) {
            if (!indices[y * FISH_W + x]) continue;
            return FISH_H - 1 - y;
        }
    } else {
        for (int y = 0; y < -bend; y++) {
            if (!indices[y * FISH_W + x]) continue;
            return -y;
        }
    }
    return bend;
}

static uint16_t recolor(uint16_t color, unsigned variant)
{
    const unsigned r = color >> 11, g = (color >> 5) & 63, b = color & 31;
    if (variant == 1) return (b << 11) | (((r << 1) | (r >> 4)) << 5) | (g >> 1);
    if (variant == 2) return ((g >> 1) << 11) | (((b << 1) | (b >> 4)) << 5) | r;
    return color;
}

const lv_image_dsc_t *fishing_fish_frame(unsigned species, unsigned frame)
{
    return fishing_fish_variant(species, frame, 0);
}

const lv_image_dsc_t *fishing_fish_variant(unsigned species, unsigned frame, unsigned variant)
{
    if (species >= FISH_COUNT) return NULL;
    frame %= WAVE_FRAMES;
    if (catalog_species == species && catalog_frame == frame && catalog_variant == variant) return &fish_sprite;
    catalog_species = species;
    catalog_frame = frame;
    catalog_variant = variant;
    const uint8_t *palette = fish_start + species * FISH_RECORD;
    const uint8_t *indices = palette + 512;
    uint8_t *alpha = (uint8_t *)(fish_buffer + FISH_PIXELS);
    for (int x = 0; x < FISH_W; x++) {
        const int bend = column_bend(indices, x, frame);
        for (int y = 0; y < FISH_H; y++) {
            const int source_y = y - bend;
            const unsigned p = y * FISH_W + x;
            unsigned color = 0;
            if (source_y >= 0 && source_y < FISH_H) color = indices[source_y * FISH_W + x];
            fish_buffer[p] = recolor(palette[color * 2] | ((uint16_t)palette[color * 2 + 1] << 8), variant);
            alpha[p] = color ? 255 : 0;
        }
    }
    return &fish_sprite;
}

#define TANK_BYTES (FISHING_TANK_FISH_W * FISHING_TANK_FISH_H * 3)
_Static_assert((FISH_TANK_SLOTS - 1) * TANK_BYTES <= FISHING_INTRO_SCRATCH_BYTES &&
               TANK_BYTES <= sizeof fish_buffer, "aquarium sprites must fit the borrowed buffers");
static lv_image_dsc_t tank_sprites[FISH_TANK_SLOTS];

const lv_image_dsc_t *fishing_tank_fish(unsigned slot, unsigned species, unsigned width,
                                        bool faces_left, unsigned frame, unsigned variant)
{
    if (slot >= FISH_TANK_SLOTS || species >= FISH_COUNT || !width) return NULL;
    // Reuse the intro scratch for eleven sprites and catalog scratch for one.
    uint8_t *data = slot ? (uint8_t *)fishing_intro_scratch() + (slot - 1) * TANK_BYTES
                         : (uint8_t *)fish_buffer;
    if (!slot) catalog_species = FISH_COUNT;
    const int w = width > FISHING_TANK_FISH_W ? FISHING_TANK_FISH_W : (int)width;
    const int h = (w * FISH_H + FISH_W / 2) / FISH_W;
    uint16_t *colors = (uint16_t *)data;
    uint8_t *alpha = data + w * h * 2;
    const uint8_t *palette = fish_start + species * FISH_RECORD;
    const uint8_t *indices = palette + 512;
    frame %= WAVE_FRAMES;
    for (int x = 0; x < w; x++) {
        const int source_x = (faces_left ? w - 1 - x : x) * FISH_W / w;
        const int bend = column_bend(indices, source_x, frame);
        for (int y = 0; y < h; y++) {
            const int source_y = y * FISH_H / h - bend;
            unsigned color = 0;
            if (source_y >= 0 && source_y < FISH_H) color = indices[source_y * FISH_W + source_x];
            colors[y * w + x] = recolor(palette[color * 2] | ((uint16_t)palette[color * 2 + 1] << 8), variant);
            alpha[y * w + x] = color ? 255 : 0;
        }
    }
    lv_image_dsc_t *sprite = &tank_sprites[slot];
    *sprite = (lv_image_dsc_t){
        .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565A8,
                   .w = w, .h = h, .stride = w * 2},
        .data_size = w * h * 3, .data = data,
    };
    return sprite;
}
