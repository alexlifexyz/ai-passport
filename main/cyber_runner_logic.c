// main/cyber_runner_logic.c —— 《霓虹疾行：影刃闪现》核心算法与物理状态机实现
#include "cyber_runner_logic.h"
#include <string.h>
#include <math.h>

#define CR_GRAVITY          1150.0f   // 重力加速度 (px/s^2)
#define CR_BASE_SPEED       180.0f    // 初始场景滚动速度 (px/s)
#define CR_MAX_SPEED        320.0f    // 极限奔跑速度

static inline uint32_t cr_rand(cr_game_t *g)
{
    g->rng_state = g->rng_state * 1664525u + 1013904223u;
    return g->rng_state;
}

bool cyber_runner_check_box(float x1, float y1, float w1, float h1,
                            float x2, float y2, float w2, float h2)
{
    return (x1 < x2 + w2) && (x1 + w1 > x2) &&
           (y1 < y2 + h2) && (y1 + h1 > y2);
}

void cyber_runner_emit_particle(cr_game_t *g, cr_part_type_t type,
                                float x, float y, float vx, float vy, uint32_t color)
{
    for (int i = 0; i < CR_MAX_PARTICLES; i++) {
        if (!g->particles[i].active) {
            g->particles[i].active = true;
            g->particles[i].type = type;
            g->particles[i].x = x;
            g->particles[i].y = y;
            g->particles[i].vx = vx;
            g->particles[i].vy = vy;
            g->particles[i].life = 1.0f;
            g->particles[i].decay = (type == CR_PART_SPARK) ? 2.5f : 3.5f;
            g->particles[i].color = color;
            break;
        }
    }
}

static void spawn_hazard_or_item(cr_game_t *g, cr_building_t *b)
{
    // 如果大楼过窄，不生成障碍
    if (b->w < 110.0f || b->is_glass) return;

    uint32_t r = cr_rand(g) % 100;

    // 40% 生成陷阱
    if (r < 40) {
        for (int i = 0; i < CR_MAX_HAZARDS; i++) {
            if (!g->hazards[i].active) {
                g->hazards[i].active = true;
                g->hazards[i].state_tick = 0;
                g->hazards[i].bob_phase = 0.0f;

                uint32_t ht = cr_rand(g) % 4;
                if (ht == 0) {
                    // 低位激光
                    g->hazards[i].type = CR_HAZARD_LASER_LOW;
                    g->hazards[i].w = 12.0f;
                    g->hazards[i].h = 8.0f;
                    g->hazards[i].x = b->x + b->w * 0.55f;
                    g->hazards[i].y = b->y - g->hazards[i].h;
                } else if (ht == 1) {
                    // 高位激光 (留出底部让玩家滑铲钻过)
                    g->hazards[i].type = CR_HAZARD_LASER_HIGH;
                    g->hazards[i].w = 14.0f;
                    g->hazards[i].h = 10.0f;
                    g->hazards[i].x = b->x + b->w * 0.5f;
                    g->hazards[i].y = b->y - 28.0f;
                } else if (ht == 2) {
                    // 全高激光墙 (必须空中闪现虚化穿透)
                    g->hazards[i].type = CR_HAZARD_LASER_WALL;
                    g->hazards[i].w = 8.0f;
                    g->hazards[i].h = 48.0f;
                    g->hazards[i].x = b->x + b->w * 0.6f;
                    g->hazards[i].y = b->y - g->hazards[i].h;
                } else {
                    // 浮游无人机
                    g->hazards[i].type = CR_HAZARD_DRONE;
                    g->hazards[i].w = 18.0f;
                    g->hazards[i].h = 14.0f;
                    g->hazards[i].x = b->x + b->w * 0.5f;
                    g->hazards[i].y = b->y - 36.0f;
                }
                break;
            }
        }
    } else if (r < 65) {
        // 25% 生成排风口喷射器
        for (int i = 0; i < CR_MAX_HAZARDS; i++) {
            if (!g->hazards[i].active) {
                g->hazards[i].active = true;
                g->hazards[i].type = CR_HAZARD_VENT;
                g->hazards[i].w = 20.0f;
                g->hazards[i].h = 6.0f;
                g->hazards[i].x = b->x + b->w * 0.4f;
                g->hazards[i].y = b->y - g->hazards[i].h;
                g->hazards[i].state_tick = 0;
                break;
            }
        }
    } else if (r < 85) {
        // 20% 生成收集道具
        for (int i = 0; i < CR_MAX_ITEMS; i++) {
            if (!g->items[i].active) {
                g->items[i].active = true;
                uint32_t it = cr_rand(g) % 10;
                if (it < 6) {
                    g->items[i].type = CR_ITEM_DATA_GEM;
                } else if (it < 9) {
                    g->items[i].type = CR_ITEM_BATTERY;
                } else {
                    g->items[i].type = CR_ITEM_SHIELD;
                }
                g->items[i].x = b->x + b->w * 0.5f;
                g->items[i].y = b->y - 20.0f;
                g->items[i].bob_phase = 0.0f;
                break;
            }
        }
    }
}

