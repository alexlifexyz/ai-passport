// simulator/games-impl.js —— Alex Arcade 11 大游戏核心玩法实现集合
(function(global) {
  'use strict';

  function clamp(v, min, max) { return Math.max(min, Math.min(max, v)); }

  const GAMES = {
    // ==========================================
    // 1. 雷霆飞车：极速武装 (Thunder Racer HD)
    // ==========================================
    thunderracer: {
      init() {
        this.speed = 180;
        this.minSpeed = 120;
        this.maxSpeed = 260;
        this.shield = 100;
        this.nitro = 50;
        this.ammo = 8;
        this.nitroActive = false;
        this.score = 0;
        this.lane = 1;
        this.laneX = 0;
        this.targetX = 0;
        this.vehicles = [];
        this.missiles = [];
        this.items = [];
        this.distance = 0;
        this.dead = false;
        this.combo = 0;
        this.comboTimer = 0;
        this.shake = 0;
        this.invincible = 0;
        this.rechargeTimer = 0;
      },
      onOk(snd, fx) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        // 0ms 触底开火发射高爆飞弹
        this.fireMissile(snd, fx);
      },
      fireMissile(snd, fx) {
        if (this.dead) return;
        if (this.missiles.length < 8) {
          this.missiles.push({ x: this.laneX, z: 0.05, speed: 0.08, active: true });
          if (snd) snd.playMissile();
          if (this.ammo > 0) this.ammo--;
        }
      },
      triggerNitro(snd) {
        if (this.dead || this.nitro < 30) return;
        this.nitroActive = true;
        if (snd) snd.playNitro();
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        this.distance += this.speed * 0.02 * k;

        // 弹药被动充能 (每 0.9s 自动充能 1 发，上限 8 发)
        this.rechargeTimer += dt;
        if (this.rechargeTimer > 900) {
          this.rechargeTimer = 0;
          if (this.ammo < 8) this.ammo++;
        }

        // 氮气充能与消耗
        if (keys.okHold) {
          if (!this.nitroActive && this.nitro >= 30) this.triggerNitro(snd);
        }
        if (this.nitroActive) {
          this.nitro -= 1.2 * k;
          this.speed = Math.min(320, this.speed + 3.0 * k);
          if (this.nitro <= 0) { this.nitro = 0; this.nitroActive = false; }
        } else {
          if (this.speed > 180) this.speed -= 1.0 * k;
        }

        // 车道切换
        if (keys.upEdge) { this.lane = Math.max(0, this.lane - 1); if (snd) snd.playClick(); }
        if (keys.downEdge) { this.lane = Math.min(2, this.lane + 1); if (snd) snd.playClick(); }
        const targets = [-0.65, 0.0, 0.65];
        this.targetX = targets[this.lane];
        this.laneX += (this.targetX - this.laneX) * 0.25 * k;

        if (this.invincible > 0) this.invincible -= dt;
        if (this.comboTimer > 0) {
          this.comboTimer -= dt;
          if (this.comboTimer <= 0) this.combo = 0;
        }

        // 刷新前车
        if (Math.random() < 0.045 && this.vehicles.length < 5) {
          const l = Math.floor(Math.random() * 3);
          const types = ['police', 'fast', 'slow'];
          const t = types[Math.floor(Math.random() * types.length)];
          this.vehicles.push({
            type: t,
            lane: l,
            x: targets[l],
            z: 1.0,
            speed: t === 'police' ? 140 : (t === 'fast' ? 120 : 90),
            hp: t === 'police' ? 3 : 1,
            nearMissed: false
          });
        }

        // 前车推进与碰撞/擦车判定
        for (let i = this.vehicles.length - 1; i >= 0; i--) {
          const v = this.vehicles[i];
          const relSpeed = (this.speed - v.speed) / 100;
          v.z -= 0.015 * relSpeed * k;

          // 近身超车 (Near-Miss)
          if (!v.nearMissed && v.z < 0.12 && v.z > 0.02 && Math.abs(v.x - this.laneX) > 0.25 && Math.abs(v.x - this.laneX) < 0.8) {
            v.nearMissed = true;
            this.combo++;
            this.comboTimer = 1800;
            const pts = 200 * this.combo;
            this.score += pts;
            this.nitro = Math.min(100, this.nitro + 15);
            if (fx) {
              fx.floatText(120, 160, `NEAR-MISS x${this.combo}! +${pts}`, '#00e5ff');
              fx.burst(120, 200, 10, ['#00e5ff', '#ffd700']);
            }
            if (snd) snd.playCoin();
          }

          // 撞车判定
          if (v.z < 0.08 && v.z > 0.01 && Math.abs(v.x - this.laneX) < 0.35) {
            if (this.nitroActive) {
              // 氮气撞飞前车
              this.vehicles.splice(i, 1);
              this.score += 500;
              if (fx) { fx.burst(120, 220, 20, ['#ff0055', '#ff9900', '#fff']); fx.shake = 10; }
              if (snd) snd.playExplode(true);
              continue;
            } else if (this.invincible <= 0) {
              // 撞击扣损护盾
              this.shield -= (v.type === 'police' ? 35 : 20);
              this.invincible = 900;
              this.speed = Math.max(this.minSpeed, this.speed - 50);
              this.vehicles.splice(i, 1);
              if (fx) { fx.shake = 12; fx.burst(120, 220, 15, ['#ef4444', '#ffaa00']); }
              if (snd) snd.playHit();
              if (this.shield <= 0) {
                this.shield = 0;
                this.dead = true;
                if (snd) snd.playGameOver();
              }
              continue;
            }
          }

          if (v.z < -0.1) this.vehicles.splice(i, 1);
        }

        // 飞弹向前推进
        for (let i = this.missiles.length - 1; i >= 0; i--) {
          const m = this.missiles[i];
          m.z += m.speed * k;
          let hit = false;
          for (let j = this.vehicles.length - 1; j >= 0; j--) {
            const v = this.vehicles[j];
            if (Math.abs(v.z - m.z) < 0.08 && Math.abs(v.x - m.x) < 0.35) {
              v.hp--;
              hit = true;
              if (v.hp <= 0) {
                this.vehicles.splice(j, 1);
                this.score += 350;
                if (fx) fx.burst(120 + v.x * 90, 180, 16, ['#ffd700', '#ff3344', '#fff']);
                if (snd) snd.playExplode(false);
              } else {
                if (snd) snd.playHit();
              }
              break;
            }
          }
          if (hit || m.z > 1.1) this.missiles.splice(i, 1);
        }
      },
      render(ctx, frame) {
        // 透视公路渲染
        ctx.fillStyle = '#060a14';
        ctx.fillRect(0, 0, 240, 320);

        // 公路梯形地面
        ctx.fillStyle = '#1a2333';
        ctx.beginPath();
        ctx.moveTo(80, 40); ctx.lineTo(160, 40);
        ctx.lineTo(240, 320); ctx.lineTo(0, 320);
        ctx.closePath(); ctx.fill();

        // 斑马线与车道虚线
        const dashOffset = (frame * 10) % 32;
        ctx.fillStyle = '#ffd700';
        for (let y = 40; y < 320; y += 32) {
          const py = y + dashOffset;
          if (py < 320) {
            const w = 4;
            const progress = (py - 40) / 280;
            const lx1 = 120 - (20 + progress * 58);
            const lx2 = 120 + (20 + progress * 58);
            ctx.fillRect(lx1 - w/2, py, w, 14);
            ctx.fillRect(lx2 - w/2, py, w, 14);
          }
        }

        // 前方交通车辆
        for (const v of this.vehicles) {
          const depth = 1.0 - v.z;
          const y = 40 + depth * depth * 220;
          const halfW = 18 + depth * 80;
          const x = 120 + v.x * halfW;
          const w = 12 + depth * 28;
          const h = 10 + depth * 22;

          ctx.fillStyle = v.type === 'police' ? '#111' : (v.type === 'fast' ? '#ef4444' : '#f59e0b');
          ctx.fillRect(x - w/2, y - h/2, w, h);
          if (v.type === 'police') {
            ctx.fillStyle = (frame % 8 < 4) ? '#ff0033' : '#0066ff';
            ctx.fillRect(x - 3, y - h/2 - 2, 6, 3);
          }
        }

        // 飞弹
        ctx.fillStyle = '#00ffff';
        for (const m of this.missiles) {
          const depth = 1.0 - m.z;
          const y = 40 + depth * depth * 220;
          const halfW = 18 + depth * 80;
          const x = 120 + m.x * halfW;
          ctx.fillRect(x - 2, y - 6, 4, 10);
        }

        // 玩家赛车 (永不消失，撞毁展现战损起火残骸)
        const px = 120 + this.laneX * 95;
        const py = 240;

        if (this.dead) {
          // 战损起火残骸
          ctx.fillStyle = '#222';
          ctx.fillRect(px - 14, py - 12, 28, 24);
          ctx.fillStyle = '#ff3300';
          ctx.fillRect(px - 8 + (frame % 5), py - 14, 8, 8);
          ctx.fillStyle = '#ffd700';
          ctx.fillRect(px + 2 - (frame % 4), py - 16, 6, 10);
        } else {
          // 尾焰
          const flameH = this.nitroActive ? 18 + (frame % 4) * 4 : 8 + (frame % 3) * 2;
          ctx.fillStyle = this.nitroActive ? '#00ffff' : '#ff5500';
          ctx.fillRect(px - 7, py + 16, 4, flameH);
          ctx.fillRect(px + 3, py + 16, 4, flameH);
        }

        // 4只车轮与流线车体
        ctx.fillStyle = '#111';
        ctx.fillRect(px - 16, py - 14, 4, 10);
        ctx.fillRect(px + 12, py - 14, 4, 10);
        ctx.fillRect(px - 17, py + 6, 5, 12);
        ctx.fillRect(px + 12, py + 6, 5, 12);

        const bodyColor = this.dead ? '#475569' : (this.nitroActive ? '#ff0055' : '#00e5ff');
        ctx.fillStyle = bodyColor;
        ctx.fillRect(px - 12, py - 16, 24, 32);
        ctx.fillRect(px - 6, py - 20, 12, 6);

        // 挡风玻璃
        ctx.fillStyle = '#0b1626';
        ctx.fillRect(px - 6, py - 6, 12, 12);
        ctx.fillStyle = this.dead ? '#1e293b' : '#38bdf8';
        ctx.fillRect(px - 4, py - 4, 8, 3);

        // 尾翼
        ctx.fillStyle = this.dead ? '#334155' : '#ffd700';
        ctx.fillRect(px - 14, py + 12, 28, 4);

        // 受创无敌力场圈
        if (!this.dead && this.invincible > 0 && Math.floor(this.invincible / 100) % 2 === 0) {
          ctx.strokeStyle = '#fff';
          ctx.lineWidth = 2;
          ctx.strokeRect(px - 18, py - 22, 36, 42);
        }

        // 顶部 HUD (防遮挡安全边距布局)
        ctx.fillStyle = 'rgba(0,0,0,0.85)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`SCORE:${this.score}`, 12, 18);
        ctx.fillStyle = this.nitroActive ? '#00ffff' : '#22c55e';
        ctx.fillText(`${Math.floor(this.speed)}KM/H`, 95, 18);
        ctx.fillStyle = this.shield > 30 ? '#38bdf8' : '#ef4444';
        ctx.fillText(`HP:${this.shield}%`, 172, 18);

        // 死亡弹窗
        if (this.dead) {
          ctx.fillStyle = 'rgba(10,5,5,0.9)';
          ctx.strokeStyle = '#ef4444';
          ctx.lineWidth = 2;
          ctx.fillRect(25, 110, 190, 80);
          ctx.strokeRect(25, 110, 190, 80);
          ctx.fillStyle = '#ffd700';
          ctx.font = 'bold 14px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('CAR CRASHED!', 120, 140);
          ctx.fillStyle = '#fff';
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Respawn', 120, 164);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 2. 雷霆战机：大像素版 (Thunder Striker HD)
    // ==========================================
    thunder: {
      init() {
        this.player = {
          x: 105,
          y: 260,
          w: 30,
          h: 28,
          hp: 5,
          maxHp: 5,
          shield: 0,         // 离子护盾 (0~2)
          bombs: 2,          // 全屏核弹
          weaponLevel: 0,    // 0: 平民经典双发机枪; 拾取法宝限时强化!
          weaponStyle: 'vulcan', // 'vulcan' | 'wave' | 'fire'
          hasWingman: false, // 双子浮游僚机卫星
          buffTimer: 0,      // 限时法宝倒计时 (ticks)
          combo: 0,
          comboTimer: 0,
          invincibleTimer: 0,
          shootTimer: 0,
          speed: 3.8
        };
        this.stars = [];
        for (let i = 0; i < 36; i++) {
          this.stars.push({
            x: Math.random() * 240,
            y: Math.random() * 320,
            s: 0.8 + Math.random() * 2.5,
            c: Math.random() > 0.5 ? '#00e5ff' : '#ffffff'
          });
        }
        this.bullets = [];
        this.enemyBullets = [];
        this.enemies = [];
        this.items = [];
        this.score = 0;
        this.dead = false;
        this.boss = null;
        this.waveTick = 0;
        this.flashTimer = 0;
      },
      onOk(snd, fx) {
        if (this.dead) {
          this.init();
          if (snd) snd.playClick();
          return;
        }
        // 释放全屏核弹 [B]
        if (this.player.bombs > 0) {
          this.player.bombs--;
          this.flashTimer = 10;
          if (snd) {
            if (snd.playBomb) snd.playBomb();
            else snd.playExplode(true);
          }
          if (fx) {
            fx.shake = 22;
            fx.burst(120, 160, 45, ['#00e5ff', '#ffd700', '#ff0055', '#ffffff']);
            fx.text(120, 150, 'SUPER BOMB!!', '#ff0055');
          }
          // 清空敌机所有子弹
          this.enemyBullets = [];
          // 对全体敌机造成重创
          for (const e of this.enemies) {
            e.hp -= 12;
            if (fx) fx.burst(e.x + e.w / 2, e.y + e.h / 2, 16, ['#ff3344', '#ffea00']);
            if (e.hp <= 0) {
              this.score += (e.type === 'fortress' ? 120 : 60);
            }
          }
          this.enemies = this.enemies.filter(e => e.hp > 0);
          if (this.boss) {
            this.boss.hp -= 20;
            if (fx) fx.burst(this.boss.x + this.boss.w / 2, this.boss.y + this.boss.h / 2, 35, ['#ff0055', '#ffd700']);
            if (this.boss.hp <= 0) {
              this.score += 2500;
              this.boss = null;
            }
          }
        }
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        const p = this.player;
        this.waveTick++;

        if (this.flashTimer > 0) this.flashTimer--;
        if (p.invincibleTimer > 0) p.invincibleTimer -= dt;
        if (p.comboTimer > 0) {
          p.comboTimer -= dt;
          if (p.comboTimer <= 0) p.combo = 0;
        }

        // 限时强化武器法宝倒计时 (10秒 = 约600帧/10000ms)
        if (p.buffTimer > 0) {
          p.buffTimer -= dt;
          if (p.buffTimer <= 0) {
            p.weaponLevel = 0;
            p.weaponStyle = 'vulcan';
            p.hasWingman = false;
            if (fx) fx.text(p.x + 15, p.y - 10, 'BUFF EXPIRED', '#94a3b8');
          }
        }

        // 1. 星空滚动
        for (const s of this.stars) {
          s.y += s.s * k;
          if (s.y > 320) {
            s.y = 0;
            s.x = Math.random() * 240;
          }
        }

        // 2. 玩家4向机动移动
        if (keys.left || (keys.up && !keys.down && !keys.left && !keys.right)) {
          p.x -= p.speed * k;
        }
        if (keys.right || (keys.down && !keys.up && !keys.left && !keys.right)) {
          p.x += p.speed * k;
        }
        // 当按键包含明确上下时支持纵深机动
        if (keys.up && (keys.left || keys.right)) p.y -= (p.speed * 0.9) * k;
        if (keys.down && (keys.left || keys.right)) p.y += (p.speed * 0.9) * k;
        p.x = clamp(p.x, 4, 240 - p.w - 4);
        p.y = clamp(p.y, 45, 275);

        // 3. 玩家自动开火
        p.shootTimer += dt;
        const shootInterval = (p.weaponLevel > 0) ? 130 : 160;
        if (p.shootTimer >= shootInterval) {
          p.shootTimer = 0;

          if (p.weaponLevel === 0) {
            // 平民基础机枪：双发细光 (伤害 1)
            this.bullets.push({ x: p.x + 6, y: p.y - 2, w: 3, h: 10, vy: -9.5, dmg: 1, color: '#00e5ff' });
            this.bullets.push({ x: p.x + 20, y: p.y - 2, w: 3, h: 10, vy: -9.5, dmg: 1, color: '#00e5ff' });
            if (snd) snd.playLaser();
          } else if (p.weaponStyle === 'wave') {
            // 限时法宝 [W]：幻影 S 型正弦蛇形波刃 (S弯全屏舞动，穿甲)
            this.bullets.push({ x: p.x + 8, y: p.y - 2, w: 6, h: 12, vy: -9.0, dmg: 3, color: '#00ffcc', traj: 'wave', phase: 0, baseX: p.x + 8 });
            this.bullets.push({ x: p.x + 18, y: p.y - 2, w: 6, h: 12, vy: -9.0, dmg: 3, color: '#00ffcc', traj: 'wave', phase: Math.PI, baseX: p.x + 18 });
            this.bullets.push({ x: p.x + 2, y: p.y, w: 5, h: 10, vy: -8.5, dmg: 2, color: '#88ff00', traj: 'wave', phase: Math.PI * 0.5, baseX: p.x + 2 });
            this.bullets.push({ x: p.x + 24, y: p.y, w: 5, h: 10, vy: -8.5, dmg: 2, color: '#88ff00', traj: 'wave', phase: Math.PI * 1.5, baseX: p.x + 24 });
            if (snd) {
              if (snd.playWave) snd.playWave();
              else snd.playLaser();
            }
          } else if (p.weaponStyle === 'fire') {
            // 限时法宝 [F]：炼狱爆轰烈焰火球 (狂暴火球贯穿破甲)
            this.bullets.push({ x: p.x + 11, y: p.y - 4, w: 8, h: 12, vy: -8.5, dmg: 4, color: '#ff3300', traj: 'fire', piercing: true });
            this.bullets.push({ x: p.x + 3, y: p.y - 1, w: 6, h: 10, vy: -8.0, vx: -0.8, dmg: 2, color: '#ff6600', traj: 'fire' });
            this.bullets.push({ x: p.x + 21, y: p.y - 1, w: 6, h: 10, vy: -8.0, vx: 0.8, dmg: 2, color: '#ff6600', traj: 'fire' });
            if (snd) {
              if (snd.playFire) snd.playFire();
              else snd.playLaser();
            }
          } else {
            // 限时法宝 [P]：超能突击神火激光 (扇形 4 连发巨炮)
            this.bullets.push({ x: p.x + 10, y: p.y - 3, w: 5, h: 14, vy: -10.0, vx: -0.4, dmg: 3, color: '#ffea00' });
            this.bullets.push({ x: p.x + 16, y: p.y - 3, w: 5, h: 14, vy: -10.0, vx: 0.4, dmg: 3, color: '#ffea00' });
            this.bullets.push({ x: p.x + 2, y: p.y, w: 4, h: 12, vy: -9.5, vx: -1.6, dmg: 2, color: '#00e5ff' });
            this.bullets.push({ x: p.x + 24, y: p.y, w: 4, h: 12, vy: -9.5, vx: 1.6, dmg: 2, color: '#00e5ff' });
            if (snd) snd.playLaser();
          }

          // 双子浮游僚机卫星协同开火
          if (p.hasWingman) {
            this.bullets.push({ x: p.x - 10, y: p.y + 8, w: 3, h: 8, vy: -9.5, vx: -0.8, dmg: 1, color: '#00e5ff' });
            this.bullets.push({ x: p.x + 36, y: p.y + 8, w: 3, h: 8, vy: -9.5, vx: 0.8, dmg: 1, color: '#00e5ff' });
          }
        }

        // 4. 玩家子弹移动
        for (let i = this.bullets.length - 1; i >= 0; i--) {
          const b = this.bullets[i];
          b.y += b.vy * k;
          if (b.traj === 'wave') {
            b.phase = (b.phase || 0) + 0.32 * k;
            b.baseX = (b.baseX || b.x) + (b.vx || 0) * k;
            b.x = b.baseX + Math.sin(b.phase) * 18;
          } else if (b.vx) {
            b.x += b.vx * k;
          }
          if (b.y < -20 || b.x < -20 || b.x > 260) {
            this.bullets.splice(i, 1);
          }
        }

        // 5. 敌机子弹移动与碰撞玩家
        for (let i = this.enemyBullets.length - 1; i >= 0; i--) {
          const eb = this.enemyBullets[i];
          eb.x += eb.vx * k;
          eb.y += eb.vy * k;
          if (eb.y > 330 || eb.x < -10 || eb.x > 250) {
            this.enemyBullets.splice(i, 1);
            continue;
          }
          // 击中玩家判定
          if (p.invincibleTimer <= 0 &&
              eb.x < p.x + p.w - 4 && eb.x + eb.w > p.x + 4 &&
              eb.y < p.y + p.h - 4 && eb.y + eb.h > p.y + 4) {
            this.enemyBullets.splice(i, 1);
            if (p.shield > 0) {
              p.shield--;
              p.invincibleTimer = 900;
              if (snd) {
                if (snd.playShield) snd.playShield();
                else snd.playHit();
              }
              if (fx) {
                fx.shake = 8;
                fx.burst(p.x + p.w / 2, p.y + p.h / 2, 16, ['#00e5ff', '#88ffff', '#ffffff']);
                fx.text(p.x + 15, p.y - 12, 'SHIELD BLOCK!', '#00e5ff');
              }
            } else {
              p.hp--;
              p.invincibleTimer = 1100;
              if (snd) snd.playHit();
              if (fx) {
                fx.shake = 12;
                fx.burst(p.x + p.w / 2, p.y + p.h / 2, 18, ['#ff3344', '#ffaa00']);
              }
              if (p.hp <= 0) {
                this.dead = true;
                if (snd) snd.playGameOver();
              }
            }
          }
        }

        // 6. 掉落法宝道具推进与拾取
        for (let i = this.items.length - 1; i >= 0; i--) {
          const it = this.items[i];
          it.y += 1.4 * k;
          if (it.y > 330) {
            this.items.splice(i, 1);
            continue;
          }
          // 拾取碰撞
          if (it.x < p.x + p.w && it.x + it.w > p.x &&
              it.y < p.y + p.h && it.y + it.h > p.y) {
            this.items.splice(i, 1);
            if (snd) snd.playPowerup();
            this.score += 50;

            if (it.type === 'P') {
              p.weaponStyle = 'vulcan';
              p.weaponLevel = 2;
              p.hasWingman = true;
              p.buffTimer = 12000;
              if (fx) { fx.text(p.x + 15, p.y - 14, '突击神火 [P] !!', '#ffea00'); fx.burst(p.x + 15, p.y, 15, ['#ffea00', '#fff']); }
            } else if (it.type === 'W') {
              p.weaponStyle = 'wave';
              p.weaponLevel = 2;
              p.hasWingman = true;
              p.buffTimer = 12000;
              if (fx) { fx.text(p.x + 15, p.y - 14, '幻影波动 [W] !!', '#00ffcc'); fx.burst(p.x + 15, p.y, 15, ['#00ffcc', '#fff']); }
            } else if (it.type === 'F') {
              p.weaponStyle = 'fire';
              p.weaponLevel = 2;
              p.hasWingman = false;
              p.buffTimer = 12000;
              if (fx) { fx.text(p.x + 15, p.y - 14, '炼狱烈焰 [F] !!', '#ff3300'); fx.burst(p.x + 15, p.y, 15, ['#ff3300', '#ffea00']); }
            } else if (it.type === 'S') {
              if (p.shield < 2) p.shield++;
              if (fx) { fx.text(p.x + 15, p.y - 14, '离子护盾 [S] +1', '#00bfff'); fx.burst(p.x + 15, p.y, 15, ['#00e5ff', '#fff']); }
            } else if (it.type === 'B') {
              p.bombs++;
              if (fx) { fx.text(p.x + 15, p.y - 14, '核弹补给 [B] +1', '#ff0055'); fx.burst(p.x + 15, p.y, 15, ['#ff0055', '#fff']); }
            } else if (it.type === 'H') {
              if (p.hp < p.maxHp) p.hp++;
              if (fx) { fx.text(p.x + 15, p.y - 14, '机身维修 [H] +1', '#22c55e'); fx.burst(p.x + 15, p.y, 15, ['#22c55e', '#fff']); }
            }
          }
        }

        // 7. 生成敌机编队
        if (!this.boss && (this.waveTick % 45 === 0) && this.enemies.length < 6) {
          const r = Math.random();
          let type = 'falcon';
          let w = 26, h = 24, hp = 2, vy = 2.2;
          if (r < 0.4) {
            type = 'falcon'; // 烈隼三角隐形战机
            w = 26; h = 24; hp = 2; vy = 2.4;
          } else if (r < 0.75) {
            type = 'wyvern'; // 幻翼机械飞龙 (扇翅动态)
            w = 32; h = 28; hp = 4; vy = 1.8;
          } else {
            type = 'fortress'; // 空中巡洋堡垒 (厚血重舰)
            w = 36; h = 30; hp = 6; vy = 1.2;
          }
          this.enemies.push({
            type,
            x: 15 + Math.random() * (240 - w - 30),
            y: -35,
            w, h, hp, maxHp: hp,
            vy,
            vx: (Math.random() - 0.5) * 1.2,
            shootTimer: 25 + Math.floor(Math.random() * 40),
            animTick: Math.floor(Math.random() * 20)
          });
        }

        // 8. 召唤星海利维坦巨型龙神 BOSS
        if (!this.boss && this.score >= 280 && this.waveTick > 300) {
          this.boss = {
            type: 'boss',
            x: 86,
            y: -55,
            targetY: 36,
            w: 68,
            h: 50,
            hp: 60,
            maxHp: 60,
            vx: 1.4,
            vy: 0.9,
            shootTimer: 35,
            animTick: 0
          };
          if (fx) {
            fx.shake = 16;
            fx.text(120, 100, 'WARNING: LEVIATHAN BOSS!', '#ff0055');
          }
        }

        // BOSS 行为
        if (this.boss) {
          const b = this.boss;
          b.animTick++;
          if (b.y < b.targetY) b.y += b.vy * k;
          b.x += b.vx * k;
          if (b.x < 10 || b.x > 240 - b.w - 10) b.vx = -b.vx;

          b.shootTimer--;
          if (b.shootTimer <= 0 && b.y >= b.targetY) {
            b.shootTimer = 38;
            // 4 发散射弹幕
            const spreads = [-1.4, -0.5, 0.5, 1.4];
            for (const sp of spreads) {
              this.enemyBullets.push({
                x: b.x + b.w / 2 - 3,
                y: b.y + b.h - 4,
                w: 6, h: 6,
                vx: sp, vy: 2.8
              });
            }
          }
        }

        // 9. 敌机逻辑与射击
        for (let i = this.enemies.length - 1; i >= 0; i--) {
          const e = this.enemies[i];
          e.animTick++;
          e.x += e.vx * k;
          e.y += e.vy * k;
          if (e.x < 4 || e.x > 240 - e.w - 4) e.vx = -e.vx;

          // 敌机开火
          e.shootTimer--;
          if (e.shootTimer <= 0 && e.y > 10 && e.y < 230) {
            if (e.type === 'wyvern') {
              e.shootTimer = 55;
              this.enemyBullets.push({
                x: e.x + e.w / 2 - 3,
                y: e.y + e.h,
                w: 6, h: 6,
                vx: (p.x > e.x ? 0.9 : -0.9),
                vy: 2.6
              });
            } else {
              e.shootTimer = 65;
              this.enemyBullets.push({
                x: e.x + e.w / 2 - 2,
                y: e.y + e.h,
                w: 5, h: 5,
                vx: 0,
                vy: 2.5
              });
            }
          }

          // 碰撞玩家
          if (p.invincibleTimer <= 0 &&
              e.x < p.x + p.w && e.x + e.w > p.x &&
              e.y < p.y + p.h && e.y + e.h > p.y) {
            if (p.shield > 0) {
              p.shield--;
              p.invincibleTimer = 900;
              if (snd) snd.playHit();
              if (fx) {
                fx.shake = 10;
                fx.burst(p.x + p.w / 2, p.y + p.h / 2, 16, ['#00e5ff', '#fff']);
                fx.text(p.x + 15, p.y - 12, 'SHIELD ABSORB!', '#00e5ff');
              }
            } else {
              p.hp--;
              p.invincibleTimer = 1100;
              if (snd) snd.playHit();
              if (fx) {
                fx.shake = 14;
                fx.burst(p.x + p.w / 2, p.y + p.h / 2, 16, ['#ff3344', '#ffaa00']);
              }
              if (p.hp <= 0) {
                this.dead = true;
                if (snd) snd.playGameOver();
              }
            }
            e.hp -= 3;
            if (e.hp <= 0) {
              this.enemies.splice(i, 1);
              continue;
            }
          }

          if (e.y > 330) {
            this.enemies.splice(i, 1);
            continue;
          }

          // 玩家子弹命中敌机
          for (let j = this.bullets.length - 1; j >= 0; j--) {
            const b = this.bullets[j];
            if (b.x < e.x + e.w && b.x + b.w > e.x &&
                b.y < e.y + e.h && b.y + b.h > e.y) {
              if (!b.piercing) this.bullets.splice(j, 1);
              e.hp -= (b.dmg || 1);
              if (snd) snd.playHit();

              if (e.hp <= 0) {
                this.enemies.splice(i, 1);
                p.combo++;
                p.comboTimer = 2200;
                const mult = p.combo > 8 ? 3 : (p.combo > 3 ? 2 : 1);
                const baseScore = e.type === 'fortress' ? 40 : (e.type === 'wyvern' ? 30 : 20);
                this.score += baseScore * mult;

                if (fx) {
                  const colors = e.type === 'wyvern' ? ['#aa00ff', '#ff00aa', '#ffffff'] : ['#ffaa00', '#ffd700', '#ffffff'];
                  fx.burst(e.x + e.w / 2, e.y + e.h / 2, 18, colors);
                  if (mult > 1) fx.text(e.x + e.w / 2, e.y, `${p.combo} COMBO! x${mult}`, '#ffea00');
                }
                if (snd) snd.playExplode(e.type === 'fortress');

                // 40% 几率掉落 6 种法宝道具之一
                if (Math.random() < 0.42) {
                  const dice = Math.random();
                  let itType = 'P';
                  if (dice < 0.26) itType = 'P';
                  else if (dice < 0.48) itType = 'W';
                  else if (dice < 0.68) itType = 'F';
                  else if (dice < 0.82) itType = 'S';
                  else if (dice < 0.92) itType = 'B';
                  else itType = 'H';

                  this.items.push({
                    type: itType,
                    x: e.x + e.w / 2 - 7,
                    y: e.y,
                    w: 14, h: 14
                  });
                }
                break;
              }
            }
          }
        }

        // 10. 玩家子弹打击 BOSS
        if (this.boss) {
          const b = this.boss;
          for (let j = this.bullets.length - 1; j >= 0; j--) {
            const pb = this.bullets[j];
            if (pb.x < b.x + b.w && pb.x + pb.w > b.x &&
                pb.y < b.y + b.h && pb.y + pb.h > b.y) {
              if (!pb.piercing) this.bullets.splice(j, 1);
              b.hp -= (pb.dmg || 1);
              if (snd) snd.playHit();
              if (b.hp <= 0) {
                this.score += 2500;
                if (fx) {
                  fx.shake = 24;
                  fx.burst(b.x + b.w / 2, b.y + b.h / 2, 50, ['#ff0055', '#ffd700', '#00e5ff', '#ffffff']);
                  fx.text(120, 140, 'BOSS DESTROYED!!', '#ffd700');
                }
                if (snd) snd.playExplode(true);
                this.boss = null;
                break;
              }
            }
          }
        }
      },
      render(ctx, frame, fx) {
        // 全屏核弹白光
        if (this.flashTimer > 0) {
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(0, 0, 240, 320);
          return;
        }

        // 深邃星空背景
        ctx.fillStyle = '#060814';
        ctx.fillRect(0, 0, 240, 320);

        // 星空滚动
        for (const s of this.stars) {
          ctx.fillStyle = s.c;
          ctx.fillRect(s.x, s.y, s.s > 2 ? 2 : 1, s.s > 2 ? 2 : 1);
        }

        const p = this.player;

        // 绘制子弹 (三大流派武器特效)
        for (const b of this.bullets) {
          if (b.traj === 'wave') {
            // S型波动刃：霓虹碧翠外缘 + 亮白晶核
            ctx.fillStyle = '#00ffcc';
            ctx.fillRect(b.x - 1, b.y - 1, b.w + 2, b.h + 2);
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(b.x, b.y, b.w, b.h);
          } else if (b.traj === 'fire') {
            // 炼狱火球：外圈烈红 + 内层炽黄
            ctx.fillStyle = '#ff2200';
            ctx.fillRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4);
            ctx.fillStyle = '#ffea00';
            ctx.fillRect(b.x, b.y, b.w, b.h);
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(b.x + 2, b.y + 2, Math.max(2, b.w - 4), Math.max(2, b.h - 4));
          } else {
            // 直线激光
            ctx.fillStyle = b.color || '#ffea00';
            ctx.fillRect(b.x, b.y, b.w, b.h);
          }
        }

        // 绘制敌机子弹 (发光红光弹)
        for (const eb of this.enemyBullets) {
          ctx.fillStyle = '#ff2255';
          ctx.fillRect(eb.x - 1, eb.y - 1, eb.w + 2, eb.h + 2);
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(eb.x, eb.y, eb.w, eb.h);
        }

        // 绘制掉落法宝道具 [P] [W] [F] [S] [B] [H]
        for (const it of this.items) {
          let col = '#ffd928';
          if (it.type === 'P') col = '#ffd928';
          else if (it.type === 'W') col = '#00ffcc';
          else if (it.type === 'F') col = '#ff4400';
          else if (it.type === 'S') col = '#00bfff';
          else if (it.type === 'B') col = '#ff2255';
          else if (it.type === 'H') col = '#22c55e';

          ctx.fillStyle = '#ffffff';
          ctx.fillRect(it.x, it.y, 14, 14);
          ctx.fillStyle = col;
          ctx.fillRect(it.x + 2, it.y + 2, 10, 10);
          ctx.fillStyle = '#111827';
          ctx.font = 'bold 9px monospace';
          ctx.textAlign = 'center';
          ctx.fillText(it.type, it.x + 7, it.y + 10);
        }
        ctx.textAlign = 'left';

        // 绘制普通敌机
        for (const e of this.enemies) {
          if (e.type === 'falcon') {
            // 烈隼三角隐形战机
            ctx.fillStyle = '#00e5ff';
            ctx.fillRect(e.x + 11, e.y, 4, 16);
            ctx.fillStyle = '#1e293b';
            ctx.fillRect(e.x + 4, e.y + 8, 18, 8);
            ctx.fillStyle = '#334155';
            ctx.fillRect(e.x, e.y + 14, 26, 6);
            // 翼尖导弹
            ctx.fillStyle = '#ef4444';
            ctx.fillRect(e.x, e.y + 12, 2, 6);
            ctx.fillRect(e.x + 24, e.y + 12, 2, 6);
          } else if (e.type === 'wyvern') {
            // 幻翼机械飞龙 (动态扇翅扑动)
            const flap = (Math.floor(e.animTick / 6) % 2 === 0);
            ctx.fillStyle = '#9333ea';
            ctx.fillRect(e.x + 13, e.y + 2, 6, 12);
            ctx.fillStyle = '#ec4899';
            ctx.fillRect(e.x + 14, e.y + 4, 4, 4);
            if (flap) {
              ctx.fillStyle = '#a855f7';
              ctx.fillRect(e.x + 2, e.y + 2, 12, 6);
              ctx.fillRect(e.x + 18, e.y + 2, 12, 6);
              ctx.fillStyle = '#7e22ce';
              ctx.fillRect(e.x, e.y + 6, 32, 6);
            } else {
              ctx.fillStyle = '#a855f7';
              ctx.fillRect(e.x + 4, e.y + 10, 24, 6);
              ctx.fillStyle = '#7e22ce';
              ctx.fillRect(e.x, e.y + 14, 32, 6);
            }
            // 飞龙小血条
            ctx.fillStyle = '#000';
            ctx.fillRect(e.x + 4, e.y - 5, 24, 3);
            ctx.fillStyle = '#d946ef';
            ctx.fillRect(e.x + 4, e.y - 5, (e.hp / e.maxHp) * 24, 3);
          } else {
            // 空中巡洋堡垒
            ctx.fillStyle = '#475569';
            ctx.fillRect(e.x + 6, e.y, 24, 24);
            ctx.fillStyle = '#334155';
            ctx.fillRect(e.x, e.y + 10, 36, 12);
            ctx.fillStyle = '#ffd700';
            ctx.fillRect(e.x + 14, e.y + 8, 8, 6);
            // 堡垒血条
            ctx.fillStyle = '#000';
            ctx.fillRect(e.x + 6, e.y - 5, 24, 3);
            ctx.fillStyle = '#ef4444';
            ctx.fillRect(e.x + 6, e.y - 5, (e.hp / e.maxHp) * 24, 3);
          }
        }

        // 绘制星海利维坦 BOSS
        if (this.boss) {
          const b = this.boss;
          const flap = (Math.floor(b.animTick / 10) % 2 === 0);
          ctx.fillStyle = '#311042';
          ctx.fillRect(b.x + 24, b.y, 20, 44);
          ctx.fillStyle = '#ff0055';
          ctx.fillRect(b.x + 28, b.y + 8, 12, 16);
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(b.x + 31, b.y + 12, 6, 8);

          // 龙神巨翼
          if (flap) {
            ctx.fillStyle = '#6b21a8';
            ctx.fillRect(b.x + 4, b.y + 8, 60, 16);
            ctx.fillStyle = '#9333ea';
            ctx.fillRect(b.x, b.y + 14, 68, 12);
          } else {
            ctx.fillStyle = '#6b21a8';
            ctx.fillRect(b.x + 6, b.y + 16, 56, 18);
            ctx.fillStyle = '#9333ea';
            ctx.fillRect(b.x, b.y + 22, 68, 12);
          }
          // 龙头主炮与两翼能量晶柱
          ctx.fillStyle = '#00e5ff';
          ctx.fillRect(b.x + 28, b.y + 36, 12, 10);
          ctx.fillStyle = '#ffd700';
          ctx.fillRect(b.x + 8, b.y + 26, 6, 8);
          ctx.fillRect(b.x + 54, b.y + 26, 6, 8);

          // 顶部全幅 BOSS 血条
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(20, 32, 200, 7);
          ctx.fillStyle = '#ff0055';
          ctx.fillRect(20, 32, Math.max(0, (b.hp / b.maxHp) * 200), 7);
          ctx.strokeStyle = '#ffd700';
          ctx.lineWidth = 1;
          ctx.strokeRect(20, 32, 200, 7);
        }

        // 绘制玩家战机
        if (!this.dead) {
          const flash = (p.invincibleTimer > 0 && Math.floor(p.invincibleTimer / 100) % 2 === 0);
          if (!flash) {
            // 双尾喷射火焰动画
            const flameH = 6 + (Math.floor(frame / 4) % 3) * 3;
            ctx.fillStyle = '#ff9900';
            ctx.fillRect(p.x + 9, p.y + 26, 4, flameH);
            ctx.fillRect(p.x + 17, p.y + 26, 4, flameH);
            ctx.fillStyle = '#ffea00';
            ctx.fillRect(p.x + 10, p.y + 26, 2, flameH - 2);
            ctx.fillRect(p.x + 18, p.y + 26, 2, flameH - 2);

            // 主机身掠翼
            ctx.fillStyle = '#0077aa';
            ctx.fillRect(p.x + 11, p.y + 2, 8, 24);
            ctx.fillStyle = '#00e5ff';
            ctx.fillRect(p.x + 13, p.y, 4, 16);
            ctx.fillStyle = '#00aacc';
            ctx.fillRect(p.x + 2, p.y + 14, 26, 8);
            ctx.fillRect(p.x, p.y + 18, 30, 6);

            // 翼尖金色双联主炮
            ctx.fillStyle = '#ffd928';
            ctx.fillRect(p.x, p.y + 12, 3, 6);
            ctx.fillRect(p.x + 27, p.y + 12, 3, 6);

            // 晶透橙座舱盖高光
            ctx.fillStyle = '#ffaa00';
            ctx.fillRect(p.x + 12, p.y + 8, 6, 8);
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(p.x + 14, p.y + 9, 2, 4);

            // 双子浮游僚机卫星
            if (p.hasWingman) {
              const lwy = p.y + 8 + (Math.floor(frame / 6) % 3);
              const rwy = p.y + 8 + ((Math.floor(frame / 6) + 1) % 3);
              // 左僚机
              ctx.fillStyle = '#00e5ff';
              ctx.fillRect(p.x - 12, lwy, 6, 6);
              ctx.fillStyle = '#ffffff';
              ctx.fillRect(p.x - 10, lwy + 2, 2, 2);
              // 右僚机
              ctx.fillStyle = '#00e5ff';
              ctx.fillRect(p.x + 36, rwy, 6, 6);
              ctx.fillStyle = '#ffffff';
              ctx.fillRect(p.x + 38, rwy + 2, 2, 2);
            }

            // 离子护盾等离子呼吸力场光环
            if (p.shield > 0) {
              ctx.strokeStyle = (Math.floor(frame / 5) % 2 === 0) ? '#00e5ff' : '#88ffff';
              ctx.lineWidth = 2;
              ctx.strokeRect(p.x - 5, p.y - 5, p.w + 10, p.h + 10);
            }
          }
        }

        // 顶部 HUD
        ctx.fillStyle = 'rgba(6,8,20,0.88)';
        ctx.fillRect(0, 0, 240, 28);
        ctx.font = 'bold 11px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`SCORE:${this.score}`, 8, 18);

        // 生命红心
        for (let i = 0; i < p.maxHp; i++) {
          ctx.fillStyle = (i < p.hp) ? '#ef4444' : '#334155';
          ctx.fillRect(115 + i * 9, 11, 7, 7);
        }

        // 护盾指示
        if (p.shield > 0) {
          ctx.fillStyle = '#00e5ff';
          ctx.fillText(`🛡️${p.shield}`, 165, 18);
        }

        // 核弹指示
        ctx.fillStyle = '#ff0055';
        ctx.fillText(`💣${p.bombs}`, 200, 18);

        // 限时强化武器倒计时微条
        if (p.buffTimer > 0) {
          const ratio = p.buffTimer / 12000;
          ctx.fillStyle = '#ffea00';
          ctx.fillRect(0, 27, 240 * ratio, 2);
        }

        // 死亡画面
        if (this.dead) {
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(25, 105, 190, 95);
          ctx.fillStyle = '#ff3344';
          ctx.font = 'bold 16px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('STRIKER DOWN!', 120, 138);
          ctx.fillStyle = '#ffd700';
          ctx.font = '12px monospace';
          ctx.fillText(`FINAL SCORE: ${this.score}`, 120, 160);
          ctx.fillStyle = '#94a3b8';
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Deploy Again', 120, 182);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 3. 像素冒险岛 HD (Adventure Island HD)
    // ==========================================
    adventure: {
      init() {
        this.p = { x: 60, y: 240, vy: 0, onGround: true, stamina: 100, lives: 3, score: 0 };
        this.axes = [];
        this.enemies = [];
        this.scroll = 0;
        this.dead = false;
        this.spawn = 0;
      },
      onOk(snd, fx) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        if (this.p.onGround) {
          this.p.vy = -8.2;
          this.p.onGround = false;
          if (snd) snd.playJump();
        } else {
          this.axes.push({ x: this.p.x + 12, y: this.p.y - 6, vx: 6.5, vy: -3.0, rot: 0 });
          if (snd) snd.playLaser();
        }
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        const spd = keys.down ? 3.0 : (keys.up ? 0.6 : 1.8);
        this.scroll += spd * k;

        this.p.y += this.p.vy * k;
        this.p.vy += 0.42 * k;
        if (this.p.y >= 240) { this.p.y = 240; this.p.vy = 0; this.p.onGround = true; }

        this.p.stamina -= 0.04 * k;
        if (this.p.stamina <= 0) {
          this.p.lives--;
          this.p.stamina = 100;
          if (this.p.lives <= 0) { this.dead = true; if (snd) snd.playGameOver(); }
        }

        // 飞斧
        for (let i = this.axes.length - 1; i >= 0; i--) {
          const a = this.axes[i];
          a.x += a.vx * k;
          a.y += a.vy * k;
          a.vy += 0.25 * k;
          a.rot += 0.4 * k;
          if (a.y > 270 || a.x > 250) this.axes.splice(i, 1);
        }

        // 生成蜗牛
        this.spawn += dt;
        if (this.spawn > 1200 && this.enemies.length < 3) {
          this.spawn = 0;
          this.enemies.push({ x: 250, y: 244, vx: -1.2, dead: false });
        }

        // 判定
        for (let i = this.enemies.length - 1; i >= 0; i--) {
          const e = this.enemies[i];
          e.x -= (spd + 0.3) * k;

          for (let j = this.axes.length - 1; j >= 0; j--) {
            const a = this.axes[j];
            if (Math.abs(a.x - e.x) < 16 && Math.abs(a.y - e.y) < 16) {
              this.enemies.splice(i, 1);
              this.axes.splice(j, 1);
              this.p.score += 200;
              if (fx) fx.burst(e.x, e.y, 10, ['#ffd700', '#fff']);
              if (snd) snd.playExplode(false);
              break;
            }
          }

          if (Math.abs(this.p.x - e.x) < 14 && Math.abs(this.p.y - e.y) < 16) {
            this.p.lives--;
            this.enemies.splice(i, 1);
            if (fx) { fx.shake = 8; fx.burst(this.p.x, this.p.y, 12, ['#ff0033']); }
            if (snd) snd.playHit();
            if (this.p.lives <= 0) { this.dead = true; if (snd) snd.playGameOver(); }
          }
        }
      },
      render(ctx, frame) {
        ctx.fillStyle = '#38bdf8';
        ctx.fillRect(0, 0, 240, 240);
        ctx.fillStyle = '#f59e0b';
        ctx.fillRect(0, 240, 240, 80);

        // 树木视差
        ctx.fillStyle = '#22c55e';
        const off = (this.scroll * 0.5) % 80;
        for (let x = -40; x < 280; x += 80) {
          ctx.beginPath();
          ctx.arc(x - off + 40, 210, 25, 0, Math.PI * 2);
          ctx.fill();
        }

        // 玩家
        const p = this.p;
        ctx.fillStyle = '#ffaa66';
        ctx.fillRect(p.x - 6, p.y - 20, 12, 10);
        ctx.fillStyle = '#22c55e';
        ctx.fillRect(p.x - 7, p.y - 24, 14, 6);
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(p.x - 8, p.y - 10, 16, 12);

        // 飞斧
        ctx.fillStyle = '#94a3b8';
        for (const a of this.axes) {
          ctx.save();
          ctx.translate(a.x, a.y);
          ctx.rotate(a.rot);
          ctx.fillRect(-6, -6, 12, 12);
          ctx.restore();
        }

        // 蜗牛
        ctx.fillStyle = '#a0522d';
        for (const e of this.enemies) {
          ctx.beginPath();
          ctx.arc(e.x, e.y - 4, 8, 0, Math.PI * 2);
          ctx.fill();
        }

        // HUD
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`SCORE:${this.p.score}`, 10, 18);
        ctx.fillStyle = '#ef4444';
        ctx.fillText(`LIFE:${this.p.lives}`, 110, 18);
        ctx.fillStyle = '#22c55e';
        ctx.fillText(`STA:${Math.floor(this.p.stamina)}%`, 170, 18);

        if (this.dead) {
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(30, 110, 180, 80);
          ctx.fillStyle = '#ffd700';
          ctx.font = 'bold 14px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('ISLAND OVER!', 120, 142);
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Restart', 120, 166);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 4. 口袋魂斗罗 HD (Pocket Contra HD)
    // ==========================================
    contra: {
      init() {
        this.p = { x: 50, y: 240, vy: 0, onGround: true, hp: 3, score: 0 };
        this.bullets = [];
        this.enemies = [];
        this.dead = false;
        this.shootTimer = 0;
      },
      onOk(snd, fx) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        // 射击散弹
        this.bullets.push({ x: this.p.x + 12, y: this.p.y - 12, vx: 7, vy: -1 });
        this.bullets.push({ x: this.p.x + 12, y: this.p.y - 12, vx: 7, vy: 0 });
        this.bullets.push({ x: this.p.x + 12, y: this.p.y - 12, vx: 7, vy: 1 });
        if (snd) snd.playLaser();
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        if (keys.up && this.p.onGround) {
          this.p.vy = -7.8;
          this.p.onGround = false;
          if (snd) snd.playJump();
        }
        this.p.y += this.p.vy * k;
        this.p.vy += 0.4 * k;
        if (this.p.y >= 240) { this.p.y = 240; this.p.vy = 0; this.p.onGround = true; }

        for (let i = this.bullets.length - 1; i >= 0; i--) {
          const b = this.bullets[i];
          b.x += b.vx * k;
          b.y += b.vy * k;
          if (b.x > 250) this.bullets.splice(i, 1);
        }

        if (Math.random() < 0.03 && this.enemies.length < 3) {
          this.enemies.push({ x: 250, y: 240, vx: -2.0, hp: 1 });
        }

        for (let i = this.enemies.length - 1; i >= 0; i--) {
          const e = this.enemies[i];
          e.x += e.vx * k;

          for (let j = this.bullets.length - 1; j >= 0; j--) {
            const b = this.bullets[j];
            if (Math.abs(b.x - e.x) < 14 && Math.abs(b.y - e.y) < 16) {
              this.enemies.splice(i, 1);
              this.bullets.splice(j, 1);
              this.p.score += 150;
              if (fx) fx.burst(e.x, e.y, 10, ['#ef4444', '#ffd700']);
              if (snd) snd.playExplode(false);
              break;
            }
          }

          if (Math.abs(this.p.x - e.x) < 12 && Math.abs(this.p.y - e.y) < 16) {
            this.p.hp--;
            this.enemies.splice(i, 1);
            if (fx) fx.shake = 10;
            if (snd) snd.playHit();
            if (this.p.hp <= 0) { this.dead = true; if (snd) snd.playGameOver(); }
          }
        }
      },
      render(ctx, frame) {
        ctx.fillStyle = '#0a1020';
        ctx.fillRect(0, 0, 240, 320);
        ctx.fillStyle = '#334155';
        ctx.fillRect(0, 240, 240, 80);

        // 战士
        const p = this.p;
        ctx.fillStyle = '#ff2233';
        ctx.fillRect(p.x - 6, p.y - 20, 12, 4);
        ctx.fillStyle = '#ffaa66';
        ctx.fillRect(p.x - 5, p.y - 16, 10, 8);
        ctx.fillStyle = '#0055ff';
        ctx.fillRect(p.x - 6, p.y - 8, 12, 10);

        // 子弹
        ctx.fillStyle = '#ff0055';
        for (const b of this.bullets) {
          ctx.beginPath();
          ctx.arc(b.x, b.y, 3, 0, Math.PI * 2);
          ctx.fill();
        }

        // 敌人
        ctx.fillStyle = '#94a3b8';
        for (const e of this.enemies) {
          ctx.fillRect(e.x - 6, e.y - 18, 12, 18);
        }

        // HUD
        ctx.fillStyle = 'rgba(0,0,0,0.8)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`SCORE:${this.p.score}`, 10, 18);
        ctx.fillStyle = '#ff3344';
        ctx.fillText(`LIVES:${this.p.hp}`, 170, 18);

        if (this.dead) {
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(30, 110, 180, 80);
          ctx.fillStyle = '#ff0055';
          ctx.font = 'bold 14px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('MISSION FAILED', 120, 142);
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Respawn', 120, 166);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 5. 果蝇光剑：神经脉冲 (FlySaber HD)
    // ==========================================
    flysaber: {
      init() {
        this.score = 0;
        this.combo = 0;
        this.blocks = [];
        this.spawn = 0;
        this.dead = false;
        this.overdrive = 0;
        this.hp = 3;
      },
      onOk(snd, fx) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        // 双刀合击中央核心 / 神经超频
        let hit = false;
        for (let i = this.blocks.length - 1; i >= 0; i--) {
          const b = this.blocks[i];
          if (b.type === 'core' && b.z < 0.25 && b.z > 0.0) {
            this.blocks.splice(i, 1);
            hit = true;
            this.score += 500;
            this.combo++;
            if (fx) fx.burst(120, 220, 18, ['#ffd700', '#fff']);
            if (snd) snd.playSlash(true);
            break;
          }
        }
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        this.spawn += dt;
        if (this.spawn > 550) {
          this.spawn = 0;
          const r = Math.random();
          const type = r < 0.45 ? 'blue' : (r < 0.9 ? 'red' : 'core');
          this.blocks.push({ type, z: 1.0, side: type === 'blue' ? -1 : (type === 'red' ? 1 : 0) });
        }

        // UP 挥砍蓝剑
        if (keys.upEdge) {
          if (snd) snd.playSlash(true);
          for (let i = this.blocks.length - 1; i >= 0; i--) {
            const b = this.blocks[i];
            if (b.type === 'blue' && b.z < 0.28 && b.z > 0.0) {
              this.blocks.splice(i, 1);
              this.score += 200;
              this.combo++;
              if (fx) fx.burst(60, 220, 12, ['#00e5ff', '#fff']);
              break;
            }
          }
        }

        // DOWN 挥砍红剑
        if (keys.downEdge) {
          if (snd) snd.playSlash(false);
          for (let i = this.blocks.length - 1; i >= 0; i--) {
            const b = this.blocks[i];
            if (b.type === 'red' && b.z < 0.28 && b.z > 0.0) {
              this.blocks.splice(i, 1);
              this.score += 200;
              this.combo++;
              if (fx) fx.burst(180, 220, 12, ['#ff0055', '#fff']);
              break;
            }
          }
        }

        // 方块推进
        for (let i = this.blocks.length - 1; i >= 0; i--) {
          const b = this.blocks[i];
          b.z -= 0.02 * k;
          if (b.z < -0.05) {
            this.blocks.splice(i, 1);
            this.combo = 0;
            this.hp--;
            if (snd) snd.playHit();
            if (this.hp <= 0) { this.dead = true; if (snd) snd.playGameOver(); }
          }
        }
      },
      render(ctx, frame) {
        ctx.fillStyle = '#06040e';
        ctx.fillRect(0, 0, 240, 320);

        // 3D 霓虹轨道
        ctx.strokeStyle = '#331155';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(120, 60); ctx.lineTo(10, 320);
        ctx.moveTo(120, 60); ctx.lineTo(230, 320);
        ctx.stroke();

        // 飞入方块
        for (const b of this.blocks) {
          const depth = 1.0 - b.z;
          const y = 60 + depth * depth * 230;
          const x = 120 + b.side * (depth * 90);
          const size = 10 + depth * 28;

          ctx.fillStyle = b.type === 'blue' ? '#00e5ff' : (b.type === 'red' ? '#ff0055' : '#ffd700');
          ctx.fillRect(x - size/2, y - size/2, size, size);
        }

        // 光剑
        ctx.fillStyle = '#00e5ff';
        ctx.fillRect(40, 260, 6, 45);
        ctx.fillStyle = '#ff0055';
        ctx.fillRect(194, 260, 6, 45);

        // HUD
        ctx.fillStyle = 'rgba(0,0,0,0.8)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(`SCORE:${this.score}`, 10, 18);
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`COMBO:${this.combo}`, 110, 18);
        ctx.fillStyle = '#ff0055';
        ctx.fillText(`HP:${this.hp}`, 190, 18);

        if (this.dead) {
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(30, 110, 180, 80);
          ctx.fillStyle = '#d000ff';
          ctx.font = 'bold 14px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('SYNAPSE CUT!', 120, 142);
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Revive', 120, 166);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 6. 果蝇超跑：极速狂飙 (FlyDriver HD)
    // ==========================================
    flydriver: {
      init() {
        this.lane = 1;
        this.laneX = 0;
        this.speed = 180;
        this.score = 0;
        this.dead = false;
        this.cars = [];
      },
      onOk(snd, fx) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        this.speed = 280;
        if (snd) snd.playNitro();
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        if (keys.upEdge) { this.lane = Math.max(0, this.lane - 1); if (snd) snd.playClick(); }
        if (keys.downEdge) { this.lane = Math.min(2, this.lane + 1); if (snd) snd.playClick(); }
        const targets = [-0.65, 0, 0.65];
        this.laneX += (targets[this.lane] - this.laneX) * 0.25 * k;

        if (this.speed > 180) this.speed -= 1.0 * k;
        this.score += Math.floor(this.speed * 0.05 * k);

        if (Math.random() < 0.04 && this.cars.length < 4) {
          const l = Math.floor(Math.random() * 3);
          this.cars.push({ x: targets[l], z: 1.0 });
        }

        for (let i = this.cars.length - 1; i >= 0; i--) {
          const c = this.cars[i];
          c.z -= 0.02 * (this.speed / 180) * k;
          if (c.z < 0.08 && c.z > 0.01 && Math.abs(c.x - this.laneX) < 0.35) {
            this.dead = true;
            if (fx) fx.shake = 12;
            if (snd) snd.playExplode(true);
          }
          if (c.z < -0.1) this.cars.splice(i, 1);
        }
      },
      render(ctx, frame) {
        ctx.fillStyle = '#060814';
        ctx.fillRect(0, 0, 240, 320);

        // 赛道
        ctx.fillStyle = '#1e1b4b';
        ctx.beginPath();
        ctx.moveTo(80, 50); ctx.lineTo(160, 50);
        ctx.lineTo(230, 320); ctx.lineTo(10, 320);
        ctx.closePath(); ctx.fill();

        for (const c of this.cars) {
          const depth = 1.0 - c.z;
          const y = 50 + depth * depth * 230;
          const x = 120 + c.x * (20 + depth * 80);
          const w = 12 + depth * 24;
          const h = 10 + depth * 18;
          ctx.fillStyle = '#ffd700';
          ctx.fillRect(x - w/2, y - h/2, w, h);
        }

        const px = 120 + this.laneX * 90;
        ctx.fillStyle = '#00e5ff';
        ctx.fillRect(px - 14, 240, 28, 36);

        ctx.fillStyle = 'rgba(0,0,0,0.8)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`SCORE:${this.score}`, 10, 18);
        ctx.fillStyle = '#22c55e';
        ctx.fillText(`${Math.floor(this.speed)}KM/H`, 150, 18);

        if (this.dead) {
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(30, 110, 180, 80);
          ctx.fillStyle = '#ef4444';
          ctx.font = 'bold 14px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('CRASHED!', 120, 142);
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Retry', 120, 166);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 7. 像素飞鸟：重力狂飙 (Flappy Bird HD)
    // ==========================================
    flappy: {
      init() {
        this.y = 140;
        this.vy = 0;
        this.score = 0;
        this.dead = false;
        this.pipes = [{ x: 260, gapY: 120, passed: false }];
      },
      onOk(snd) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        this.vy = -5.4;
        if (snd) snd.playJump();
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        const k = dt / 16.6;
        this.vy += 0.32 * k;
        this.y += this.vy * k;

        if (this.y > 285 || this.y < 10) {
          this.dead = true;
          if (snd) snd.playGameOver();
        }

        for (let i = this.pipes.length - 1; i >= 0; i--) {
          const p = this.pipes[i];
          p.x -= 2.0 * k;
          if (!p.passed && p.x < 70) {
            p.passed = true;
            this.score++;
            if (snd) snd.playCoin();
          }
          if (p.x < 85 && p.x > 35) {
            if (this.y < p.gapY - 35 || this.y > p.gapY + 35) {
              this.dead = true;
              if (fx) fx.shake = 8;
              if (snd) snd.playGameOver();
            }
          }
          if (p.x < -40) this.pipes.splice(i, 1);
        }

        if (this.pipes.length < 2) {
          this.pipes.push({ x: 260, gapY: 80 + Math.random() * 110, passed: false });
        }
      },
      render(ctx, frame) {
        const isNight = Math.floor(this.score / 10) % 2 === 1;
        ctx.fillStyle = isNight ? '#0b1626' : '#38bdf8';
        ctx.fillRect(0, 0, 240, 320);

        // 地面
        ctx.fillStyle = '#22c55e';
        ctx.fillRect(0, 290, 240, 30);

        // 管道
        ctx.fillStyle = '#16a34a';
        for (const p of this.pipes) {
          ctx.fillRect(p.x, 0, 38, p.gapY - 35);
          ctx.fillRect(p.x, p.gapY + 35, 38, 300);
        }

        // 小鸟
        ctx.fillStyle = '#ffd700';
        ctx.beginPath();
        ctx.arc(60, this.y, 10, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = '#ef4444';
        ctx.fillRect(66, this.y - 2, 7, 5);

        // HUD
        ctx.fillStyle = 'rgba(0,0,0,0.6)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 13px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`SCORE:${this.score}`, 10, 18);

        if (this.dead) {
          ctx.fillStyle = 'rgba(0,0,0,0.85)';
          ctx.fillRect(30, 110, 180, 80);
          ctx.fillStyle = '#ffd700';
          ctx.font = 'bold 14px monospace';
          ctx.textAlign = 'center';
          ctx.fillText('GAME OVER!', 120, 142);
          ctx.font = '11px monospace';
          ctx.fillText('Press OK to Flap', 120, 166);
          ctx.textAlign = 'left';
        }
      }
    },

    // ==========================================
    // 8. 变异乒乓：混沌球局 (Chaos Pong HD)
    // ==========================================
    pong: {
      init() {
        this.paddleX = 100;
        this.aiX = 100;
        this.ballX = 120;
        this.ballY = 160;
        this.vx = 2.4;
        this.vy = 3.2;
        this.pScore = 0;
        this.aiScore = 0;
        this.mutation = 'normal';
      },
      onOk(snd) {
        // 切击旋球
        this.vx *= 1.3;
        if (snd) snd.playClick();
      },
      update(dt, keys, snd, fx) {
        const k = dt / 16.6;
        if (keys.up) this.paddleX -= 4.2 * k;
        if (keys.down) this.paddleX += 4.2 * k;
        this.paddleX = clamp(this.paddleX, 5, 175);

        // AI 跟踪
        this.aiX += (this.ballX - (this.aiX + 30)) * 0.12 * k;
        this.aiX = clamp(this.aiX, 5, 175);

        this.ballX += this.vx * k;
        this.ballY += this.vy * k;

        if (this.ballX < 8 || this.ballX > 232) {
          this.vx = -this.vx;
          if (snd) snd.playHit();
        }

        // 玩家接球
        if (this.ballY > 280 && this.ballY < 292 && this.ballX > this.paddleX && this.ballX < this.paddleX + 60) {
          this.vy = -Math.abs(this.vy);
          this.vx += (this.ballX - (this.paddleX + 30)) * 0.08;
          if (snd) snd.playLaser();
        }

        // AI 接球
        if (this.ballY < 40 && this.ballY > 28 && this.ballX > this.aiX && this.ballX < this.aiX + 60) {
          this.vy = Math.abs(this.vy);
          if (snd) snd.playLaser();
        }

        // 得分判定
        if (this.ballY > 320) {
          this.aiScore++;
          this.ballX = 120; this.ballY = 160; this.vy = -3.2;
          if (snd) snd.playHit();
        }
        if (this.ballY < 0) {
          this.pScore++;
          this.ballX = 120; this.ballY = 160; this.vy = 3.2;
          if (snd) snd.playCoin();
        }
      },
      render(ctx) {
        ctx.fillStyle = '#0a0f1d';
        ctx.fillRect(0, 0, 240, 320);

        ctx.strokeStyle = '#334155';
        ctx.setLineDash([6, 6]);
        ctx.beginPath();
        ctx.moveTo(0, 160); ctx.lineTo(240, 160);
        ctx.stroke();
        ctx.setLineDash([]);

        // AI 挡板
        ctx.fillStyle = '#ef4444';
        ctx.fillRect(this.aiX, 30, 60, 8);

        // 玩家挡板
        ctx.fillStyle = '#00e5ff';
        ctx.fillRect(this.paddleX, 282, 60, 8);

        // 球
        ctx.fillStyle = '#ffd700';
        ctx.beginPath();
        ctx.arc(this.ballX, this.ballY, 6, 0, Math.PI * 2);
        ctx.fill();

        // 比分
        ctx.font = 'bold 18px monospace';
        ctx.fillStyle = '#ef4444';
        ctx.fillText(`${this.aiScore}`, 115, 140);
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(`${this.pScore}`, 115, 190);
      }
    },

    // ==========================================
    // 9. 恶魔轮盘：生死对决 (Buckshot Roulette)
    // ==========================================
    roulette: {
      init() {
        this.pLife = 4;
        this.dLife = 4;
        this.shells = [];
        this.turn = 'player'; // player / dealer
        this.saw = false;
        this.msg = 'ROUND START';
        this.dead = false;
        this.reload();
      },
      reload() {
        this.shells = [true, false, true, false, true, false].sort(() => Math.random() - 0.5);
        this.msg = 'CHAMBER LOADED';
      },
      onOk(snd, fx) {
        if (this.dead) { this.init(); if (snd) snd.playClick(); return; }
        // 对准自己射击
        if (this.turn !== 'player') return;
        const live = this.shells.pop();
        if (live) {
          const dmg = this.saw ? 2 : 1;
          this.pLife -= dmg;
          this.msg = 'BANG! IT WAS LIVE!';
          if (snd) snd.playExplode(true);
          if (fx) fx.shake = 15;
          this.turn = 'dealer';
        } else {
          this.msg = 'CLICK! BLANK! EXTRA TURN!';
          if (snd) snd.playCoin();
        }
        this.saw = false;
        this.check();
      },
      shootDealer(snd, fx) {
        if (this.dead || this.turn !== 'player') return;
        const live = this.shells.pop();
        if (live) {
          const dmg = this.saw ? 2 : 1;
          this.dLife -= dmg;
          this.msg = 'BOOM! HIT DEALER!';
          if (snd) snd.playExplode(true);
          if (fx) fx.shake = 12;
        } else {
          this.msg = 'CLICK... BLANK!';
          if (snd) snd.playClick();
        }
        this.saw = false;
        this.turn = 'dealer';
        this.check();
      },
      check() {
        if (this.pLife <= 0 || this.dLife <= 0) {
          this.dead = true;
          this.msg = this.pLife <= 0 ? 'YOU LOST TO DEMON' : 'YOU DEFEATED DEMON!';
        } else if (this.shells.length === 0) {
          this.reload();
        }
      },
      update(dt, keys, snd, fx) {
        if (this.dead) return;
        if (keys.downEdge && this.turn === 'player') {
          this.shootDealer(snd, fx);
        }
        if (keys.upEdge && this.turn === 'player') {
          this.saw = true;
          this.msg = 'SAW EQUIPPED (2X DMG)';
          if (snd) snd.playClick();
        }

        // 恶魔行动
        if (this.turn === 'dealer' && !this.dead) {
          setTimeout(() => {
            if (this.dead) return;
            const live = this.shells.pop();
            if (live) {
              this.pLife -= 1;
              this.msg = 'DEALER SHOT YOU: LIVE!';
              if (snd) snd.playExplode(true);
            } else {
              this.msg = 'DEALER SHOT YOU: BLANK!';
              if (snd) snd.playClick();
            }
            this.turn = 'player';
            this.check();
          }, 800);
          this.turn = 'waiting';
        }
      },
      render(ctx) {
        ctx.fillStyle = '#0a0505';
        ctx.fillRect(0, 0, 240, 320);

        // 恶魔
        ctx.fillStyle = '#ef4444';
        ctx.beginPath();
        ctx.arc(120, 90, 30, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = '#ffd700';
        ctx.fillRect(108, 85, 6, 6);
        ctx.fillRect(126, 85, 6, 6);

        // 桌面散弹枪
        ctx.fillStyle = '#334155';
        ctx.fillRect(70, 180, 100, 16);
        ctx.fillStyle = '#854d0e';
        ctx.fillRect(50, 184, 25, 12);

        // 状态文字
        ctx.fillStyle = 'rgba(0,0,0,0.8)';
        ctx.fillRect(0, 0, 240, 26);
        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#ef4444';
        ctx.fillText(`DEMON HP:${this.dLife}`, 10, 18);
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(`YOU HP:${this.pLife}`, 160, 18);

        ctx.font = 'bold 11px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.textAlign = 'center';
        ctx.fillText(this.msg, 120, 230);
        ctx.fillStyle = '#94a3b8';
        ctx.fillText(`SHELLS LEFT: ${this.shells.length}`, 120, 250);
        ctx.font = '10px monospace';
        ctx.fillText('UP: Saw (2x) | DN: Shoot Demon | OK: Shoot Self', 120, 275);
        ctx.textAlign = 'left';
      }
    },

    // ==========================================
    // 10. 星火仙女棒与赛博烛火 (Sparkler & Candle)
    // ==========================================
    sparkler: {
      init() {
        this.mode = 'sparkler'; // sparkler / candle
        this.particles = [];
        this.progress = 0;
      },
      onOk(snd) {
        this.mode = (this.mode === 'sparkler') ? 'candle' : 'sparkler';
        if (snd) snd.playCoin();
      },
      update(dt, keys, snd, fx) {
        const k = dt / 16.6;
        const blow = keys.blow;
        const genCount = blow ? 6 : 2;

        for (let i = 0; i < genCount; i++) {
          const angle = Math.random() * Math.PI * 2;
          const spd = (blow ? 3.5 : 1.5) * Math.random();
          this.particles.push({
            x: 120,
            y: this.mode === 'sparkler' ? 140 : 180,
            vx: Math.cos(angle) * spd,
            vy: Math.sin(angle) * spd + 0.3,
            life: 25,
            color: this.mode === 'sparkler' ? '#ffd700' : '#ff5500'
          });
        }

        for (let i = this.particles.length - 1; i >= 0; i--) {
          const p = this.particles[i];
          p.x += p.vx * k;
          p.y += p.vy * k;
          p.life--;
          if (p.life <= 0) this.particles.splice(i, 1);
        }
      },
      render(ctx) {
        ctx.fillStyle = '#06060c';
        ctx.fillRect(0, 0, 240, 320);

        if (this.mode === 'candle') {
          // 蜡烛身
          ctx.fillStyle = '#f8fafc';
          ctx.fillRect(108, 190, 24, 70);
          ctx.fillStyle = '#ff9900';
          ctx.beginPath();
          ctx.arc(120, 180, 8, 0, Math.PI * 2);
          ctx.fill();
        } else {
          // 仙女棒金属棒
          ctx.fillStyle = '#64748b';
          ctx.fillRect(118, 150, 4, 110);
        }

        for (const p of this.particles) {
          ctx.fillStyle = p.color;
          ctx.fillRect(p.x, p.y, 2, 2);
        }

        ctx.font = 'bold 12px monospace';
        ctx.fillStyle = '#ffd700';
        ctx.fillText(this.mode === 'sparkler' ? 'SPARKLER MODE' : 'CANDLE MODE', 10, 24);
        ctx.font = '10px monospace';
        ctx.fillStyle = '#94a3b8';
        ctx.fillText('Press OK: Switch | Press B: Blow Air', 10, 40);
      }
    },

    // ==========================================
    // 11. 世界时钟：跨时区罗盘 (World Timezone)
    // ==========================================
    worldtime: {
      init() {
        this.cities = [
          { name: 'BEIJING', offset: 8 },
          { name: 'LONDON', offset: 0 },
          { name: 'NEW YORK', offset: -5 },
          { name: 'TOKYO', offset: 9 },
          { name: 'SAN FRANCISCO', offset: -8 }
        ];
        this.idx = 0;
      },
      onOk(snd) {
        if (snd) snd.playCoin();
      },
      update(dt, keys, snd) {
        if (keys.upEdge) {
          this.idx = (this.idx - 1 + this.cities.length) % this.cities.length;
          if (snd) snd.playClick();
        }
        if (keys.downEdge) {
          this.idx = (this.idx + 1) % this.cities.length;
          if (snd) snd.playClick();
        }
      },
      render(ctx) {
        ctx.fillStyle = '#060a16';
        ctx.fillRect(0, 0, 240, 320);

        const c = this.cities[this.idx];
        const now = new Date();
        const utcHour = now.getUTCHours();
        const localHour = (utcHour + c.offset + 24) % 24;
        const isDay = localHour >= 6 && localHour < 18;

        // 表盘
        ctx.strokeStyle = isDay ? '#ffd700' : '#38bdf8';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.arc(120, 140, 70, 0, Math.PI * 2);
        ctx.stroke();

        ctx.fillStyle = '#fff';
        ctx.font = 'bold 18px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(c.name, 120, 130);

        ctx.font = 'bold 22px monospace';
        ctx.fillStyle = isDay ? '#ffd700' : '#38bdf8';
        const hStr = String(localHour).padStart(2, '0');
        const mStr = String(now.getUTCMinutes()).padStart(2, '0');
        ctx.fillText(`${hStr}:${mStr}`, 120, 160);

        ctx.font = '11px monospace';
        ctx.fillStyle = '#94a3b8';
        ctx.fillText(isDay ? '☀️ DAYTIME (WORK)' : '🌙 NIGHT (REST)', 120, 230);
        ctx.fillText('UP/DN: Switch City | OK: Align', 120, 260);
        ctx.textAlign = 'left';
      }
    }
  };

  global.GAMES = GAMES;

})(typeof window !== 'undefined' ? window : this);
