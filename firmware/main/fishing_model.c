#include "fishing_model.h"

const fish_species_t fish_species[FISH_COUNT] = {
#include "fishing_species.inc"
};

static unsigned pick(fish_game_t *game, unsigned count)
{
    uint32_t t = (game->rng += UINT32_C(0x6d2b79f5));
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (uint64_t)(t ^ (t >> 14)) * count >> 32;
}

void fish_init(fish_game_t *game, fish_save_t save, uint32_t seed)
{
    if (save.rod < 1 || save.rod > FISH_MAX_ROD || save.seen[3] > 15) {
        save = (fish_save_t){.rod = 1};
    }
    *game = (fish_game_t){.save = save, .rng = seed};
}

static void cast(fish_game_t *game, uint32_t now)
{
    game->page = FISH_WAIT;
    game->cast_at = now;
    game->bite_at = now + 1800u + pick(game, 2201);
}

void fish_tick(fish_game_t *game, uint32_t now)
{
    if ((int32_t)(now - game->bite_at) < 0) return;
    if (game->page == FISH_WAIT) game->page = FISH_BITE;
    if (game->page == FISH_BITE && now - game->bite_at >= 5200u) {
        game->page = FISH_RESULT;
        game->result = FISH_TIMEOUT;
    }
}

unsigned fish_cursor(const fish_game_t *game, uint32_t now)
{
    unsigned pos = (now - game->bite_at) % 2600u;
    return pos > 1300u ? 2600u - pos : pos;
}

unsigned fish_zone_half(const fish_game_t *game) { return 9 + game->save.rod * 2; }
bool fish_seen(const fish_game_t *game, unsigned id)
{
    return id < FISH_COUNT && (game->save.seen[id / 32] & (UINT32_C(1) << (id % 32))) != 0;
}

unsigned fish_found(const fish_game_t *game, unsigned first, unsigned last)
{
    unsigned count = 0;
    for (unsigned i = first; i < last; i++) count += fish_seen(game, i);
    return count;
}

static unsigned catch_fish(fish_game_t *game)
{
    uint8_t eligible[FISH_COUNT], unseen[FISH_COUNT];
    unsigned count = 0, new_count = 0;
    for (unsigned i = 0; i < FISH_COUNT; i++) {
        if (fish_species[i].level > game->save.rod) continue;
        eligible[count++] = i;
        if (!fish_seen(game, i)) unseen[new_count++] = i;
    }
    if (new_count && pick(game, 3) == 0) return unseen[pick(game, new_count)];
    return eligible[pick(game, count)];
}

unsigned fish_cost(const fish_game_t *game) { return game->save.rod * 45; }

bool fish_press(fish_game_t *game, fish_button_t button, uint32_t now)
{
    fish_page_t before = game->page;
    fish_tick(game, now);
    if (before != FISH_RESULT && game->page == FISH_RESULT && button == FISH_OK) return false;
    switch (game->page) {
        case FISH_HOME:
            if (button == FISH_UP) game->selected = (game->selected + 2) % 3;
            if (button == FISH_DOWN) game->selected = (game->selected + 1) % 3;
            if (button == FISH_OK) {
                if (game->selected == 0) cast(game, now);
                if (game->selected == 1) game->page = FISH_SHOP;
                if (game->selected == 2) game->page = FISH_BOOK;
            }
            break;
        case FISH_WAIT:
            if (button == FISH_UP) game->page = FISH_HOME;
            break;
        case FISH_BITE:
            if (button == FISH_UP) game->page = FISH_HOME;
            if (button != FISH_OK) break;
            game->page = FISH_RESULT;
            game->result = FISH_MISSED;
            unsigned pos = fish_cursor(game, now);
            unsigned half = fish_zone_half(game) * 13;
            if (pos < 650 - half || pos > 650 + half) break;
            game->caught = catch_fish(game);
            unsigned value = fish_species[game->caught].value;
            game->save.coins = game->save.coins > UINT32_MAX - value
                ? UINT32_MAX : game->save.coins + value;
            game->save.seen[game->caught / 32] |= UINT32_C(1) << (game->caught % 32);
            game->result = FISH_CAUGHT;
            return true;
        case FISH_RESULT:
            if (button == FISH_UP) game->page = FISH_HOME;
            if (button == FISH_OK) cast(game, now);
            break;
        case FISH_SHOP:
            if (button == FISH_UP) { game->page = FISH_HOME; game->poor = false; }
            if (button != FISH_OK || game->save.rod == FISH_MAX_ROD) break;
            if (game->save.coins < fish_cost(game)) { game->poor = true; break; }
            game->save.coins -= fish_cost(game);
            game->save.rod++;
            game->poor = false;
            return true;
        case FISH_BOOK: {
            const unsigned groups = FISH_COUNT / FISH_GROUP_SIZE + 1;
            if (button == FISH_UP) game->group = (game->group + groups - 1) % groups;
            if (button == FISH_DOWN) game->group = (game->group + 1) % groups;
            if (button == FISH_OK) {
                game->page = game->group == groups - 1 ? FISH_HOME : FISH_BOOK_FISH;
                game->book = 0;
            }
            break;
        }
        case FISH_BOOK_FISH:
            if (button == FISH_UP) game->book = (game->book + FISH_GROUP_SIZE - 1) % FISH_GROUP_SIZE;
            if (button == FISH_DOWN) game->book = (game->book + 1) % FISH_GROUP_SIZE;
            if (button == FISH_OK) game->page = FISH_BOOK;
            break;
    }
    return false;
}