static void recycle_building(cr_game_t *g, int idx)
{
    // 找出当前所有建筑最右侧边界
    float max_right = 0.0f;
    float last_y = 220.0f;
    for (int i = 0; i < CR_MAX_BUILDINGS; i++) {
        if (g->buildings[i].active && (g->buildings[i].x + g->buildings[i].w > max_right)) {
            max_right = g->buildings[i].x + g->buildings[i].w;
            last_y = g->buildings[i].y;
        }
    }

    // 随机跨度 (间隙与宽度)
    float gap = 36.0f + (float)(cr_rand(g) % 36); // 36 ~ 72 px
    float width = 130.0f + (float)(cr_rand(g) % 90); // 130 ~ 220 px
    
    // 高度平滑过渡 (防止出现不可逾越的陡壁)
    float dy = ((float)(cr_rand(g) % 70) - 35.0f);
    float target_y = last_y + dy;
    if (target_y < 190.0f) target_y = 190.0f;
    if (target_y > 255.0f) target_y = 255.0f;

    cr_building_t *b = &g->buildings[idx];
    b->active = true;
    b->x = max_right + gap;
    b->y = target_y;
    b->w = width;
    b->h = (float)CR_SCREEN_H - target_y + 10.0f;
    b->crumble_timer_ms = 0.0f;
    b->crumbled = false;
    b->win_seed = cr_rand(g);

    // 易碎天窗生成概率 (距离 > 120m 后概率 18%)
    if (g->distance_m > 120 && (cr_rand(g) % 100 < 18)) {
        b->is_glass = true;
    } else {
        b->is_glass = false;
    }

    spawn_hazard_or_item(g, b);
}

void cyber_runner_init(cr_game_t *g, uint32_t seed)
{
    memset(g, 0, sizeof(cr_game_t));
    g->rng_state = seed ? seed : 0x20260919;
    g->world_speed = CR_BASE_SPEED;
    g->hp = 3;
    g->max_hp = 3;
    g->blink_charges = 3;
    g->air_jumps_left = 1;
    g->stance = CR_STANCE_RUN;

    // 初始平稳建筑布局
    float start_xs[CR_MAX_BUILDINGS] = { 0.0f, 190.0f, 360.0f, 530.0f, 710.0f };
    float start_ys[CR_MAX_BUILDINGS] = { 230.0f, 220.0f, 240.0f, 215.0f, 230.0f };
    float start_ws[CR_MAX_BUILDINGS] = { 170.0f, 150.0f, 150.0f, 160.0f, 160.0f };

    for (int i = 0; i < CR_MAX_BUILDINGS; i++) {
        g->buildings[i].active = true;
        g->buildings[i].x = start_xs[i];
        g->buildings[i].y = start_ys[i];
        g->buildings[i].w = start_ws[i];
        g->buildings[i].h = (float)CR_SCREEN_H - start_ys[i] + 10.0f;
        g->buildings[i].is_glass = false;
        g->buildings[i].crumbled = false;
        g->buildings[i].win_seed = cr_rand(g);
    }

    // 角色立于第 0 座大楼表面
    g->y = g->buildings[0].y;
    g->vy = 0.0f;

    // 初始化围巾节点
    for (int i = 0; i < CR_MAX_SCARF_NODES; i++) {
        g->scarf[i].x = (float)(CR_PLAYER_X - i * 6);
        g->scarf[i].y = g->y - 20.0f;
    }
}

