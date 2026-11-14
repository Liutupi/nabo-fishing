#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
#ifdef FISH_LEGACY_CATALOG
    FISH_COUNT = 100,  // Host parity with the released browser rules.
#else
    FISH_COUNT = 116,
#endif
    FISH_MAX_ROD = 4, FISH_GROUP_SIZE = 10, FISH_SAVE_WORDS = 4,
    FISH_GROUP_COUNT = (FISH_COUNT + FISH_GROUP_SIZE - 1) / FISH_GROUP_SIZE,
    FISH_BASE_SLOTS = 6,
#ifdef FISH_LEGACY_CATALOG
    FISH_TANK_SLOTS = FISH_BASE_SLOTS, FISH_HOME_ITEMS = 4,
#else
    FISH_TANK_SLOTS = 12, FISH_HOME_ITEMS = 5,
#endif
    FISH_TANK_TYPES = 4, FISH_DECOR_COUNT = 10, FISH_SHELF_ITEMS = 11,
    FISH_BAIT_COUNT = 4, FISH_CHUM_COST = 3,
    FISH_MAX_GROWTH = 3, FISH_FEED_COST = 2,
    FISH_RECORD_WORDS_V3 = 8 + FISH_BASE_SLOTS,
    FISH_RECORD_WORDS_V4 = FISH_RECORD_WORDS_V3 + 1,
#ifdef FISH_LEGACY_CATALOG
    FISH_RECORD_WORDS = FISH_RECORD_WORDS_V4,
#else
    FISH_RECORD_WORDS = 13 + FISH_TANK_SLOTS,
#endif
    // The clock counts casts: 3 per period, 4 periods per day, 3 days per season.
    FISH_PERIOD_CASTS = 3, FISH_YEAR_CASTS = 144, FISH_PREP_ITEMS = 7,
};
_Static_assert(FISH_COUNT > (FISH_SAVE_WORDS - 1) * 32 &&
               FISH_COUNT <= FISH_SAVE_WORDS * 32, "catalog must fit the save");
#define FISH_LAST_SEEN_MASK (UINT32_MAX >> (FISH_SAVE_WORDS * 32 - FISH_COUNT))

static inline unsigned fish_group_size(unsigned group)
{
    const unsigned remaining = FISH_COUNT - group * FISH_GROUP_SIZE;
    return remaining < FISH_GROUP_SIZE ? remaining : FISH_GROUP_SIZE;
}

typedef enum {
    FISH_HOME, FISH_WAIT, FISH_BITE, FISH_RESULT, FISH_SHOP, FISH_BOOK, FISH_BOOK_FISH,
    FISH_PREP, FISH_CAST, FISH_FIGHT, FISH_TANK, FISH_CARE, FISH_SELL, FISH_STORE, FISH_BOX, FISH_DECOR,
} fish_page_t;
typedef enum { FISH_UP, FISH_DOWN, FISH_OK } fish_button_t;
typedef enum { FISH_CAUGHT, FISH_MISSED, FISH_TIMEOUT, FISH_SNAPPED } fish_result_t;
typedef enum { FISH_NOTICE_NONE, FISH_NOTICE_FULL, FISH_NOTICE_KEPT, FISH_NOTICE_SOLD,
               FISH_NOTICE_FED, FISH_NOTICE_POOR, FISH_NOTICE_FULLY_GROWN, FISH_NOTICE_OWNED, FISH_NOTICE_BOUGHT } fish_notice_t;

typedef struct { uint8_t species, growth, fed, variant; } fish_pet_t; // species uses one-based IDs
typedef struct {
    uint32_t coins;
    uint8_t rod, line, pending, clock;
    uint32_t seen[FISH_SAVE_WORDS];
    uint8_t capacity, tanks_owned, tank_type;
    uint16_t decor_owned, decor_shown;
    fish_pet_t tank[FISH_TANK_SLOTS];
} fish_save_t;

typedef struct { const char *name; uint8_t value, level; } fish_species_t;

typedef struct {
    fish_save_t save;
    fish_page_t page;
    fish_result_t result;
    fish_notice_t notice;
    uint32_t rng, cast_at, bite_at, aim_at, fight_at, tension_at;
    uint8_t selected, group, book, caught, prep, bait, gear, slot;
    uint8_t tension, landed, required;
    uint8_t store, decor, prize, prize_id, prize_variant;
    uint32_t box_at;
    uint16_t prize_coins;
    bool chum, chummed, precise;
} fish_game_t;

extern const fish_species_t fish_species[FISH_COUNT];
extern const char *const fish_bait_names[FISH_BAIT_COUNT];
void fish_init(fish_game_t *game, fish_save_t save, uint32_t seed);
void fish_tick(fish_game_t *game, uint32_t now);
bool fish_press(fish_game_t *game, fish_button_t button, uint32_t now);
unsigned fish_cursor(const fish_game_t *game, uint32_t now);
unsigned fish_zone_half(const fish_game_t *game);
bool fish_seen(const fish_game_t *game, unsigned id);
unsigned fish_found(const fish_game_t *game, unsigned first, unsigned last);
unsigned fish_cost(const fish_game_t *game);
unsigned fish_gear_level(const fish_game_t *game);
unsigned fish_tank_count(const fish_game_t *game);
unsigned fish_tank_value(const fish_pet_t *pet);
unsigned fish_bait_for(unsigned id);
// Season 0-3 is spring to winter; period 0-3 is dawn, day, dusk, night.
unsigned fish_season(unsigned clock);
unsigned fish_period(unsigned clock);
unsigned fish_seasons_for(unsigned id);  // bit per season
unsigned fish_periods_for(unsigned id);  // bit per period
bool fish_active(unsigned id, unsigned clock);
unsigned fish_active_count(const fish_game_t *game);
bool fish_pulling(const fish_game_t *game, uint32_t now);

void fish_save_encode(const fish_save_t *save, uint32_t record[FISH_RECORD_WORDS]);
bool fish_save_decode(fish_save_t *save, const uint32_t *record, unsigned words);

typedef enum { FISH_ITEM_BOX, FISH_ITEM_ROOM, FISH_ITEM_DECOR, FISH_ITEM_TANK, FISH_ITEM_EXIT } fish_item_kind_t;
typedef enum { FISH_PRIZE_COINS, FISH_PRIZE_DECOR, FISH_PRIZE_FISH, FISH_PRIZE_RARE, FISH_PRIZE_TANK } fish_prize_t;
typedef struct { fish_item_kind_t kind; uint8_t id; } fish_item_t;
typedef struct { const char *name; uint16_t price; } fish_shop_item_t;
extern const fish_shop_item_t fish_tank_types[FISH_TANK_TYPES], fish_decors[FISH_DECOR_COUNT];
extern const fish_item_t fish_shelf[FISH_SHELF_ITEMS];
unsigned fish_room_cost(unsigned capacity);
unsigned fish_item_price(const fish_game_t *game, fish_item_t item);
bool fish_item_owned(const fish_game_t *game, fish_item_t item);
fish_item_t fish_decor_item(const fish_game_t *game, unsigned index);
bool fish_is_aquarium(fish_page_t page);
