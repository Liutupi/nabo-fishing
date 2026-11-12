#include "fishing_model.h"

const fish_species_t fish_species[FISH_COUNT] = {
#include "fishing_species.inc"
};
const char *const fish_bait_names[FISH_BAIT_COUNT] = {"面团", "蚯蚓", "拟饵", "星光饵"};

unsigned fish_bait_for(unsigned id)
{
    // In order of the stable catalog IDs: dough, worm, lure and starlight.
    static const char preferences[] =
        "0010331102" "2112320000" "0212121222" "0011100112" "1011112222"
        "2222222222" "2211222222" "2010010100" "3333333333" "3333333333";
    _Static_assert(sizeof preferences == FISH_COUNT + 1, "bait preferences must cover the catalog");
    return id < FISH_COUNT ? (unsigned)(preferences[id] - '0') : 0;
}

static unsigned hex_digit(char c) { return c <= '9' ? (unsigned)(c - '0') : (unsigned)(c - 'a' + 10); }

unsigned fish_seasons_for(unsigned id)
{
    // One hex digit per fish: spring 1, summer 2, autumn 4, winter 8.
    static const char masks[] =
        "ffcfff3cc6" "3f9c6fffc6" "639cc6399c" "638969639c" "c6399c6839"
        "c4639cc629" "9c6339c663" "9cc6399c63" "3813239c26" "3f8424386f";
    _Static_assert(sizeof masks == FISH_COUNT + 1, "season masks must cover the catalog");
    return id < FISH_COUNT ? hex_digit(masks[id]) : 0;
}

unsigned fish_periods_for(unsigned id)
{
    // One hex digit per fish: dawn 1, day 2, dusk 4, night 8.
    static const char masks[] =
        "f7c788f663" "9c639cff39" "c6399c639c" "63f98339c6" "639c639cc6"
        "3fc6399c33" "9c6339c639" "c6639c639c" "c33f66398c" "2886283949";
    _Static_assert(sizeof masks == FISH_COUNT + 1, "period masks must cover the catalog");
    return id < FISH_COUNT ? hex_digit(masks[id]) : 0;
}

unsigned fish_period(unsigned clock) { return clock / FISH_PERIOD_CASTS % 4; }
unsigned fish_season(unsigned clock) { return clock / (FISH_PERIOD_CASTS * 12) % 4; }

bool fish_active(unsigned id, unsigned clock)
{
    return (fish_seasons_for(id) >> fish_season(clock) & 1) && (fish_periods_for(id) >> fish_period(clock) & 1);
}

unsigned fish_active_count(const fish_game_t *game)
{
    unsigned count = 0;
    for (unsigned i = 0; i < FISH_COUNT; i++)
        count += fish_species[i].level <= game->save.rod && fish_active(i, game->save.clock);
    return count;
}

static unsigned pick(fish_game_t *game, unsigned count)
{
    uint32_t t = (game->rng += UINT32_C(0x6d2b79f5));
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (uint64_t)(t ^ (t >> 14)) * count >> 32;
}

void fish_init(fish_game_t *game, fish_save_t save, uint32_t seed)
{
    if (save.rod < 1 || save.rod > FISH_MAX_ROD || save.seen[3] > 15) save = (fish_save_t){.rod = 1};
    if (save.line < 1 || save.line > FISH_MAX_ROD) save.line = 1;
    if (save.pending > FISH_COUNT) save.pending = 0;
    if (save.clock >= FISH_YEAR_CASTS) save.clock = 0;
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
        fish_pet_t *pet = &save.tank[i];
        if (!pet->species || pet->species > FISH_COUNT || pet->growth > FISH_MAX_GROWTH || pet->fed > 1)
            *pet = (fish_pet_t){0};
    }
    *game = (fish_game_t){.save = save, .rng = seed};
    if (save.pending) {
        game->page = FISH_RESULT;
        game->caught = save.pending - 1;
    }
}

static void set_page(fish_game_t *game, fish_page_t page)
{
    game->page = page;
    game->notice = FISH_NOTICE_NONE;
}

bool fish_pulling(const fish_game_t *game, uint32_t now)
{
    return ((now - game->fight_at) / 550u + game->caught) % 4 >= 2;
}

