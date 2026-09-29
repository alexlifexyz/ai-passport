// simulator/games/flydriver.js — 由 flydriver.html 移植到统一引擎（原版 30FPS 逻辑，60Hz 下隔帧推进）
(function () {
  'use strict';
  Arcade.define({
    id: 'flydriver',
    create(api) {
      let ctx = null;
      const snd = { playDrift() { api.sfx.noise(0.06, { vol: 0.06, f: 2500 }); }, playNearMiss() { api.sfx.tone(800, 0.12, { type: 'sawtooth', vol: 0.08, to: 320 }); }, playBoostPad() { api.sfx.pickup(); }, playNitro() { api.sfx.whoosh(); api.sfx.power(); }, playCrash() { api.sfx.hurt(); api.fx.addShake(6); }, playGameOver() { api.sfx.boom(true); } };
    let playerX = 0.0;
    let playerSpeed = 220.0;
    let targetSpeed = 260.0;
    let steerDir = 0;
    let steerTimer = 0;

    let nitroGauge = 40;
    let nitroActive = false;
    let nitroTimer = 0;

    let shields = 6;
    let maxShields = 6;
    let invincibleTimer = 0;
    let crashFlash = 0;

    let trackCurve = 0.0;
    let targetCurve = 0.0;
    let curveTimer = 0;
    let trackOffset = 0.0;

    let score = 0;
    let distance = 0;
    let nearMissCount = 0;
    let tick = 0;
    let spawnTimer = 0;
    let gameOver = false;

    let feedbackMsg = "ENGINE READY";
    let feedbackTimer = 0;
    let feedbackColor = "#00e5ff";

    let traffic = [];
    let particles = [];

    function calcCoord(trackX, z, curve) {
      let depth = 1.0 - z;
      if (depth < 0) depth = 0;
      if (depth > 1.2) depth = 1.2;
      const depthCurved = depth * depth;
      const y = Math.floor(25.0 + depthCurved * (215.0 - 25.0));

      const curveShift = (1.0 - depth) * curve * 42.0;
      const centerX = 120.0 + curveShift;
      const halfTrackW = 14.0 + depth * 86.0;
      const objX = centerX + trackX * halfTrackW;

      const w = Math.floor(6.0 + depth * 28.0);
      const h = Math.floor(4.0 + depth * 20.0);
      return { x: objX - w / 2, y: y - h / 2, w, h };
    }

    function spawnTraffic() {
      const lanes = [-0.65, -0.25, 0.25, 0.65];
      const laneX = lanes[Math.floor(Math.random() * lanes.length)];
      const roll = Math.random();
      let type = 'SCOUT';
      let speed = 170.0 + Math.random() * 30;

      if (roll < 0.45) {
        type = 'SCOUT';
      } else if (roll < 0.75) {
        type = 'TRUCK';
        speed = 130.0 + Math.random() * 20;
      } else if (roll < 0.90) {
        type = 'BOOST_PAD';
        speed = 0.0;
      } else {
        type = 'LASER_GATE';
        speed = 0.0;
      }

      traffic.push({
        x: laneX,
        z: 1.0,
        speed,
        type,
        nearMissCounted: false
      });
    }

    function spawnParticles(x, y, color, count = 8) {
      for (let i = 0; i < count; i++) {
        const angle = Math.random() * Math.PI * 2;
        const speed = 2 + Math.random() * 4;
        particles.push({
          x, y,
          vx: Math.cos(angle) * speed,
          vy: Math.sin(angle) * speed,
          life: 12,
          color
        });
      }
    }

    function steerLeft() {
      if (gameOver) return;
      playerX -= 0.22;
      if (playerX < -0.85) playerX = -0.85;
      steerDir = -1;
      steerTimer = 5;
      snd.playDrift();
    }

    function steerRight() {
      if (gameOver) return;
      playerX += 0.22;
      if (playerX > 0.85) playerX = 0.85;
      steerDir = 1;
      steerTimer = 5;
      snd.playDrift();
    }

    function triggerNitro() {
      if (gameOver) {
        return;
        return;
      }
      if (nitroGauge >= 40 && !nitroActive) {
        nitroActive = true;
        nitroTimer = (nitroGauge >= 100) ? 90 : 50;
        nitroGauge = 0;
        invincibleTimer = nitroTimer;
        playerSpeed = 420.0;
        snd.playNitro();
        feedbackMsg = "WARP NITRO BOOST!!";
        feedbackColor = "#ffd700";
        feedbackTimer = 22;
      }
    }

    function update() {
      if (!gameOver) {
        tick++;
        if (steerTimer > 0) {
          steerTimer--;
          if (steerTimer <= 0) steerDir = 0;
        }
        if (invincibleTimer > 0) invincibleTimer--;
        if (crashFlash > 0) crashFlash--;
        if (feedbackTimer > 0) feedbackTimer--;

        // 氮气逻辑
        if (nitroActive) {
          nitroTimer--;
          playerSpeed = 420.0;
          if (nitroTimer <= 0) {
            nitroActive = false;
            playerSpeed = targetSpeed;
          }
        } else {
          if (playerSpeed < targetSpeed) playerSpeed += 1.8;
          else if (playerSpeed > targetSpeed + 5) playerSpeed -= 2.5;

          if (tick % 5 === 0 && nitroGauge < 100) nitroGauge++;
        }

        targetSpeed = Math.min(350.0, 260.0 + Math.floor(distance / 800) * 10);
        distance += Math.floor(playerSpeed * 0.035);
        score += Math.floor(playerSpeed * 0.04) * (nitroActive ? 3 : 1);

        trackOffset = (trackOffset + playerSpeed * 0.005) % 1.0;

        // 动态弯道
        curveTimer++;
        if (curveTimer >= 140) {
          curveTimer = 0;
          targetCurve = (Math.floor(Math.random() * 3) - 1) * 0.75;
        }
        trackCurve += (targetCurve - trackCurve) * 0.03;
        playerX -= trackCurve * 0.006;

        // 光流场边缘自居中反射
        if (playerX < -0.75) playerX += 0.015;
        else if (playerX > 0.75) playerX -= 0.015;
        if (playerX < -0.85) playerX = -0.85;
        if (playerX > 0.85) playerX = 0.85;

        // 交通生成
        spawnTimer++;
        const interval = playerSpeed > 360 ? 18 : 26;
        if (spawnTimer >= interval) {
          spawnTimer = 0;
          spawnTraffic();
        }

        // 交通更新与碰撞
        for (let i = traffic.length - 1; i >= 0; i--) {
          const t = traffic[i];
          const relSpeed = Math.max(0.01, (playerSpeed - t.speed) / 2200.0);
          const prevZ = t.z;
          t.z -= relSpeed;

          const inHitZone = (t.z <= 0.18 && t.z >= -0.18) || (prevZ >= 0.0 && t.z <= 0.0);
          if (inHitZone) {
            const dx = Math.abs(playerX - t.x);
            const hitBox = (t.type === 'TRUCK') ? 0.36 : ((t.type === 'LASER_GATE') ? 0.40 : 0.28);

            if (dx <= hitBox) {
              if (t.type === 'BOOST_PAD') {
                playerSpeed = 400.0;
                nitroGauge = Math.min(100, nitroGauge + 25);
                if (shields < maxShields) shields++;
                score += 300;
                snd.playBoostPad();
                feedbackMsg = "BOOST PAD! +1 SHIELD";
                feedbackColor = "#00e5ff";
                feedbackTimer = 16;
                traffic.splice(i, 1);
                continue;
              } else if (nitroActive) {
                score += 600;
                const pos = calcCoord(t.x, t.z, trackCurve);
                spawnParticles(pos.x + pos.w / 2, pos.y + pos.h / 2, '#ffd700', 12);
                snd.playCrash();
                feedbackMsg = "WARP SMASH! +600";
                feedbackColor = "#ffd700";
                feedbackTimer = 18;
                traffic.splice(i, 1);
                continue;
              } else if (invincibleTimer <= 0) {
                shields--;
                crashFlash = 5;
                invincibleTimer = 48; // 48 帧充分保护
                playerSpeed = 140.0;
                snd.playCrash();
                feedbackMsg = "COLLISION! -1 SHIELD";
                feedbackColor = "#ff3333";
                feedbackTimer = 20;

                const pos = calcCoord(t.x, t.z, trackCurve);
                spawnParticles(pos.x + pos.w / 2, pos.y + pos.h / 2, '#ff2255', 10);

                if (shields <= 0) {
                  shields = 0;
                  gameOver = true;
                  snd.playGameOver();
                }
                traffic.splice(i, 1);
                continue;
              }
            } else if (dx <= 0.52 && !t.nearMissCounted && t.type !== 'BOOST_PAD') {
              t.nearMissCounted = true;
              nearMissCount++;
              score += 250;
              nitroGauge = Math.min(100, nitroGauge + 15);
              if (nearMissCount % 2 === 0 && shields < maxShields) {
                shields++;
              }
              snd.playNearMiss();
              feedbackMsg = "NEAR MISS! +15% NITRO";
              feedbackColor = "#33ff66";
              feedbackTimer = 15;
            }
          }

          if (t.z < -0.25) {
            traffic.splice(i, 1);
          }
        }

        // 粒子
        particles.forEach(p => {
          p.x += p.vx;
          p.y += p.vy;
          p.life--;
        });
        particles = particles.filter(p => p.life > 0);
      }

    }

    function render() {
      // 1. 天空
      ctx.fillStyle = crashFlash > 0 ? '#551111' : (nitroActive ? '#180d2b' : '#060814');
      ctx.fillRect(0, 0, 240, 320);

      // 远景地平线
      ctx.fillStyle = '#334466';
      ctx.fillRect(0, 24, 240, 1);

      // 2. 伪 3D 赛道梯形与地面
      const numSegments = 18;
      for (let seg = numSegments - 1; seg >= 0; seg--) {
        const z = seg / numSegments;
        const depth = 1.0 - z;
        const y = Math.floor(25.0 + depth * depth * (215.0 - 25.0));
        const nextY = Math.floor(25.0 + Math.pow(depth + 1.0 / numSegments, 2) * (215.0 - 25.0));
        const segH = Math.max(1, nextY - y + 1);

        const curveShift = (1.0 - depth) * trackCurve * 42.0;
        const cx = Math.floor(120 + curveShift);
        const halfW = Math.floor(14.0 + depth * 86.0);

        const stripeDark = ((Math.floor(depth * 6.0 + trackOffset * 3.0)) % 2 === 0);
        ctx.fillStyle = stripeDark ? (nitroActive ? '#28123b' : '#121424') : (nitroActive ? '#3b1956' : '#191c30');
        ctx.fillRect(cx - halfW, y, halfW * 2, segH);

        // 路肩
        ctx.fillStyle = stripeDark ? '#00e5ff' : '#ff2255';
        const curbW = Math.floor(2.0 + depth * 5.0);
        ctx.fillRect(cx - halfW - curbW, y, curbW, segH);
        ctx.fillRect(cx + halfW, y, curbW, segH);

        // 中心车道线
        if (depth > 0.2 && !stripeDark) {
          ctx.fillStyle = '#ffffff';
          ctx.fillRect(cx - 1, y, 2, segH);
        }

        // 视流飞线
        if (seg % 3 === 0) {
          ctx.fillStyle = nitroActive ? '#ffd700' : '#335588';
          const flowL = cx - halfW - curbW - Math.floor(depth * 18.0);
          const flowR = cx + halfW + curbW + Math.floor(depth * 18.0);
          ctx.fillRect(flowL, y, Math.floor(1 + depth * 3), 1);
          ctx.fillRect(flowR, y, Math.floor(1 + depth * 3), 1);
        }
      }

      // 3. 交通与障碍物
      traffic.forEach(t => {
        const pos = calcCoord(t.x, t.z, trackCurve);
        if (t.type === 'SCOUT') {
          ctx.fillStyle = '#118844';
          ctx.fillRect(pos.x, pos.y, pos.w, pos.h);
          ctx.fillStyle = '#22dd66';
          ctx.fillRect(pos.x + 1, pos.y + 1, pos.w - 2, pos.h - 2);
          if (pos.w > 8) {
            ctx.fillStyle = '#00ff88';
            ctx.fillRect(pos.x + 2, pos.y + pos.h - 2, 2, 2);
            ctx.fillRect(pos.x + pos.w - 4, pos.y + pos.h - 2, 2, 2);
          }
        } else if (t.type === 'TRUCK') {
          ctx.fillStyle = '#881122';
          ctx.fillRect(pos.x, pos.y - 2, pos.w, pos.h + 2);
          ctx.fillStyle = '#ff2244';
          ctx.fillRect(pos.x + 2, pos.y, pos.w - 4, pos.h - 2);
          if (pos.w > 10) {
            ctx.fillStyle = '#ffaa00';
            ctx.fillRect(pos.x + 2, pos.y + pos.h - 2, 3, 2);
            ctx.fillRect(pos.x + pos.w - 5, pos.y + pos.h - 2, 3, 2);
          }
        } else if (t.type === 'BOOST_PAD') {
          ctx.fillStyle = '#d0a000';
          ctx.fillRect(pos.x, pos.y, pos.w, Math.floor(pos.h / 2) + 1);
          ctx.fillStyle = '#ffd700';
          ctx.fillRect(pos.x + 1, pos.y + 1, pos.w - 2, Math.floor(pos.h / 2) - 1);
        } else if (t.type === 'LASER_GATE') {
          ctx.fillStyle = '#555577';
          ctx.fillRect(pos.x, pos.y - 4, 3, pos.h + 6);
          ctx.fillRect(pos.x + pos.w - 3, pos.y - 4, 3, pos.h + 6);
          ctx.fillStyle = '#ff0044';
          ctx.fillRect(pos.x + 3, pos.y, pos.w - 6, 2);
        }
      });

      // 4. 粒子
      particles.forEach(p => {
        ctx.fillStyle = p.color;
        ctx.fillRect(Math.floor(p.x), Math.floor(p.y), 3, 3);
      });

      // 5. 玩家赛博果蝇超跑
      if (!gameOver) {
        const pos = calcCoord(playerX, 0.0, trackCurve);
        const pw = 32;
        const ph = 22;
        const px = Math.floor(pos.x - 2);
        const py = Math.floor(pos.y);

        const hide = (invincibleTimer > 0 && (invincibleTimer % 2 === 0));
        if (!hide) {
          // 尾部喷焰
          const flameLen = nitroActive ? 16 : (5 + (tick % 3) * 2);
          ctx.fillStyle = nitroActive ? '#ffd700' : '#ff8800';
          ctx.fillRect(px + 7, py + ph, 4, flameLen);
          ctx.fillRect(px + pw - 11, py + ph, 4, flameLen);

          // 车体
          ctx.fillStyle = '#223048';
          ctx.fillRect(px + 4, py + 2, pw - 8, ph - 2);
          ctx.fillStyle = '#192233';
          ctx.fillRect(px + 2, py + 8, pw - 4, ph - 10);

          // 定风翼 (倾斜)
          const wingDy = (steerDir === -1) ? -2 : ((steerDir === 1) ? 2 : 0);
          ctx.fillStyle = '#00e5ff';
          ctx.fillRect(px, py + 6 + wingDy, 4, 10);
          ctx.fillRect(px + pw - 4, py + 6 - wingDy, 4, 10);

          // 复眼座舱
          ctx.fillStyle = '#00e5ff';
          ctx.fillRect(px + 9, py + 5, 5, 6);
          ctx.fillStyle = '#ff3366';
          ctx.fillRect(px + pw - 14, py + 5, 5, 6);

          // 车尾刹车条
          ctx.fillStyle = '#ff2233';
          ctx.fillRect(px + 8, py + ph - 4, pw - 16, 2);
        }
      }

      // 6. HUD
      ctx.font = '11px monospace';
      ctx.fillStyle = '#ffffff';
      ctx.fillText(`${Math.floor(playerSpeed)} KM/H`, 8, 18);

      ctx.fillStyle = shields >= 2 ? '#33ff66' : '#ff3333';
      let sh = "";
      for (let i = 0; i < maxShields; i++) sh += (i < shields ? "[*]" : "[ ]");
      ctx.fillText(`SHIELD:${sh}`, 88, 18);

      if (nitroActive) {
        ctx.fillStyle = '#ffd700';
        ctx.fillText("WARP 3x!", 174, 18);
      } else if (nitroGauge >= 40) {
        ctx.fillStyle = '#ffd700';
        ctx.fillText(`[OK] ${nitroGauge}%`, 168, 18);
      } else {
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(`NITRO:${nitroGauge}%`, 168, 18);
      }

      // 底部信息
      ctx.textAlign = 'center';
      ctx.font = '10px monospace';
      if (feedbackTimer > 0) {
        ctx.fillStyle = feedbackColor;
        ctx.fillText(feedbackMsg, 120, 288);
      } else {
        ctx.fillStyle = '#00e5ff';
        ctx.fillText(`DIST: ${distance}m | SCORE: ${score}`, 120, 284);
        ctx.fillText("UP: STEER L | DOWN: STEER R | OK: NITRO", 120, 298);
      }

      if (gameOver) {
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(0, 100, 240, 80);
        ctx.fillStyle = '#ff3333';
        ctx.font = 'bold 12px monospace';
        ctx.fillText("CAR DESTROYED!", 120, 135);
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
            if (input.upP) steerLeft();
            if (input.downP) steerRight();
            if (input.okP) triggerNitro();
          }
          half ^= 1;
          if (half) update();
          api.stats.score = score; api.stats.dist = distance; api.stats.nearMiss = nearMissCount;
          api.stats.time = t / 60;
          if (gameOver && !ended) { ended = true; }
          if (ended && ++endT === 45) api.end();
        },
        draw(c) { ctx = c; ctx.save(); render(); ctx.restore(); }
      };
    }
  });
})();
