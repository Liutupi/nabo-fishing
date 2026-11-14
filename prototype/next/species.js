const nextCatalogSpecies = [
    ...(typeof module === 'undefined' ? catalogSpecies : require('../species.js')),
    ...[{"name":"金斧鱼","value":82,"level":4},{"name":"彩尾鱼","value":84,"level":4},{"name":"黄点泥鱼","value":86,"level":4},{"name":"紫带鳗","value":88,"level":4},{"name":"红斑隆头","value":90,"level":4},{"name":"大眼晶鱼","value":92,"level":4},{"name":"银剑鲟","value":94,"level":4},{"name":"黄昏鲈","value":96,"level":4},{"name":"紫云海鱼","value":80,"level":4},{"name":"水晶鲶","value":82,"level":4},{"name":"斑纹鲈","value":84,"level":4},{"name":"金长尾","value":86,"level":4},{"name":"蓝星灯鱼","value":88,"level":4},{"name":"水草海龙","value":90,"level":4},{"name":"樱花长尾鱼","value":92,"level":4},{"name":"黑白云纹鱼","value":94,"level":4}],
];
if (typeof module !== 'undefined') module.exports = nextCatalogSpecies;
