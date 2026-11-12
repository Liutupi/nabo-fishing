#include <stdio.h>
#include "fishing_tank.h"
#include "fishing_motion.h"

LV_FONT_DECLARE(fishing_font_14);
LV_FONT_DECLARE(fishing_font_20);

#define SUB 16                 // swimmer positions are in 1/16 pixel
#define FRAME_MS 40
#define FEED_MS 1100

typedef struct {
    uint8_t species, width, frame, speed;   // species is one-based; speed in px/s
    bool left, drawn_left;
    uint8_t drawn_species, drawn_width, drawn_frame;
    int16_t x, y, target_x, target_y;       // sprite centre
    uint16_t pause_ms, wag_ms;
} swimmer_t;

static const uint32_t ink = 0x102f38, paper = 0xffefd3, muted = 0xa6c3bc, gold = 0xffc85e;
static const uint8_t widths[FISH_MAX_GROWTH + 1] = {48, 56, 64, FISHING_TANK_FISH_W};
static lv_obj_t *root, *fish[FISH_TANK_SLOTS], *bubbles[6], *pellets[5], *marker;
static lv_obj_t *count_text, *coin_text, *pips[FISH_TANK_SLOTS + 1], *empty_text;
static lv_obj_t *name_text, *status_text, *keys_text;
static swimmer_t swimmers[FISH_TANK_SLOTS];
static uint32_t random_state, last_frame, fed_at;
static bool feeding;

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t rgb, int radius, lv_opa_t opa)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    lv_obj_set_style_bg_color(object, lv_color_hex(rgb), 0);
    lv_obj_set_style_bg_opa(object, opa, 0);
    lv_obj_set_style_radius(object, radius, 0);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, uint32_t rgb, const lv_font_t *font,
                       lv_text_align_t align)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, w);
    lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(object, align, 0);
    return object;
}

