#include <inttypes.h>
#include <stdio.h>
#include "fishing_model.h"

int main(void)
{
    fish_game_t game;
    fish_init(&game, (fish_save_t){.rod = 1}, 0);
    char op;
    uint32_t now, seed, coins;
    unsigned rod, seen, key;
    while (scanf(" %c", &op) == 1) {
        bool changed = false;
        switch (op) {
            case 'i':
                if (scanf("%" SCNu32 " %" SCNu32 " %u %u", &seed, &coins, &rod, &seen) != 4)
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
            default: return 1;
        }
        printf("%u %u %" PRIu32 " %" PRIu32 " %u %u %u %u %u %" PRIu32 " %u %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %u\n",
               game.page, game.result, game.rng, game.bite_at, game.selected, game.group, game.book,
               game.caught, game.poor, game.save.coins, game.save.rod, game.save.seen[0], game.save.seen[1], game.save.seen[2], game.save.seen[3], changed);
    }
    return 0;
}