void fish_tick(fish_game_t *game, uint32_t now)
{
    if (game->page == FISH_WAIT && (int32_t)(now - game->bite_at) >= 0) set_page(game, FISH_BITE);
    if (game->page == FISH_BITE && (int32_t)(now - game->bite_at) >= 5200) {
        set_page(game, FISH_RESULT);
        game->result = FISH_TIMEOUT;
    }
    if (game->page != FISH_FIGHT || (int32_t)(now - game->fight_at) < 0) return;
    if (now - game->fight_at >= 14500u) {
        set_page(game, FISH_RESULT);
        game->result = FISH_TIMEOUT;
        return;
    }
    if ((int32_t)(now - game->tension_at) < 0) return;
    unsigned steps = (now - game->tension_at) / 300;
    unsigned decay = steps * 5;
    game->tension = decay >= game->tension ? 0 : game->tension - decay;
    game->tension_at += steps * 300;
}

unsigned fish_cursor(const fish_game_t *game, uint32_t now)
{
    uint32_t start = game->page == FISH_CAST ? game->aim_at : game->bite_at;
    unsigned pos = (now - start) % 2600u;
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

static unsigned choose_fish(fish_game_t *game)
{
    bool prefer_tier = game->precise && pick(game, 100) < 55;
    bool prefer_unseen = pick(game, 3) == 0;
    unsigned total = 0;
    unsigned weights[FISH_COUNT];
    for (unsigned i = 0; i < FISH_COUNT; i++) {
        unsigned level = fish_species[i].level;
        unsigned weight = level <= game->save.rod && fish_active(i, game->save.clock);
        if (fish_bait_for(i) == game->bait) weight *= game->chummed ? 12 : 6;
        if (prefer_tier && level == game->save.rod) weight *= 4;
        if (prefer_unseen && !fish_seen(game, i)) weight *= 3;
        weights[i] = weight;
        total += weight;
    }
    unsigned roll = pick(game, total);
    for (unsigned i = 0; i < FISH_COUNT; i++) {
        if (roll < weights[i]) return i;
        roll -= weights[i];
    }
    return 0;
}

static bool cast(fish_game_t *game, uint32_t now)
{
    unsigned pos = fish_cursor(game, now);
    unsigned half = fish_zone_half(game) * 13;
    game->precise = pos >= 650 - half && pos <= 650 + half;
    game->chummed = game->chum && game->save.coins >= FISH_CHUM_COST;
    game->chum = false;
    if (game->chummed) game->save.coins -= FISH_CHUM_COST;
    // Time passes while the float is out.
    game->save.clock = (game->save.clock + 1) % FISH_YEAR_CASTS;
    set_page(game, FISH_WAIT);
    game->cast_at = now;
    game->bite_at = now + (game->chummed ? 1000u + pick(game, 901) : 1800u + pick(game, 2201));
    return true;
}

static bool reel(fish_game_t *game, uint32_t now)
{
    unsigned tier = fish_species[game->caught].level;
    bool pulling = fish_pulling(game, now);
    unsigned rise = (pulling ? 31u : 17u) + tier * 2 - game->save.line * 3;
    unsigned tension = game->tension + rise;
    if (tension >= 100) {
        set_page(game, FISH_RESULT);
        game->result = FISH_SNAPPED;
        return false;
    }
    game->tension = tension;
    game->landed += pulling ? 1 : 2;
    if (game->landed < game->required) return false;

    unsigned caught = game->caught;
    game->save.seen[caught / 32] |= UINT32_C(1) << (caught % 32);
    game->save.pending = caught + 1;
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
        fish_pet_t *pet = &game->save.tank[i];
        if (pet->species && pet->fed && pet->growth < FISH_MAX_GROWTH) pet->growth++;
        pet->fed = 0;
    }
    set_page(game, FISH_RESULT);
    game->result = FISH_CAUGHT;
    return true;
}

unsigned fish_gear_level(const fish_game_t *game) { return game->gear ? game->save.line : game->save.rod; }
unsigned fish_cost(const fish_game_t *game) { return fish_gear_level(game) * (game->gear ? 32 : 45); }

unsigned fish_tank_count(const fish_game_t *game)
{
    unsigned count = 0;
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) count += game->save.tank[i].species != 0;
    return count;
}

unsigned fish_tank_value(const fish_pet_t *pet)
{
    if (!pet->species) return 0;
    unsigned base = fish_species[pet->species - 1].value;
    return base + (base * pet->growth + 3) / 4;
}

static void pay(fish_game_t *game, unsigned value)
{
    game->save.coins = game->save.coins > UINT32_MAX - value ? UINT32_MAX : game->save.coins + value;
}