static void visible(lv_obj_t *object, bool show)
{
    if (show) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

// Only cosmetic motion uses this generator; the game's own seed is left alone.
static unsigned roll(unsigned count)
{
    random_state = random_state * 1664525u + 1013904223u;
    return (random_state >> 8) % count;
}

void fishing_tank_init(lv_obj_t *screen)
{
    root = block(screen, 0, 0, 320, 240, ink, 0, LV_OPA_COVER);
    lv_obj_t *backdrop = lv_image_create(root);
    lv_image_set_src(backdrop, &fishing_tank_backdrop);
    lv_obj_set_pos(backdrop, 0, 0);
    for (unsigned i = 0; i < 6; i++)
        bubbles[i] = block(root, 0, 0, 2 + i % 2, 2 + i % 2, 0xcdfff0, 1, LV_OPA_60);
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) fish[i] = lv_image_create(root);
    for (unsigned i = 0; i < 5; i++) pellets[i] = block(root, 0, 0, 3, 3, 0xe7a14b, 1, LV_OPA_COVER);
    marker = block(root, 0, 0, 8, 5, gold, 2, LV_OPA_COVER);
    empty_text = label(root, 20, 100, 280, 0xc9f1e6, &fishing_font_14, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text(empty_text, "鱼缸还空着，钓到鱼后按上键放进来");

    lv_obj_t *chip = block(root, 8, 8, 100, 23, ink, 7, LV_OPA_90);
    count_text = label(chip, 0, 2, 100, paper, &fishing_font_14, LV_TEXT_ALIGN_CENTER);
    chip = block(root, 216, 8, 96, 23, ink, 7, LV_OPA_90);
    coin_text = label(chip, 0, 2, 96, gold, &fishing_font_14, LV_TEXT_ALIGN_CENTER);
    for (unsigned i = 0; i <= FISH_TANK_SLOTS; i++)   // the last pip is the way out
        pips[i] = block(root, 116 + i * 13, 15, i == FISH_TANK_SLOTS ? 14 : 9, 8, ink, 3, LV_OPA_COVER);

    lv_obj_t *panel = block(root, 8, 184, 304, 50, ink, 10, LV_OPA_90);
    name_text = label(panel, 10, 1, 112, paper, &fishing_font_20, LV_TEXT_ALIGN_LEFT);
    status_text = label(panel, 122, 6, 174, muted, &fishing_font_14, LV_TEXT_ALIGN_RIGHT);
    keys_text = label(panel, 10, 28, 286, gold, &fishing_font_14, LV_TEXT_ALIGN_LEFT);
    visible(root, false);
}

void fishing_tank_show(bool show)
{
    if (show == !lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return;
    visible(root, show);
    // The sprites share buffers with other screens, so decode them afresh.
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) swimmers[i].drawn_species = 0;
}

static void wander(swimmer_t *s)
{
    s->target_x = (44 + roll(233)) * SUB;
    s->target_y = (50 + roll(110)) * SUB;
    s->speed = 12 + roll(20);
}

void fishing_tank_draw(const fish_game_t *game, uint32_t now)
{
    static char count[24], coins[24], status[64];
    const unsigned slot = game->slot;
    const unsigned kept = fish_tank_count(game);
    if (!random_state) random_state = now | 1u;
    snprintf(count, sizeof count, "我的鱼缸 %u/%u", kept, FISH_TANK_SLOTS);
    snprintf(coins, sizeof coins, "贝壳 %lu", (unsigned long)game->save.coins);
    lv_label_set_text_static(count_text, count);
    lv_label_set_text_static(coin_text, coins);
    visible(empty_text, kept == 0);
    for (unsigned i = 0; i <= FISH_TANK_SLOTS; i++) {
        bool occupied = i < FISH_TANK_SLOTS && game->save.tank[i].species;
        lv_obj_set_style_bg_color(pips[i], lv_color_hex(i == slot ? gold : occupied ? 0x68dbb9 : ink), 0);
    }
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
        const fish_pet_t *pet = &game->save.tank[i];
        swimmer_t *s = &swimmers[i];
        if (pet->species && s->species != pet->species) {   // a newcomer swims in from its own corner
            *s = (swimmer_t){.x = (50 + (int)i * 44) * SUB, .y = (62 + (int)i * 53 % 100) * SUB,
                             .frame = i * 2};
            wander(s);
        }
        s->species = pet->species;
        s->width = widths[pet->growth];
        visible(fish[i], pet->species != 0);
    }

    const char *name = "返回岸边", *keys = "OK 返回   下 / 上 选鱼";
    status[0] = 0;
    const fish_pet_t *pet = slot < FISH_TANK_SLOTS ? &game->save.tank[slot] : NULL;
    if (pet && !pet->species) {
        static char vacancy[16];
        snprintf(vacancy, sizeof vacancy, "空位 %u", slot + 1);
        name = vacancy;
        snprintf(status, sizeof status, "已养 %u/%u", kept, FISH_TANK_SLOTS);
        keys = "下 后一格   上 前一格";
    } else if (pet) {
        name = fish_species[pet->species - 1].name;
        snprintf(status, sizeof status, "成长 %u/%u · %s", pet->growth, FISH_MAX_GROWTH,
                 pet->fed ? "已喂" : "待喂食");
        keys = "OK 照料   下 后一条   上 前一条";
        if (game->page == FISH_CARE) {
            snprintf(status, sizeof status, "%s",
                     game->notice == FISH_NOTICE_POOR ? "贝壳不足" :
                     pet->growth == FISH_MAX_GROWTH ? "已经长大了" :
                     pet->fed ? "已喂，钓到鱼会成长" : "喂食 2 贝壳，钓鱼后成长");
            keys = "OK 喂食   下 出售   上 返回";
        } else if (game->page == FISH_SELL) {
            snprintf(status, sizeof status, "确认出售 · %u 贝壳", fish_tank_value(pet));
            keys = "OK 出售   上 取消";
        }
    }
    lv_obj_set_style_text_color(name_text, lv_color_hex(game->page == FISH_SELL ? 0xff9a7a : paper), 0);
    lv_label_set_text(name_text, name);
    lv_label_set_text_static(status_text, status);
    lv_label_set_text(keys_text, keys);

    bool fed = game->page == FISH_CARE && game->notice == FISH_NOTICE_FED;
    if (fed && !feeding) fed_at = now;
    feeding = fed;
}

