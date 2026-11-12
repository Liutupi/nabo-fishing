#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "fishing_ui.h"
#include "fishing_motion.h"
#include "fishing_intro.h"
#include "fishing_tank.h"

LV_FONT_DECLARE(fishing_font_14);
LV_FONT_DECLARE(fishing_font_20);
#define CARD_W 84
#define CARD_H 60
#define LAND_MS 660
#define MISS_MS 420
// The aquarium is landscape: the badge is turned a quarter clockwise so its keys
// run along the bottom. Use LV_DISPLAY_ROTATION_270 if the picture is upside down.
#define TANK_ROTATION LV_DISPLAY_ROTATION_90
#define SPECKS 8

static lv_obj_t *scene, *actor, *fish_image, *leaping_fish, *placard, *title, *subtitle;
static lv_obj_t *wallet, *calendar, *footer, *status, *specks[SPECKS];
static lv_obj_t *hero, *hero_text, *tiles[4], *tile_text[4], *progress, *progress_fill;
static lv_obj_t *tension_label, *reel_label;
static lv_obj_t *meter, *zone, *marker, *bobber, *ripple, *line, *rod, *drops[4];
static lv_point_precise_t rod_points[3], line_points[3];
static uint32_t page_started;
static int illustrated_species = -1;
static const uint32_t ink = 0x102f38, paper = 0xffefd3;
static const uint32_t muted = 0xa6c3bc, gold = 0xffc85e, panel = 0x194451;

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t rgb, int radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    lv_obj_set_style_bg_color(object, lv_color_hex(rgb), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, uint32_t rgb, const lv_font_t *font)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, w);
    lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
    return object;
}

void fishing_ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(ink), 0);
    scene = lv_image_create(screen);
    lv_image_set_src(scene, fishing_shore(0, 0));
    lv_obj_set_pos(scene, 0, 0);
    rod = lv_line_create(screen);
    lv_obj_set_style_line_width(rod, 2, 0);
    lv_obj_set_style_line_color(rod, lv_color_hex(0x3d342b), 0);
    lv_obj_set_style_line_rounded(rod, true, 0);
    line = lv_line_create(screen);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(paper), 0);
    actor = lv_image_create(screen);
    lv_obj_set_pos(actor, 0, 12);
    lv_image_set_src(actor, fishing_pose(NABO_IDLE, false));
    ripple = block(screen, 161, 122, 23, 2, 0xa0e9d3, 1);
    bobber = block(screen, 169, 111, 7, 12, 0xff714a, 3);
    for (unsigned i = 0; i < 4; i++)
        drops[i] = block(screen, 0, 0, 3, 3, 0xc7ffeb, 1);
    for (unsigned i = 0; i < SPECKS; i++)
        specks[i] = block(screen, 0, 0, 3, 2, paper, 1);
    leaping_fish = lv_image_create(screen);
    lv_obj_t *tag = block(screen, 116, 12, 108, 23, ink, 7);
    wallet = label(tag, 0, 2, 108, gold, &fishing_font_14);
    tag = block(screen, 16, 12, 92, 23, ink, 7);
    calendar = label(tag, 0, 2, 92, paper, &fishing_font_14);
    placard = block(screen, 10, 153, 220, 69, panel, 11);
    title = label(placard, 4, 8, 212, paper, &fishing_font_20);
    subtitle = label(placard, 4, 39, 212, muted, &fishing_font_14);
    for (int i = 0; i < 4; i++) {
        tiles[i] = block(screen, 10 + 56 * i, 231, 53, 49, panel, 8);
        tile_text[i] = label(tiles[i], 1, 15, 51, paper, &fishing_font_14);
    }
    progress = block(screen, 57, 269, 165, 8, panel, 4);
    progress_fill = block(progress, 0, 0, 1, 8, 0x68dbb9, 4);
    tension_label = label(screen, 13, 238, 40, muted, &fishing_font_14);
    lv_label_set_text(tension_label, "张力");
    reel_label = label(screen, 13, 263, 40, muted, &fishing_font_14);
    lv_label_set_text(reel_label, "进度");
    meter = block(screen, 18, 236, 204, 27, panel, 7);
    zone = block(meter, 0, 3, 44, 21, gold, 5);
    marker = block(meter, 0, 0, 6, 27, paper, 2);
    hero = block(screen, 36, 53, 168, 84, panel, 12);
    hero_text = label(hero, 0, 28, 168, gold, &fishing_font_20);
    fish_image = lv_image_create(hero);
    lv_obj_set_pos(fish_image, (168 - CARD_W) / 2, (84 - CARD_H) / 2);
    footer = label(screen, 23, 292, 194, paper, &fishing_font_14);
    status = label(screen, 119, 134, 106, paper, &fishing_font_14);
    fishing_tank_init(screen);
    fishing_intro_init(screen);
}

