#include "thunder_logic.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void thunder_init(thunder_game_t *g)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));

    g->player_w = 30;
    g->player_h = 28;
    g->player_x = (SCREEN_W - g->player_w) / 2.0f;
    g->player_y = 260.0f;
    g->player_hp = 6;
    g->player_max_hp = 6;
    g->bombs = 4;
    g->weapon_level = 1;
    g->score = 0;
    g->game_over = false;

    // 初始化星空粒子
    for (int i = 0; i < THUNDER_MAX_STARS; i++) {
        g->stars[i].x = (float)(rand() % SCREEN_W);
        g->stars[i].y = (float)(rand() % SCREEN_H);
        g->stars[i].speed = 0.8f + (rand() % 100) / 40.0f; // 0.8 ~ 3.3
        g->stars[i].size = (rand() % 10 > 7) ? 2 : 1;
        g->stars[i].color = (rand() % 2 == 0) ? 0x00E5FF : 0xFFFFFF;
    }
}

void thunder_move_left(thunder_game_t *g)
{
    if (!g || g->game_over) return;
    g->player_x -= 16.0f;
    if (g->player_x < 4.0f) g->player_x = 4.0f;
}

void thunder_move_right(thunder_game_t *g)
{
    if (!g || g->game_over) return;
    g->player_x += 16.0f;
    if (g->player_x > SCREEN_W - g->player_w - 4.0f) {
        g->player_x = SCREEN_W - g->player_w - 4.0f;
    }
}

static void create_explosion(thunder_game_t *g, float x, float y, uint32_t color, int count)
{
    for (int i = 0; i < count; i++) {
        for (int p = 0; p < THUNDER_MAX_PARTICLES; p++) {
            if (g->particles[p].life <= 0) {
                float angle = (float)(rand() % 628) / 100.0f;
                float spd = 1.0f + (rand() % 30) / 10.0f;
                g->particles[p].x = x;
                g->particles[p].y = y;
                g->particles[p].vx = cosf(angle) * spd;
                g->particles[p].vy = sinf(angle) * spd;
                g->particles[p].life = 10 + (rand() % 10);
                g->particles[p].max_life = 20;
                g->particles[p].size = (rand() % 2 == 0) ? 2 : 3;
                g->particles[p].color = (i % 3 == 0) ? 0xFFFFFF : ((i % 2 == 0) ? 0xFFEA00 : color);
                break;
            }
        }
    }
}

bool thunder_use_bomb(thunder_game_t *g)
{
    if (!g || g->game_over || g->bombs <= 0) return false;
    g->bombs--;
    g->bomb_triggered = true;
    g->screen_shake = 16;
    g->snd_bomb = true;

    // 清空所有敌机弹幕
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        g->enemy_bullets[i].active = false;
    }

    // 对全体敌机造成重创
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        if (g->enemies[i].active) {
            g->enemies[i].hp -= 10;
            create_explosion(g, g->enemies[i].x + g->enemies[i].w / 2.0f, g->enemies[i].y + g->enemies[i].h / 2.0f, 0xFF3344, 12);
            if (g->enemies[i].hp <= 0) {
                g->enemies[i].active = false;
                g->score += (g->enemies[i].type == ENEMY_BOSS) ? 200 : 20;
                if (g->enemies[i].type == ENEMY_BOSS) g->boss_active = false;
            }
        }
    }
    return true;
}

static void spawn_bullet(thunder_game_t *g, float x, float y, float vx, float vy, int w, int h, int dmg, uint32_t color)
{
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (!g->bullets[i].active) {
            g->bullets[i].active = true;
            g->bullets[i].x = x;
            g->bullets[i].y = y;
            g->bullets[i].vx = vx;
            g->bullets[i].vy = vy;
            g->bullets[i].w = w;
            g->bullets[i].h = h;
            g->bullets[i].dmg = dmg;
            g->bullets[i].color = color;
            return;
        }
    }
}

static void spawn_item(thunder_game_t *g, float x, float y)
{
    for (int i = 0; i < THUNDER_MAX_ITEMS; i++) {
        if (!g->items[i].active) {
            g->items[i].active = true;
            g->items[i].x = x;
            g->items[i].y = y;
            g->items[i].w = 14;
            g->items[i].h = 14;
            g->items[i].type = (item_type_t)(rand() % 3);
            return;
        }
    }
}

