// main/geartrooper_logic.c —— 《齿轮骑兵：蒸汽过载》核心算法与物理状态机实现
#include "geartrooper_logic.h"
#include <string.h>

#define GRAVITY             980.0f
#define JUMP_IMPULSE        380.0f
#define PLUNGE_IMPULSE      480.0f
#define SLIDE_DURATION_MS   360
#define LANCE_DURATION_MS   180
#define INVULN_DURATION_MS  1000
#define OVERDRIVE_TIME_MS   6000

// 轻量 XORShift 伪随机数
static uint32_t gt_rand(gt_game_t *game)
{
    if (game->rng_state == 0) game->rng_state = 0x87654321;
    uint32_t x = game->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng_state = x;
    return x;
}

// AABB 碰撞检测矩形相交
bool geartrooper_check_collision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2)
{
    return (x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2);
}

// 激发粒子 (零堆分配，循环对象池)
void geartrooper_emit_particle(gt_game_t *game, gt_part_type_t type, float x, float y, float vx, float vy, uint32_t color)
{
    for (int i = 0; i < GT_MAX_PARTICLES; i++) {
        if (!game->particles[i].active) {
            game->particles[i].active = true;
            game->particles[i].type = type;
            game->particles[i].x = x;
            game->particles[i].y = y;
            game->particles[i].vx = vx;
            game->particles[i].vy = vy;
            game->particles[i].life = 1.0f;
            game->particles[i].decay = (type == PART_STEAM) ? 0.035f : 0.06f;
            game->particles[i].color = color;
            break;
        }
    }
}

// 生成敌兵
void geartrooper_spawn_enemy(gt_game_t *game, gt_enemy_type_t type, float x, float y)
{
    for (int i = 0; i < GT_MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) {
            game->enemies[i].active = true;
            game->enemies[i].type = type;
            game->enemies[i].x = x;
            game->enemies[i].y = y;
            game->enemies[i].anim_tick = 0;

            if (type == ENEMY_SPIDER) {
                game->enemies[i].vx = -120.0f;
                game->enemies[i].vy = 0.0f;
                game->enemies[i].hp = 1;
                game->enemies[i].max_hp = 1;
            } else if (type == ENEMY_FALCON) {
                game->enemies[i].vx = -140.0f;
                game->enemies[i].vy = 0.0f;
                game->enemies[i].hp = 2;
                game->enemies[i].max_hp = 2;
            } else if (type == ENEMY_GOLEM) {
                game->enemies[i].vx = -70.0f;
                game->enemies[i].vy = 0.0f;
                game->enemies[i].hp = 4;
                game->enemies[i].max_hp = 4;
            }
            break;
        }
    }
}

// 初始化游戏状态
void geartrooper_init(gt_game_t *game, uint32_t seed)
{
    if (!game) return;
    memset(game, 0, sizeof(gt_game_t));
    game->rng_state = (seed == 0) ? 0xABCDEF12 : seed;
    game->y = GT_GROUND_Y;
    game->vy = 0.0f;
    game->stance = STANCE_RUN;
    game->gear_rpm = 120;
    game->steam_psi = 0;
    game->hp = 5;
    game->max_hp = 5;
    game->lance_reach_px = 38;

    // 初始化地面咬合齿轮组
    float base_x[GT_MAX_GEARS] = {30.0f, 90.0f, 150.0f, 210.0f};
    float base_y[GT_MAX_GEARS] = {265.0f, 275.0f, 266.0f, 276.0f};
    float base_r[GT_MAX_GEARS] = {36.0f, 28.0f, 34.0f, 30.0f};
    float base_spd[GT_MAX_GEARS] = {180.0f, -230.0f, 190.0f, -210.0f};
    int base_teeth[GT_MAX_GEARS] = {12, 10, 12, 10};

    for (int i = 0; i < GT_MAX_GEARS; i++) {
        game->gears[i].x = base_x[i];
        game->gears[i].y = base_y[i];
        game->gears[i].radius = base_r[i];
        game->gears[i].angle_deg = (float)(i * 45);
        game->gears[i].speed_deg = base_spd[i];
        game->gears[i].teeth_count = base_teeth[i];
    }
}

// UP 键：地面起跳，空中斜下刺
void geartrooper_input_up(gt_game_t *game)
{
    if (!game || game->game_over) return;

    if (game->y >= GT_GROUND_Y) {
        // 地面跃马
        game->vy = -JUMP_IMPULSE;
        game->stance = STANCE_JUMP;
        game->pending_sound = GT_SND_JUMP;
        // 蹄底激发电浆火花
        for (int i = 0; i < 4; i++) {
            geartrooper_emit_particle(game, PART_SPARK, GT_HORSE_X + 8, GT_GROUND_Y + 12, -40.0f + (gt_rand(game) % 80), -60.0f - (gt_rand(game) % 60), 0xF59E0B);
        }
    } else {
        // 空中重碾下刺
        if (game->stance != STANCE_PLUNGE) {
            game->vy = PLUNGE_IMPULSE;
            game->stance = STANCE_PLUNGE;
            game->lance_active = true;
            game->lance_timer_ms = 0;
            game->pending_sound = GT_SND_LANCE;
        }
    }
}

