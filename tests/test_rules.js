'use strict';
const assert = require('node:assert/strict');
const {spawnSync} = require('node:child_process');
const {mkdtempSync, rmSync} = require('node:fs');
const {tmpdir} = require('node:os');
const {join, resolve} = require('node:path');
const {create, press, tick, species, seen, baitFor, pulling, tankValue, active, seasonOf, periodOf} = require('../prototype/rules.js');

const commands = [], expected = [];
const [up, down, ok] = [0, 1, 2];
let game;
function snapshot(changed = false) {
    expected.push([game.page, game.result, game.rng, game.biteAt, game.selected,
        game.group, game.book, game.caught, game.notice, game.prep, game.bait, game.gear,
        game.slot, game.tension, game.landed, game.required, +game.chum, +game.chummed,
        +game.precise, game.save.coins, game.save.rod, ...game.save.seen, game.save.line,
        game.save.pending, ...game.save.tank.map(p => p.species | p.growth << 8 | p.fed << 16), game.save.clock,
        +changed].join(' '));
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
function reload(seed) {
    game = create(JSON.parse(JSON.stringify(game.save)), seed);
    commands.push('r ' + seed);
    snapshot();
}
function prepare(time, bait = 0) {
    assert.equal(game.page, 7);
    while (game.prep !== bait) key(down, time++);
    key(ok, time);
    key(ok, time + 650);
    assert.equal(game.page, 1);
    assert.equal(game.precise, true);
    return time + 650;
}
function hook() {
    const time = game.biteAt;
    advance(time - 1);
    assert.equal(game.page, 1);
    advance(time);
    assert.equal(game.page, 2);
    key(ok, time + 650);
    assert.equal(game.page, 9);
    return time + 650;
}
function land(time) {
    while (game.page === 9) {
        time = (time + 310) >>> 0;
        if (game.tension > 42 || pulling(game, time)) key(down, time);
        else key(ok, time);
    }
    assert.equal(game.result, 0);
    assert.equal(game.save.pending, game.caught + 1);
    return time;
}

let matches = 0, chummedMatches = 0;
for (let seed = 0; seed < 160; seed++) {
    for (let useChum = 0; useChum <= 1; useChum++) {
        init(seed, 8, 4);
        const start = 0xfffffff0;
        key(ok, start);
        if (useChum) {
            for (let i = 0; i < 4; i++) key(down, start + i);
            key(ok, start + 4);
            assert.equal(game.save.coins, 8);
        }
        const castAt = prepare(start + 8, seed % 4);
        const elapsed = (game.biteAt - castAt) >>> 0;
        assert.ok(elapsed >= (useChum ? 1000 : 1800) && elapsed <= (useChum ? 1900 : 4000));
        assert.equal(game.save.coins, useChum ? 5 : 8);
        const time = hook();
        if (baitFor(game.caught) === game.bait) {
            if (useChum) chummedMatches++; else matches++;
        }
        assert.ok(species[game.caught].level <= game.save.rod);
        assert.ok(active(game.caught, game.save.clock));
        assert.equal(game.save.clock, 1);
        const end = land(time);
        key(down, end + 10);
        assert.ok(seen(game, game.caught));
        assert.equal(game.save.pending, 0);
    }
}
assert.ok(matches > 80 && chummedMatches > matches, `bait matches ${matches}, chummed ${chummedMatches}`);

init(42, 60, 1, 3);
key(down, 0); key(ok, 10); key(down, 20); key(ok, 30);
assert.equal(game.save.line, 2);
assert.equal(game.save.rod, 1);
assert.equal(game.save.coins, 28);
key(down, 40); key(ok, 50);
assert.equal(game.notice, 5);
assert.equal(game.save.rod, 1);
key(up, 60); key(up, 70); key(ok, 80);
prepare(90);
let at = land(hook());
const caught = game.caught;
const coins = game.save.coins;
reload(123);
assert.equal(game.page, 3);
assert.equal(game.caught, caught);
key(up, at += 50);
assert.equal(game.save.tank[0].species, caught + 1);
assert.equal(game.save.coins, coins);
key(up, at += 50); key(up, at += 50); key(ok, at += 50); key(ok, at += 50);
assert.equal(game.page, 11);
key(ok, at += 50); key(ok, at += 50);
assert.equal(game.save.coins, coins - 2);
assert.equal(game.save.tank[0].fed, 1);
reload(52);
key(ok, at += 50); prepare(at += 50);
at = land(hook());
assert.equal(game.save.tank[0].growth, 1);
assert.equal(game.save.tank[0].fed, 0);
key(down, at += 50); key(up, at += 50); key(up, at += 50); key(ok, at += 50); key(ok, at += 50);
const grownValue = tankValue(game.save.tank[0]);
const beforeSell = game.save.coins;
key(down, at += 50); key(up, at += 50);
assert.equal(game.save.tank[0].growth, 1);
key(down, at += 50); key(ok, at += 50);
assert.equal(game.save.coins, beforeSell + grownValue);
assert.equal(game.save.tank[0].species, 0);

init(9, 0, 4);
key(ok, 0); prepare(10);
let time = hook();
while (!pulling(game, time)) time += 50;
while (game.page === 9) key(ok, ++time);
assert.equal(game.result, 3);
assert.equal(game.save.pending, 0);
key(ok, time += 100);
prepare(time);
advance(game.biteAt + 5200);
assert.equal(game.result, 2);

init(2026, 0, 4);
time = 0;
key(ok, time);
for (let i = 0; i < 1500 && game.save.seen[3] <= 15; i++) {
    prepare(time, i % 4);
    time = land(hook()) + 200;
    key(down, time);
    key(ok, time + 100);
    time += 250;
    if (game.save.seen.every((bits, j) => bits === (j === 3 ? 15 : 0xffffffff))) break;
}
assert.deepEqual(game.save.seen, [0xffffffff, 0xffffffff, 0xffffffff, 15]);

// Resting skips to the next period; a year wraps after 144 casts, and night fish stay in the night.
init(7, 0, 4);
key(ok, 0);
for (let i = 0; i < 5; i++) key(down, 10 + i);
assert.equal(game.prep, 5);
key(ok, 20);
assert.equal(game.save.clock, 3);
assert.equal(game.page, 7);
for (let i = 0; i < 47; i++) key(ok, 30 + i);
assert.equal(game.save.clock, 0);
for (let i = 0; i < 3; i++) key(ok, 100 + i);
assert.equal(periodOf(game.save.clock), 3);
reload(99);
assert.equal(game.save.clock, 9);
key(ok, 200);
time = 300;
for (let i = 0; i < 2; i++) {
    prepare(time, 3);
    time = land(hook()) + 200;
    assert.ok(active(game.caught, game.save.clock));
    key(down, time); key(ok, time + 100);
    time += 250;
}
assert.equal(periodOf(game.save.clock), 3);
assert.equal(seasonOf(game.save.clock), 0);

const root = resolve(__dirname, '..');
const dir = mkdtempSync(join(tmpdir(), 'nabo-fish-tests-'));
const exe = join(dir, process.platform === 'win32' ? 'rules.exe' : 'rules');
function run(command, args, opts = {}) {
    const out = spawnSync(command, args, {cwd: root, encoding: 'utf8', maxBuffer: 12_000_000, ...opts});
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
    console.log('PASS: ' + actual.length + ' C/JS steps; old saves, bait/chum, fighting, 100 fish and aquarium');
    console.log('Matching bait in fixed seed run: ' + matches + '/160, chummed: ' + chummedMatches + '/160');
} finally {
    rmSync(dir, {recursive: true, force: true});
}
