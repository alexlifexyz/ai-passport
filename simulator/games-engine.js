// simulator/games-engine.js —— Alex Arcade 11合1 纯前端像素/伪3D游戏引擎
// 零外部依赖 · 纯数学矢量像素绘制 · Web Audio 实时物理合成音效

(function(global) {
  'use strict';

  // --- 1. 高品质 8-Bit / 赛博复古音效合成器 ---
  class SoundEngine {
    constructor() {
      this.ctx = null;
      this.muted = false;
    }
    init() {
      if (this.muted) return;
      if (!this.ctx) {
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        if (AudioContext) {
          this.ctx = new AudioContext();
        }
      }
      if (this.ctx && this.ctx.state === 'suspended') {
        this.ctx.resume();
      }
    }

    playLaser() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(880, t);
      osc.frequency.exponentialRampToValueAtTime(160, t + 0.08);
      g.gain.setValueAtTime(0.18, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.08);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.08);
    }

    playMissile() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(520, t);
      osc.frequency.exponentialRampToValueAtTime(980, t + 0.12);
      g.gain.setValueAtTime(0.2, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.12);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.12);
    }

    playJump() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'square';
      osc.frequency.setValueAtTime(180, t);
      osc.frequency.exponentialRampToValueAtTime(620, t + 0.11);
      g.gain.setValueAtTime(0.16, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.11);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.11);
    }

    playExplode(isBig = false) {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const dur = isBig ? 0.45 : 0.22;
      const bufSize = Math.floor(this.ctx.sampleRate * dur);
      const buf = this.ctx.createBuffer(1, bufSize, this.ctx.sampleRate);
      const data = buf.getChannelData(0);
      for (let i = 0; i < bufSize; i++) {
        data[i] = (Math.random() * 2 - 1) * Math.exp(-i / (this.ctx.sampleRate * (isBig ? 0.12 : 0.05)));
      }
      const noise = this.ctx.createBufferSource();
      noise.buffer = buf;
      const filter = this.ctx.createBiquadFilter();
      filter.type = 'lowpass';
      filter.frequency.setValueAtTime(isBig ? 550 : 750, t);
      filter.frequency.linearRampToValueAtTime(60, t + dur);
      const g = this.ctx.createGain();
      g.gain.setValueAtTime(isBig ? 0.45 : 0.25, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + dur);
      noise.connect(filter); filter.connect(g); g.connect(this.ctx.destination);
      noise.start(t);
    }

    playCoin() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      [987.77, 1318.51].forEach((freq, i) => {
        const osc = this.ctx.createOscillator();
        const g = this.ctx.createGain();
        osc.type = 'sine';
        osc.frequency.setValueAtTime(freq, t + i * 0.06);
        g.gain.setValueAtTime(0.18, t + i * 0.06);
        g.gain.exponentialRampToValueAtTime(0.001, t + i * 0.06 + 0.12);
        osc.connect(g); g.connect(this.ctx.destination);
        osc.start(t + i * 0.06); osc.stop(t + i * 0.06 + 0.12);
      });
    }

    playNitro() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(140, t);
      osc.frequency.exponentialRampToValueAtTime(480, t + 0.28);
      g.gain.setValueAtTime(0.24, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.28);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.28);
    }

    playSlash(isBlue = true) {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(isBlue ? 1100 : 780, t);
      osc.frequency.exponentialRampToValueAtTime(isBlue ? 450 : 280, t + 0.09);
      g.gain.setValueAtTime(0.2, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.09);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.09);
    }

    playHit() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'triangle';
      osc.frequency.setValueAtTime(520, t);
      osc.frequency.exponentialRampToValueAtTime(140, t + 0.06);
      g.gain.setValueAtTime(0.2, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.06);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.06);
    }

    playGameOver() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      [392, 349.23, 311.13, 261.63].forEach((f, idx) => {
        const osc = this.ctx.createOscillator();
        const g = this.ctx.createGain();
        osc.type = 'sawtooth';
        osc.frequency.setValueAtTime(f, t + idx * 0.12);
        g.gain.setValueAtTime(0.18, t + idx * 0.12);
        g.gain.exponentialRampToValueAtTime(0.001, t + idx * 0.12 + 0.18);
        osc.connect(g); g.connect(this.ctx.destination);
        osc.start(t + idx * 0.12); osc.stop(t + idx * 0.12 + 0.18);
      });
    }

    playClick() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(600, t);
      g.gain.setValueAtTime(0.08, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.03);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.03);
    }

    playWave() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'square';
      osc.frequency.setValueAtTime(950, t);
      osc.frequency.linearRampToValueAtTime(1350, t + 0.04);
      osc.frequency.exponentialRampToValueAtTime(450, t + 0.09);
      g.gain.setValueAtTime(0.14, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.09);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.09);
    }

    playFire() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(320, t);
      osc.frequency.exponentialRampToValueAtTime(80, t + 0.1);
      g.gain.setValueAtTime(0.24, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.1);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.1);
    }

    playShield() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      const osc = this.ctx.createOscillator();
      const g = this.ctx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(650, t);
      osc.frequency.linearRampToValueAtTime(1450, t + 0.08);
      g.gain.setValueAtTime(0.16, t);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.08);
      osc.connect(g); g.connect(this.ctx.destination);
      osc.start(t); osc.stop(t + 0.08);
    }

    playBomb() {
      if (this.muted) return; this.init();
      this.playExplode(true);
      setTimeout(() => this.playExplode(true), 120);
      setTimeout(() => this.playExplode(true), 240);
    }

    playPowerup() {
      if (this.muted) return; this.init(); if (!this.ctx) return;
      const t = this.ctx.currentTime;
      [440, 554.37, 659.25, 880].forEach((freq, idx) => {
        const osc = this.ctx.createOscillator();
        const g = this.ctx.createGain();
        osc.type = 'square';
        osc.frequency.setValueAtTime(freq, t + idx * 0.045);
        g.gain.setValueAtTime(0.16, t + idx * 0.045);
        g.gain.exponentialRampToValueAtTime(0.001, t + idx * 0.045 + 0.07);
        osc.connect(g); g.connect(this.ctx.destination);
        osc.start(t + idx * 0.045); osc.stop(t + idx * 0.045 + 0.07);
      });
    }
  }

  // --- 2. 炫彩粒子与屏幕震动系统 ---
  class FXEngine {
    constructor() {
      this.particles = [];
      this.floats = [];
      this.shake = 0;
    }
    reset() {
      this.particles = [];
      this.floats = [];
      this.shake = 0;
    }
    burst(x, y, count = 12, colors = ['#00e5ff', '#ff0055', '#ffd700']) {
      for (let i = 0; i < count; i++) {
        const angle = Math.random() * Math.PI * 2;
        const speed = 1.2 + Math.random() * 3.5;
        this.particles.push({
          x, y,
          vx: Math.cos(angle) * speed,
          vy: Math.sin(angle) * speed,
          life: 18 + Math.random() * 14,
          maxLife: 32,
          size: 2 + Math.random() * 3,
          color: colors[i % colors.length]
        });
      }
    }
    floatText(x, y, text, color = '#ffd700') {
      this.floats.push({ x, y, text, color, life: 30, maxLife: 30, vy: -0.9 });
    }
    text(x, y, text, color = '#ffd700') {
      this.floatText(x, y, text, color);
    }
    update() {
      if (this.shake > 0) this.shake *= 0.88;
      if (this.shake < 0.2) this.shake = 0;

      for (let i = this.particles.length - 1; i >= 0; i--) {
        const p = this.particles[i];
        p.x += p.vx;
        p.y += p.vy;
        p.life--;
        if (p.life <= 0) this.particles.splice(i, 1);
      }

      for (let i = this.floats.length - 1; i >= 0; i--) {
        const f = this.floats[i];
        f.y += f.vy;
        f.life--;
        if (f.life <= 0) this.floats.splice(i, 1);
      }
    }
    render(ctx) {
      for (const p of this.particles) {
        ctx.globalAlpha = Math.max(0, p.life / p.maxLife);
        ctx.fillStyle = p.color;
        ctx.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size);
      }
      ctx.globalAlpha = 1.0;

      ctx.font = 'bold 12px monospace';
      for (const f of this.floats) {
        ctx.globalAlpha = Math.max(0, f.life / f.maxLife);
        ctx.fillStyle = f.color;
        ctx.fillText(f.text, f.x, f.y);
      }
      ctx.globalAlpha = 1.0;
    }
  }

  // --- 3. 游戏注册表元数据定义 (11 大游戏档案) ---
  const GAME_REGISTRY = [
    {
      id: 'thunderracer',
      title: '雷霆飞车：极速武装',
      subtitle: 'THUNDER RACER: ARMORED SPEED',
      icon: '🏎️',
      category: 'racing',
      badge: 'HOT FLASHER',
      difficulty: 3,
      tags: ['3D透视公路', '0ms极速飞弹', '氮气超频', '擦车连击'],
      desc: '驾驶重装武装超跑在赛博高速上极速狂飙！0ms触底瞬发高爆飞弹轰碎前车，近身擦车激活倍数连击，按住OK点燃氮气暴走无敌冲撞！',
      upHint: '左变道 (0ms)',
      downHint: '右变道 (0ms)',
      okHint: '极速飞弹 / 长按氮气',
      help: '• <strong>UP / DOWN</strong>：0ms 触底切换车道躲避警车与障碍。<br>• <strong>OK 键连按</strong>：无限战术充能飞弹，按多快射多快！<br>• <strong>OK 键长按</strong>：点燃超频氮气，无敌冲撞沿途一切！<br>• <strong>擦车奖励</strong>：在极限距离超越前车可触发 Near-Miss 积分倍增与能量奖励。'
    },
    {
      id: 'thunder',
      title: '雷霆战机：大像素街机版',
      subtitle: 'THUNDER STRIKER: ARCADE HD',
      icon: '⚡',
      category: 'shooter',
      badge: 'FLAGSHIP',
      difficulty: 3,
      tags: ['三大法宝流派', 'S型蛇形波刃', '炼狱烈焰', '离子护盾力场', '全屏核弹'],
      desc: '全面升级的雷霆战机！三大神装流派：[P]突击神火、[W]幻影S型蛇形波刃、[F]炼狱烈焰。配备[S]离子护盾与双子浮游僚机，决战烈隼机群、动态展翅飞龙与星海利维坦巨龙BOSS！',
      upHint: '左平移走位',
      downHint: '右平移走位',
      okHint: '全屏毁灭核弹',
      help: '• <strong>UP / DOWN</strong>：左右机动避弹（PC键盘支持 ↑↓←→ 全方位走位）。<br>• <strong>三大法宝流派</strong>：[P] 突击神火激光 | [W] 幻影 S 型蛇形回旋波刃 | [F] 炼狱爆轰烈焰火球。<br>• <strong>力场与僚机</strong>：[S] 激活离子护盾防御罩 | 强化时激活双子浮游卫星僚机协同射击。<br>• <strong>OK 键</strong>：释放全屏毁灭核弹，清除全屏敌弹并造成毁灭打击！<br>• <strong>[B] 补充全屏核弹 | [H] 维修机身回复生命</strong>。'
    },
    {
      id: 'adventure',
      title: '像素冒险岛 HD',
      subtitle: 'PIXEL ADVENTURE ISLAND',
      icon: '🏝️',
      category: 'action',
      badge: 'CLASSIC',
      difficulty: 2,
      tags: ['经典横版跑酷', '旋转抛物飞斧', '踩踏连跳', '金蛋道具'],
      desc: '重温经典海岛冒险！角色自走狂奔，按UP减速避险，按DOWN加速冲刺，空中按OK投掷旋转战斧，沿途搜集香蕉菠萝保持饱食体力！',
      upHint: '刹车后退',
      downHint: '加速冲刺',
      okHint: '起跳 / 飞斧投掷',
      help: '• 角色<strong>自动向前跑</strong>，UP 减速刹车，DOWN 加速冲刺。<br>• <strong>OK 键</strong>：地面起跳，空中投掷旋转石斧。<br>• 路上捡取 <strong>A 石斧 / K 飞刀 / P 穿甲月刃</strong> 切换武器。<br>• 踩踏怪物可触发弹跳连击；金蛋可增加生命。'
    },
    {
      id: 'contra',
      title: '口袋魂斗罗 HD',
      subtitle: 'POCKET CONTRA: CYBER OPS',
      icon: '🔫',
      category: 'action',
      badge: 'HARDCORE',
      difficulty: 4,
      tags: ['横版跳跃射击', 'S弹/激光枪/机枪', '俯卧避弹', '多阶段机械BOSS'],
      desc: '微掌机硬核横版枪战！单兵突入敌方核心基地，跳跃翻滚与卧倒躲避子弹，击落飞行胶囊升级散弹(S)与穿透激光(L)，决战巨型机械BOSS！',
      upHint: '翻滚起跳',
      downHint: '卧倒避弹 / 瞄准',
      okHint: '开火 / 切换武器',
      help: '• <strong>UP 键</strong>：翻滚起跳越过地堡障碍。<br>• <strong>DOWN 键</strong>：匍匐卧倒避开敌军平射弹幕。<br>• <strong>OK 键</strong>：全自动机枪泼洒弹雨；击毁飞天胶囊升级 S/L/M 重型武装。<br>• 摧毁地堡炮台，攻克具有防御力场与反击激光的要塞巨兽。'
    },
    {
      id: 'flysaber',
      title: '果蝇光剑：神经脉冲',
      subtitle: 'FLYSABER: CYBER RHYTHM',
      icon: '🪰',
      category: 'rhythm',
      badge: 'BIOMIMETIC',
      difficulty: 4,
      tags: ['3D立体节拍', '红蓝双通道', '双刀十字合击', '子弹时间'],
      desc: '基于果蝇全脑连接组仿生设计的 3D 透视节奏劈砍音游！UP挥动蓝刀砍蓝块，DOWN挥动红刀砍红块，OK双刀合璧斩核心，蓄力爆发30ms子弹时间！',
      upHint: '左手蓝光剑 (R7)',
      downHint: '右手红光剑 (R8)',
      okHint: '双刀合击 / 神经超频',
      help: '• <strong>UP 键</strong>：挥动左侧蓝光剑劈斩飞来的蓝色突触块。<br>• <strong>DOWN 键</strong>：挥动右侧红光剑劈斩红色突触块。<br>• <strong>OK 键</strong>：双刀合力劈砍中央金色核心。<br>• 蓄满 100% 神经能量后按 OK，触发 30ms 减速 55% 的极速超频子弹时间！'
    },
    {
      id: 'flydriver',
      title: '果蝇超跑：极速狂飙',
      subtitle: 'FLYDRIVER: OPTIC FLOW RACER',
      icon: '⚡',
      category: 'racing',
      badge: 'OPTIC FLOW',
      difficulty: 3,
      tags: ['光流算法渲染', '极限避障擦车', '曲率隧道', '氮气撞击'],
      desc: '高帧率极速光流伪3D赛车！以果蝇复眼运动感知为算法灵感，在充满未来流光的立体隧道中闪避障碍车，极限近身超车赢取能量与积分！',
      upHint: '向左高速变道',
      downHint: '向右高速变道',
      okHint: '氮气曲率推进',
      help: '• <strong>UP / DOWN</strong>：左右切换赛道躲避前方低速车辆与障碍物。<br>• <strong>极限擦车</strong>：以高速贴身近距离超车，获取 Near-Miss 积分暴击。<br>• <strong>OK 键</strong>：激活氮气曲率推进，赛车进入无敌冲撞冲刺状态！'
    },
    {
      id: 'flappy',
      title: '像素飞鸟：重力狂飙',
      subtitle: 'FLAPPY BIRD HD: GRAVITY RUSH',
      icon: '🐦',
      category: 'casual',
      badge: 'ENDLESS',
      difficulty: 3,
      tags: ['真实重力加速度', '昼夜模式交替', '动态水管振荡', '黄金水管双倍'],
      desc: '经典重力扑翼穿梭！精确像素级碰撞体积检测，每穿过10根水管昼夜模式自动轮换，高分后迎来带有垂直呼吸振荡的黄金高分水管！',
      upHint: '微调平衡',
      downHint: '俯冲加速',
      okHint: '扑翼向上跃升',
      help: '• <strong>OK 键</strong>：轻按向上扑动翅膀克服重力下坠。<br>• 保持平稳节奏穿过上下水管狭窄空隙，避开天花板与地面。<br>• 每 10 分场景自动在白昼与静谧黑夜中平滑交替。<br>• 遇到金光闪烁的黄金水管，成功穿过可一次性奖励双倍积分！'
    },
    {
      id: 'pong',
      title: '变异乒乓：混沌球局',
      subtitle: 'CHAOS PONG: MUTATION BATTLE',
      icon: '🏓',
      category: 'party',
      badge: 'CHAOS AI',
      difficulty: 2,
      tags: ['混沌物理弹射', '巨化/极速/香蕉弧线', '切球旋球削球', '自主对战'],
      desc: '打破传统乒乓物理法则！球体在碰撞中随机触发五大混沌突变：巨型重力球、极速光子球、诡异香蕉弧线球与分身双球，击球瞬间切击可加剧旋转！',
      upHint: '挡板向左平移',
      downHint: '挡板向右平移',
      okHint: '发球 / 极限切旋球',
      help: '• <strong>UP / DOWN</strong>：控制底部防御挡板左右平移接球。<br>• <strong>OK 键</strong>：回合开始时发球；击球瞬间按 OK 可打出高旋转弧线球！<br>• 观察球体突变光芒：紫色巨化、青色极速、黄色香蕉弧线、红光分身双球！'
    },
    {
      id: 'roulette',
      title: '恶魔轮盘：生死对决',
      subtitle: 'BUCKSHOT ROULETTE: NEON DUEL',
      icon: '🎲',
      category: 'party',
      badge: 'PSYCHOLOGY',
      difficulty: 3,
      tags: ['实弹空弹博弈', '霰弹测谎心理战', '手锯双倍威力', '香烟回复战术'],
      desc: '经典的霰弹生死心理博弈！台面上摆着霰弹枪与战术道具，你与恶魔庄家轮流开火。你可以朝对方射击，也可以冒着虚弹风险朝自己射击赢取额外回合！',
      upHint: '选择战术道具',
      downHint: '朝对方恶魔射击',
      okHint: '朝自己射击 / 确认',
      help: '• <strong>DOWN 键</strong>：举枪对准恶魔开火！实弹造成重创，空弹交换回合。<br>• <strong>OK 键</strong>：对准自己开火！若是空弹，本回合不切换，继续由你行动！<br>• <strong>UP 键</strong>：使用放大镜测探当前弹药、手锯使伤害翻倍、香烟回复生命。'
    },
    {
      id: 'sparkler',
      title: '星火仙女棒与赛博烛火',
      subtitle: 'SPARKLER & CYBER CANDLE',
      icon: '🎆',
      category: 'creative',
      badge: 'PHYSICAL MIC',
      difficulty: 1,
      tags: ['麦克风真实吹气', '四色烟火爆点', '烛光摇曳物理', '长按重燃'],
      desc: '温暖浪漫的物理粒子互动小品！包含“绚烂仙女棒”与“赛博微光烛火”双模式，对着麦克风吹气（或按B键吹气）火花四溅激荡，猛吹可将蜡烛熄灭并升起缕缕青烟！',
      upHint: '火花燃烧加速',
      downHint: '火花燃烧减速',
      okHint: '切换仙女棒/蜡烛 (按B吹气)',
      help: '• <strong>OK 键</strong>：在【绚烂仙女棒】与【赛博蜡烛】之间双模切换。<br>• <strong>吹气互动</strong>：对着麦克风吹气（或按键盘 <kbd>B</kbd> 键模拟吹气），仙女棒火花狂乱飞舞！<br>• 蜡烛模式下持续猛吹 200ms 可将烛火吹灭，飘出逼真微粒青烟！'
    },
    {
      id: 'worldtime',
      title: '世界时钟：跨时区罗盘',
      subtitle: 'WORLD TIMEZONE COMPANION',
      icon: '⏰',
      category: 'creative',
      badge: 'UTILITY',
      difficulty: 1,
      tags: ['全球城市时区', '昼夜太阳光照', '跨国会议黄金窗口', '指针模拟时钟'],
      desc: '专为全球化极客与出海团队打造的时区对齐罗盘！快速在伦敦、纽约、东京、北京等主流国际都市间轮转，直观指示日出日落昼夜光线，自动推算跨时区会议最佳交集时段！',
      upHint: '上一个国际城市',
      downHint: '下一个国际城市',
      okHint: '推算会议黄金重叠时段',
      help: '• <strong>UP / DOWN</strong>：在全球 12 大国际核心时区都市间自由切换。<br>• 顶部直观展示当前城市的太阳晨昏昼夜轨迹与当地标准时间。<br>• <strong>OK 键</strong>：一键调出跨时区全天候会议对齐矩阵，寻找商务交集重叠黄金时间。'
    }
  ];

  // 挂载全局对象
  global.SoundEngine = SoundEngine;
  global.FXEngine = FXEngine;
  global.GAME_REGISTRY = GAME_REGISTRY;

})(typeof window !== 'undefined' ? window : this);
