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
    decoded.pending = 100;
    decoded.tank[5] = (fish_pet_t){.species = 97, .growth = 2, .fed = 1};
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
    record[14] = 143;
    assert(fish_save_decode(&decoded, record, FISH_RECORD_WORDS) && decoded.clock == 143);
    record[14] = 144;
    assert(!fish_save_decode(&decoded, record, FISH_RECORD_WORDS));
}

int main(void)
{
    save_checks();
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