void cyber_runner_input_up(cr_game_t *g)
{
    if (g->game_over) {
        cyber_runner_init(g, g->tick_count + 1);
        return;
    }

    if (g->stance == CR_STANCE_RUN || g->stance == CR_STANCE_SLIDE) {
        // 地面起跳
        g->stance = CR_STANCE_JUMP;
        g->vy = -440.0f;
        g->air_jumps_left = 1;
        g->pending_sound = CR_SND_JUMP;
        cyber_runner_emit_particle(g, CR_PART_SPARK, CR_PLAYER_X + 6, g->y, -40.0f, 10.0f, 0x06B6D4);
    } else if (g->air_jumps_left > 0) {
        // 空中二段喷气跳跃
        g->air_jumps_left = 0;
        g->stance = CR_STANCE_DOUBLE_JUMP;
        g->vy = -390.0f;
        g->pending_sound = CR_SND_AIR_BOOST;
        // 向下喷涌离子粒子
        for (int i = 0; i < 4; i++) {
            cyber_runner_emit_particle(g, CR_PART_NEON_BURST,
                                       CR_PLAYER_X + 8, g->y - 4,
                                       ((float)(cr_rand(g) % 40) - 20.0f),
                                       60.0f + (float)(cr_rand(g) % 40),
                                       0x22D3EE);
        }
    }
}

void cyber_runner_input_down(cr_game_t *g)
{
    if (g->game_over) return;

    if (g->stance == CR_STANCE_RUN) {
        // 地面极速滑铲
        g->stance = CR_STANCE_SLIDE;
        g->stance_timer_ms = 420;
        g->pending_sound = CR_SND_SLIDE;
        cyber_runner_emit_particle(g, CR_PART_SPARK, CR_PLAYER_X, g->y, -70.0f, -20.0f, 0xF59E0B);
    } else if (g->stance == CR_STANCE_JUMP || g->stance == CR_STANCE_DOUBLE_JUMP ||
               g->stance == CR_STANCE_FALL || g->stance == CR_STANCE_BLINK) {
        // 空中急降俯冲
        g->stance = CR_STANCE_DIVE;
        g->vy = 650.0f;
    }
}

void cyber_runner_input_ok(cr_game_t *g)
{
    if (g->game_over) {
        cyber_runner_init(g, g->tick_count + 1);
        return;
    }

    // 空中或地面幽灵闪现
    if (g->blink_charges > 0 && !g->phase_shift) {
        g->blink_charges--;
        g->phase_shift = true;
        g->phase_timer_ms = 180;
        g->stance = CR_STANCE_BLINK;
        g->vy = 0.0f; // 重力清零，平飞闪现
        g->render_x_offset = 36.0f;
        g->pending_sound = CR_SND_BLINK;

        // 生成 3 道虚化残影
        for (int i = 0; i < CR_MAX_AFTERIMAGES; i++) {
            g->afterimages[i].x = (float)CR_PLAYER_X - (float)(i + 1) * 12.0f;
            g->afterimages[i].y = g->y;
            g->afterimages[i].alpha = 0.8f - (float)i * 0.25f;
            g->afterimages[i].color = (i % 2 == 0) ? 0x06B6D4 : 0xEC4899;
        }

        // 闪现爆发粒子
        for (int i = 0; i < 6; i++) {
            cyber_runner_emit_particle(g, CR_PART_NEON_BURST,
                                       CR_PLAYER_X + 10, g->y - 14,
                                       ((float)(cr_rand(g) % 80) - 40.0f),
                                       ((float)(cr_rand(g) % 80) - 40.0f),
                                       0xEC4899);
        }
    }
}

