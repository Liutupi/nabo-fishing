'use strict';

const species = typeof module === 'undefined' ? catalogSpecies : require('./species.js');
const baitNames = ['面团', '蚯蚓', '拟饵', '星光饵'];
const preferences = '0010331102211232000002121212220011100112101111222222222222222211222222201001010033333333333333333333';
const baitFor = id => Number(preferences[id]);
// One hex digit per fish. Season bits: spring 1, summer 2, autumn 4, winter 8.
// Period bits: dawn 1, day 2, dusk 4, night 8.
const seasonMasks = 'ffcfff3cc63f9c6fffc6639cc6399c638969639cc6399c6839c4639cc6299c6339c6639cc6399c633813239c263f8424386f';
const periodMasks = 'f7c788f6639c639cff39c6399c639c63f98339c6639c639cc63fc6399c339c6339c639c6639c639cc33f66398c2886283949';
const seasonNames = ['春', '夏', '秋', '冬'];
const periodNames = ['清晨', '白天', '黄昏', '夜晚'];
// The clock counts casts: 3 per period, 4 periods per day, 3 days per season.
const PERIOD_CASTS = 3, YEAR_CASTS = 144;
const periodOf = clock => Math.floor(clock / PERIOD_CASTS) % 4;
const seasonOf = clock => Math.floor(clock / (PERIOD_CASTS * 12)) % 4;
const dayOf = clock => Math.floor(clock / (PERIOD_CASTS * 4)) % 3;
const seasonsFor = id => parseInt(seasonMasks[id], 16);
const periodsFor = id => parseInt(periodMasks[id], 16);
const active = (id, clock) => !!(seasonsFor(id) >> seasonOf(clock) & 1 && periodsFor(id) >> periodOf(clock) & 1);
const maxCoins = 0xffffffff;
const [HOME, WAIT, BITE, RESULT, SHOP, BOOK, BOOK_FISH, PREP, CAST, FIGHT, TANK, CARE, SELL] =
    Array.from({length: 13}, (_, i) => i);
const [UP, DOWN, OK] = [0, 1, 2];
const [NONE, FULL, KEPT, SOLD, FED, POOR, GROWN] = [0, 1, 2, 3, 4, 5, 6];

const emptyPet = () => ({species: 0, growth: 0, fed: 0});
function create(save, seed) {
    if (save && Number.isInteger(save.seen) && save.seen >= 0 && save.seen <= 63)
        save = {...save, seen: [save.seen, 0, 0, 0]};
    if (!save || !Number.isInteger(save.coins) || save.coins < 0 || save.coins > maxCoins ||
        !Number.isInteger(save.rod) || save.rod < 1 || save.rod > 4 ||
        !Array.isArray(save.seen) || save.seen.length !== 4 || save.seen[3] > 15 ||
        save.seen.some(word => !Number.isInteger(word) || word < 0 || word > maxCoins))
        save = {coins: 0, rod: 1, seen: [0, 0, 0, 0]};
    const line = Number.isInteger(save.line) && save.line >= 1 && save.line <= 4 ? save.line : 1;
    const clock = Number.isInteger(save.clock) && save.clock >= 0 && save.clock < YEAR_CASTS ? save.clock : 0;
    const pending = Number.isInteger(save.pending) && save.pending >= 1 && save.pending <= 100 ? save.pending : 0;
    const tank = Array.from({length: 6}, (_, i) => {
        const pet = Array.isArray(save.tank) && save.tank[i];
        if (!pet || !Number.isInteger(pet.species) || pet.species < 1 || pet.species > 100 ||
            !Number.isInteger(pet.growth) || pet.growth < 0 || pet.growth > 3 || ![0, 1].includes(pet.fed))
            return emptyPet();
        return {...pet};
    });
    return {save: {coins: save.coins, rod: save.rod, line, pending, clock, seen: [...save.seen], tank},
        page: pending ? RESULT : HOME, result: 0, notice: NONE, rng: seed >>> 0, castAt: 0,
        biteAt: 0, aimAt: 0, fightAt: 0, tensionAt: 0, selected: 0, group: 0, book: 0,
        caught: pending ? pending - 1 : 0, prep: 0, bait: 0, gear: 0, slot: 0, tension: 0,
        landed: 0, required: 0, chum: false, chummed: false, precise: false};
}

function pick(game, count) {
    game.rng = (game.rng + 0x6d2b79f5) >>> 0;
    let t = game.rng;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return Math.floor(((t ^ (t >>> 14)) >>> 0) * count / 0x100000000);
}

