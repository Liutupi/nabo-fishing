#include <assert.h>
#include <string.h>
#include "fishing_model.h"

int main(void)
{
    const uint32_t old[FISH_RECORD_WORDS_V4] = {
        5000, 4, 0, 0, 0, 1u << 19, 3, 0, 101 | 2u << 8 | 1u << 16, 0, 0, 0, 0, 0, 143,
    };
    fish_save_t save;
    assert(fish_save_decode(&save, old, FISH_RECORD_WORDS_V4));
    assert(save.capacity == 6 && save.tanks_owned == 1 && save.clock == 143);
    assert(save.tank[0].species == 101 && save.tank[0].fed && save.tank[0].growth == 2);

    fish_game_t game;
    fish_init(&game, save, 11);
    fish_press(&game, FISH_UP, 0);
    fish_press(&game, FISH_OK, 0);
    assert(game.page == FISH_STORE);
    fish_press(&game, FISH_DOWN, 0);
    for (unsigned capacity = 6; capacity < 12; capacity++) {
        const uint32_t coins = game.save.coins;
        assert(fish_press(&game, FISH_OK, 0));
        assert(game.save.capacity == capacity + 1 && game.save.coins == coins - (capacity - 5) * 80);
    }
    assert(!fish_press(&game, FISH_OK, 0));
    fish_press(&game, FISH_DOWN, 0);
    assert(fish_press(&game, FISH_OK, 0));
    assert(game.save.decor_owned == 1 && game.save.decor_shown == 1);
    for (unsigned i = 0; i < 6; i++) fish_press(&game, FISH_DOWN, 0);
    assert(fish_press(&game, FISH_OK, 0));
    assert(game.save.tank_type == 1 && game.save.tanks_owned == 3);
    game.page = FISH_TANK;
    game.slot = game.save.capacity;
    fish_press(&game, FISH_OK, 0);
    assert(game.page == FISH_DECOR);
    assert(fish_press(&game, FISH_OK, 0) && game.save.tank_type == 0);
    fish_press(&game, FISH_DOWN, 0);
    assert(fish_press(&game, FISH_OK, 0) && game.save.decor_shown == 0);

    game.page = FISH_STORE;
    game.store = 0;
    game.save.coins = 20000;
    unsigned prizes = 0;
    unsigned full_fish_prizes = 0;
    for (unsigned i = 0; i < 300; i++) {
        const uint32_t now = i * 2000, coins = game.save.coins;
        const unsigned count = fish_tank_count(&game);
        assert(fish_press(&game, FISH_OK, now));
        assert(game.page == FISH_BOX && game.save.coins == coins - 40 + game.prize_coins);
        prizes |= 1u << game.prize;
        if (game.prize == FISH_PRIZE_FISH && count == 12) {
            assert(game.prize_coins == 2 * fish_species[game.prize_id].value);
            full_fish_prizes++;
        }
        fish_press(&game, FISH_OK, now + 899);
        assert(game.page == FISH_BOX);
        fish_press(&game, FISH_OK, now + 900);
        assert(game.page == FISH_STORE);
    }
    assert(prizes == 31 && full_fish_prizes > 0 && fish_tank_count(&game) == 12);
    assert(game.save.seen[3] == old[5]);
    assert(game.save.tank[11].variant == 1 || game.save.tank[11].variant == 2);
    uint32_t record[FISH_RECORD_WORDS], roundtrip[FISH_RECORD_WORDS];
    fish_save_encode(&game.save, record);
    assert(fish_save_decode(&save, record, FISH_RECORD_WORDS));
    fish_save_encode(&save, roundtrip);
    assert(memcmp(record, roundtrip, sizeof record) == 0);
}
