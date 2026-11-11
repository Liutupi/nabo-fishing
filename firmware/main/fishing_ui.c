#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "fishing_ui.h"

LV_FONT_DECLARE(fishing_font_14);
LV_FONT_DECLARE(fishing_font_20);
extern const uint8_t lake_start[] asm("_binary_lake_rgb565_start");
extern const uint8_t fish_start[] asm("_binary_fish_rgb565_start");

#define CARD_W 84
#define CARD_H 60
#define CARD_BYTES (CARD_W * CARD_H * 2)
static lv_image_dsc_t lake = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = 240, .h = 160, .stride = 480},
    .data_size = 240 * 160 * 2,
    .data = lake_start,
};
static lv_image_dsc_t fish_cards[FISH_COUNT];
static lv_obj_t *scene, *fish_image, *placard, *title, *subtitle, *wallet, *footer, *status;
static lv_obj_t *boot, *boot_scene, *boot_words;
static lv_obj_t *hero, *hero_text, *tiles[3], *tile_text[3];
static lv_obj_t *meter, *zone, *marker, *bobber, *ripple, *line;
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
    lv_image_set_src(scene, &lake);
    lv_obj_set_pos(scene, 0, 0);
    line = block(screen, 172, 46, 1, 74, paper, 0);
    ripple = block(screen, 161, 122, 23, 2, 0xa0e9d3, 1);
    bobber = block(screen, 169, 111, 7, 12, 0xff714a, 3);
    lv_obj_t *tag = block(screen, 110, 12, 114, 23, ink, 7);
    wallet = label(tag, 0, 2, 114, gold, &fishing_font_14);
    placard = block(screen, 10, 153, 220, 69, panel, 11);
    title = label(placard, 4, 8, 212, paper, &fishing_font_20);
    subtitle = label(placard, 4, 39, 212, muted, &fishing_font_14);
    for (int i = 0; i < 3; i++) {
        tiles[i] = block(screen, 12 + 73 * i, 231, 70, 49, panel, 8);
        tile_text[i] = label(tiles[i], 1, 15, 68, paper, &fishing_font_14);
    }
    meter = block(screen, 18, 236, 204, 27, panel, 7);
    zone = block(meter, 0, 3, 44, 21, gold, 5);
    marker = block(meter, 0, 0, 6, 27, paper, 2);
    hero = block(screen, 36, 53, 168, 84, panel, 12);
    hero_text = label(hero, 0, 28, 168, gold, &fishing_font_20);
    fish_image = lv_image_create(hero);
    lv_obj_set_pos(fish_image, (168 - CARD_W) / 2, (84 - CARD_H) / 2);
    footer = label(screen, 23, 292, 194, paper, &fishing_font_14);
    status = label(screen, 119, 134, 106, paper, &fishing_font_14);
    for (unsigned i = 0; i < FISH_COUNT; ++i) {
        fish_cards[i] = (lv_image_dsc_t){
            .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                       .w = CARD_W, .h = CARD_H, .stride = CARD_W * 2},
            .data_size = CARD_BYTES, .data = fish_start + i * CARD_BYTES,
        };
    }
    boot = block(screen, 0, 0, 240, 320, ink, 0);
    boot_scene = lv_image_create(boot);
    lv_image_set_src(boot_scene, &lake);
    lv_obj_set_pos(boot_scene, 0, 0);
    lv_obj_t *boot_title = label(boot, 14, 190, 212, paper, &fishing_font_20);
    lv_label_set_text(boot_title, "NABO");
    boot_words = label(boot, 14, 226, 212, gold, &fishing_font_20);
    lv_label_set_text(boot_words, "陪你钓鱼");
    lv_obj_t *skip = label(boot, 24, 290, 192, muted, &fishing_font_14);
    lv_label_set_text(skip, "任意键跳过");
    lv_obj_add_flag(boot, LV_OBJ_FLAG_HIDDEN);
}