function page(game, next) {
    game.page = next;
    game.notice = NONE;
}

function tick(game, now) {
    const sinceBite = (now - game.biteAt) >>> 0;
    if (game.page === WAIT && (sinceBite | 0) >= 0) page(game, BITE);
    if (game.page === BITE && (sinceBite | 0) >= 5200) {
        page(game, RESULT);
        game.result = 2;
    }
    const elapsed = (now - game.fightAt) | 0;
    if (game.page !== FIGHT || elapsed < 0) return;
    if (elapsed >= 14500) {
        page(game, RESULT);
        game.result = 2;
        return;
    }
    const sinceTension = (now - game.tensionAt) | 0;
    if (sinceTension < 0) return;
    const steps = Math.floor(sinceTension / 300);
    game.tension = Math.max(0, game.tension - steps * 5);
    game.tensionAt = (game.tensionAt + steps * 300) >>> 0;
}

function cursor(game, now) {
    const start = game.page === CAST ? game.aimAt : game.biteAt;
    const position = ((now - start) >>> 0) % 2600;
    return position > 1300 ? 2600 - position : position;
}
function halfZone(game) { return 9 + game.save.rod * 2; }
function gearLevel(game) { return game.gear ? game.save.line : game.save.rod; }
function cost(game) { return gearLevel(game) * (game.gear ? 32 : 45); }
function seen(game, id) { return !!(game.save.seen[id >>> 5] & (1 << (id % 32))); }
function pulling(game, now) {
    return (Math.floor(((now - game.fightAt) >>> 0) / 550) + game.caught) % 4 >= 2;
}
function tankValue(pet) {
    const base = pet.species ? species[pet.species - 1].value : 0;
    return base + Math.ceil(base * pet.growth / 4);
}

function chooseFish(game) {
    const preferTier = game.precise && pick(game, 100) < 55;
    const preferUnseen = pick(game, 3) === 0;
    const weights = species.map((fish, id) => {
        let weight = Number(fish.level <= game.save.rod && active(id, game.save.clock));
        if (baitFor(id) === game.bait) weight *= game.chummed ? 12 : 6;
        if (preferTier && fish.level === game.save.rod) weight *= 4;
        if (preferUnseen && !seen(game, id)) weight *= 3;
        return weight;
    });
    let roll = pick(game, weights.reduce((a, b) => a + b, 0));
    for (let i = 0; i < weights.length; i++) {
        if (roll < weights[i]) return i;
        roll -= weights[i];
    }
    return 0;
}

function cast(game, now) {
    game.precise = Math.abs(cursor(game, now) - 650) <= halfZone(game) * 13;
    game.chummed = game.chum && game.save.coins >= 3;
    game.chum = false;
    if (game.chummed) game.save.coins -= 3;
    // Time passes while the float is out.
    game.save.clock = (game.save.clock + 1) % YEAR_CASTS;
    page(game, WAIT);
    game.castAt = now;
    game.biteAt = (now + (game.chummed ? 1000 + pick(game, 901) : 1800 + pick(game, 2201))) >>> 0;
    return true;
}

function reel(game, now) {
    const tier = species[game.caught].level;
    const fishPulls = pulling(game, now);
    const tension = game.tension + (fishPulls ? 31 : 17) + tier * 2 - game.save.line * 3;
    if (tension >= 100) {
        page(game, RESULT);
        game.result = 3;
        return false;
    }
    game.tension = tension;
    game.landed += fishPulls ? 1 : 2;
    if (game.landed < game.required) return false;
    const word = game.caught >>> 5;
    game.save.seen[word] = (game.save.seen[word] | (1 << (game.caught % 32))) >>> 0;
    game.save.pending = game.caught + 1;
    for (const pet of game.save.tank) {
        if (pet.species && pet.fed && pet.growth < 3) pet.growth++;
        pet.fed = 0;
    }
    page(game, RESULT);
    game.result = 0;
    return true;
}

function pay(game, value) { game.save.coins = Math.min(maxCoins, game.save.coins + value); }