bool fish_press(fish_game_t *game, fish_button_t button, uint32_t now)
{
    fish_page_t before = game->page;
    fish_tick(game, now);
    if (before != FISH_RESULT && game->page == FISH_RESULT && button == FISH_OK) return false;
    switch (game->page) {
        case FISH_HOME: {
            static const fish_page_t pages[] = {FISH_PREP, FISH_SHOP, FISH_BOOK, FISH_TANK};
            if (button == FISH_UP) game->selected = (game->selected + 3) % 4;
            if (button == FISH_DOWN) game->selected = (game->selected + 1) % 4;
            if (button == FISH_OK) set_page(game, pages[game->selected]);
            break;
        }
        case FISH_PREP:
            if (button == FISH_UP) game->prep = (game->prep + FISH_PREP_ITEMS - 1) % FISH_PREP_ITEMS;
            if (button == FISH_DOWN) game->prep = (game->prep + 1) % FISH_PREP_ITEMS;
            if (button != FISH_OK) break;
            if (game->prep < FISH_BAIT_COUNT) {
                game->bait = game->prep;
                game->aim_at = now;
                set_page(game, FISH_CAST);
            } else if (game->prep == FISH_BAIT_COUNT) {
                if (game->chum || game->save.coins >= FISH_CHUM_COST) game->chum = !game->chum;
                else game->notice = FISH_NOTICE_POOR;
            } else if (game->prep == FISH_BAIT_COUNT + 1) {
                // Rest until the next period begins.
                game->save.clock = (game->save.clock / FISH_PERIOD_CASTS + 1) * FISH_PERIOD_CASTS % FISH_YEAR_CASTS;
                return true;
            } else set_page(game, FISH_HOME);
            break;
        case FISH_CAST:
            if (button == FISH_UP) set_page(game, FISH_PREP);
            if (button == FISH_OK) return cast(game, now);
            break;
        case FISH_WAIT:
            if (button == FISH_UP) set_page(game, FISH_HOME);
            break;
        case FISH_BITE: {
            if (button == FISH_UP) set_page(game, FISH_HOME);
            if (button != FISH_OK || (int32_t)(now - game->bite_at) < 0) break;
            unsigned pos = fish_cursor(game, now);
            unsigned half = fish_zone_half(game) * 13;
            if (pos < 650 - half || pos > 650 + half) {
                set_page(game, FISH_RESULT);
                game->result = FISH_MISSED;
                break;
            }
            set_page(game, FISH_FIGHT);
            game->caught = choose_fish(game);
            game->fight_at = game->tension_at = now;
            game->tension = 18;
            game->landed = 0;
            game->required = 4 + fish_species[game->caught].level;
            break;
        }
        case FISH_FIGHT:
            if (button == FISH_UP) set_page(game, FISH_HOME);
            if (button == FISH_DOWN) game->tension = game->tension > 33 ? game->tension - 33 : 0;
            if (button == FISH_OK) return reel(game, now);
            break;
        case FISH_RESULT:
            if (game->save.pending) {
                if (button == FISH_DOWN) {
                    pay(game, fish_species[game->caught].value);
                    game->save.pending = 0;
                    game->notice = FISH_NOTICE_SOLD;
                    return true;
                }
                if (button == FISH_UP) {
                    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
                        if (game->save.tank[i].species) continue;
                        game->save.tank[i] = (fish_pet_t){.species = game->save.pending};
                        game->save.pending = 0;
                        game->notice = FISH_NOTICE_KEPT;
                        return true;
                    }
                    game->notice = FISH_NOTICE_FULL;
                }
                break;
            }
            if (button == FISH_UP) set_page(game, FISH_HOME);
            if (button == FISH_OK) set_page(game, FISH_PREP);
            break;
        case FISH_SHOP:
            if (button == FISH_UP) set_page(game, FISH_HOME);
            if (button == FISH_DOWN) { game->gear = !game->gear; game->notice = FISH_NOTICE_NONE; }
            if (button != FISH_OK || fish_gear_level(game) == FISH_MAX_ROD) break;
            if (game->save.coins < fish_cost(game)) { game->notice = FISH_NOTICE_POOR; break; }
            game->save.coins -= fish_cost(game);
            if (game->gear) game->save.line++; else game->save.rod++;
            game->notice = FISH_NOTICE_NONE;
            return true;
        case FISH_BOOK: {
            const unsigned groups = FISH_COUNT / FISH_GROUP_SIZE + 1;
            if (button == FISH_UP) game->group = (game->group + groups - 1) % groups;
            if (button == FISH_DOWN) game->group = (game->group + 1) % groups;
            if (button == FISH_OK) {
                set_page(game, game->group == groups - 1 ? FISH_HOME : FISH_BOOK_FISH);
                game->book = 0;
            }
            break;
        }
        case FISH_BOOK_FISH:
            if (button == FISH_UP) game->book = (game->book + FISH_GROUP_SIZE - 1) % FISH_GROUP_SIZE;
            if (button == FISH_DOWN) game->book = (game->book + 1) % FISH_GROUP_SIZE;
            if (button == FISH_OK) set_page(game, FISH_BOOK);
            break;
        case FISH_TANK:
            if (button == FISH_UP) game->slot = (game->slot + FISH_TANK_SLOTS) % (FISH_TANK_SLOTS + 1);
            if (button == FISH_DOWN) game->slot = (game->slot + 1) % (FISH_TANK_SLOTS + 1);
            if (button == FISH_OK) {
                if (game->slot == FISH_TANK_SLOTS) set_page(game, FISH_HOME);
                else if (game->save.tank[game->slot].species) set_page(game, FISH_CARE);
            }
            break;
        case FISH_CARE: {
            fish_pet_t *pet = &game->save.tank[game->slot];
            if (button == FISH_UP) set_page(game, FISH_TANK);
            if (button == FISH_DOWN) set_page(game, FISH_SELL);
            if (button != FISH_OK || pet->fed) break;
            if (pet->growth == FISH_MAX_GROWTH) { game->notice = FISH_NOTICE_FULLY_GROWN; break; }
            if (game->save.coins < FISH_FEED_COST) { game->notice = FISH_NOTICE_POOR; break; }
            game->save.coins -= FISH_FEED_COST;
            pet->fed = 1;
            game->notice = FISH_NOTICE_FED;
            return true;
        }
        case FISH_SELL:
            if (button == FISH_UP) set_page(game, FISH_CARE);
            if (button != FISH_OK) break;
            pay(game, fish_tank_value(&game->save.tank[game->slot]));
            game->save.tank[game->slot] = (fish_pet_t){0};
            set_page(game, FISH_TANK);
            game->notice = FISH_NOTICE_SOLD;
            return true;
    }
    return false;
}