static void visible(lv_obj_t *object, bool show)
{
    if (show) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void fishing_ui_boot(unsigned elapsed_ms, bool show)
{
    visible(boot, show);
    if (!show) return;
    unsigned slide = elapsed_ms < 450 ? 450 - elapsed_ms : 0;
    lv_obj_set_y(boot_scene, -(int)(slide * slide / 17000));
    lv_obj_set_style_text_opa(boot_words, elapsed_ms < 400 ? elapsed_ms * 255 / 400 : 255, 0);
}

static void animate(const fish_game_t *game, uint32_t now)
{
    if (game->page == FISH_BITE) {
        lv_obj_set_height(line, 74);
        int half = (int)fish_zone_half(game) * 198 / 100;
        lv_obj_set_x(zone, 102 - half);
        lv_obj_set_width(zone, 2 * half);
        lv_obj_set_x(marker, fish_cursor(game, now) * 198 / 1300);
        lv_obj_set_y(bobber, 109 + (now / 90 % 2) * 8);
        unsigned width = 18 + (now / 23 % 18);
        lv_obj_set_width(ripple, width);
        lv_obj_set_x(ripple, 172 - width / 2);
    } else if (game->page == FISH_WAIT) {
        unsigned drop = now - game->cast_at;
        if (drop > 200) drop = 200;
        lv_obj_set_y(bobber, 77 + drop * 34 / 200 + (now / 240 % 2) * 2);
        lv_obj_set_height(line, 36 + drop * 38 / 200);
        lv_obj_set_width(ripple, 22);
        lv_obj_set_x(ripple, 161);
    }
}

void fishing_ui_draw(const fish_game_t *game, uint32_t now, bool save_error)
{
    static fish_game_t drawn;
    static bool initialized, last_error;
    if (initialized && drawn.page == game->page && drawn.selected == game->selected &&
        drawn.group == game->group && drawn.book == game->book &&
        drawn.save.coins == game->save.coins && drawn.save.rod == game->save.rod &&
        memcmp(drawn.save.seen, game->save.seen, sizeof game->save.seen) == 0 &&
        drawn.result == game->result && drawn.caught == game->caught &&
        drawn.poor == game->poor && last_error == save_error) {
        animate(game, now);
        return;
    }
    drawn = *game;
    initialized = true;
    last_error = save_error;
    const char *heading = "", *hint = "", *keys = "", *emblem = "";
    char detail[80], symbol[24];
    int species = -1;
    const char *menu[] = {"去钓鱼", "工坊", "图鉴"};
    bool fishing = game->page == FISH_WAIT || game->page == FISH_BITE;
    bool lake_page = fishing || game->page == FISH_HOME;
    lv_label_set_text_fmt(wallet, "贝壳 %lu", (unsigned long)game->save.coins);
    for (unsigned i = 0; i < 3; i++) {
        bool selected = i == game->selected;
        visible(tiles[i], game->page == FISH_HOME);
        lv_obj_set_style_bg_color(tiles[i], lv_color_hex(selected ? gold : panel), 0);
        lv_obj_set_style_text_color(tile_text[i], lv_color_hex(selected ? ink : paper), 0);
        lv_label_set_text(tile_text[i], menu[i]);
    }
    visible(scene, lake_page);
    visible(line, fishing); visible(bobber, fishing); visible(ripple, fishing);
    visible(meter, game->page == FISH_BITE);
    visible(hero, !lake_page);
    lv_label_set_text(status, save_error ? "存档待重试" : "");
    switch (game->page) {
        case FISH_HOME:
            heading = "NABO 钓鱼小岛";
            snprintf(detail, sizeof detail, "鱼竿 Lv.%u   图鉴 %u/%u", game->save.rod,
                     fish_found(game, 0, FISH_COUNT), FISH_COUNT);
            hint = detail;
            keys = "上下 选择    OK 确认";
            break;
        case FISH_WAIT:
            heading = "等鱼上钩"; hint = "留意水面上的浮漂";
            keys = "上 返回";
            break;
        case FISH_BITE:
            heading = "鱼咬钩了"; hint = "指针进入金色区就收竿";
            keys = "上 返回    OK 收竿";
            break;
        case FISH_RESULT:
            if (game->result == FISH_CAUGHT) {
                species = game->caught;
                heading = fish_species[species].name;
                snprintf(detail, sizeof detail, "+%u 贝壳", fish_species[species].value);
                hint = detail;
            } else {
                emblem = "空竿";
                heading = game->result == FISH_TIMEOUT ? "鱼儿游走了" : "差一点";
                hint = "再抛一竿试试";
            }
            keys = "上 返回    OK 再钓";
            break;
        case FISH_SHOP:
            heading = "鱼竿工坊";
            snprintf(symbol, sizeof symbol, "Lv. %u / %u", game->save.rod, FISH_MAX_ROD);
            emblem = symbol;
            if (game->save.rod == FISH_MAX_ROD) hint = "鱼竿已满级";
            else if (game->poor) hint = "贝壳不足，先去钓鱼吧";
            else {
                snprintf(detail, sizeof detail, "升级需 %u 贝壳", fish_cost(game));
                hint = detail;
            }
            keys = game->save.rod == FISH_MAX_ROD ? "上 返回" : "上 返回    OK 升级";
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
            snprintf(detail, sizeof detail, "编号 %03u    鱼竿 Lv.%u", id + 1, fish_species[id].level);
            hint = detail;
            keys = "上下 翻鱼    OK 目录";
            break;
        }
    }
    visible(fish_image, species >= 0);
    if (species >= 0) lv_image_set_src(fish_image, &fish_cards[species]);
    lv_label_set_text(hero_text, emblem);
    lv_label_set_text(title, heading);
    lv_label_set_text(subtitle, hint);
    lv_label_set_text(footer, keys);
    animate(game, now);
}
