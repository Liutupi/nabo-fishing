(() => {
    'use strict';
    const {species, create, press, tick, cursor, halfZone, cost, seen} = fishingRules;
    const key = 'nabo-fishing-prototype-v1';
    const time = () => Math.floor(performance.now()) >>> 0;
    const seed = () => Math.floor(Math.random() * 0x100000000);
    let save;
    try { save = JSON.parse(localStorage.getItem(key)); } catch (_) {}
    let game = create(save, seed());
    const $ = id => document.getElementById(id);

    function persist() {
        try { localStorage.setItem(key, JSON.stringify(game.save)); } catch (_) {}
    }

    function render(now) {
        const {save, page} = game;
        $('screen').dataset.page = page;
        $('coins').textContent = '贝壳 ' + save.coins;
        $('meter').hidden = page !== 2;
        $('line').hidden = $('float').hidden = page !== 1 && page !== 2;
        const id = page === 3 && game.result === 0 ? game.caught :
            page === 6 && seen(game, game.group * 10 + game.book) ? game.group * 10 + game.book : -1;
        $('fish-art').hidden = id < 0;
        if (id >= 0) $('fish-art').style.backgroundPosition =
            -(id % 10) * 84 + 'px ' + -Math.floor(id / 10) * 60 + 'px';
        let title, hint, items;
        switch (page) {
            case 0:
                title = '今天也来钓鱼吧';
                hint = '鱼竿 Lv.' + save.rod + '　图鉴 ' +
                    species.filter((_, i) => seen(game, i)).length + '/' + species.length;
                items = ['去钓鱼', '工坊', '图鉴'];
                break;
            case 1:
                title = '等鱼上钩';
                hint = '看浮漂，鱼儿正在靠近';
                items = ['↑ 返回岸边'];
                break;
            case 2:
                $('zone').style.left = (50 - halfZone(game)) + '%';
                $('zone').style.width = (2 * halfZone(game)) + '%';
                $('cursor').style.left = (cursor(game, now) / 13) + '%';
                title = '鱼咬钩了';
                hint = '指针进金色区时按 OK 收竿';
                items = ['↑ 返回岸边'];
                break;
            case 3: {
                title = '钓鱼日记';
                const fish = species[game.caught];
                hint = ['钓到 ' + fish.name + '！自动出售，获得 ' + fish.value + ' 贝壳。',
                    '差一点，鱼儿逃走了', '鱼儿游走了'][game.result];
                items = ['OK 再钓一次', '↑ 返回岸边'];
                break;
            }
            case 4:
                title = '鱼竿工坊';
                hint = save.rod === 4 ? '鱼竿已满级' : game.poor ? '贝壳不足，先去钓鱼吧' :
                    '升级到 Lv.' + (save.rod + 1) + ' 需要 ' + cost(game) + ' 贝壳';
                items = save.rod === 4 ? ['↑ 返回岸边'] : ['OK 升级鱼竿', '↑ 返回岸边'];
                break;
            case 5:
                title = '鱼类图鉴';
                hint = game.group === 10 ? '返回岸边' : '第 ' + (game.group + 1) + ' 册';
                items = ['↑ / ↓ 选册', 'OK 确认'];
                break;
            case 6: {
                const id = game.group * 10 + game.book;
                const fish = species[id];
                title = seen(game, id) ? fish.name : '未知鱼种';
                hint = '编号 ' + (id + 1) + '　鱼竿 Lv.' + fish.level;
                items = ['↑ / ↓ 翻鱼', 'OK 返回目录'];
                break;
            }
        }
        $('title').textContent = title;
        $('hint').textContent = hint;
        $('menu').innerHTML = items.map((text, i) => '<div' +
            (page === 0 && i === game.selected ? ' class="sel"' : '') + '>' + text + '</div>').join('');
    }

    function act(button) {
        const now = time();
        if (press(game, button, now)) persist();
        render(now);
    }
    ['up', 'down', 'ok'].forEach((id, i) => $(id).addEventListener('click', () => act(i)));
    document.addEventListener('keydown', event => {
        const button = {ArrowUp: 0, ArrowDown: 1, Enter: 2}[event.key];
        if (button === undefined || event.repeat) return;
        event.preventDefault();
        act(button);
    });
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
        if (previous !== game.page || game.page === 2) render(now);
        requestAnimationFrame(frame);
    }
    render(time());
    requestAnimationFrame(frame);
})();