void fish_save_encode(const fish_save_t *save, uint32_t record[FISH_RECORD_WORDS])
{
    record[0] = save->coins;
    record[1] = save->rod;
    for (unsigned i = 0; i < FISH_SAVE_WORDS; i++) record[2 + i] = save->seen[i];
    record[6] = save->line;
    record[7] = save->pending;
    for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
        const fish_pet_t *pet = &save->tank[i];
        record[8 + i] = pet->species | ((uint32_t)pet->growth << 8) | ((uint32_t)pet->fed << 16);
    }
    record[8 + FISH_TANK_SLOTS] = save->clock;
}

bool fish_save_decode(fish_save_t *save, const uint32_t *record, unsigned words)
{
    if (words != 3 && words != 6 && words != FISH_RECORD_WORDS_V3 && words != FISH_RECORD_WORDS) return false;
    if (record[1] < 1 || record[1] > FISH_MAX_ROD) return false;
    fish_save_t value = {.coins = record[0], .rod = record[1], .line = 1};
    if (words == 3 && record[2] > 63) return false;
    unsigned seen_words = words == 3 ? 1 : FISH_SAVE_WORDS;
    for (unsigned i = 0; i < seen_words; i++) value.seen[i] = record[2 + i];
    if (value.seen[3] > 15) return false;
    if (words >= FISH_RECORD_WORDS_V3) {
        if (record[6] < 1 || record[6] > FISH_MAX_ROD || record[7] > FISH_COUNT) return false;
        value.line = record[6];
        value.pending = record[7];
        for (unsigned i = 0; i < FISH_TANK_SLOTS; i++) {
            uint32_t packed = record[8 + i];
            unsigned species = packed & 255, growth = (packed >> 8) & 255, fed = packed >> 16;
            if (species > FISH_COUNT || growth > FISH_MAX_GROWTH || fed > 1 ||
                (!species && (growth || fed))) return false;
            value.tank[i] = (fish_pet_t){species, growth, fed};
        }
        if (words == FISH_RECORD_WORDS) {
            if (record[8 + FISH_TANK_SLOTS] >= FISH_YEAR_CASTS) return false;
            value.clock = record[8 + FISH_TANK_SLOTS];
        }
    }
    *save = value;
    return true;
}
