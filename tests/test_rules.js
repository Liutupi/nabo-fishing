'use strict';
const assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
const {mkdtempSync, rmSync} = require('node:fs');
const {tmpdir} = require('node:os');
const {join, resolve} = require('node:path');
const {create, press, tick, species, seen} = require('../prototype/rules.js');

const commands = [], expected = [];
let game;
function snapshot(changed = false) {
    expected.push([game.page, game.result, game.rng, game.biteAt, game.selected,
        game.group, game.book, game.caught, +game.poor, game.save.coins, game.save.rod,
        ...game.save.seen, +changed].join(' '));
}
function init(seed, coins, rod, seen = 0) {
    game = create({coins, rod, seen}, seed);
    commands.push('i ' + [seed, coins, rod, seen].join(' '));
    snapshot();
}
function key(button, now) {
    now >>>= 0;
    commands.push('p ' + button + ' ' + now);
    snapshot(press(game, button, now));
}
function advance(now) {
    now >>>= 0;
    commands.push('t ' + now);
    tick(game, now);
    snapshot();
}
const [up, down, ok] = [0, 1, 2];

for (let seed = 0; seed < 128; seed++) {
    for (let rod = 1; rod <= 4; rod++) {
        init(seed, 0, rod);
        key(ok, 0xfffffff0);
        advance(game.biteAt - 1);
        assert.equal(game.page, 1);
        advance(game.biteAt);
        assert.equal(game.page, 2);
        const left = 650 - (9 + 2 * rod) * 13;
        key(ok, game.biteAt + left);
        assert.equal(game.result, 0);
        assert.ok(species[game.caught].level <= rod);
        assert.ok(seen(game, game.caught));
        key(ok, game.biteAt + 900);
        key(ok, game.biteAt + left - 1);
        assert.equal(game.result, 1);
        key(ok, game.biteAt + 1000);
        key(ok, game.biteAt + 5200);
        assert.equal(game.result, 2);
        assert.equal(game.page, 3);
        key(up, game.biteAt + 5300);
        key(up, game.biteAt + 5400);
        key(ok, game.biteAt + 5500);
        key(ok, game.biteAt + 5550);
        key(up, game.biteAt + 5600);
        assert.equal(game.book, 9);
        key(down, game.biteAt + 5700);
        assert.equal(game.book, 0);
    }
}

init(42, 44, 1);
key(down, 0);
key(ok, 10);
key(ok, 20);
assert.equal(game.poor, true);
assert.deepEqual(game.save, {coins: 44, rod: 1, seen: [0, 0, 0, 0]});
init(42, 270, 1, 3);
key(down, 0);
key(ok, 10);
for (let i = 0; i < 4; i++) key(ok, 20 + i);
assert.deepEqual(game.save, {coins: 0, rod: 4, seen: [3, 0, 0, 0]});
init(42, 0xffffffff, 4, 3);
key(ok, 0);
key(ok, game.biteAt + 650);
assert.equal(game.save.coins, 0xffffffff);
init(42, 99, 0, 255);
assert.deepEqual(game.save, {coins: 0, rod: 1, seen: [0, 0, 0, 0]});

init(2026, 0, 4);
let time = 0;
for (let i = 0; i < 650; i++) {
    key(ok, time);
    time = game.biteAt + 650;
    key(ok, time);
    time += 200;
}
assert.equal(species.length, 100);
assert.deepEqual(game.save.seen, [0xffffffff, 0xffffffff, 0xffffffff, 15]);
const reloaded = create(JSON.parse(JSON.stringify(game.save)), 0);
assert.equal(seen(reloaded, 99), true);

const root = resolve(__dirname, '..');
const dir = mkdtempSync(join(tmpdir(), 'nabo-fish-tests-'));
const exe = join(dir, process.platform === 'win32' ? 'rules.exe' : 'rules');
function run(command, args, opts = {}) {
    const out = spawnSync(command, args, {cwd: root, encoding: 'utf8', ...opts});
    if (out.error) throw out.error;
    assert.equal(out.status, 0, out.stderr);
    return out.stdout;
}
try {
    run(process.env.CC || 'cc', ['-std=c11', '-Wall', '-Wextra', '-Werror',
        '-I', 'firmware/main', 'tests/rules_cli.c', 'firmware/main/fishing_model.c', '-o', exe]);
    const actual = run(exe, [], {input: commands.join('\n')}).trimEnd().split(/\r?\n/);
    assert.equal(actual.length, expected.length);
    actual.forEach((line, i) => assert.equal(line, expected[i], 'step ' + (i + 1)));
    console.log('PASS: ' + actual.length + ' C/JS steps, catch boundaries, timeout, wrap, upgrades and collection');
} finally {
    rmSync(dir, {recursive: true, force: true});
}