// DOWN 键：俯身齿轮滑铲
void geartrooper_input_down(gt_game_t *game)
{
    if (!game || game->game_over) return;

    if (game->y >= GT_GROUND_Y) {
        game->stance = STANCE_SLIDE;
        game->stance_timer_ms = 0;
        game->pending_sound = GT_SND_SLIDE;
        // 滑铲摩擦火花
        for (int i = 0; i < 5; i++) {
            geartrooper_emit_particle(game, PART_SPARK, GT_HORSE_X + 16, GT_GROUND_Y + 14, -80.0f - (gt_rand(game) % 60), -20.0f - (gt_rand(game) % 40), 0xFBBF24);
        }
    }
}

// OK 键：向前刺出旋转骑枪；满压力激活过载
void geartrooper_input_ok(gt_game_t *game)
{
    if (!game) return;

    if (game->game_over) {
        geartrooper_init(game, game->rng_state + 1);
        game->pending_sound = GT_SND_JUMP;
        return;
    }

    // 若满蒸汽压力且尚未过载：激活过载冲锋
    if (game->steam_psi >= 100 && !game->overdrive_active) {
        game->overdrive_active = true;
        game->overdrive_ms = OVERDRIVE_TIME_MS;
        game->steam_psi = 0;
        game->pending_sound = GT_SND_OVERDRIVE;
        return;
    }

    // 骑枪突刺
    game->lance_active = true;
    game->lance_timer_ms = 0;
    game->pending_sound = GT_SND_LANCE;
}

