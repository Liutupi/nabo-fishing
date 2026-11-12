(() => {
    'use strict';
    const {species, baitNames, baitFor, create, press, tick, cursor, halfZone,
        cost, gearLevel, seen, tankValue, pulling, seasonNames, periodNames,
        periodOf, seasonOf, dayOf, seasonsFor, periodsFor, active, tanks, decors, shelf, decorMenu,
        roomCost, BOX_COST} = fishingRules;
    const key = 'nabo-fishing-prototype-next';   // kept apart from the released prototype's save
    const time = () => Math.floor(performance.now()) >>> 0;
    const seed = () => Math.floor(Math.random() * 0x100000000);
    let save;
    try { save = JSON.parse(localStorage.getItem(key)); } catch (_) {}
    let game = create(save, seed());
    const $ = id => document.getElementById(id);
    const font = (size, bold) => (bold ? 'bold ' : '') + size + "px 'Microsoft YaHei', 'Noto Sans CJK SC', sans-serif";

    function persist() {
        try { localStorage.setItem(key, JSON.stringify(game.save)); } catch (_) {}
    }

    function habitat(id) {
        const pick = (mask, names, all) => mask === 15 ? all :
            names.filter((_, i) => mask >> i & 1).join('');
        return pick(seasonsFor(id), seasonNames, '四季') + ' · ' + pick(periodsFor(id), ['晨', '昼', '昏', '夜'], '全天');
    }

    // ---- Shore weather: a few drifting specks per season, drawn over the backdrop.
    const weather = $('weather').getContext('2d');
    function drawWeather(now) {
        const season = seasonOf(game.save.clock), night = periodOf(game.save.clock) === 3;
        weather.clearRect(0, 0, 240, 160);
        if (game.page >= 3 && game.page <= 7 || game.page >= 13) return;
        const t = now / 1000;
        for (let i = 0; i < 12; i++) {
            const a = i * 37.7 % 1, b = i * 0.618 % 1;
            if (season === 1) {                       // summer: fireflies after dark
                if (!night || i > 6) continue;
                const x = 120 + 110 * Math.sin(t * (0.21 + a * 0.2) + i * 2.1);
                const y = 96 + 34 * Math.sin(t * (0.33 + b * 0.2) + i);
                weather.fillStyle = 'rgba(232,255,140,' + (0.45 + 0.45 * Math.sin(t * 3 + i * 1.7)) + ')';
                weather.fillRect(Math.round(x), Math.round(y), 2, 2);
                continue;
            }
            const fall = season === 3 ? 16 + b * 12 : 11 + b * 9;
            const y = (b * 160 + t * fall + i * 29) % 168 - 4;
            const drift = season === 3 ? 4 : 13;
            const x = (a * 240 + i * 53 - t * drift + 7 * Math.sin(t * 1.4 + i)) % 240;
            weather.fillStyle = season === 0 ? '#ffc2d6' : season === 2 ? (i % 2 ? '#e8892f' : '#c9502b') : '#ffffff';
            const size = season === 3 ? 2 : 3;
            weather.fillRect(Math.round((x + 240) % 240), Math.round(y), size, season === 3 ? 2 : 2);
        }
    }

    // ---- Landscape aquarium, 320 × 240.
    const tank = $('tank').getContext('2d');
    const atlas = new Image();
    atlas.src = '../../art/fish-catalog-alpha.png';
    const picture = path => { const image = new Image(); image.src = '../../art/' + path; return image; };
    const backdrops = tanks.map((_, i) => picture('tanks/tank-' + i + '.png'));
    const ornaments = decors.map((_, i) => picture('decor/decor-' + i + '.png'));
    // Where each ornament stands: centre x and the y of its base. The jellyfish lamp floats.
    const spots = [[52, 190], [100, 192], [296, 190], [140, 190], [18, 190], [228, 188],
        [180, 192], [266, 190], [76, 96], [124, 150]];
    const swimmers = [];
    let fedAt = -1e9;
    const sizes = [48, 56, 64, 72].map(width => width / 84);   // sprite widths on the badge

    function swim(slot, pet, now, dt) {
        let s = swimmers[slot];
        if (!s || s.species !== pet.species) {
            s = swimmers[slot] = {species: pet.species, x: 40 + slot * 34, y: 62 + slot * 53 % 100,
                tx: 44 + Math.random() * 232, ty: 50 + Math.random() * 118,
                speed: 14 + slot * 3, dir: 1, phase: slot * 1.3, pause: 0};
        }
        s.phase += dt * (5 + s.speed / 6);
        if (s.pause > 0) { s.pause -= dt; return s; }
        const dx = s.tx - s.x, dy = s.ty - s.y, distance = Math.hypot(dx, dy);
        if (distance < 3) {
            s.tx = 44 + Math.random() * 232;
            s.ty = 50 + Math.random() * 110;
            s.speed = 12 + Math.random() * 20;
            s.pause = Math.random() < 0.35 ? 0.4 + Math.random() : 0;
            return s;
        }
        if (Math.abs(dx) > 4) s.dir = dx > 0 ? 1 : -1;
        s.x += dx / distance * s.speed * dt;
        s.y += dy / distance * s.speed * dt * 0.6;
        return s;
    }

    function drawFish(s, scale, variant) {
        const id = s.species - 1, sx = id % 10 * 84, sy = Math.floor(id / 10) * 60;
        tank.save();
        if (variant) tank.filter = 'url(#variant' + variant + ')';   // off-colour fish swap colour channels
        tank.translate(Math.round(s.x), Math.round(s.y));
        tank.scale(s.dir * scale, scale);
        for (let k = 0; k < 7; k++) {                // the art faces right; the tail strips wag most
            const sway = Math.round(Math.sin(s.phase - k * 0.8) * 3 * ((6 - k) / 6) ** 2);
            tank.drawImage(atlas, sx + k * 12, sy, 12, 60, -42 + k * 12, -30 + sway, 12.4, 60);
        }
        tank.restore();
    }

    function chip(x, y, w, text, color, align) {
        tank.fillStyle = 'rgba(16,47,56,0.86)';
        tank.beginPath(); tank.roundRect(x, y, w, 22, 7); tank.fill();
        tank.fillStyle = color; tank.font = font(13); tank.textAlign = align || 'center';
        tank.fillText(text, x + w / 2, y + 16);
    }

    let lastTank = 0;
    function drawTank(now) {
        const dt = Math.min(0.1, (now - lastTank) / 1000); lastTank = now;
        const {save, page, slot} = game;
        const t = now / 1000;
        tank.imageSmoothingEnabled = false;
        tank.drawImage(backdrops[save.tankType], 0, 0);
        const menu = decorMenu(game);
        const arranging = page === 15 && menu[game.decor].kind === 'decor' ? menu[game.decor].id : -1;
        ornaments.forEach((image, id) => {
            const shown = save.decorShown >> id & 1;
            if (!shown && id !== arranging) return;
            const [x, base] = spots[id];
            const lift = id === 8 ? Math.round(Math.sin(t * 1.3) * 4) : 0;
            tank.globalAlpha = shown ? 1 : 0.35;      // a put-away ornament shows faintly while it is selected
            tank.drawImage(image, Math.round(x - image.width / 2), base - image.height + lift);
            tank.globalAlpha = 1;
            if (id === arranging) {
                tank.fillStyle = '#ffc85e';
                tank.fillRect(Math.round(x) - 4, base - image.height + lift - 9 + Math.round(Math.sin(t * 5) * 2), 8, 4);
            }
        });
        if (save.decorShown >> 4 & 1) {              // the bubble stone adds its own stream
            tank.fillStyle = 'rgba(205,255,240,0.7)';
            for (let i = 0; i < 5; i++)
                tank.fillRect(16 + Math.round(3 * Math.sin(t * 3 + i * 2)), Math.round(160 - (t * 30 + i * 31) % 150), 2, 2);
        }
        tank.fillStyle = 'rgba(205,255,240,0.55)';
        for (let i = 0; i < 9; i++) {                // bubbles
            const y = 186 - (t * (16 + i % 3 * 6) + i * 41) % 180;
            tank.fillRect(Math.round([64, 70, 150, 236, 242, 60, 156, 230, 248][i] + 3 * Math.sin(t * 2 + i)), Math.round(y), 2 + i % 2, 2 + i % 2);
        }
        const order = save.tank.map((pet, i) => i).filter(i => save.tank[i].species)
            .sort((a, b) => (a === slot) - (b === slot));
        let chosen = null;
        for (const i of order) {
            const pet = save.tank[i], s = swim(i, pet, now, dt);
            drawFish(s, sizes[pet.growth], pet.variant);
            if (i === slot) chosen = s;
        }
        if (chosen) {
            const lift = 30 * sizes[save.tank[slot].growth] + 7 + Math.round(Math.sin(t * 5) * 2);
            tank.fillStyle = '#ffc85e';
            tank.beginPath(); tank.moveTo(chosen.x - 6, chosen.y - lift - 7); tank.lineTo(chosen.x + 6, chosen.y - lift - 7);
            tank.lineTo(chosen.x, chosen.y - lift); tank.fill();
            const fed = now - fedAt;
            if (fed < 1100) {                        // feed pellets sink to the fish
                tank.fillStyle = '#e7a14b';
                for (let i = 0; i < 5; i++) {
                    const p = Math.min(1, fed / (700 + i * 90));
                    tank.fillRect(Math.round(chosen.x - 12 + i * 6), Math.round(6 + (chosen.y - 12) * p), 3, 3);
                }
            }
        }
        const count = save.tank.filter(pet => pet.species).length;
        if (!count) {
            tank.fillStyle = '#c9f1e6'; tank.font = font(14); tank.textAlign = 'center';
            tank.fillText('鱼缸还空着，钓到鱼后按上键放进来', 160, 112);
        }
        chip(8, 8, 96, tanks[save.tankType].name + ' ' + count + '/' + save.capacity, '#ffefd3');
        chip(220, 8, 92, '贝壳 ' + save.coins, '#ffc85e');
        const stops = save.capacity + 2;             // fish slots, then arrange, then the way out
        for (let i = 0; i < stops; i++) {
            const lit = page === 15 ? i === stops - 2 : i === slot;
            tank.fillStyle = lit ? '#ffc85e' : i < save.capacity && save.tank[i].species ? '#68dbb9' : 'rgba(16,47,56,0.8)';
            tank.beginPath(); tank.roundRect(110 + i * 10, 15, i >= save.capacity ? 9 : 7, 8, 3); tank.fill();
        }
        let name, status = '', keys;
        const pet = slot < save.capacity ? save.tank[slot] : null;
        if (page === 15) {
            const entry = menu[game.decor];
            if (entry.kind === 'tank') {
                const owned = tanks.filter((_, i) => save.tanksOwned >> i & 1).length;
                name = tanks[save.tankType].name; status = '已有 ' + owned + ' 款鱼缸';
                keys = owned > 1 ? 'OK 换一款   下 / 上 选择' : '商店和盲盒里有更多鱼缸';
            } else if (entry.kind === 'decor') {
                name = decors[entry.id].name;
                status = save.decorShown >> entry.id & 1 ? '摆放中' : '已收起';
                keys = 'OK ' + (save.decorShown >> entry.id & 1 ? '收起' : '摆出来') + '   下 / 上 选择';
            } else { name = '布置好了'; keys = 'OK 返回   下 / 上 选择'; }
        }
        else if (slot === save.capacity) { name = '布置鱼缸'; status = '换鱼缸、摆装饰'; keys = 'OK 进入   下 / 上 选鱼'; }
        else if (!pet) { name = '返回岸边'; keys = 'OK 返回   下 / 上 选鱼'; }
        else if (!pet.species) { name = '空位 ' + (slot + 1); status = '已养 ' + count + '/' + save.capacity; keys = '下 后一格   上 前一格'; }
        else {
            name = (pet.variant ? '异色' : '') + species[pet.species - 1].name;
            status = '成长 ' + pet.growth + '/3 · ' + (pet.fed ? '已喂' : '待喂食');
            keys = 'OK 照料   下 后一条   上 前一条';
            if (page === 11) {
                status = game.notice === 5 ? '贝壳不足' : pet.growth === 3 ? '已经长大了' :
                    pet.fed ? '已喂，钓到鱼会成长' : '喂食 2 贝壳，钓鱼后成长';
                keys = 'OK 喂食   下 出售   上 返回';
            } else if (page === 12) {
                status = '确认出售 · ' + tankValue(pet) + ' 贝壳';
                keys = 'OK 出售   上 取消';
            }
        }
        tank.fillStyle = 'rgba(16,47,56,0.88)';
        tank.beginPath(); tank.roundRect(8, 184, 304, 50, 10); tank.fill();
        tank.textAlign = 'left'; tank.fillStyle = page === 12 ? '#ff9a7a' : '#ffefd3'; tank.font = font(15, true);
        tank.font = font(18, true);
        tank.fillText(name, 18, 206);
        tank.textAlign = 'right'; tank.fillStyle = '#a6c3bc'; tank.font = font(12);
        tank.fillText(status, 302, 204);
        tank.textAlign = 'left'; tank.fillStyle = '#ffc85e';
        tank.fillText(keys, 18, 226);
    }

    function render(now) {
        const {save, page} = game;
        const aquarium = page >= 10 && page <= 12 || page === 15;
        $('device').classList.toggle('landscape', aquarium);
        $('screen').dataset.page = page;
        if (aquarium) { drawTank(now); return; }
        const season = seasonOf(save.clock), period = periodOf(save.clock);
        $('screen').style.setProperty('--shore', "url('../../art/shore/shore-" + season + period + ".png')");
        $('clock').textContent = seasonNames[season] + ' · ' + periodNames[period];
        const tankCount = save.tank.filter(slot => slot.species).length;
        const aiming = page === 8 || page === 2;
        const fighting = page === 9;
        $('coins').textContent = '贝壳 ' + save.coins;
        $('meter').hidden = !aiming && !fighting;
        $('meter').classList.toggle('danger', fighting);
        $('zone').style.left = (fighting ? 72 : 50 - halfZone(game)) + '%';
        $('zone').style.width = (fighting ? 28 : 2 * halfZone(game)) + '%';
        $('cursor').style.left = (fighting ? game.tension : cursor(game, now) / 13) + '%';
        $('progress').hidden = !fighting;
        $('progress').firstElementChild.style.width = (fighting ? 100 * game.landed / game.required : 0) + '%';
        $('line').hidden = $('float').hidden = page !== 1 && page !== 2 && !fighting;
        const bookId = game.group * 10 + game.book;
        const id = page === 3 && game.result === 0 ? game.caught :
            page === 6 && seen(game, bookId) ? bookId : -1;
        $('fish-art').hidden = id < 0;
        drawIcon(now);
        if (id >= 0) $('fish-art').style.backgroundPosition =
            -(id % 10) * 84 + 'px ' + -Math.floor(id / 10) * 60 + 'px';
        let title = '', hint = '', items = [];
        switch (page) {
            case 0:
                title = 'NABO 钓鱼小岛';
                hint = '图鉴 ' + species.filter((_, i) => seen(game, i)).length + '/100 · 出没 ' +
                    species.filter((fish, i) => fish.level <= save.rod && active(i, save.clock)).length + ' 种';
                items = ['钓鱼', '工坊', '图鉴', '鱼缸', '商店'];
                break;
            case 7:
                title = '钓前准备';
                if (game.prep < 4) {
                    hint = baitNames[game.prep] + (game.chum ? ' · 已选打窝' : '');
                    items = ['↑↓ 选饵', 'OK 装饵'];
                } else if (game.prep === 4) {
                    hint = game.notice === 5 ? '贝壳不足' :
                        game.chum ? '打窝已选，抛竿时扣费' : '打窝花 3 贝壳，等口更快';
                    items = ['↑↓ 选择', 'OK 切换'];
                } else if (game.prep === 5) {
                    hint = '等到' + periodNames[(period + 1) % 4] + '再钓';
                    items = ['↑↓ 选择', 'OK 歇一会'];
                } else {
                    hint = '返回岸边';
                    items = ['↑↓ 选择', 'OK 返回'];
                }
                break;
            case 8:
                title = '瞄准抛竿';
                hint = '指针进金区，优先好鱼';
                items = ['↑ 换饵', 'OK 抛竿'];
                break;
            case 1:
                title = '等鱼上钩';
                hint = baitNames[game.bait] + (game.chummed ? ' · 已打窝' : ' · 看浮漂');
                items = ['↑ 返回岸边'];
                break;
            case 2:
                title = '鱼咬钩了';
                hint = '指针进金区时刺鱼';
                items = ['↑ 返回', 'OK 刺鱼'];
                break;
            case 9:
                title = '遛鱼收线';
                hint = pulling(game, now) ? '鱼在挣扎，先放线' : '鱼缓下来了，快收线';
                items = ['↑ 退出', '↓ 放线', 'OK 收线'];
                break;
            case 3:
                if (game.result === 0) {
                    const fish = species[game.caught];
                    title = fish.name;
                    hint = save.pending ? '出售 ' + fish.value + ' 贝壳 · 鱼缸 ' + tankCount + '/' + save.capacity :
                        game.notice === 2 ? '已放进鱼缸' : '已出售';
                    if (game.notice === 1) hint = '鱼缸满了，可以出售';
                    items = save.pending ? ['↑ 放进鱼缸', '↓ 出售'] : ['↑ 回岸', 'OK 再钓'];
                } else {
                    title = ['', '差一点', '鱼儿游走了', '鱼线断了'][game.result];
                    hint = '再抛一竿试试';
                    items = ['↑ 回岸', 'OK 再钓'];
                }
                break;
            case 4:
                title = game.gear ? '升级鱼线' : '升级鱼竿';
                hint = gearLevel(game) === 4 ? '已满级' : game.notice === 5 ? '贝壳不足' :
                    'Lv.' + gearLevel(game) + ' · ' + cost(game) + ' 贝壳';
                items = ['↑ 返回', '↓ 换装备', 'OK 升级'];
                break;
            case 5:
                title = '鱼类图鉴';
                hint = game.group === 10 ? '返回岸边' : '第 ' + (game.group + 1) + ' 册';
                items = ['↑↓ 选册', 'OK 确认'];
                break;
            case 6:
                title = seen(game, bookId) ? species[bookId].name : '未知鱼种';
                hint = seen(game, bookId) ? baitNames[baitFor(bookId)] + ' · ' + habitat(bookId) :
                    '竿 Lv.' + species[bookId].level + ' · 习性未知';
                items = ['↑↓ 翻鱼', 'OK 返回目录'];
                break;
        }
        if (page === 13) {
            const entry = shelf[game.store];
            const owned = entry.kind === 'decor' ? save.decorOwned >> entry.id & 1 :
                entry.kind === 'tank' ? save.tanksOwned >> entry.id & 1 : entry.kind === 'room' && save.capacity === 8;
            const price = entry.kind === 'box' ? BOX_COST : entry.kind === 'room' ? roomCost(save.capacity) :
                entry.kind === 'decor' ? decors[entry.id].price : entry.kind === 'tank' ? tanks[entry.id].price : 0;
            title = {box: '盲盒', room: '鱼缸扩容', exit: '返回岸边'}[entry.kind] ||
                (entry.kind === 'decor' ? decors : tanks)[entry.id].name;
            hint = entry.kind === 'exit' ? '商店 ' + (game.store + 1) + '/' + shelf.length :
                game.notice === 5 ? '贝壳不足，要 ' + price + ' 贝壳' : game.notice === 8 ? '买到了' :
                owned ? (entry.kind === 'room' ? '已经是最大的 8 格' : '已拥有') :
                entry.kind === 'box' ? price + ' 贝壳 · 可开出稀有鱼缸和装饰' :
                entry.kind === 'room' ? price + ' 贝壳 · ' + save.capacity + ' 格变 ' + (save.capacity + 1) + ' 格' :
                price + ' 贝壳';
            items = ['↑↓ 选商品', entry.kind === 'exit' ? 'OK 返回' : entry.kind === 'box' ? 'OK 开一个' : 'OK 购买'];
        } else if (page === 14) {
            if (now - game.boxAt < 900) { title = '盲盒打开中'; hint = '会是什么呢'; }
            else {
                const again = game.prizeCoins && game.prize !== 0;   // a duplicate, or no room for the fish
                title = game.prize === 0 ? game.prizeCoins + ' 贝壳' :
                    game.prize === 2 ? '异色' + species[game.prizeId].name :
                    game.prize === 4 ? tanks[game.prizeId].name : decors[game.prizeId].name;
                hint = game.prize === 0 ? '一小袋贝壳' :
                    again ? (game.prize === 2 ? '鱼缸满了' : '已经有了') + '，换成 ' + game.prizeCoins + ' 贝壳' :
                    game.prize === 2 ? '稀有的异色鱼，已放进鱼缸' :
                    game.prize === 4 ? '稀有鱼缸，已经换上' :
                    game.prize === 3 ? '稀有装饰，已摆进鱼缸' : '新装饰，已摆进鱼缸';
                items = ['↑ 回商店', 'OK 继续'];
            }
        }
        $('title').textContent = title;
        $('hint').textContent = hint;
        $('menu').innerHTML = items.map((text, i) => '<div' +
            (page === 0 && i === game.selected ? ' class="sel"' : '') + '>' + text + '</div>').join('');
    }

    // The store and the blind box show a picture where the catalog shows a fish.
    const icon = $('icon').getContext('2d');
    function drawIcon(now) {
        const {save, page} = game;
        $('icon').hidden = page !== 13 && page !== 14;
        if ($('icon').hidden) return;
        icon.imageSmoothingEnabled = false;
        icon.clearRect(0, 0, 120, 90);
        const opening = page === 14 && now - game.boxAt < 900;
        const entry = page === 13 ? shelf[game.store] :
            opening ? {kind: 'box'} : {kind: ['coins', 'decor', 'fish', 'decor', 'tank'][game.prize], id: game.prizeId};
        if (entry.kind === 'tank') icon.drawImage(backdrops[entry.id], 0, 0, 320, 240, 12, 9, 96, 72);
        else if (entry.kind === 'room') icon.drawImage(backdrops[save.tankType], 0, 0, 320, 240, 12, 9, 96, 72);
        else if (entry.kind === 'decor') {
            const image = ornaments[entry.id];
            icon.drawImage(image, 60 - image.width, 84 - image.height * 2, image.width * 2, image.height * 2);
        } else if (entry.kind === 'fish') {
            icon.filter = 'url(#variant' + (save.tank.find(pet => pet.species === entry.id + 1 && pet.variant) || {variant: 1}).variant + ')';
            icon.drawImage(atlas, entry.id % 10 * 84, Math.floor(entry.id / 10) * 60, 84, 60, 18, 15, 84, 60);
            icon.filter = 'none';
        } else if (entry.kind === 'coins') {
            icon.fillStyle = '#ffc85e';
            [[44, 56], [60, 50], [76, 56], [52, 66], [68, 66]].forEach(([x, y]) => {
                icon.beginPath(); icon.arc(x, y, 9, 0, 7); icon.fill();
            });
        } else if (entry.kind === 'box') {
            const shake = opening ? Math.round(Math.sin(now / 35) * 3) : 0;
            icon.fillStyle = '#ff8a5c'; icon.fillRect(34 + shake, 30, 52, 44);
            icon.fillStyle = '#ffc85e'; icon.fillRect(30 + shake, 22, 60, 12); icon.fillRect(56 + shake, 22, 8, 52);
            icon.fillStyle = '#102f38'; icon.font = font(22, true); icon.textAlign = 'center';
            icon.fillText('?', 60 + shake + 14, 62);
        } else {
            icon.fillStyle = '#a6c3bc'; icon.font = font(30, true); icon.textAlign = 'center';
            icon.fillText('←', 60, 56);
        }
    }

    function act(button) {
        const now = time();
        if (press(game, button, now)) persist();
        if (game.notice === 4) fedAt = now;
        render(now);
    }
    ['up', 'down', 'ok'].forEach((id, i) => $(id).addEventListener('click', () => act(i)));
    document.addEventListener('keydown', event => {
        const button = {ArrowUp: 0, ArrowDown: 1, Enter: 2}[event.key];
        if (button === undefined || event.repeat) return;
        event.preventDefault();
        act(button);
    });
    function debug(id, change) {
        $(id).addEventListener('click', () => { change(game.save); persist(); render(time()); });
    }
    debug('skip-period', s => { s.clock = (Math.floor(s.clock / 3) + 1) * 3 % 144; });
    debug('skip-season', s => { s.clock = (s.clock + 36) % 144; });
    debug('add-coins', s => { s.coins += 200; });
    debug('fill-tank', s => s.tank.forEach((pet, i) => {
        if (pet.species || i >= s.capacity) return;
        pet.species = 1 + Math.floor(Math.random() * 100);
        pet.growth = Math.floor(Math.random() * 4);
    }));
    $('reset').addEventListener('click', () => {
        if (!confirm('确定清除本浏览器中的原型进度？')) return;
        game = create(null, seed());
        persist();
        render(time());
    });
    function frame() {
        const now = time();
        const previous = game.page;
        tick(game, now);
        if (game.page >= 10 && game.page <= 12 || game.page === 15) drawTank(now);
        else {
            if (previous !== game.page || [2, 8, 9, 14].includes(game.page)) render(now);
            drawWeather(now);
        }
        requestAnimationFrame(frame);
    }
    render(time());
    requestAnimationFrame(frame);
})();