static void visible(lv_obj_t *object, bool show)
{
    if (show) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void fishing_ui_boot(unsigned elapsed_ms, bool show)
{
    fishing_intro_draw(elapsed_ms, show);
}

static void polyline(lv_obj_t *object, lv_point_precise_t points[3],
                     lv_point_precise_t a, lv_point_precise_t b, lv_point_precise_t c)
{
    int x = LV_MIN(a.x, LV_MIN(b.x, c.x)), y = LV_MIN(a.y, LV_MIN(b.y, c.y));
    lv_point_precise_t next[] = {{a.x - x, a.y - y}, {b.x - x, b.y - y}, {c.x - x, c.y - y}};
    if (memcmp(points, next, sizeof next) == 0 &&
        lv_obj_get_x(object) == x && lv_obj_get_y(object) == y) return;
    lv_obj_invalidate(object);
    memcpy(points, next, sizeof next);
    lv_obj_set_pos(object, x, y);
    lv_line_set_points(object, points, 3);
}

static int arc_height(unsigned t, unsigned duration, int height)
{
    return 4 * height * (int)t * (int)(duration - t) / (int)(duration * duration);
}

static void draw_fish(lv_obj_t *object, unsigned species, unsigned phase)
{
    static lv_obj_t *last_object;
    static unsigned last_species, last_phase;
    phase %= 12;
    if (last_object == object && last_species == species && last_phase == phase) return;
    const lv_image_dsc_t *image = fishing_fish_frame(species, phase);
    if (lv_image_get_src(object) != image) lv_image_set_src(object, image);
    lv_obj_invalidate(object);
    last_object = object;
    last_species = species;
    last_phase = phase;
}

// Petals, fireflies, leaves or snow over the lake, by season.
static void drift(const fish_game_t *game, uint32_t now, bool show)
{
    const unsigned season = fish_season(game->save.clock);
    const bool night = fish_period(game->save.clock) == 3;
    static const uint32_t tints[4][2] = {{0xffc2d6, 0xffc2d6}, {0xe8ff8c, 0xe8ff8c},
                                         {0xe8892f, 0xc9502b}, {0xffffff, 0xffffff}};
    static unsigned styled = 4;
    if (styled != season) {
        for (unsigned i = 0; i < SPECKS; i++) {
            lv_obj_set_style_bg_color(specks[i], lv_color_hex(tints[season][i % 2]), 0);
            lv_obj_set_size(specks[i], season == 0 || season == 2 ? 3 : 2, 2);
        }
        styled = season;
    }
    for (unsigned i = 0; i < SPECKS; i++) {
        bool on = show && (season != 1 || (night && i < 6));
        int x, y;
        if (season == 1) {                      // fireflies wander and blink after dark
            unsigned a = now / (30 + i * 7) % 400, b = (now / (45 + i * 5) + i * 40) % 120;
            x = 20 + (int)(a > 200 ? 400 - a : a);
            y = 66 + (int)(b > 60 ? 120 - b : b);
            on = on && (now / 300 + i) % 3 != 0;
        } else {
            const unsigned fall = (season == 3 ? 16 : 11) + i % 4 * 3, slide = season == 3 ? 4 : 13;
            y = (int)((i * 53 % 160 + now / 8 * fall / 125 + i * 29) % 156) - 4;
            x = (int)((i * 97 + 240 - now / 8 * slide / 125 % 240) % 240);
        }
        visible(specks[i], on);
        if (on) lv_obj_set_pos(specks[i], x, y);
    }
}

static void animate(const fish_game_t *game, uint32_t now)
{
    if (game->page == FISH_TANK || game->page == FISH_CARE || game->page == FISH_SELL) {
        fishing_tank_animate(game, now);
        return;
    }
    unsigned age = now - page_started;
    unsigned cast_age = now - game->cast_at;
    bool waiting = game->page == FISH_WAIT;
    bool biting = game->page == FISH_BITE;
    bool fighting = game->page == FISH_FIGHT;
    bool aiming = game->page == FISH_CAST;
    bool success = game->page == FISH_RESULT && game->result == FISH_CAUGHT;
    bool landing = game->page == FISH_RESULT && age < (success ? LAND_MS : MISS_MS);
    bool lake_visible = game->page == FISH_HOME || waiting || biting || fighting || aiming || landing;
    bool swim = illustrated_species >= 0 && !landing;
    visible(scene, lake_visible);
    visible(actor, lake_visible);
    visible(rod, lake_visible);
    visible(line, waiting || biting || fighting || landing);
    visible(bobber, waiting || biting || fighting);
    visible(ripple, waiting || biting || fighting || landing);
    visible(hero, !lake_visible);
    visible(fish_image, swim);
    visible(leaping_fish, landing && success);
    drift(game, now, lake_visible);

    nabo_pose_t pose = NABO_IDLE;
    int bob_y = 114, bob_x = 181;
    int tip_x = 173, tip_y = 46;
    if (aiming) {
        pose = NABO_WINDUP;
        tip_x = 141; tip_y = 32;
    } else if (fighting) {
        bool pulling = fish_pulling(game, now);
        pose = pulling ? NABO_HOOK : NABO_WINDUP;
        tip_x = pulling ? 147 : 158;
        tip_y = pulling ? 38 + (int)(age / 70 % 4) : 42;
        bob_x = 181 - 4 * game->landed;
        bob_y += pulling ? (int)(age / 95 % 2) * 7 : 0;
        static int previous_pull = -1;
        const char *state = pulling ? "鱼在挣扎，先放线" : "鱼缓下来了，快收线";
        if (previous_pull != pulling || strcmp(lv_label_get_text(subtitle), state) != 0)
            lv_label_set_text(subtitle, state);
        previous_pull = pulling;
    } else if (waiting && cast_age < 350) {
        if (cast_age < 100) {
            pose = NABO_WINDUP;
            tip_x = 139 - (int)cast_age / 5;
            tip_y = 28 - (int)cast_age / 12;
            bob_x = tip_x + 11;
            bob_y = tip_y + 25;
        } else {
            pose = cast_age < 305 ? NABO_CAST : NABO_IDLE;
            unsigned t = cast_age - 100;
            tip_x = 182 - (int)t / 28;
            tip_y = 36 + (int)t / 22;
            bob_x = 135 + (int)(46 * t / 250);
            bob_y = 60 + (int)(54 * t / 250) - arc_height(t, 250, 30);
        }
    } else if (waiting) {
        bob_y += now / 200 % 2;
    } else if (biting) {
        pose = age < 190 || (age / 240 % 3) != 2 ? NABO_WINDUP : NABO_IDLE;
        tip_x = 155 + (int)(age / 65 % 3) * 3;
        tip_y = 44 + (int)(age / 75 % 3) * 3;
        bob_y += (int)(age / 85 % 2) * 8;
    } else if (landing) {
        pose = success ? NABO_HOOK : NABO_WINDUP;
        tip_x = 150 - (int)age / 20;
        tip_y = 24 + (int)age / 58;
        if (success) {
            bob_x = 181 - (int)(age * 42 / LAND_MS);
            bob_y = 119 - arc_height(age, LAND_MS, 40) - (int)age / 27;
            draw_fish(leaping_fish, game->caught, age / 30);
            lv_obj_set_pos(leaping_fish, bob_x - CARD_W / 2, bob_y - CARD_H / 2);
        } else {
            bob_y -= (int)age / 7;
            bob_x -= (int)age / 14;
        }
    }

    if (aiming || biting) {
        int half = (int)fish_zone_half(game) * 198 / 100;
        lv_obj_set_x(zone, 102 - half);
        lv_obj_set_width(zone, 2 * half);
        lv_obj_set_x(marker, fish_cursor(game, now) * 198 / 1300);
    } else if (fighting) {
        lv_obj_set_x(zone, 117);
        lv_obj_set_width(zone, 45);
        lv_obj_set_x(marker, game->tension * 159 / 100);
        lv_obj_set_width(progress_fill, LV_MAX(1, 165 * game->landed / game->required));
    }
    if (lake_visible) {
        const lv_image_dsc_t *image = fishing_pose(pose, fish_period(game->save.clock) == 3);
        if (lv_image_get_src(actor) != image) lv_image_set_src(actor, image);
        int breath = pose == NABO_IDLE ? (int)(now / 360 % 2) : 0;
        lv_obj_set_y(actor, 12 - breath);
        lv_point_precise_t grip = fishing_grip(pose);
        grip.y -= breath;
        lv_point_precise_t tip = {tip_x, tip_y};
        polyline(rod, rod_points, grip,
                 (lv_point_precise_t){(grip.x + tip.x) / 2, (grip.y + tip.y) / 2 - 8}, tip);
        if (waiting || biting || fighting || landing) {
            polyline(line, line_points, tip,
                     (lv_point_precise_t){(tip_x + bob_x) / 2 + 3, (tip_y + bob_y) / 2},
                     (lv_point_precise_t){bob_x, bob_y});
            lv_obj_set_pos(bobber, bob_x - 3, bob_y);
            unsigned width = 16 + (biting || fighting || landing ? age / 19 % 17 : age / 50 % 8);
            lv_obj_set_pos(ripple, bob_x - (int)width / 2, 124);
            lv_obj_set_width(ripple, width);
        }
    }

    unsigned splash_ms = waiting ? cast_age - 350u : age;
    bool splash = (waiting || biting || landing) && splash_ms < 220;
    for (unsigned i = 0; i < 4; i++) {
        visible(drops[i], splash);
        if (splash) {
            int dx = ((int)i * 2 - 3) * (int)(4 + splash_ms / 24);
            int dy = arc_height(splash_ms, 220, i % 2 ? 16 : 23);
            lv_obj_set_pos(drops[i], 181 + dx, 124 - dy);
        }
    }
    if (swim) {
        unsigned phase = age / 44;
        draw_fish(fish_image, illustrated_species, phase);
        unsigned drift = phase % 24;
        if (drift > 12) drift = 24 - drift;
        lv_obj_set_pos(fish_image, (168 - CARD_W) / 2 + (int)drift - 6,
                       (84 - CARD_H) / 2 + (int)(drift / 3) - 2);
    }
}

void fishing_ui_draw(const fish_game_t *game, uint32_t now, bool save_error)
{
    static fish_game_t drawn;
    static bool initialized, last_error;
    if (initialized && drawn.page == game->page && drawn.selected == game->selected &&
        drawn.group == game->group && drawn.book == game->book &&
        memcmp(&drawn.save, &game->save, sizeof game->save) == 0 &&
        drawn.result == game->result && drawn.caught == game->caught &&
        drawn.prep == game->prep && drawn.bait == game->bait && drawn.gear == game->gear &&
        drawn.slot == game->slot && drawn.notice == game->notice && drawn.chum == game->chum &&
        drawn.chummed == game->chummed && drawn.precise == game->precise && last_error == save_error) {
        animate(game, now);
        return;
    }
    if (!initialized || drawn.page != game->page) page_started = now;
    drawn = *game;
    initialized = true;
    last_error = save_error;
    const char *heading = "", *hint = "", *keys = "", *emblem = "";
    char detail[80], symbol[24];
    int species = -1;
    const char *menu[] = {"钓鱼", "工坊", "图鉴", "鱼缸"};
    lv_label_set_text_fmt(wallet, "贝壳 %lu", (unsigned long)game->save.coins);
    for (unsigned i = 0; i < 4; i++) {
        bool selected = i == game->selected;
        visible(tiles[i], game->page == FISH_HOME);
        lv_obj_set_style_bg_color(tiles[i], lv_color_hex(selected ? gold : panel), 0);
        lv_obj_set_style_text_color(tile_text[i], lv_color_hex(selected ? ink : paper), 0);
        lv_label_set_text(tile_text[i], menu[i]);
    }
    bool fighting = game->page == FISH_FIGHT;
    bool aquarium = game->page == FISH_TANK || game->page == FISH_CARE || game->page == FISH_SELL;
    visible(meter, fighting || game->page == FISH_BITE || game->page == FISH_CAST);
    visible(progress, fighting);
    visible(tension_label, fighting);
    visible(reel_label, fighting);
    lv_obj_set_x(meter, fighting ? 57 : 18);
    lv_obj_set_width(meter, fighting ? 165 : 204);
    lv_obj_set_style_bg_color(zone, lv_color_hex(fighting ? 0xb65042 : gold), 0);
    lv_display_t *display = lv_display_get_default();
    const lv_display_rotation_t rotation = aquarium ? TANK_ROTATION : LV_DISPLAY_ROTATION_0;
    if (lv_display_get_rotation(display) != rotation) lv_display_set_rotation(display, rotation);
    fishing_tank_show(aquarium);
    if (aquarium) {
        fishing_tank_draw(game, now);
        illustrated_species = -1;
        animate(game, now);
        return;
    }
    static const char *const seasons[] = {"春", "夏", "秋", "冬"};
    static const char *const periods[] = {"清晨", "白天", "黄昏", "夜晚"};
    const unsigned season = fish_season(game->save.clock), period = fish_period(game->save.clock);
    lv_label_set_text_fmt(calendar, "%s · %s", seasons[season], periods[period]);
    const lv_image_dsc_t *shore = fishing_shore(season, period);
    if (lv_image_get_src(scene) != shore) lv_image_set_src(scene, shore);
    lv_label_set_text(status, save_error ? "存档待重试" : "");
    switch (game->page) {
        case FISH_HOME:
            heading = "NABO 钓鱼小岛";
            snprintf(detail, sizeof detail, "图鉴 %u/%u · 出没 %u 种", fish_found(game, 0, FISH_COUNT),
                     FISH_COUNT, fish_active_count(game));
            hint = detail;
            keys = "上下 选择    OK 确认";
            break;
        case FISH_PREP:
            heading = "钓前准备";
            if (game->prep < FISH_BAIT_COUNT) {
                static const char *const uses[] = {"鲫鱼和鲤鱼爱吃", "偏向小型淡水鱼", "偏向掠食性鱼", "吸引幻想鱼"};
                emblem = fish_bait_names[game->prep];
                snprintf(detail, sizeof detail, "%s%s", uses[game->prep], game->chum ? " · 已选打窝" : "");
                hint = detail;
                keys = "上下 选饵    OK 装饵";
            } else if (game->prep == FISH_BAIT_COUNT) {
                emblem = game->chum ? "打窝已选" : "打窝";
                hint = game->notice == FISH_NOTICE_POOR ? "贝壳不足" :
                    game->chum ? "抛竿时扣费，等口更快" : "花 3 贝壳，目标鱼更常来";
                keys = "上下 选择    OK 切换";
            } else if (game->prep == FISH_BAIT_COUNT + 1) {
                emblem = "歇一会";
                snprintf(detail, sizeof detail, "等到%s再钓", periods[(period + 1) % 4]);
                hint = detail;
                keys = "上下 选择    OK 歇一会";
            } else {
                emblem = "返回岸边";
                keys = "上下 选择    OK 返回";
            }
            break;
        case FISH_CAST:
            heading = "瞄准抛竿";
            hint = "指针进金区，优先好鱼";
            keys = "上 换饵    OK 抛竿";
            break;
        case FISH_WAIT:
            heading = "等鱼上钩";
            snprintf(detail, sizeof detail, "%s · %s", fish_bait_names[game->bait],
                     game->chummed ? "已打窝" : game->precise ? "精准抛竿" : "看浮漂");
            hint = detail;
            keys = "上 返回";
            break;
        case FISH_BITE:
            heading = "鱼咬钩了"; hint = "指针进金区时刺鱼";
            keys = "上 返回    OK 刺鱼";
            break;
        case FISH_FIGHT:
            heading = "遛鱼收线";
            keys = "上 退出  下 放线  OK 收线";
            break;
        case FISH_RESULT:
            if (game->result == FISH_CAUGHT) {
                species = game->caught;
                heading = fish_species[species].name;
                if (game->save.pending) {
                    snprintf(detail, sizeof detail, "出售 %u 贝壳 · 鱼缸 %u/%u", fish_species[species].value,
                             fish_tank_count(game), FISH_TANK_SLOTS);
                    hint = game->notice == FISH_NOTICE_FULL ? "鱼缸满了，可以出售" : detail;
                    keys = "上 放进鱼缸    下 出售";
                } else {
                    hint = game->notice == FISH_NOTICE_KEPT ? "已放进鱼缸" : "已出售";
                    keys = "上 回岸    OK 再钓";
                }
            } else {
                emblem = "空竿";
                heading = game->result == FISH_SNAPPED ? "鱼线断了" :
                          game->result == FISH_TIMEOUT ? "鱼儿游走了" : "差一点";
                hint = "再抛一竿试试";
                keys = "上 回岸    OK 再钓";
            }
            break;
        case FISH_SHOP:
            heading = game->gear ? "升级鱼线" : "升级鱼竿";
            snprintf(symbol, sizeof symbol, "Lv. %u / %u", fish_gear_level(game), FISH_MAX_ROD);
            emblem = symbol;
            if (fish_gear_level(game) == FISH_MAX_ROD) hint = "已满级";
            else if (game->notice == FISH_NOTICE_POOR) hint = "贝壳不足，先去钓鱼吧";
            else {
                snprintf(detail, sizeof detail, "%s · %u 贝壳",
                         game->gear ? "降低收线张力" : "解锁更好的鱼", fish_cost(game));
                hint = detail;
            }
            keys = "上 返回  下 换装备  OK 升级";
            break;
        case FISH_BOOK: {
            unsigned groups = FISH_COUNT / FISH_GROUP_SIZE;
            heading = "鱼类图鉴";
            if (game->group == groups) {
                emblem = "返回岸边";
                keys = "上下 选册    OK 返回";
            } else {
                unsigned first = game->group * FISH_GROUP_SIZE;
                snprintf(symbol, sizeof symbol, "第 %02u 册", game->group + 1);
                snprintf(detail, sizeof detail, "本册已发现 %u / %u",
                         fish_found(game, first, first + FISH_GROUP_SIZE), FISH_GROUP_SIZE);
                emblem = symbol;
                hint = detail;
                keys = "上下 选册    OK 打开";
            }
            break;
        }
        case FISH_BOOK_FISH: {
            unsigned id = game->group * FISH_GROUP_SIZE + game->book;
            bool found = fish_seen(game, id);
            if (found) species = id;
            else emblem = "未发现";
            heading = found ? fish_species[id].name : "未知鱼种";
            if (found) {
                static const char *const short_periods[] = {"晨", "昼", "昏", "夜"};
                char when[2][16] = {"四季", "全天"};
                const unsigned masks[] = {fish_seasons_for(id), fish_periods_for(id)};
                for (unsigned part = 0; part < 2; part++) {
                    if (masks[part] == 15) continue;
                    when[part][0] = 0;
                    for (unsigned bit = 0; bit < 4; bit++)
                        if (masks[part] >> bit & 1) strcat(when[part], part ? short_periods[bit] : seasons[bit]);
                }
                snprintf(detail, sizeof detail, "%s · %s · %s", fish_bait_names[fish_bait_for(id)],
                         when[0], when[1]);
            } else snprintf(detail, sizeof detail, "竿 Lv.%u · 习性未知", fish_species[id].level);
            hint = detail;
            keys = "上下 翻鱼    OK 目录";
            break;
        }
        default: break;
    }
    illustrated_species = species;
    lv_label_set_text(hero_text, emblem);
    lv_label_set_text(title, heading);
    lv_label_set_text(subtitle, hint);
    lv_label_set_text(footer, keys);
    animate(game, now);
}