void thunder_step(thunder_game_t *g)
{
    if (!g || g->game_over) return;
    g->wave_tick++;
    if (g->invincible_timer > 0) g->invincible_timer--;

    // 清空瞬时音效触发标记
    g->snd_laser = false;
    g->snd_hit = false;
    g->snd_explode = false;
    g->snd_explode_big = false;
    g->snd_powerup = false;
    g->snd_gameover = false;

    // 0. 星空背景与粒子更新
    for (int i = 0; i < THUNDER_MAX_STARS; i++) {
        g->stars[i].y += g->stars[i].speed;
        if (g->stars[i].y >= SCREEN_H) {
            g->stars[i].y = 0;
            g->stars[i].x = (float)(rand() % SCREEN_W);
        }
    }
    for (int i = 0; i < THUNDER_MAX_PARTICLES; i++) {
        if (g->particles[i].life > 0) {
            g->particles[i].x += g->particles[i].vx;
            g->particles[i].y += g->particles[i].vy;
            g->particles[i].life--;
        }
    }
    if (g->screen_shake > 0) g->screen_shake--;

    // 1. 玩家自动开火
    g->shoot_timer++;
    if (g->shoot_timer >= THUNDER_SHOOT_INTERVAL) {
        g->shoot_timer = 0;
        g->snd_laser = true;
        if (g->weapon_level == 1) {
            // Level 1: 开局高能三联激光炮 —— 中间贯穿穿甲激光 (dmg 2) + 左右高速脉冲光刃 (dmg 1)
            // 一轮齐射总伤害 4 点，开局就能瞬间秒杀红煞轰炸机 (4 HP)！
            spawn_bullet(g, g->player_x + 13, g->player_y - 2, 0, -13.0f, 4, 14, 2, 0xFFD928);
            spawn_bullet(g, g->player_x + 2,  g->player_y,     0, -12.0f, 4, 12, 1, 0x00E5FF);
            spawn_bullet(g, g->player_x + 24, g->player_y,     0, -12.0f, 4, 12, 1, 0x00E5FF);
        } else if (g->weapon_level == 2) {
            // Level 2: 四联重型等离子加农炮 —— 中间双联聚能炮 (dmg 3) + 左右外侧高速炮 (dmg 2)
            spawn_bullet(g, g->player_x + 10, g->player_y - 3, -0.5f, -13.0f, 5, 14, 3, 0xFFEA00);
            spawn_bullet(g, g->player_x + 16, g->player_y - 3,  0.5f, -13.0f, 5, 14, 3, 0xFFEA00);
            spawn_bullet(g, g->player_x + 1,  g->player_y,     -2.0f, -12.0f, 4, 12, 2, 0x00E5FF);
            spawn_bullet(g, g->player_x + 25, g->player_y,      2.0f, -12.0f, 4, 12, 2, 0x00E5FF);
        } else if (g->weapon_level == 3) {
            // Level 3: 五联狂暴等离子歼灭炮 —— 中间巨型等离子核脉冲单发直接秒杀轰炸机！(单发 dmg 4 >= 4 HP)
            // 外侧 4 发广角等离子死光 (dmg 2)
            spawn_bullet(g, g->player_x + 11, g->player_y - 4, 0, -14.0f, 8, 16, 4, 0xFF00BB);
            spawn_bullet(g, g->player_x + 5,  g->player_y - 1, -1.2f, -13.0f, 4, 12, 2, 0xFFD928);
            spawn_bullet(g, g->player_x + 21, g->player_y - 1,  1.2f, -13.0f, 4, 12, 2, 0xFFD928);
            spawn_bullet(g, g->player_x + 0,  g->player_y + 2, -2.8f, -12.0f, 4, 12, 2, 0x00E5FF);
            spawn_bullet(g, g->player_x + 26, g->player_y + 2,  2.8f, -12.0f, 4, 12, 2, 0x00E5FF);
        } else {
            // Level 4: 七联终极超载·歼星光幕 (Star-Buster Hyper Beam) —— 毁天灭地！
            // 中间超级光矛死光 (单发 dmg 8，摧枯拉朽，秒杀一切常规机！)
            // 两侧等离子重炮 (dmg 4，单发亦能秒杀轰炸机！)
            // 外翼 4 发广角散射幕 (dmg 2)
            spawn_bullet(g, g->player_x + 10, g->player_y - 6, 0, -15.0f, 10, 18, 8, 0xFFFFFF);
            spawn_bullet(g, g->player_x + 4,  g->player_y - 3, -0.8f, -14.0f, 6, 14, 4, 0xFF00BB);
            spawn_bullet(g, g->player_x + 20, g->player_y - 3,  0.8f, -14.0f, 6, 14, 4, 0xFF00BB);
            spawn_bullet(g, g->player_x + 0,  g->player_y,     -2.0f, -13.0f, 4, 12, 2, 0xFFEA00);
            spawn_bullet(g, g->player_x + 26, g->player_y,      2.0f, -13.0f, 4, 12, 2, 0xFFEA00);
            spawn_bullet(g, g->player_x - 3,  g->player_y + 3, -3.6f, -12.0f, 4, 10, 2, 0x00E5FF);
            spawn_bullet(g, g->player_x + 29, g->player_y + 3,  3.6f, -12.0f, 4, 10, 2, 0x00E5FF);
        }
    }

    // 2. 玩家子弹移动
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (g->bullets[i].active) {
            g->bullets[i].x += g->bullets[i].vx;
            g->bullets[i].y += g->bullets[i].vy;
            if (g->bullets[i].y < -15.0f || g->bullets[i].x < -10 || g->bullets[i].x > SCREEN_W + 10) {
                g->bullets[i].active = false;
            }
        }
    }

    // 3. 敌机子弹移动
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        if (g->enemy_bullets[i].active) {
            g->enemy_bullets[i].x += g->enemy_bullets[i].vx;
            g->enemy_bullets[i].y += g->enemy_bullets[i].vy;
            if (g->enemy_bullets[i].y > SCREEN_H + 10 || g->enemy_bullets[i].x < -10 || g->enemy_bullets[i].x > SCREEN_W + 10) {
                g->enemy_bullets[i].active = false;
                continue;
            }
            // 命中玩家判定
            if (g->invincible_timer <= 0) {
                bullet_t *eb = &g->enemy_bullets[i];
                if (eb->x < g->player_x + g->player_w - 4 && eb->x + eb->w > g->player_x + 4 &&
                    eb->y < g->player_y + g->player_h - 4 && eb->y + eb->h > g->player_y + 4) {
                    eb->active = false;
                    g->player_hp--;
                    g->invincible_timer = 40;
                    g->snd_hit = true;
                    create_explosion(g, g->player_x + g->player_w / 2.0f, g->player_y + g->player_h / 2.0f, 0xFF3344, 14);
                    if (g->player_hp <= 0) {
                        g->game_over = true;
                        g->snd_gameover = true;
                    }
                }
            }
        }
    }

    // 4. 道具移动与拾取
    for (int i = 0; i < THUNDER_MAX_ITEMS; i++) {
        if (g->items[i].active) {
            g->items[i].y += 1.5f;
            if (g->items[i].y > SCREEN_H + 10) {
                g->items[i].active = false;
                continue;
            }
            // 拾取
            if (g->items[i].x < g->player_x + g->player_w && g->items[i].x + g->items[i].w > g->player_x &&
                g->items[i].y < g->player_y + g->player_h && g->items[i].y + g->items[i].h > g->player_y) {
                g->snd_powerup = true;
                if (g->items[i].type == ITEM_TYPE_POWER && g->weapon_level < 4) {
                    g->weapon_level++;
                } else if (g->items[i].type == ITEM_TYPE_BOMB) {
                    g->bombs++;
                } else if (g->items[i].type == ITEM_TYPE_HEAL && g->player_hp < g->player_max_hp) {
                    g->player_hp++;
                }
                g->items[i].active = false;
                g->score += 30;
            }
        }
    }

    // 5. 敌机刷新生成
    if (!g->boss_active && (g->wave_tick % 50 == 0)) {
        for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
            if (!g->enemies[i].active) {
                bool is_bomber = (rand() % 100 < 35);
                g->enemies[i].active = true;
                g->enemies[i].type = is_bomber ? ENEMY_BOMBER : ENEMY_SCOUT;
                g->enemies[i].w = is_bomber ? 32 : 24;
                g->enemies[i].h = is_bomber ? 28 : 22;
                g->enemies[i].x = 10 + (rand() % (SCREEN_W - g->enemies[i].w - 20));
                g->enemies[i].y = -30.0f;
                g->enemies[i].vx = ((rand() % 100) - 50) / 40.0f;
                g->enemies[i].vy = is_bomber ? 1.4f : 2.2f;
                g->enemies[i].hp = is_bomber ? 4 : 1;
                g->enemies[i].max_hp = g->enemies[i].hp;
                g->enemies[i].shoot_timer = 20 + (rand() % 40);
                break;
            }
        }
    }

    // 6. BOSS 刷新
    if (!g->boss_active && g->score >= 200 && g->wave_tick > 250) {
        for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
            if (!g->enemies[i].active) {
                g->enemies[i].active = true;
                g->enemies[i].type = ENEMY_BOSS;
                g->enemies[i].w = 64;
                g->enemies[i].h = 44;
                g->enemies[i].x = (SCREEN_W - 64) / 2.0f;
                g->enemies[i].y = -50.0f;
                g->enemies[i].vx = 1.2f;
                g->enemies[i].vy = 0.8f;
                g->enemies[i].hp = 50;
                g->enemies[i].max_hp = 50;
                g->enemies[i].shoot_timer = 30;
                g->boss_active = true;
                g->boss_idx = i;
                break;
            }
        }
    }

    // 7. 敌机逻辑与碰撞检测
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        if (g->enemies[i].active) {
            enemy_t *e = &g->enemies[i];
            e->x += e->vx;
            e->y += e->vy;

            if (e->type == ENEMY_BOSS) {
                if (e->y > 35.0f) e->vy = 0; // 停在屏幕上方
                if (e->x < 10.0f || e->x > SCREEN_W - e->w - 10.0f) e->vx = -e->vx;
            } else {
                if (e->x < 4.0f || e->x > SCREEN_W - e->w - 4.0f) e->vx = -e->vx;
                if (e->y > SCREEN_H + 20) {
                    e->active = false;
                    continue;
                }
            }

            // 敌机射击
            e->shoot_timer--;
            if (e->shoot_timer <= 0 && e->y > 10.0f && e->y < 220.0f) {
                if (e->type == ENEMY_BOSS) {
                    e->shoot_timer = 40;
                    float spreads[3] = { -0.8f, 0.0f, 0.8f };
                    for (int s = 0; s < 3; s++) {
                        for (int b = 0; b < THUNDER_MAX_ENEMY_BULLETS; b++) {
                            if (!g->enemy_bullets[b].active) {
                                g->enemy_bullets[b].active = true;
                                g->enemy_bullets[b].x = e->x + e->w / 2.0f - 3;
                                g->enemy_bullets[b].y = e->y + e->h;
                                g->enemy_bullets[b].w = 6;
                                g->enemy_bullets[b].h = 6;
                                g->enemy_bullets[b].vx = spreads[s];
                                g->enemy_bullets[b].vy = 2.8f;
                                break;
                            }
                        }
                    }
                } else {
                    e->shoot_timer = 60;
                    for (int b = 0; b < THUNDER_MAX_ENEMY_BULLETS; b++) {
                        if (!g->enemy_bullets[b].active) {
                            g->enemy_bullets[b].active = true;
                            g->enemy_bullets[b].x = e->x + e->w / 2.0f - 2;
                            g->enemy_bullets[b].y = e->y + e->h;
                            g->enemy_bullets[b].w = 5;
                            g->enemy_bullets[b].h = 5;
                            g->enemy_bullets[b].vx = 0;
                            g->enemy_bullets[b].vy = 2.5f;
                            break;
                        }
                    }
                }
            }

            // 玩家子弹打击敌机
            for (int b = 0; b < THUNDER_MAX_BULLETS; b++) {
                if (g->bullets[b].active) {
                    bullet_t *pb = &g->bullets[b];
                    if (pb->x < e->x + e->w && pb->x + pb->w > e->x &&
                        pb->y < e->y + e->h && pb->y + pb->h > e->y) {
                        pb->active = false;
                        int dmg = pb->dmg > 0 ? pb->dmg : 1;
                        e->hp -= dmg;
                        g->snd_hit = true;
                        if (e->hp <= 0) {
                            e->active = false;
                            g->score += (e->type == ENEMY_BOMBER ? 30 : (e->type == ENEMY_BOSS ? 200 : 10));
                            if (e->type == ENEMY_BOSS) {
                                g->boss_active = false;
                                g->snd_explode_big = true;
                                create_explosion(g, e->x + e->w / 2.0f, e->y + e->h / 2.0f, 0xFF0055, 20);
                            } else if (e->type == ENEMY_BOMBER) {
                                g->snd_explode_big = true;
                                create_explosion(g, e->x + e->w / 2.0f, e->y + e->h / 2.0f, 0xFF6600, 16);
                            } else {
                                g->snd_explode = true;
                                create_explosion(g, e->x + e->w / 2.0f, e->y + e->h / 2.0f, 0x88FF33, 10);
                            }
                            if (rand() % 100 < 38) {
                                spawn_item(g, e->x + e->w / 2.0f - 7, e->y);
                            }
                            break;
                        }
                    }
                }
            }

            // 敌机与玩家相撞
            if (e->active && g->invincible_timer <= 0) {
                if (g->player_x < e->x + e->w && g->player_x + g->player_w > e->x &&
                    g->player_y < e->y + e->h && g->player_y + g->player_h > e->y) {
                    g->player_hp--;
                    g->invincible_timer = 40;
                    g->snd_hit = true;
                    create_explosion(g, g->player_x + g->player_w / 2.0f, g->player_y + g->player_h / 2.0f, 0xFF3344, 14);
                    if (e->type != ENEMY_BOSS) e->active = false;
                    if (g->player_hp <= 0) {
                        g->game_over = true;
                        g->snd_gameover = true;
                    }
                }
            }
        }
    }
}

