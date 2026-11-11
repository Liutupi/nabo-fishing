'use strict';

const species = typeof module === 'undefined' ? catalogSpecies : require('./species.js');
const maxCoins = 0xffffffff;

function create(save, seed) {
    if (save && Number.isInteger(save.seen) && save.seen >= 0 && save.seen <= 63) {
        save = {...save, seen: [save.seen, 0, 0, 0]};
    }
    if (!save || !Number.isInteger(save.coins) || save.coins < 0 || save.coins > maxCoins ||
        !Number.isInteger(save.rod) || save.rod < 1 || save.rod > 4 ||
        !Array.isArray(save.seen) || save.seen.length !== 4 || save.seen[3] > 15 ||
        save.seen.some(word => !Number.isInteger(word) || word < 0 || word > maxCoins)) {
        save = {coins: 0, rod: 1, seen: [0, 0, 0, 0]};
    }
    return {save: {...save, seen: [...save.seen]}, page: 0, result: 0, rng: seed >>> 0,
        biteAt: 0, selected: 0, group: 0, book: 0, caught: 0, poor: false};
}

function pick(game, count) {
    game.rng = (game.rng + 0x6d2b79f5) >>> 0;
    let t = game.rng;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return Math.floor(((t ^ (t >>> 14)) >>> 0) * count / 0x100000000);
}

function cast(game, now) {
    game.page = 1;
    game.biteAt = (now + 1800 + pick(game, 2201)) >>> 0;
}

function tick(game, now) {
    const elapsed = (now - game.biteAt) | 0;
    if (elapsed < 0) return;
    if (game.page === 1) game.page = 2;
    if (game.page === 2 && elapsed >= 5200) {
        game.page = 3;
        game.result = 2;
    }
}

function cursor(game, now) {
    const position = ((now - game.biteAt) >>> 0) % 2600;
    return position > 1300 ? 2600 - position : position;
}
function halfZone(game) { return 9 + game.save.rod * 2; }
function cost(game) { return game.save.rod * 45; }

function seen(game, id) { return !!(game.save.seen[id >>> 5] & (1 << (id % 32))); }

function catchFish(game) {
    const eligible = species.map((fish, id) => fish.level <= game.save.rod ? id : -1)
        .filter(id => id !== -1);
    const unseen = eligible.filter(id => !seen(game, id));
    const pool = unseen.length && pick(game, 3) === 0 ? unseen : eligible;
    return pool[pick(game, pool.length)];
}

function press(game, button, now) {
    const before = game.page;
    tick(game, now);
    if (before !== 3 && game.page === 3 && button === 2) return false;
    switch (game.page) {
        case 0:
            if (button === 0) game.selected = (game.selected + 2) % 3;
            if (button === 1) game.selected = (game.selected + 1) % 3;
            if (button === 2) {
                if (game.selected === 0) cast(game, now);
                else game.page = game.selected === 1 ? 4 : 5;
            }
            break;
        case 1:
            if (button === 0) game.page = 0;
            break;
        case 2:
            if (button === 0) game.page = 0;
            if (button !== 2) break;
            game.page = 3;
            game.result = 1;
            if (Math.abs(cursor(game, now) - 650) > halfZone(game) * 13) break;
            game.caught = catchFish(game);
            game.save.coins = Math.min(maxCoins, game.save.coins + species[game.caught].value);
            const word = game.caught >>> 5;
            game.save.seen[word] = (game.save.seen[word] | (1 << (game.caught % 32))) >>> 0;
            game.result = 0;
            return true;
        case 3:
            if (button === 0) game.page = 0;
            if (button === 2) cast(game, now);
            break;
        case 4:
            if (button === 0) { game.page = 0; game.poor = false; }
            if (button !== 2 || game.save.rod === 4) break;
            if (game.save.coins < cost(game)) { game.poor = true; break; }
            game.save.coins -= cost(game);
            game.save.rod++;
            game.poor = false;
            return true;
        case 5:
            if (button === 0) game.group = (game.group + 10) % 11;
            if (button === 1) game.group = (game.group + 1) % 11;
            if (button === 2) {
                game.page = game.group === 10 ? 0 : 6;
                game.book = 0;
            }
            break;
        case 6:
            if (button === 0) game.book = (game.book + 9) % 10;
            if (button === 1) game.book = (game.book + 1) % 10;
            if (button === 2) game.page = 5;
            break;
    }
    return false;
}

const fishingRules = {species, create, press, tick, cursor, halfZone, cost, seen};
if (typeof module !== 'undefined') module.exports = fishingRules;