static void advance(swimmer_t *s, unsigned elapsed)
{
    s->wag_ms += elapsed;
    const unsigned beat = 95 - s->speed;             // faster fish beat their tails faster
    while (s->wag_ms >= beat) {
        s->wag_ms -= beat;
        s->frame = (s->frame + 1) % 12;
    }
    if (s->pause_ms) {
        s->pause_ms = s->pause_ms > elapsed ? s->pause_ms - elapsed : 0;
        return;
    }
    const int dx = s->target_x - s->x, dy = s->target_y - s->y;
    if (LV_ABS(dx) < 3 * SUB && LV_ABS(dy) < 3 * SUB) {
        wander(s);
        if (roll(100) < 35) s->pause_ms = 400 + roll(1000);
        return;
    }
    const int step = LV_MAX(1, (int)(s->speed * SUB * elapsed / 1000));
    if (LV_ABS(dx) > 4 * SUB) s->left = dx < 0;
    s->x += LV_CLAMP(-step, dx, step);
    s->y += LV_CLAMP(-(step * 6 / 10 + 1), dy, step * 6 / 10 + 1);
}

void fishing_tank_animate(const fish_game_t *game, uint32_t now)
{
    unsigned elapsed = now - last_frame;
    if (elapsed < FRAME_MS) return;
    last_frame = now;
    if (elapsed > 120) elapsed = 120;

    static const uint16_t columns[6] = {64, 70, 150, 236, 242, 156};
    for (unsigned i = 0; i < 6; i++) {
        unsigned rise = (now / 8 * (16 + i % 3 * 6) / 125 + i * 41) % 180;
        lv_obj_set_pos(bubbles[i], columns[i], 186 - (int)rise);
    }
    const swimmer_t *chosen = NULL;
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
        swimmer_t *s = &swimmers[i];
        if (!s->species) continue;
        advance(s, elapsed);
        if (s->drawn_species != s->species || s->drawn_width != s->width ||
            s->drawn_left != s->left || s->drawn_frame != s->frame) {
            const lv_image_dsc_t *sprite = fishing_tank_fish(i, s->species - 1, s->width, s->left, s->frame);
            if (s->drawn_species != s->species || s->drawn_width != s->width) lv_image_set_src(fish[i], sprite);
            lv_obj_invalidate(fish[i]);
            s->drawn_species = s->species;
            s->drawn_width = s->width;
            s->drawn_left = s->left;
            s->drawn_frame = s->frame;
        }
        const int height = (s->width * 60 + 42) / 84;
        lv_obj_set_pos(fish[i], s->x / SUB - s->width / 2, s->y / SUB - height / 2);
        if (i == game->slot) chosen = s;
    }
    visible(marker, chosen != NULL);
    unsigned fed = now - fed_at;
    bool dropping = chosen && feeding && fed < FEED_MS;
    for (unsigned i = 0; i < 5; i++) visible(pellets[i], dropping);
    if (!chosen) return;
    const int x = chosen->x / SUB, y = chosen->y / SUB;
    const int top = y - (chosen->width * 60 + 42) / 84 / 2;
    lv_obj_set_pos(marker, x - 4, top - 9 + (int)(now / 250 % 2) * 2);
    if (dropping) {
        for (unsigned i = 0; i < 5; i++) {              // feed sinks from the surface to the fish
            unsigned span = 700 + i * 90;
            unsigned done = fed < span ? fed : span;
            lv_obj_set_pos(pellets[i], x - 12 + (int)i * 6, 6 + (y - 12) * (int)done / (int)span);
        }
    }
}
