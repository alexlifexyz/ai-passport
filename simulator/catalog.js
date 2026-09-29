// simulator/catalog.js — Alex Arcade 唯一游戏目录
// 大厅、游戏页、封面都只读这里。任务 test(stats) 的字段由各游戏在 api.stats 上实时写入。
(function (G) {
  'use strict';
  const s = (n) => (st) => (st.score || 0) >= n;

  G.CATEGORIES = [
    { id: 'all', label: '全部' },
    { id: 'shoot', label: '射击' },
    { id: 'action', label: '动作' },
    { id: 'race', label: '竞速' },
    { id: 'puzzle', label: '益智' },
    { id: 'casual', label: '休闲' },
    { id: 'toy', label: '小品' }
  ];

  G.CATALOG = [
    // ---------------- 主打（重制） ----------------
    {
      id: 'thunder', title: '雷霆战机', en: 'Thunder Striker', icon: '✈️', cat: 'shoot', color: '#38e1ff', featured: true, remade: true,
      pitch: '自动开火，擦弹攒炸弹，金币连锁越吃越值钱',
      desc: '竖版弹幕射击。战机自动开火，你只管左右走位。子弹贴身擦过会充能炸弹，击落敌机掉落的金币不断链就越来越值钱，每一关尽头都有会变招的 BOSS。',
      controls: { up: '向左移动', down: '向右移动', ok: '投放炸弹（清屏）' },
      tips: ['只有机身中央的小亮点才是判定点，大胆贴着子弹走。', '擦弹条满了自动送一颗炸弹。', '金币落地前接住才不断链，链越长单价越高。'],
      missions: [
        { text: '击败第一个 BOSS', test: (st) => st.bosses >= 1 },
        { text: '单局擦弹 40 次', test: (st) => st.graze >= 40 },
        { text: '金币单价涨到 1000', test: (st) => st.medal >= 1000 }
      ],
      ranks: [80000, 35000, 12000]
    },
    {
      id: 'thunderracer', title: '雷霆飞车', en: 'Thunder Racer', icon: '🏎️', cat: 'race', color: '#ff5a5a', featured: true, remade: true,
      pitch: '限时冲关，擦车攒氮气，弯道别被甩出去',
      desc: '伪 3D 公路狂飙。倒计时归零就结束，冲过检查点续时间。贴着车流擦过去会攒氮气，弯道离心力会把车往外甩，冲出路面会减速。',
      controls: { up: '向左转向', down: '向右转向', ok: '按住开氮气' },
      tips: ['入弯前提前往内侧打方向。', '擦车越近奖励越多，还会回氮气。', '开氮气时撞车不会减速太多，但会扣时间。'],
      missions: [
        { text: '通过 3 个检查点', test: (st) => st.checkpoints >= 3 },
        { text: '单局擦车 15 次', test: (st) => st.nearMiss >= 15 },
        { text: '时速突破 320 km/h', test: (st) => st.topSpeed >= 320 }
      ],
      ranks: [60000, 30000, 12000]
    },
    {
      id: 'battlecity', title: '坦克大战', en: 'Tank 1990', icon: '🪖', cat: 'shoot', color: '#ffd23f', featured: true, remade: true,
      pitch: '三键开坦克：自动前进，左右拐弯，按住原地连射',
      desc: '保卫基地老鹰。坦克自己往前开，UP / DOWN 在下一个路口左拐或右拐，OK 开炮，按住 OK 原地停车连射。打掉闪光坦克会掉道具，星星能把炮升到打穿钢板。',
      controls: { up: '左转', down: '右转', ok: '开炮 · 按住停车连射' },
      tips: ['转向会自动对齐到格子，提前按就行。', '老鹰被打掉直接结束，别让敌人绕到下面。', '铲子道具会把基地围墙换成钢板。'],
      missions: [
        { text: '守住第 1 关', test: (st) => st.stage >= 2 },
        { text: '单局击毁 30 辆坦克', test: (st) => st.kills >= 30 },
        { text: '把主炮升到满级', test: (st) => st.tier >= 3 }
      ],
      ranks: [30000, 15000, 6000]
    },
    {
      id: 'adventure', title: '冒险岛', en: 'Adventure Island', icon: '🏝️', cat: 'action', color: '#6ee06e', featured: true, remade: true,
      pitch: '一路狂奔，体力一直掉，吃水果续命',
      desc: '自动奔跑的横版冒险。体力条会一直往下掉，吃水果才能续上。蛋里有石斧和滑板，按住 OK 跳得更高，DOWN 冲刺更快但更耗体力。每跑完一段就有旗子存档。',
      controls: { up: '扔石斧', down: '按住冲刺', ok: '跳跃（按住跳更高）' },
      tips: ['蛋要踢两下：先碰开，再拿道具。', '滑板能多挡一次伤害。', '火堆打不掉，只能跳过。'],
      missions: [
        { text: '到达第 3 区', test: (st) => st.area >= 3 },
        { text: '单局吃 40 个水果', test: (st) => st.fruit >= 40 },
        { text: '用石斧打倒 20 个敌人', test: (st) => st.axeKills >= 20 }
      ],
      ranks: [40000, 20000, 8000]
    },
    {
      id: 'contra', title: '口袋魂斗罗', en: 'Pocket Contra', icon: '🔫', cat: 'shoot', color: '#7cc7ff', featured: true, remade: true,
      pitch: '一边跑一边自动开火，抬枪、趴下、跳跃',
      desc: '横版跑射。枪一直在打，你决定往哪打：按住 UP 斜向上射，按住 DOWN 趴下躲子弹，OK 空翻跳。打下飞行胶囊换 S 散弹、L 激光、M 机枪，关底要拆掉整面炮台墙。',
      controls: { up: '按住抬枪（斜上）', down: '按住卧倒', ok: '空翻跳' },
      tips: ['趴下能躲开大部分水平子弹。', 'BOSS 的炮口在上方，一定要抬枪。', '死亡会丢掉当前武器。'],
      missions: [
        { text: '击败第 1 关 BOSS', test: (st) => st.bosses >= 1 },
        { text: '拿到 S 散弹枪', test: (st) => st.gotS },
        { text: '单局消灭 60 个敌人', test: (st) => st.kills >= 60 }
      ],
      ranks: [50000, 25000, 10000]
    },
    {
      id: 'flappy', title: '像素飞鸟', en: 'Pixel Flappy', icon: '🐦', cat: 'casual', color: '#ffd23f', featured: true, remade: true,
      pitch: '一键扑翼，吃金币、擦管子、拿奖牌',
      desc: '经典一键扑翼，但每十根管子换一段风景：黄昏、夜晚、会上下呼吸的管子。管缝里有金币，贴着管子飞过会有擦边加分，结束时按成绩发铜银金白金奖牌。',
      controls: { up: '扑翼', down: '扑翼', ok: '扑翼' },
      tips: ['三个键都能扑翼，用最顺手的那个。', '擦边加分只看你和管子的距离。'],
      missions: [
        { text: '穿过 10 根管子', test: (st) => st.pipes >= 10 },
        { text: '穿过 40 根管子', test: (st) => st.pipes >= 40 },
        { text: '单局吃 20 枚金币', test: (st) => st.coins >= 20 }
      ],
      ranks: [6000, 3000, 1000]
    },
    {
      id: 'pawssprint', title: '短腿爪爪运动会', en: 'Paws Sprint', icon: '🐶', cat: 'race', color: '#ff8fc7', featured: true, remade: true,
      pitch: '柯基柴犬小海豹，三场杯赛争第一',
      desc: '四只小短腿同场赛跑。三条跑道换道、跳栏、躲泥坑和香蕉皮，吃骨头攒涡轮。跳起来再按一次 OK 就是空中涡轮冲刺。三场杯赛都要进前二才能捧杯。',
      controls: { up: '换到左道 · 选人', down: '换到右道 · 选人', ok: '跳 · 空中再按涡轮冲刺' },
      tips: ['踩泥坑减速，踩香蕉皮原地转圈。', '三块骨头换一次涡轮。', '涡轮冲刺可以直接撞开栏杆。'],
      missions: [
        { text: '拿到一次第一名', test: (st) => st.wins >= 1 },
        { text: '捧起奖杯（三场都进前二）', test: (st) => st.cup },
        { text: '单局吃 30 块骨头', test: (st) => st.bones >= 30 }
      ],
      ranks: [15000, 9000, 4000]
    },
    {
      id: 'match3', title: '霓虹宝石', en: 'Neon Columns', icon: '💎', cat: 'puzzle', color: '#ff5ad2', featured: true, remade: true,
      pitch: '三颗宝石往下落，横竖斜连三就消',
      desc: '三键友好的掉落式消除。三颗一组的宝石往下掉，左右移动、OK 轮换顺序，横、竖、斜连成三颗即消，掉下来再连就是连锁。每消 30 颗升一级，速度更快。',
      controls: { up: '左移', down: '右移', ok: '轮换顺序 · 按住加速下落' },
      tips: ['斜线也算！', '连锁倍率会一路叠上去。', '偶尔出现的闪光宝石会清掉它落下位置那种颜色。'],
      missions: [
        { text: '打出一次 4 连锁', test: (st) => st.maxChain >= 4 },
        { text: '单局消除 150 颗', test: (st) => st.cleared >= 150 },
        { text: '升到第 6 级', test: (st) => st.level >= 6 }
      ],
      ranks: [40000, 18000, 7000]
    },
    {
      id: 'fishhunter', title: '大鱼吃小鱼', en: 'Fish Hunter', icon: '🐟', cat: 'casual', color: '#3ee0d0', featured: true, remade: true,
      pitch: '吃比你小的，躲比你大的，长成鱼王',
      desc: '海底生存成长。小鱼一直在游，UP / DOWN 上下，OK 掉头冲刺。只能吃比自己小的鱼，连续吞食有连击，吃够就长一级。水母会让你缩小，大鲨鱼要绕着走。',
      controls: { up: '向上游', down: '向下游', ok: '掉头冲刺' },
      tips: ['鱼身边的颜色圈：绿色能吃，红色会吃你。', '冲刺期间速度翻倍，用来追猎物或逃命。', '长到 6 级就是鱼王，可以反吃鲨鱼。'],
      missions: [
        { text: '长到 4 级', test: (st) => st.level >= 4 },
        { text: '连续吞食 12 条', test: (st) => st.maxCombo >= 12 },
        { text: '成为鱼王', test: (st) => st.level >= 6 }
      ],
      ranks: [20000, 9000, 3000]
    },
    {
      id: 'cyberrunner', title: '赛博信使', en: 'Cyber Runner', icon: '⚡', cat: 'action', color: '#b58cff', featured: true, remade: true,
      pitch: '屋顶跑酷，影刃冲刺斩无人机',
      desc: '霓虹城市屋顶上的跑酷。OK 跳（可二段跳），DOWN 滑铲钻栏杆、空中按是急坠，UP 影刃冲刺能斩开无人机，斩中立刻回满冲刺。越跑越快。',
      controls: { up: '影刃冲刺', down: '滑铲 · 空中急坠', ok: '跳跃 · 二段跳' },
      tips: ['冲刺斩中无人机会刷新冲刺和二段跳。', '红色激光栏只能滑过去。', '楼缝前先跳，别等到边缘。'],
      missions: [
        { text: '单局跑过 1500m', test: (st) => st.dist >= 1500 },
        { text: '单局斩杀 20 架无人机', test: (st) => st.kills >= 20 },
        { text: '连斩 5 架不落地', test: (st) => st.maxChain >= 5 }
      ],
      ranks: [30000, 15000, 6000]
    },
    {
      id: 'pong', title: '变异乒乓', en: 'Mutant Pong', icon: '🏓', cat: 'casual', color: '#5eead4', remade: true,
      pitch: '三个对手，七分一局，球会变异',
      desc: '竖版乒乓。球靠近球拍时按 OK 是扣杀，时机越准球越快。球撞到中场的变异方块会变大、分裂、拐弯或加速。连续赢下三个越来越强的对手就通关。',
      controls: { up: '球拍左移', down: '球拍右移', ok: '扣杀（球到拍前时按）' },
      tips: ['用球拍边缘接球会打出角度。', '扣杀窗口在球拍上方一小段，出现白圈就是时机。'],
      missions: [
        { text: '赢下一局', test: (st) => st.wins >= 1 },
        { text: '打出 10 次完美扣杀', test: (st) => st.perfect >= 10 },
        { text: '击败全部 3 个对手', test: (st) => st.wins >= 3 }
      ],
      ranks: [12000, 7000, 3000]
    },
    {
      id: 'bouncy', title: '几何弹射', en: 'Bouncy Blaster', icon: '🎯', cat: 'puzzle', color: '#8b8bff', remade: true,
      pitch: '十二关弹球谜题，算好反弹一发入魂',
      desc: '台球加神枪手。弹球会在墙和钢板上反弹，瞄准线会预告前两次反弹。子弹有限，打掉所有红色目标过关，炸药桶会连带炸掉周围的东西。',
      controls: { up: '逆时针瞄准', down: '顺时针瞄准', ok: '发射' },
      tips: ['按住方向键会越转越快，点按是精调。', '剩余子弹越多，过关分越高。', '蓝色镜面会把球折成直角。'],
      missions: [
        { text: '通过第 4 关', test: (st) => st.level >= 5 },
        { text: '一发击倒 3 个目标', test: (st) => st.multi >= 3 },
        { text: '全部 12 关通关', test: (st) => st.clear }
      ],
      ranks: [30000, 20000, 10000]
    },
    {
      id: 'splasher', title: '水枪狂欢节', en: 'Hydro Splash', icon: '💦', cat: 'shoot', color: '#22d3ee', remade: true,
      pitch: '给灰扑扑的打工人一身彩色',
      desc: '六十秒水枪射击馆。窗口和街上会冒出灰扑扑的打工人，抛物线水弹命中就变成夏威夷衬衫跳舞。连续命中有连击，别误伤猫猫。',
      controls: { up: '抬高枪口', down: '压低枪口', ok: '发射水弹 · 按住连射' },
      tips: ['水弹是抛物线，远处要抬高。', '金色的老板值 5 倍分。', '打到猫会断连击。'],
      missions: [
        { text: '单局染色 25 人', test: (st) => st.hits >= 25 },
        { text: '连击 10 次', test: (st) => st.maxCombo >= 10 },
        { text: '单局得分 8000', test: s(8000) }
      ],
      ranks: [12000, 7000, 3500]
    },
    {
      id: 'lasercat', title: '猫猫激光笔', en: 'Laser Cat', icon: '🐱', cat: 'puzzle', color: '#ff6b6b', remade: true,
      pitch: '用红点指挥猫，把电池箱推进插槽',
      desc: '猫会追着红点飞扑。转动激光笔，按住 OK 打出红点，猫扑过去会把箱子撞开。把电池箱推进插槽就过关，一共八个房间。',
      controls: { up: '激光左转', down: '激光右转', ok: '按住打出红点' },
      tips: ['猫扑的方向就是箱子被推的方向。', '松开 OK 猫会停下来，用来微调。', '步数越少分越高。'],
      missions: [
        { text: '通过第 3 个房间', test: (st) => st.level >= 4 },
        { text: '20 秒内过一关', test: (st) => st.fast },
        { text: '八个房间全通', test: (st) => st.clear }
      ],
      ranks: [20000, 12000, 6000]
    },

    // ---------------- 原作接入（统一壳层） ----------------
    {
      id: 'gearcavalry', title: '齿轮骑兵', en: 'Gear Cavalry', icon: '⚙️', cat: 'action', color: '#fbbf24',
      pitch: '骑着发条战马冲锋，攒满蒸汽开过载',
      desc: '蒸汽朋克横版冲锋。跃马、下刺、滑铲、骑枪突刺，攒满 100 PSI 蒸汽开启过载。',
      controls: { up: '跃马 · 空中二段', down: '下刺 · 滑铲', ok: '骑枪突刺 · 满气过载' },
      missions: [
        { text: '单局得分 5000', test: s(5000) },
        { text: '单局得分 15000', test: s(15000) },
        { text: '坚持 90 秒', test: (st) => st.time >= 90 }
      ],
      ranks: [30000, 15000, 5000]
    },
    {
      id: 'flydriver', title: '果蝇超跑', en: 'Fly Driver', icon: '🚀', cat: 'race', color: '#a5b4fc',
      pitch: '隧道光流里左右闪避',
      desc: '伪 3D 隧道赛车，左右变道闪开障碍车，贴身超车攒氮气。',
      controls: { up: '左变道', down: '右变道', ok: '氮气' },
      missions: [
        { text: '单局得分 1000', test: s(1000) },
        { text: '单局得分 3000', test: s(3000) },
        { text: '坚持 60 秒', test: (st) => st.time >= 60 }
      ],
      ranks: [5000, 2500, 1000]
    },
    {
      id: 'flysaber', title: '果蝇光剑', en: 'Fly Saber', icon: '⚔️', cat: 'casual', color: '#fb7185',
      pitch: '蓝刀砍蓝块，红刀砍红块',
      desc: '三键节奏劈砍。UP 蓝刀、DOWN 红刀、OK 双刀合击核心块。',
      controls: { up: '蓝刀', down: '红刀', ok: '双刀合击' },
      missions: [
        { text: '单局得分 1500', test: s(1500) },
        { text: '单局得分 5000', test: s(5000) },
        { text: '坚持 60 秒', test: (st) => st.time >= 60 }
      ],
      ranks: [8000, 4000, 1500]
    },
    {
      id: 'wave', title: '浪涌漫游者', en: 'Wave Walker', icon: '🦦', cat: 'race', color: '#38bdf8', time: 90,
      pitch: '海獭冲浪，九十秒能滑多远',
      desc: '顺坡压板加速，浪尖起跳，空中翻滚做特技。九十秒一轮。',
      controls: { up: '空中翻滚', down: '压板加速', ok: '起跳 · 校正' },
      missions: [
        { text: '单轮得分 3000', test: s(3000) },
        { text: '单轮得分 8000', test: s(8000) },
        { text: '单轮得分 15000', test: s(15000) }
      ],
      ranks: [15000, 8000, 3000]
    },
    {
      id: 'wind', title: '风与纸翼', en: 'Wind Rider', icon: '🪁', cat: 'casual', color: '#2dd4bf', time: 90,
      pitch: '按住俯冲，松开冲天，永不坠毁',
      desc: '折纸飞机的心流滑翔。按住 OK 俯冲，松开迎风拉起，九十秒一轮。',
      controls: { up: '抬头', down: '压低', ok: '按住俯冲 · 松开上冲' },
      missions: [
        { text: '单轮得分 1000', test: s(1000) },
        { text: '单轮得分 3000', test: s(3000) },
        { text: '单轮得分 6000', test: s(6000) }
      ],
      ranks: [6000, 3000, 1000]
    },
    {
      id: 'bubble', title: '飞针破泡', en: 'Bubble Needle', icon: '🪡', cat: 'puzzle', color: '#38bdf8', time: 90,
      pitch: '飞针会撞墙反弹，雷云泡连锁引爆',
      desc: '调角度发射飞针，靠墙反弹扎破气泡。雷云泡范围连锁，冰冻泡定格全场。九十秒一轮。',
      controls: { up: '向左调角', down: '向右调角', ok: '发射 · 长按贯穿针' },
      missions: [
        { text: '单轮得分 2000', test: s(2000) },
        { text: '单轮得分 6000', test: s(6000) },
        { text: '单轮得分 12000', test: s(12000) }
      ],
      ranks: [12000, 6000, 2000]
    },
    {
      id: 'huddle', title: '午后温泉', en: 'Fluffy Huddle', icon: '♨️', cat: 'puzzle', color: '#f472b6', time: 120,
      pitch: '把萌物滑进温泉，同类抱团进阶',
      desc: '合成类小品。把圆滚滚的小动物投进温泉，相同的会抱团合成更大的一只。两分钟一轮。',
      controls: { up: '左移', down: '右移', ok: '投放 · 长按抚摸' },
      missions: [
        { text: '单轮得分 3000', test: s(3000) },
        { text: '单轮得分 8000', test: s(8000) },
        { text: '单轮得分 15000', test: s(15000) }
      ],
      ranks: [15000, 8000, 3000]
    },
    {
      id: 'smash', title: '万物皆可敲', en: 'Smash Frenzy', icon: '🔨', cat: 'casual', color: '#fb923c', time: 45,
      pitch: '四十五秒，能敲碎多少东西',
      desc: '解压敲击。切换木槌、充气锤、雷神锤和人字拖，短按连敲，长按蓄力暴击。',
      controls: { up: '换工具', down: '换工具', ok: '敲 · 长按蓄力' },
      missions: [
        { text: '单轮得分 3000', test: s(3000) },
        { text: '单轮得分 8000', test: s(8000) },
        { text: '单轮得分 15000', test: s(15000) }
      ],
      ranks: [15000, 8000, 3000]
    },
    {
      id: 'roulette', title: '恶魔轮盘', en: 'Devil Roulette', icon: '🎰', cat: 'casual', color: '#fca5a5',
      pitch: '一把霰弹枪，你和恶魔轮流开火',
      desc: '霰弹心理博弈。朝恶魔开枪，或冒险朝自己开枪赢得额外回合。',
      controls: { up: '选道具', down: '朝恶魔开枪', ok: '朝自己开枪 · 确认' },
      missions: [
        { text: '打赢恶魔一次', test: (st) => st.clear },
        { text: '单局得分 1000', test: s(1000) },
        { text: '单局得分 3000', test: s(3000) }
      ],
      ranks: [3000, 1500, 500]
    },
    {
      id: 'fish', title: '会躲起来的鱼', en: 'Shy Fish', icon: '🐠', cat: 'toy', color: '#67e8f9', noScore: true,
      pitch: '鱼缸里只有一条会害羞的像素鱼',
      desc: '一个安静的小品。敲敲缸壁它会探出头，太久不理它就钻进水草。没有分数，也不会死。',
      controls: { up: '轻敲缸壁', down: '白天 / 夜里', ok: '叫它一声' },
      missions: []
    },
    {
      id: 'sparkler', title: '仙女棒', en: 'Sparkler', icon: '🎇', cat: 'toy', color: '#fdba74', noScore: true,
      pitch: '仙女棒和蜡烛，按 B 吹一口气',
      desc: '粒子小品。调节燃烧速度，切换仙女棒和蜡烛。',
      controls: { up: '烧快一点', down: '烧慢一点', ok: '仙女棒 / 蜡烛' },
      missions: []
    },
    {
      id: 'worldtime', title: '世界时钟', en: 'World Clock', icon: '🕰️', cat: 'toy', color: '#fde047', noScore: true,
      pitch: '北京、东京、伦敦、纽约，找会议重叠时段',
      desc: '三键时区罗盘。切换城市，OK 打开会议重叠矩阵。',
      controls: { up: '上一个城市', down: '下一个城市', ok: '会议矩阵' },
      missions: []
    }
  ];
})(window);
