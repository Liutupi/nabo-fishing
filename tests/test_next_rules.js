'use strict';
// Checks for the v0.6 design preview in prototype/next: store, blind boxes, ornaments.
// It runs on the browser rules alone; the firmware does not have these features yet.
const assert = require('node:assert/strict');
const rules = require('../prototype/next/rules.js');
const [up, down, ok] = [0, 1, 2];
const STORE = 13, BOX = 14, DECOR = 15;

let game = rules.create({coins: 39, rod: 1, seen: [0, 0, 0, 0]}, 5);
assert.equal(game.save.capacity, 6);
assert.equal(game.save.tankType, 0);
rules.press(game, up, 0);
rules.press(game, ok, 1);
assert.equal(game.page, STORE);
assert.equal(rules.press(game, ok, 2), false, 'a box costs more than 39 shells');
assert.equal(game.save.coins, 39);

// Open many boxes: coins never go negative, nothing is owned twice, fish respect the capacity.
game = rules.create({coins: 4000, rod: 1, seen: [0, 0, 0, 0]}, 11);
rules.press(game, up, 0);
rules.press(game, ok, 1);
const prizes = new Set();
let time = 10;
for (let i = 0; i < 300 && game.save.coins >= rules.BOX_COST; i++) {
    const before = game.save.coins;
    assert.equal(rules.press(game, ok, time), true);
    assert.equal(game.page, BOX);
    assert.equal(game.save.coins, before - rules.BOX_COST + game.prizeCoins);
    prizes.add(game.prize);
    rules.press(game, ok, time + 100);
    assert.equal(game.page, BOX, 'the lid needs 0.9 s to open');
    rules.press(game, ok, time + 1000);
    assert.equal(game.page, STORE);
    time += 2000;
}
assert.equal(prizes.size, 5, 'every kind of prize turned up');
assert.equal(game.save.decorShown & ~game.save.decorOwned, 0);
assert.ok(game.save.tank.slice(game.save.capacity).every(pet => !pet.species));
assert.ok(game.save.tank.filter(pet => pet.species).every(pet => pet.variant === 1 || pet.variant === 2));

// A save survives a reload unchanged, and the ornament menu toggles what is on show.
const copy = rules.create(JSON.parse(JSON.stringify(game.save)), 1);
assert.deepEqual(copy.save, game.save);
rules.press(copy, up, 0);
rules.press(copy, up, 1);
rules.press(copy, ok, 2);
for (let i = 0; i < copy.save.capacity; i++) rules.press(copy, down, 3 + i);
rules.press(copy, ok, 20);
assert.equal(copy.page, DECOR);
const menu = rules.decorMenu(copy);
assert.ok(menu.length > 2, 'some ornament is owned after 100 boxes');
rules.press(copy, down, 21);
const shown = copy.save.decorShown;
assert.equal(rules.press(copy, ok, 22), true);
assert.equal(copy.save.decorShown, shown ^ 1 << menu[1].id);
console.log('PASS: v0.6 preview rules (store, blind boxes, ornaments)');