function press(game, button, now) {
    const before = game.page;
    tick(game, now);
    if (before !== RESULT && game.page === RESULT && button === OK) return false;
    switch (game.page) {
        case HOME:
            if (button === UP) game.selected = (game.selected + 3) % 4;
            if (button === DOWN) game.selected = (game.selected + 1) % 4;
            if (button === OK) page(game, [PREP, SHOP, BOOK, TANK][game.selected]);
            break;
        case PREP:
            if (button === UP) game.prep = (game.prep + 6) % 7;
            if (button === DOWN) game.prep = (game.prep + 1) % 7;
            if (button !== OK) break;
            if (game.prep < 4) {
                game.bait = game.prep;
                game.aimAt = now;
                page(game, CAST);
            } else if (game.prep === 4) {
                if (game.chum || game.save.coins >= 3) game.chum = !game.chum;
                else game.notice = POOR;
            } else if (game.prep === 5) {
                // Rest until the next period begins.
                game.save.clock = (Math.floor(game.save.clock / PERIOD_CASTS) + 1) * PERIOD_CASTS % YEAR_CASTS;
                return true;
            } else page(game, HOME);
            break;
        case CAST:
            if (button === UP) page(game, PREP);
            if (button === OK) return cast(game, now);
            break;
        case WAIT:
            if (button === UP) page(game, HOME);
            break;
        case BITE:
            if (button === UP) page(game, HOME);
            if (button !== OK || ((now - game.biteAt) | 0) < 0) break;
            if (Math.abs(cursor(game, now) - 650) > halfZone(game) * 13) {
                page(game, RESULT);
                game.result = 1;
                break;
            }
            page(game, FIGHT);
            game.caught = chooseFish(game);
            game.fightAt = game.tensionAt = now;
            game.tension = 18;
            game.landed = 0;
            game.required = 4 + species[game.caught].level;
            break;
        case FIGHT:
            if (button === UP) page(game, HOME);
            if (button === DOWN) game.tension = Math.max(0, game.tension - 33);
            if (button === OK) return reel(game, now);
            break;
        case RESULT:
            if (game.save.pending) {
                if (button === DOWN) {
                    pay(game, species[game.caught].value);
                    game.save.pending = 0;
                    game.notice = SOLD;
                    return true;
                }
                if (button === UP) {
                    const pet = game.save.tank.find(slot => !slot.species);
                    if (!pet) game.notice = FULL;
                    else {
                        pet.species = game.save.pending;
                        game.save.pending = 0;
                        game.notice = KEPT;
                        return true;
                    }
                }
                break;
            }
            if (button === UP) page(game, HOME);
            if (button === OK) page(game, PREP);
            break;
        case SHOP:
            if (button === UP) page(game, HOME);
            if (button === DOWN) { game.gear = 1 - game.gear; game.notice = NONE; }
            if (button !== OK || gearLevel(game) === 4) break;
            if (game.save.coins < cost(game)) { game.notice = POOR; break; }
            game.save.coins -= cost(game);
            if (game.gear) game.save.line++; else game.save.rod++;
            game.notice = NONE;
            return true;
        case BOOK:
            if (button === UP) game.group = (game.group + 10) % 11;
            if (button === DOWN) game.group = (game.group + 1) % 11;
            if (button === OK) {
                page(game, game.group === 10 ? HOME : BOOK_FISH);
                game.book = 0;
            }
            break;
        case BOOK_FISH:
            if (button === UP) game.book = (game.book + 9) % 10;
            if (button === DOWN) game.book = (game.book + 1) % 10;
            if (button === OK) page(game, BOOK);
            break;
        case TANK:
            if (button === UP) game.slot = (game.slot + 6) % 7;
            if (button === DOWN) game.slot = (game.slot + 1) % 7;
            if (button === OK) {
                if (game.slot === 6) page(game, HOME);
                else if (game.save.tank[game.slot].species) page(game, CARE);
            }
            break;
        case CARE: {
            const pet = game.save.tank[game.slot];
            if (button === UP) page(game, TANK);
            if (button === DOWN) page(game, SELL);
            if (button !== OK || pet.fed) break;
            if (pet.growth === 3) { game.notice = GROWN; break; }
            if (game.save.coins < 2) { game.notice = POOR; break; }
            game.save.coins -= 2;
            pet.fed = 1;
            game.notice = FED;
            return true;
        }
        case SELL:
            if (button === UP) page(game, CARE);
            if (button !== OK) break;
            pay(game, tankValue(game.save.tank[game.slot]));
            game.save.tank[game.slot] = emptyPet();
            page(game, TANK);
            game.notice = SOLD;
            return true;
    }
    return false;
}

const fishingRules = {species, baitNames, baitFor, create, press, tick, cursor, halfZone, cost,
    gearLevel, pulling, seen, tankValue, seasonNames, periodNames, periodOf, seasonOf, dayOf,
    seasonsFor, periodsFor, active};
if (typeof module !== 'undefined') module.exports = fishingRules;