// 核心物理与帧步进
void geartrooper_step(gt_game_t *game, uint32_t dt_ms)
{
    if (!game || game->game_over) return;

    float dt = (float)dt_ms / 1000.0f;
    game->tick_count++;
    game->distance_m += (game->gear_rpm * dt_ms) / 1000;

    // 1. 无敌时间与过载倒计时
    if (game->invuln_timer_ms > dt_ms) {
        game->invuln_timer_ms -= dt_ms;
    } else {
        game->invuln_timer_ms = 0;
    }

    if (game->overdrive_active) {
        if (game->overdrive_ms > dt_ms) {
            game->overdrive_ms -= dt_ms;
        } else {
            game->overdrive_active = false;
            game->overdrive_ms = 0;
        }
    }

    // 2. 齿轮旋转推进
    for (int i = 0; i < GT_MAX_GEARS; i++) {
        game->gears[i].angle_deg += game->gears[i].speed_deg * dt;
        if (game->gears[i].angle_deg >= 360.0f) game->gears[i].angle_deg -= 360.0f;
        if (game->gears[i].angle_deg < 0.0f) game->gears[i].angle_deg += 360.0f;
    }

    // 3. 竖直物理 (重力与跳跃)
    if (game->y < GT_GROUND_Y || game->vy != 0.0f) {
        game->vy += GRAVITY * dt;
        game->y += game->vy * dt;
        if (game->y >= GT_GROUND_Y) {
            game->y = GT_GROUND_Y;
            game->vy = 0.0f;
            if (game->stance == STANCE_JUMP || game->stance == STANCE_PLUNGE) {
                game->stance = STANCE_RUN;
                // 着地蒸汽微爆
                geartrooper_emit_particle(game, PART_STEAM, GT_HORSE_X + 10, GT_GROUND_Y + 10, -20.0f, -30.0f, 0xE2E8F0);
            }
        }
    }

    // 4. 滑铲动作计时
    if (game->stance == STANCE_SLIDE) {
        game->stance_timer_ms += dt_ms;
        if (game->stance_timer_ms >= SLIDE_DURATION_MS) {
            game->stance = STANCE_RUN;
        }
    }

    // 5. 骑枪突刺计时
    if (game->lance_active) {
        game->lance_timer_ms += dt_ms;
        if (game->lance_timer_ms >= LANCE_DURATION_MS) {
            game->lance_active = false;
        }
    }

    // 6. 排气管蒸汽飘散
    if (game->tick_count % 3 == 0) {
        float sx = GT_HORSE_X - 10.0f;
        float sy = game->y - 18.0f;
        geartrooper_emit_particle(game, PART_STEAM, sx, sy, -30.0f - (gt_rand(game) % 20), -40.0f - (gt_rand(game) % 30), 0xF8FAFC);
    }

    // 7. 敌兵生成
    game->enemy_spawn_timer_ms += dt_ms;
    if (game->enemy_spawn_timer_ms >= 1400) {
        game->enemy_spawn_timer_ms = 0;
        uint32_t r = gt_rand(game) % 100;
        if (r < 45) {
            // 生成地面发条蜘蛛
            geartrooper_spawn_enemy(game, ENEMY_SPIDER, GT_SCREEN_W + 10, GT_GROUND_Y + 4);
        } else if (r < 75) {
            // 生成空中飞隼
            geartrooper_spawn_enemy(game, ENEMY_FALCON, GT_SCREEN_W + 10, GT_GROUND_Y - 45 - (gt_rand(game) % 30));
        } else {
            // 生成重型蒸汽魔偶
            geartrooper_spawn_enemy(game, ENEMY_GOLEM, GT_SCREEN_W + 10, GT_GROUND_Y - 8);
        }
    }

    // 8. 骑兵碰撞盒子定义 (宽32，高28)
    float px = GT_HORSE_X;
    float py = (game->stance == STANCE_SLIDE) ? game->y + 6 : game->y - 18;
    float pw = 32.0f;
    float ph = (game->stance == STANCE_SLIDE) ? 14.0f : 28.0f;

    // 骑枪判定盒子
    float lx = px + pw;
    float ly = py + 4;
    float lw = (float)game->lance_reach_px;
    float lh = 12.0f;
    if (game->overdrive_active) { lw *= 1.5f; }

    // 9. 更新敌兵与碰撞判定
    for (int i = 0; i < GT_MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) continue;
        gt_enemy_t *e = &game->enemies[i];

        e->x += e->vx * dt;
        e->anim_tick++;

        // 飞隼正弦波微幅起伏
        if (e->type == ENEMY_FALCON) {
            e->y += (float)((e->anim_tick % 20 < 10) ? 0.8f : -0.8f);
        }

        // 移出屏幕左侧销毁
        if (e->x < -30) {
            e->active = false;
            continue;
        }

        float ew = (e->type == ENEMY_GOLEM) ? 28.0f : (e->type == ENEMY_FALCON ? 22.0f : 20.0f);
        float eh = (e->type == ENEMY_GOLEM) ? 34.0f : (e->type == ENEMY_FALCON ? 18.0f : 16.0f);

        // A. 骑枪命中敌兵
        if (game->lance_active && geartrooper_check_collision(lx, ly, lw, lh, e->x, e->y, ew, eh)) {
            e->hp--;
            game->pending_sound = GT_SND_HIT;
            // 金属火花
            for (int k = 0; k < 6; k++) {
                geartrooper_emit_particle(game, PART_SPARK, e->x + ew / 2, e->y + eh / 2, 40.0f - (gt_rand(game) % 80), -50.0f - (gt_rand(game) % 60), 0xF59E0B);
            }
            if (e->hp <= 0) {
                e->active = false;
                game->combo_count++;
                uint32_t add = (e->type == ENEMY_GOLEM ? 300 : 100) * (game->overdrive_active ? 2 : 1);
                game->score += add * game->combo_count;
                // 蒸汽充压
                game->steam_psi += 15;
                if (game->steam_psi > 100) game->steam_psi = 100;
            }
            continue;
        }

        // B. 滑铲碾碎发条蜘蛛
        if (game->stance == STANCE_SLIDE && e->type == ENEMY_SPIDER) {
            if (geartrooper_check_collision(px, py, pw, ph, e->x, e->y, ew, eh)) {
                e->active = false;
                game->score += 150;
                game->steam_psi += 12;
                if (game->steam_psi > 100) game->steam_psi = 100;
                game->pending_sound = GT_SND_HIT;
                for (int k = 0; k < 5; k++) {
                    geartrooper_emit_particle(game, PART_SPARK, e->x + 8, e->y + 8, -30.0f, -40.0f - (gt_rand(game) % 40), 0xEF4444);
                }
                continue;
            }
        }

        // C. 撞击玩家本体
        if (geartrooper_check_collision(px, py, pw, ph, e->x, e->y, ew, eh)) {
            if (game->overdrive_active) {
                // 过载无敌状态直接撞死敌兵！
                e->active = false;
                game->score += 200;
                game->pending_sound = GT_SND_HIT;
                for (int k = 0; k < 8; k++) {
                    geartrooper_emit_particle(game, PART_SPARK, e->x, e->y, (float)(gt_rand(game) % 100 - 50), -60.0f, 0x38BDF8);
                }
            } else if (game->invuln_timer_ms == 0) {
                // 受到伤害
                game->hp--;
                game->invuln_timer_ms = INVULN_DURATION_MS;
                game->combo_count = 0;
                game->pending_sound = GT_SND_HURT;
                if (game->hp <= 0) {
                    game->hp = 0;
                    game->game_over = true;
                    game->pending_sound = GT_SND_GAMEOVER;
                }
            }
        }
    }

    // 10. 更新粒子
    for (int i = 0; i < GT_MAX_PARTICLES; i++) {
        if (!game->particles[i].active) continue;
        gt_particle_t *p = &game->particles[i];
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        if (p->type == PART_SPARK) p->vy += 400.0f * dt; // 火花下坠
        p->life -= p->decay;
        if (p->life <= 0.0f) p->active = false;
    }
}
