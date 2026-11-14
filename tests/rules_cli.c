#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "fishing_model.h"

static void save_checks(void)
{
    uint32_t old[6] = {456, 3, 63, 4, 0, 8};
    fish_save_t decoded;
    assert(fish_save_decode(&decoded, old, 6));
    assert(decoded.coins == 456 && decoded.rod == 3 && decoded.line == 1);
    assert(decoded.seen[3] == 8 && !decoded.pending && !decoded.tank[0].species);
    assert(fish_save_decode(&decoded, old, 3));
    assert(decoded.seen[0] == 63 && decoded.seen[1] == 0);
    decoded.line = 2;
    decoded.pending = FISH_COUNT;
    decoded.seen[FISH_SAVE_WORDS - 1] = FISH_LAST_SEEN_MASK;
    decoded.tank[5] = (fish_pet_t){.species = 97, .growth = 2, .fed = 1};
    fish_game_t initialized;
    fish_init(&initialized, decoded, 1);
    decoded = initialized.save;
    uint32_t record[FISH_RECORD_WORDS], copy[FISH_RECORD_WORDS];
    fish_save_encode(&decoded, record);
    assert(fish_save_decode(&decoded, record, FISH_RECORD_WORDS));
    fish_save_encode(&decoded, copy);
    assert(memcmp(record, copy, sizeof record) == 0);
    record[13] = 65536;
    assert(!fish_save_decode(&decoded, record, FISH_RECORD_WORDS));
    record[13] = copy[13];
    assert(fish_save_decode(&decoded, record, FISH_RECORD_WORDS_V3) && decoded.clock == 0);
    assert(decoded.tank[5].species == 97 && decoded.line == 2);
    record[8 + FISH_TANK_SLOTS] = 143;
    assert(fish_save_decode(&decoded, record, FISH_RECORD_WORDS) && decoded.clock == 143);
    record[8 + FISH_TANK_SLOTS] = 144;
    assert(!fish_save_decode(&decoded, record, FISH_RECORD_WORDS));
}

int main(int argc, char **argv)
{
    save_checks();
    if (argc == 2 && strcmp(argv[1], "--catalog") == 0) {
        fish_game_t book;
        fish_init(&book, (fish_save_t){.rod = 1}, 0);
        book.page = FISH_BOOK;
        book.group = FISH_GROUP_COUNT - 1;
        fish_press(&book, FISH_OK, 0);
        fish_press(&book, FISH_UP, 0);
        assert(book.group * FISH_GROUP_SIZE + book.book == FISH_COUNT - 1);
        fish_press(&book, FISH_DOWN, 0);
        assert(book.book == 0);
        for (unsigned i = 0; i < FISH_COUNT; i++)
            printf("%s\t%u\t%u\t%u\t%u\t%u\n", fish_species[i].name, fish_species[i].value,
                   fish_species[i].level, fish_bait_for(i), fish_seasons_for(i), fish_periods_for(i));
        return 0;
    }
    fish_game_t game;
    fish_init(&game, (fish_save_t){.rod = 1}, 0);
    char op;
    uint32_t now, seed, coins, seen;
    unsigned rod, key;
    while (scanf(" %c", &op) == 1) {
        bool changed = false;
        switch (op) {
            case 'i':
                if (scanf("%" SCNu32 " %" SCNu32 " %u %" SCNu32, &seed, &coins, &rod, &seen) != 4)
                    return 1;
                fish_init(&game, (fish_save_t){.coins = coins, .rod = rod, .seen = {seen, 0, 0, 0}}, seed);
                break;
            case 'p':
                if (scanf("%u %" SCNu32, &key, &now) != 2 || key > FISH_OK) return 1;
                changed = fish_press(&game, key, now);
                break;
            case 't':
                if (scanf("%" SCNu32, &now) != 1) return 1;
                fish_tick(&game, now);
                break;
            case 'r': {
                if (scanf("%" SCNu32, &seed) != 1) return 1;
                uint32_t record[FISH_RECORD_WORDS];
                fish_save_t save;
                fish_save_encode(&game.save, record);
                assert(fish_save_decode(&save, record, FISH_RECORD_WORDS));
                fish_init(&game, save, seed);
                break;
            }
            default: return 1;
        }
        printf("%u %u %" PRIu32 " %" PRIu32 " %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
               game.page, game.result, game.rng, game.bite_at, game.selected, game.group, game.book,
               game.caught, game.notice, game.prep, game.bait, game.gear, game.slot, game.tension,
               game.landed, game.required, game.chum, game.chummed, game.precise);
        uint32_t record[FISH_RECORD_WORDS];
        fish_save_encode(&game.save, record);
        for (unsigned i = 0; i < FISH_RECORD_WORDS; i++) printf(" %" PRIu32, record[i]);
        printf(" %u\n", changed);
    }
    return 0;
}
