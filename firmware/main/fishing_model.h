#pragma once

#include <stdbool.h>
#include <stdint.h>

enum { FISH_COUNT = 100, FISH_MAX_ROD = 4, FISH_GROUP_SIZE = 10, FISH_SAVE_WORDS = 4 };
typedef enum { FISH_HOME, FISH_WAIT, FISH_BITE, FISH_RESULT, FISH_SHOP, FISH_BOOK, FISH_BOOK_FISH } fish_page_t;
typedef enum { FISH_UP, FISH_DOWN, FISH_OK } fish_button_t;
typedef enum { FISH_CAUGHT, FISH_MISSED, FISH_TIMEOUT } fish_result_t;

typedef struct {
    uint32_t coins;
    uint8_t rod;
    uint32_t seen[FISH_SAVE_WORDS];
} fish_save_t;

typedef struct {
    const char *name;
    uint8_t value, level;
} fish_species_t;

typedef struct {
    fish_save_t save;
    fish_page_t page;
    fish_result_t result;
    uint32_t rng;
    uint32_t cast_at, bite_at;
    uint8_t selected, group, book, caught;
    bool poor;
} fish_game_t;

extern const fish_species_t fish_species[FISH_COUNT];
void fish_init(fish_game_t *game, fish_save_t save, uint32_t seed);
void fish_tick(fish_game_t *game, uint32_t now);
bool fish_press(fish_game_t *game, fish_button_t button, uint32_t now);
unsigned fish_cursor(const fish_game_t *game, uint32_t now);
unsigned fish_zone_half(const fish_game_t *game);
bool fish_seen(const fish_game_t *game, unsigned id);
unsigned fish_found(const fish_game_t *game, unsigned first, unsigned last);
unsigned fish_cost(const fish_game_t *game);
