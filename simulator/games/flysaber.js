// simulator/games/flysaber.js — 由 flysaber.html 移植到统一引擎（原版 30FPS 逻辑，60Hz 下隔帧推进）
(function () {
  'use strict';
  Arcade.define({
    id: 'flysaber',
    create(api) {
      let ctx = null;
      const snd = { playSlashBlue() { api.sfx.tone(1200, 0.08, { type: 'sawtooth', vol: 0.05, to: 600 }); }, playSlashRed() { api.sfx.tone(800, 0.08, { type: 'sawtooth', vol: 0.05, to: 350 }); }, playHitPerfect() { api.sfx.combo(Math.min(combo, 18)); }, playHitGood() { api.sfx.pickup(); }, playMiss() { api.sfx.hurt(); }, playOverdrive() { api.sfx.power(); api.fx.flash('#d000ff', 0.3); }, playGameOver() { api.sfx.boom(true); } };
    let syncHp = 100;
    let lives = 3;
    const maxLives = 3;
    let score = 0;
    let combo = 0;
    let maxCombo = 0;
    let overdriveGauge = 0;
    let isOverdrive = false;
    let overdriveTimer = 0;
    let gameOver = false;

    function applySyncDamage(dmg) {
      syncHp -= dmg;
      if (syncHp <= 0) {
        if (lives > 1) {
          lives--;
          syncHp = 75;
          isOverdrive = true;
          overdriveTimer = 60;
          hitMsg = ">> EMERGENCY SYNAPSE REBOOT! <<";
          hitMsgColor = "#ffd700";
          hitMsgTimer = 25;
          snd.playOverdrive();
        } else {
          lives = 0;
          syncHp = 0;
          gameOver = true;
          snd.playGameOver();
        }
      }
    }

    let slashLeftTimer = 0;
    let slashRightTimer = 0;
    let slashDualTimer = 0;
    let hitMsg = "";
    let hitMsgTimer = 0;
    let hitMsgColor = "#ffd700";

    let tick = 0;
    let spawnTimer = 0;
    let blocks = [];
    let particles = [];

    const VP_X = 120;
    const VP_Y = 20;
    const DEST_LEFT = 55;
    const DEST_RIGHT = 185;
    const DEST_CTR = 120;
    const DEST_Y = 225;
    const STRIKE_T = 0.85;

    function calcPos(p, lane) {
      const destX = (lane === 0) ? DEST_LEFT : ((lane === 1) ? DEST_RIGHT : DEST_CTR);
      const curvedP = p * (0.4 + 0.6 * p);
      const cx = VP_X + curvedP * (destX - VP_X);
      const cy = VP_Y + curvedP * (DEST_Y - VP_Y);
      const w = Math.floor(6 + curvedP * 26);
      const h = Math.floor(4 + curvedP * 18);
      return { x: cx - w / 2, y: cy - h / 2, w, h };
    }

    function spawnParticles(x, y, color, count = 8) {
      for (let i = 0; i < count; i++) {
        const angle = Math.random() * Math.PI * 2;
        const speed = 2 + Math.random() * 4;
        particles.push({
          x, y,
          vx: Math.cos(angle) * speed,
          vy: Math.sin(angle) * speed,
          life: 12 + Math.floor(Math.random() * 6),
          color
        });
      }
    }

    const PATTERNS = [
      { lane: 0, type: 'BLUE' },
      { lane: 1, type: 'RED' },
      { lane: 0, type: 'BLUE' },
      { lane: 0, type: 'BLUE' },
      { lane: 1, type: 'RED' },
      { lane: 2, type: 'DUAL' },
      { lane: 0, type: 'SPIKE' },
      { lane: 1, type: 'RED' },
      { lane: 1, type: 'SPIKE' },
      { lane: 0, type: 'BLUE' },
      { lane: 2, type: 'DUAL' },
    ];
    let patternStep = 0;

    function spawnBlock() {
      const item = PATTERNS[patternStep];
      patternStep = (patternStep + 1) % PATTERNS.length;
      const baseSpeed = 0.022 + (score / 4000) * 0.003;
      blocks.push({
        lane: item.lane,
        type: item.type,
        progress: 0.0,
        speed: Math.min(baseSpeed, 0.045),
        sliced: false
      });
    }

    function slashLeft() {
      if (gameOver) return;
      slashLeftTimer = 5;
      const targets = blocks.filter(b => b.lane === 0 && !b.sliced && Math.abs(b.progress - STRIKE_T) <= 0.18);
      if (targets.length > 0) {
        targets.sort((a, b) => Math.abs(a.progress - STRIKE_T) - Math.abs(b.progress - STRIKE_T));
        const target = targets[0];
        target.sliced = true;
        const pos = calcPos(target.progress, target.lane);

        if (target.type === 'SPIKE') {
          combo = 0;
          hitMsg = "HAZARD SPIKE HIT! -8%";
          hitMsgColor = "#ff9900";
          hitMsgTimer = 16;
          snd.playMiss();
          applySyncDamage(8);
        } else if (target.type === 'BLUE') {
          const diff = Math.abs(target.progress - STRIKE_T);
          spawnParticles(pos.x + pos.w / 2, pos.y + pos.h / 2, '#00e5ff', 10);
          if (diff <= 0.08) {
            score += isOverdrive ? 300 : 100;
            combo++;
            if (combo > 0 && combo % 15 === 0 && lives < maxLives) lives++;
            if (!isOverdrive) overdriveGauge = Math.min(100, overdriveGauge + 10);
            syncHp = Math.min(100, syncHp + 8);
            hitMsg = `PERFECT REFLEX! x${combo}`;
            hitMsgColor = "#ffd700";
            snd.playHitPerfect();
          } else {
            score += isOverdrive ? 150 : 50;
            combo++;
            if (!isOverdrive) overdriveGauge = Math.min(100, overdriveGauge + 5);
            syncHp = Math.min(100, syncHp + 4);
            hitMsg = `GOOD HIT! x${combo}`;
            hitMsgColor = "#33ff66";
            snd.playHitGood();
          }
          hitMsgTimer = 14;
        } else {
          combo = 0;
          hitMsg = "WRONG COLOR! R7 MISMATCH -4%";
          hitMsgColor = "#ff2255";
          hitMsgTimer = 14;
          snd.playMiss();
          applySyncDamage(4);
        }
      } else {
        snd.playSlashBlue();
      }
    }

    function slashRight() {
      if (gameOver) return;
      slashRightTimer = 5;
      const targets = blocks.filter(b => b.lane === 1 && !b.sliced && Math.abs(b.progress - STRIKE_T) <= 0.18);
      if (targets.length > 0) {
        targets.sort((a, b) => Math.abs(a.progress - STRIKE_T) - Math.abs(b.progress - STRIKE_T));
        const target = targets[0];
        target.sliced = true;
        const pos = calcPos(target.progress, target.lane);

        if (target.type === 'SPIKE') {
          combo = 0;
          hitMsg = "HAZARD SPIKE HIT! -8%";
          hitMsgColor = "#ff9900";
          hitMsgTimer = 16;
          snd.playMiss();
          applySyncDamage(8);
        } else if (target.type === 'RED') {
          const diff = Math.abs(target.progress - STRIKE_T);
          spawnParticles(pos.x + pos.w / 2, pos.y + pos.h / 2, '#ff3366', 10);
          if (diff <= 0.08) {
            score += isOverdrive ? 300 : 100;
            combo++;
            if (combo > 0 && combo % 15 === 0 && lives < maxLives) lives++;
            if (!isOverdrive) overdriveGauge = Math.min(100, overdriveGauge + 10);
            syncHp = Math.min(100, syncHp + 8);
            hitMsg = `PERFECT REFLEX! x${combo}`;
            hitMsgColor = "#ffd700";
            snd.playHitPerfect();
          } else {
            score += isOverdrive ? 150 : 50;
            combo++;
            if (!isOverdrive) overdriveGauge = Math.min(100, overdriveGauge + 5);
            syncHp = Math.min(100, syncHp + 4);
            hitMsg = `GOOD HIT! x${combo}`;
            hitMsgColor = "#33ff66";
            snd.playHitGood();
          }
          hitMsgTimer = 14;
        } else {
          combo = 0;
          hitMsg = "WRONG COLOR! R8 MISMATCH -4%";
          hitMsgColor = "#ff2255";
          hitMsgTimer = 14;
          snd.playMiss();
          applySyncDamage(4);
        }
      } else {
        snd.playSlashRed();
      }
    }

    function slashDual() {
      if (gameOver) {
        return;
        return;
      }
      slashDualTimer = 6;
      slashLeftTimer = 6;
      slashRightTimer = 6;

      if (overdriveGauge >= 100) {
        isOverdrive = true;
        overdriveTimer = 120;
        overdriveGauge = 0;
        hitMsg = ">> 30ms NEURO OVERDRIVE! <<";
        hitMsgColor = "#ffd700";
        hitMsgTimer = 22;
        snd.playOverdrive();

        blocks.forEach(b => {
          if (!b.sliced && b.type !== 'SPIKE') {
            b.sliced = true;
            score += 300;
            combo++;
            const pos = calcPos(b.progress, b.lane);
            spawnParticles(pos.x + pos.w / 2, pos.y + pos.h / 2, '#ffd700', 8);
          }
        });
        return;
      }

      // 中央双色核心判定
      let hit = false;
      const ctrTargets = blocks.filter(b => b.lane === 2 && !b.sliced && Math.abs(b.progress - STRIKE_T) <= 0.18);
      if (ctrTargets.length > 0) {
        const t = ctrTargets[0];
        t.sliced = true;
        const pos = calcPos(t.progress, t.lane);
        spawnParticles(pos.x + pos.w / 2, pos.y + pos.h / 2, '#d000ff', 12);
        score += isOverdrive ? 350 : 150;
        combo++;
        if (combo > 0 && combo % 15 === 0 && lives < maxLives) lives++;
        overdriveGauge = Math.min(100, overdriveGauge + 15);
        hitMsg = `DUAL CORE SPLIT! x${combo}`;
        hitMsgColor = "#d000ff";
        hitMsgTimer = 14;
        snd.playHitPerfect();
        hit = true;
      }

      // 左右同时切
      const leftT = blocks.find(b => b.lane === 0 && !b.sliced && b.type === 'BLUE' && Math.abs(b.progress - STRIKE_T) <= 0.18);
      const rightT = blocks.find(b => b.lane === 1 && !b.sliced && b.type === 'RED' && Math.abs(b.progress - STRIKE_T) <= 0.18);
      if (leftT) {
        leftT.sliced = true;
        score += 100;
        combo++;
        if (combo > 0 && combo % 15 === 0 && lives < maxLives) lives++;
        hit = true;
      }
      if (rightT) {
        rightT.sliced = true;
        score += 100;
        combo++;
        if (combo > 0 && combo % 15 === 0 && lives < maxLives) lives++;
        hit = true;
      }

      if (!hit) {
        snd.playSlashBlue();
      }
    }

    // 主渲染与游戏循环 (30 FPS)
    function update() {
      if (!gameOver) {
        tick++;
        if (slashLeftTimer > 0) slashLeftTimer--;
        if (slashRightTimer > 0) slashRightTimer--;
        if (slashDualTimer > 0) slashDualTimer--;
        if (hitMsgTimer > 0) hitMsgTimer--;

        if (isOverdrive) {
          overdriveTimer--;
          if (overdriveTimer <= 0) isOverdrive = false;
        }

        spawnTimer++;
        let threshold = (score > 3000) ? 18 : ((score > 1000) ? 22 : 28);
        if (isOverdrive) threshold = Math.floor(threshold * 1.5);
        if (spawnTimer >= threshold) {
          spawnTimer = 0;
          spawnBlock();
        }

        const speedFactor = isOverdrive ? 0.45 : 1.0;
        blocks.forEach(b => {
          b.progress += b.speed * speedFactor;
          if (b.progress > 1.08 && !b.sliced) {
            if (b.type === 'SPIKE') {
              score += 20; // 躲避奖励
            } else {
              combo = 0;
              hitMsg = "SYNAPSE MISS! -6%";
              hitMsgColor = "#ff4444";
              hitMsgTimer = 14;
              snd.playMiss();
              applySyncDamage(6);
            }
          }
        });
        blocks = blocks.filter(b => b.progress <= 1.15 && !b.sliced);

        particles.forEach(p => {
          p.x += p.vx;
          p.y += p.vy;
          p.life--;
        });
        particles = particles.filter(p => p.life > 0);
      }

    }

    function render() {
      // 1. 画布背景
      ctx.fillStyle = isOverdrive ? '#1b1404' : '#070611';
      ctx.fillRect(0, 0, 240, 320);

      // 2. 顶部 HUD
      ctx.font = '11px monospace';
      ctx.fillStyle = '#ffffff';
      ctx.fillText(`SCORE:${score}`, 8, 18);

      const hpCol = syncHp > 50 ? '#33ff66' : (syncHp > 25 ? '#ffaa00' : '#ff3333');
      ctx.fillStyle = hpCol;
      ctx.fillText(`HP:${syncHp}% x${lives}`, 92, 18);

      if (isOverdrive) {
        ctx.fillStyle = '#ffd700';
        ctx.fillText("BURST 3x!", 172, 18);
      } else if (overdriveGauge >= 100) {
        ctx.fillStyle = '#ffd700';
        ctx.fillText("[OK] READY", 168, 18);
      } else {
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(`AP:${overdriveGauge}%`, 172, 18);
      }

      // 3. 3D 透视视神经隧道
      const gridCol = isOverdrive ? '#553300' : '#1a1b30';
      ctx.strokeStyle = gridCol;
      ctx.lineWidth = 1;

      // 远景消失点放射轨道线
      for (let s = 0; s < 16; s++) {
        const p = s / 16;
        const l = calcPos(p, 0);
        const r = calcPos(p, 1);
        const c = calcPos(p, 2);
        ctx.fillStyle = isOverdrive ? '#886611' : '#003366';
        ctx.fillRect(l.x + l.w / 2 - 1, l.y + l.h / 2, 3, 2);
        ctx.fillStyle = isOverdrive ? '#995511' : '#661122';
        ctx.fillRect(r.x + r.w / 2 - 1, r.y + r.h / 2, 3, 2);
      }

      // 飞掠深度环
      const ringOffset = (tick * 2) % 36;
      for (let r = 0; r < 4; r++) {
        const p = (r * 36 + ringOffset) / 144;
        if (p >= 0.05 && p <= 1.0) {
          const y = Math.floor(VP_Y + p * p * (DEST_Y - VP_Y));
          const span = Math.floor(20 + p * 180);
          ctx.fillStyle = gridCol;
          ctx.fillRect(120 - span / 2, y, span, 1);
        }
      }

      // 判定线
      const strikeY = 196;
      ctx.fillStyle = '#334466';
      ctx.fillRect(20, strikeY, 200, 2);
      // 左判定靶 (Cyan)
      ctx.fillStyle = '#00e5ff';
      ctx.fillRect(44, strikeY - 2, 24, 5);
      // 右判定靶 (Red)
      ctx.fillStyle = '#ff3366';
      ctx.fillRect(172, strikeY - 2, 24, 5);
      // 中判定靶 (Gold)
      ctx.fillStyle = '#ffd700';
      ctx.fillRect(110, strikeY - 2, 20, 5);

      // 4. 绘制方块
      blocks.forEach(b => {
        const pos = calcPos(b.progress, b.lane);
        if (b.type === 'BLUE') {
          ctx.fillStyle = '#0055aa';
          ctx.fillRect(pos.x, pos.y, pos.w, pos.h);
          ctx.fillStyle = '#00e5ff';
          ctx.fillRect(pos.x + 1, pos.y + 1, pos.w - 2, pos.h - 2);
          if (pos.w > 10) {
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(pos.x + pos.w / 2 - 2, pos.y + pos.h / 2 - 1, 4, 2);
          }
        } else if (b.type === 'RED') {
          ctx.fillStyle = '#881122';
          ctx.fillRect(pos.x, pos.y, pos.w, pos.h);
          ctx.fillStyle = '#ff2255';
          ctx.fillRect(pos.x + 1, pos.y + 1, pos.w - 2, pos.h - 2);
          if (pos.w > 10) {
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(pos.x + pos.w / 2 - 2, pos.y + pos.h / 2 - 1, 4, 2);
          }
        } else if (b.type === 'DUAL') {
          ctx.fillStyle = '#d000ff';
          ctx.fillRect(pos.x, pos.y, pos.w, pos.h);
          ctx.fillStyle = '#ffd700';
          ctx.fillRect(pos.x + 2, pos.y + 2, pos.w - 4, pos.h - 4);
        } else if (b.type === 'SPIKE') {
          ctx.fillStyle = '#ff9900';
          ctx.fillRect(pos.x, pos.y, pos.w, pos.h);
          ctx.fillStyle = '#111111';
          ctx.fillRect(pos.x + 2, pos.y + 2, pos.w - 4, pos.h - 4);
        }
      });

      // 5. 粒子
      particles.forEach(p => {
        ctx.fillStyle = p.color;
        ctx.fillRect(Math.floor(p.x), Math.floor(p.y), 3, 3);
      });

      // 6. 机械果蝇角色机甲
      if (!gameOver) {
        const fx = 120;
        const fy = 216;
        const wingY = (tick % 2 === 0) ? -2 : 1;

        // 左翼与右翼
        ctx.fillStyle = '#0088bb';
        ctx.fillRect(fx - 24, fy - 6 + wingY, 18, 5);
        ctx.fillStyle = '#00e5ff';
        ctx.fillRect(fx - 20, fy - 8 + wingY, 12, 3);

        ctx.fillStyle = '#bb2244';
        ctx.fillRect(fx + 6, fy - 6 + wingY, 18, 5);
        ctx.fillStyle = '#ff3366';
        ctx.fillRect(fx + 8, fy - 8 + wingY, 12, 3);

        // 躯体与头
        ctx.fillStyle = '#334455';
        ctx.fillRect(fx - 6, fy - 3, 12, 14);
        ctx.fillStyle = '#445566';
        ctx.fillRect(fx - 5, fy - 9, 10, 6);

        // 复眼 (R7 蓝 / R8 红)
        ctx.fillStyle = '#00e5ff';
        ctx.fillRect(fx - 8, fy - 9, 4, 6);
        ctx.fillStyle = '#ff2255';
        ctx.fillRect(fx + 4, fy - 9, 4, 6);

        // 光剑弧线
        if (slashLeftTimer > 0) {
          ctx.fillStyle = '#00e5ff';
          ctx.fillRect(fx - 58, fy - 26, 44, 4);
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(fx - 48, fy - 22, 36, 5);
        } else {
          ctx.fillStyle = '#00e5ff';
          ctx.fillRect(fx - 18, fy - 14, 4, 12);
          ctx.fillRect(fx - 22, fy - 22, 5, 10);
        }

        if (slashRightTimer > 0) {
          ctx.fillStyle = '#ff2255';
          ctx.fillRect(fx + 14, fy - 26, 44, 4);
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(fx + 12, fy - 22, 36, 5);
        } else {
          ctx.fillStyle = '#ff2255';
          ctx.fillRect(fx + 14, fy - 14, 4, 12);
          ctx.fillRect(fx + 17, fy - 22, 5, 10);
        }

        if (slashDualTimer > 0) {
          ctx.fillStyle = '#ffd700';
          ctx.fillRect(fx - 24, fy - 28, 48, 6);
          ctx.fillStyle = '#d000ff';
          ctx.fillRect(fx - 16, fy - 34, 32, 16);
        }
      }

      // 7. 底部提示
      ctx.textAlign = 'center';
      ctx.font = '10px monospace';
      if (hitMsgTimer > 0) {
        ctx.fillStyle = hitMsgColor;
        ctx.fillText(hitMsg, 120, 290);
      } else {
        ctx.fillStyle = '#00e5ff';
        ctx.fillText("UP: BLUE (R7) | DOWN: RED (R8)", 120, 286);
        ctx.fillText("OK: DUAL / OVERDRIVE", 120, 300);
      }

      if (gameOver) {
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(0, 100, 240, 80);
        ctx.fillStyle = '#ff3333';
        ctx.font = 'bold 12px monospace';
        ctx.fillText("140K SYNAPSE LOST!", 120, 135);
        ctx.fillStyle = '#ffd700';
        ctx.font = '10px monospace';
        ctx.fillText("PRESS [OK] / SPACE RETRY", 120, 155);
      }
      ctx.textAlign = 'left';
    }

      let half = 0, ended = false, endT = 0, t = 0;
      return {
        update(input) {
          t++;
          if (!ended) {
            if (input.upP) slashLeft();
            if (input.downP) slashRight();
            if (input.okP) slashDual();
          }
          half ^= 1;
          if (half) update();
          api.stats.score = score; api.stats.maxCombo = maxCombo;
          api.stats.time = t / 60;
          if (gameOver && !ended) { ended = true; }
          if (ended && ++endT === 45) api.end();
        },
        draw(c) { ctx = c; ctx.save(); render(); ctx.restore(); }
      };
    }
  });
})();