static void apply_damage(cr_game_t *g)
{
    if (g->invuln_timer_ms > 0 || g->phase_shift) return;

    if (g->has_shield) {
        // 护盾抵挡
        g->has_shield = false;
        g->invuln_timer_ms = 1200;
        g->pending_sound = CR_SND_HURT;
        cyber_runner_emit_particle(g, CR_PART_NEON_BURST, CR_PLAYER_X + 8, g->y - 15, 0, 0, 0x38BDF8);
    } else {
        // 受到伤害扣除生命
        if (g->hp > 0) g->hp--;
        g->combo_count = 0;
        g->invuln_timer_ms = 1500;
        g->pending_sound = CR_SND_HURT;

        if (g->hp <= 0) {
            g->game_over = true;
            g->pending_sound = CR_SND_GAMEOVER;
        }
    }
}

void cyber_runner_step(cr_game_t *g, uint32_t dt_ms)
{
    if (g->game_over) return;

    float dt = (float)dt_ms / 1000.0f;
    g->tick_count++;

    // 1. 速度增长与行进里程
    if (g->world_speed < CR_MAX_SPEED) {
        g->world_speed += 0.8f * dt;
    }
    float dx = g->world_speed * dt;
    g->distance_m += (uint32_t)(dx * 0.12f);
    g->score += (uint32_t)(dx * 0.2f);

    // 视差滚动偏移累加
    g->bg_far_scroll_px += dx * 0.15f;
    if (g->bg_far_scroll_px >= 240.0f) g->bg_far_scroll_px -= 240.0f;
    g->bg_mid_scroll_px += dx * 0.45f;
    if (g->bg_mid_scroll_px >= 240.0f) g->bg_mid_scroll_px -= 240.0f;

    // 2. 状态计时器递减
    if (g->invuln_timer_ms > dt_ms) {
        g->invuln_timer_ms -= dt_ms;
    } else {
        g->invuln_timer_ms = 0;
    }

    if (g->combo_timer_ms > dt_ms) {
        g->combo_timer_ms -= dt_ms;
    } else {
        g->combo_timer_ms = 0;
        g->combo_count = 0;
    }

    // 闪现充能自动缓慢回复 (在地面跑动时每 4.5s 回满一格)
    if (g->blink_charges < 3 && (g->stance == CR_STANCE_RUN || g->stance == CR_STANCE_SLIDE)) {
        g->blink_recharge_ms += (float)dt_ms;
        if (g->blink_recharge_ms >= 4500.0f) {
            g->blink_charges++;
            g->blink_recharge_ms = 0.0f;
        }
    }

    // 虚化闪现状态更新
    if (g->phase_shift) {
        g->render_x_offset *= 0.88f;
        if (g->phase_timer_ms > dt_ms) {
            g->phase_timer_ms -= dt_ms;
        } else {
            g->phase_timer_ms = 0;
            g->phase_shift = false;
            if (g->stance == CR_STANCE_BLINK) {
                g->stance = CR_STANCE_FALL;
            }
        }
    }

    // 滑铲计时
    if (g->stance == CR_STANCE_SLIDE) {
        if (g->stance_timer_ms > dt_ms) {
            g->stance_timer_ms -= dt_ms;
            // 持续溅射火花
            if (g->tick_count % 3 == 0) {
                cyber_runner_emit_particle(g, CR_PART_SPARK, CR_PLAYER_X + 2, g->y,
                                           -60.0f - (float)(cr_rand(g) % 30),
                                           -10.0f - (float)(cr_rand(g) % 20),
                                           0xF59E0B);
            }
        } else {
            g->stance = CR_STANCE_RUN;
        }
    }

    // 3. 玩家垂直物理计算
    float prev_y = g->y;
    if (!g->phase_shift && g->stance != CR_STANCE_RUN && g->stance != CR_STANCE_SLIDE) {
        g->vy += CR_GRAVITY * dt;
        g->y += g->vy * dt;
    }

    // 4. 建筑滚动与脚下碰撞检测
    bool on_ground = false;
    float current_roof_y = 999.0f;

    for (int i = 0; i < CR_MAX_BUILDINGS; i++) {
        cr_building_t *b = &g->buildings[i];
        if (!b->active) continue;

        b->x -= dx;

        // 踏在楼顶范围检测 (X 范围在主角脚底)
        float foot_x1 = (float)CR_PLAYER_X + 2.0f;
        float foot_x2 = (float)CR_PLAYER_X + 14.0f;

        if (b->x <= foot_x2 && (b->x + b->w) >= foot_x1 && !b->crumbled) {
            // 在此大楼上方或恰好踩在上面
            if (prev_y <= b->y + 4.0f && g->y >= b->y - 1.0f) {
                on_ground = true;
                current_roof_y = b->y;

                // 易碎天窗触发
                if (b->is_glass) {
                    b->crumble_timer_ms += (float)dt_ms;
                    if (b->crumble_timer_ms >= 350.0f) {
                        b->crumbled = true;
                        // 碎裂粒子
                        for (int p = 0; p < 8; p++) {
                            cyber_runner_emit_particle(g, CR_PART_NEON_BURST,
                                                       b->x + (float)(cr_rand(g) % (int)b->w),
                                                       b->y,
                                                       ((float)(cr_rand(g) % 60) - 30.0f),
                                                       40.0f + (float)(cr_rand(g) % 40),
                                                       0x38BDF8);
                        }
                    }
                }
            }
        }

        // 建筑移出屏幕左侧回收
        if (b->x + b->w < -20.0f) {
            recycle_building(g, i);
        }
    }

    // 地面着陆逻辑
    if (on_ground) {
        if (g->stance == CR_STANCE_DIVE) {
            // 俯冲砸地
            g->pending_sound = CR_SND_DIVE_SLAM;
            for (int p = 0; p < 6; p++) {
                cyber_runner_emit_particle(g, CR_PART_SPARK, CR_PLAYER_X + 8, current_roof_y,
                                           ((float)(cr_rand(g) % 120) - 60.0f),
                                           -30.0f - (float)(cr_rand(g) % 30),
                                           0xEC4899);
            }
        }
        g->y = current_roof_y;
        g->vy = 0.0f;
        g->air_jumps_left = 1;
        if (g->stance != CR_STANCE_SLIDE) {
            g->stance = CR_STANCE_RUN;
        }
    } else {
        // 空中踏空脱离
        if (g->stance == CR_STANCE_RUN || g->stance == CR_STANCE_SLIDE) {
            g->stance = CR_STANCE_FALL;
        }
    }

    // 坠入深渊
    if (g->y > (float)CR_SCREEN_H + 10.0f) {
        g->hp = 0;
        g->game_over = true;
        g->pending_sound = CR_SND_GAMEOVER;
        return;
    }

    // 5. 陷阱机关位移与碰撞检测
    float px = (float)CR_PLAYER_X + g->render_x_offset;
    float pw = (float)CR_PLAYER_W;
    float ph = (g->stance == CR_STANCE_SLIDE) ? (float)CR_SLIDE_H : (float)CR_PLAYER_H;
    float py = g->y - ph;

    for (int i = 0; i < CR_MAX_HAZARDS; i++) {
        cr_hazard_t *h = &g->hazards[i];
        if (!h->active) continue;

        h->x -= dx;
        h->state_tick++;

        // 无人机上下浮动
        if (h->type == CR_HAZARD_DRONE) {
            h->bob_phase += dt * 3.5f;
            h->y += sinf(h->bob_phase) * 0.8f;
        }

        // 移出屏幕回收
        if (h->x + h->w < -20.0f) {
            h->active = false;
            continue;
        }

        // 碰撞判断
        if (cyber_runner_check_box(px, py, pw, ph, h->x, h->y, h->w, h->h)) {
            if (h->type == CR_HAZARD_VENT) {
                // 超导排风口弹射
                g->vy = -560.0f;
                g->stance = CR_STANCE_JUMP;
                g->air_jumps_left = 1;
                g->pending_sound = CR_SND_VENT_BOOST;
                for (int p = 0; p < 5; p++) {
                    cyber_runner_emit_particle(g, CR_PART_VENT_STEAM,
                                               h->x + 10.0f, h->y,
                                               ((float)(cr_rand(g) % 30) - 15.0f),
                                               -90.0f - (float)(cr_rand(g) % 50),
                                               0x06B6D4);
                }
            } else if (h->type == CR_HAZARD_DRONE) {
                if (g->phase_shift) {
                    // 幽灵闪现穿爆无人机！
                    h->active = false;
                    g->score += 150 * (1 + g->combo_count);
                    g->combo_count++;
                    g->combo_timer_ms = 2500;
                    g->pending_sound = CR_SND_DRONE_POP;
                    for (int p = 0; p < 8; p++) {
                        cyber_runner_emit_particle(g, CR_PART_EXPLOSION,
                                                   h->x + 8.0f, h->y + 6.0f,
                                                   ((float)(cr_rand(g) % 100) - 50.0f),
                                                   ((float)(cr_rand(g) % 100) - 50.0f),
                                                   0xF43F5E);
                    }
                } else {
                    apply_damage(g);
                }
            } else if (h->type == CR_HAZARD_LASER_WALL ||
                       h->type == CR_HAZARD_LASER_LOW ||
                       h->type == CR_HAZARD_LASER_HIGH) {
                if (g->phase_shift) {
                    // 闪现虚化穿透激光
                    g->score += 50;
                    g->combo_count++;
                    g->combo_timer_ms = 2000;
                } else {
                    apply_damage(g);
                }
            }
        }
    }

    // 6. 收集物位移与拾取
    for (int i = 0; i < CR_MAX_ITEMS; i++) {
        cr_item_t *it = &g->items[i];
        if (!it->active) continue;

        it->x -= dx;
        it->bob_phase += dt * 4.0f;
        float draw_y = it->y + sinf(it->bob_phase) * 3.0f;

        if (it->x < -20.0f) {
            it->active = false;
            continue;
        }

        if (cyber_runner_check_box(px, py, pw, ph, it->x - 6.0f, draw_y - 6.0f, 14.0f, 14.0f)) {
            it->active = false;
            g->pending_sound = CR_SND_GEM;
            if (it->type == CR_ITEM_DATA_GEM) {
                g->score += 50 * (1 + g->combo_count);
                g->combo_count++;
                g->combo_timer_ms = 2500;
                cyber_runner_emit_particle(g, CR_PART_NEON_BURST, it->x, draw_y, 0, -20, 0x06B6D4);
            } else if (it->type == CR_ITEM_BATTERY) {
                g->blink_charges = 3;
                g->score += 100;
                cyber_runner_emit_particle(g, CR_PART_NEON_BURST, it->x, draw_y, 0, -30, 0x10B981);
            } else if (it->type == CR_ITEM_SHIELD) {
                g->has_shield = true;
                g->score += 150;
                cyber_runner_emit_particle(g, CR_PART_NEON_BURST, it->x, draw_y, 0, -30, 0x38BDF8);
            }
        }
    }

    // 7. 围巾动态追随物理模拟 (飘逸流光尾迹)
    float neck_x = px + 4.0f;
    float neck_y = py + 7.0f;
    g->scarf[0].x = neck_x;
    g->scarf[0].y = neck_y;

    for (int i = 1; i < CR_MAX_SCARF_NODES; i++) {
        float target_x = g->scarf[i - 1].x - 5.5f;
        float wave = sinf((float)g->tick_count * 0.25f + (float)i * 0.8f) * 2.2f;
        float target_y = g->scarf[i - 1].y + wave + (g->vy * 0.015f);

        g->scarf[i].x += (target_x - g->scarf[i].x) * 0.65f;
        g->scarf[i].y += (target_y - g->scarf[i].y) * 0.55f;
    }

    // 8. 残影衰减
    for (int i = 0; i < CR_MAX_AFTERIMAGES; i++) {
        if (g->afterimages[i].alpha > 0.0f) {
            g->afterimages[i].alpha -= dt * 4.0f;
            if (g->afterimages[i].alpha < 0.0f) g->afterimages[i].alpha = 0.0f;
        }
    }

    // 9. 粒子生命周期
    for (int i = 0; i < CR_MAX_PARTICLES; i++) {
        if (g->particles[i].active) {
            g->particles[i].x += g->particles[i].vx * dt;
            g->particles[i].y += g->particles[i].vy * dt;
            if (g->particles[i].type == CR_PART_SPARK) {
                g->particles[i].vy += 600.0f * dt; // 火花受重力
            }
            g->particles[i].life -= g->particles[i].decay * dt;
            if (g->particles[i].life <= 0.0f) {
                g->particles[i].active = false;
            }
        }
    }
}
