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
    g->shield = 1;         // 初始带 1 层能量护盾
    g->bombs = 4;
    g->weapon_level = 1;
    g->weapon_style = WEAPON_STYLE_VULCAN;
    g->has_wingman = true; // 初始标配双子浮游僚机
    g->combo = 0;
    g->combo_timer = 0;
    g->score = 0;
    g->game_over = false;
    g->paused = false;

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
    if (!g || g->game_over || g->paused) return;
    g->player_x -= 16.0f;
    if (g->player_x < 4.0f) g->player_x = 4.0f;
}

void thunder_move_right(thunder_game_t *g)
{
    if (!g || g->game_over || g->paused) return;
    g->player_x += 16.0f;
    if (g->player_x > SCREEN_W - g->player_w - 4.0f) {
        g->player_x = SCREEN_W - g->player_w - 4.0f;
    }
}

void thunder_move_up(thunder_game_t *g)
{
    if (!g || g->game_over || g->paused) return;
    g->player_y -= 16.0f;
    if (g->player_y < 45.0f) g->player_y = 45.0f; // 避开顶部 HUD
}

void thunder_move_down(thunder_game_t *g)
{
    if (!g || g->game_over || g->paused) return;
    g->player_y += 16.0f;
    if (g->player_y > 275.0f) g->player_y = 275.0f; // 避开底部操作区
}

void thunder_toggle_pause(thunder_game_t *g)
{
    if (!g || g->game_over) return;
    g->paused = !g->paused;
    g->snd_pause = true;
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
    if (!g || g->game_over || g->paused || g->bombs <= 0) return false;
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
            g->enemies[i].hp -= 12;
            create_explosion(g, g->enemies[i].x + g->enemies[i].w / 2.0f, g->enemies[i].y + g->enemies[i].h / 2.0f, 0xFF3344, 12);
            if (g->enemies[i].hp <= 0) {
                g->enemies[i].active = false;
                g->score += (g->enemies[i].type == ENEMY_BOSS) ? 250 : 25;
                if (g->enemies[i].type == ENEMY_BOSS) g->boss_active = false;
            }
        }
    }
    return true;
}

static void spawn_bullet(thunder_game_t *g, float x, float y, float vx, float vy, int w, int h, int dmg, bullet_traj_t traj, uint32_t color)
{
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (!g->bullets[i].active) {
            g->bullets[i].active = true;
            g->bullets[i].x = x;
            g->bullets[i].y = y;
            g->bullets[i].base_x = x;
            g->bullets[i].vx = vx;
            g->bullets[i].vy = vy;
            g->bullets[i].w = w;
            g->bullets[i].h = h;
            g->bullets[i].dmg = dmg;
            g->bullets[i].traj = traj;
            g->bullets[i].phase = (float)(rand() % 628) / 100.0f;
            g->bullets[i].piercing = (traj == BULLET_TRAJ_FIRE || dmg >= 6);
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
            int r = rand() % 100;
            if (r < 30)       g->items[i].type = ITEM_TYPE_POWER;
            else if (r < 50)  g->items[i].type = ITEM_TYPE_WAVE;
            else if (r < 70)  g->items[i].type = ITEM_TYPE_FIRE;
            else if (r < 85)  g->items[i].type = ITEM_TYPE_SHIELD;
            else if (r < 95)  g->items[i].type = ITEM_TYPE_BOMB;
            else              g->items[i].type = ITEM_TYPE_HEAL;
            return;
        }
    }
}

void thunder_step(thunder_game_t *g)
{
    if (!g || g->game_over || g->paused) return;
    g->wave_tick++;
    if (g->invincible_timer > 0) g->invincible_timer--;

    // 连击计时
    if (g->combo_timer > 0) {
        g->combo_timer--;
        if (g->combo_timer <= 0) g->combo = 0;
    }

    // 清空瞬时音效触发标记
    g->snd_laser = false;
    g->snd_wave = false;
    g->snd_fire = false;
    g->snd_shield = false;
    g->snd_pause = false;
    g->snd_hit = false;
    g->snd_explode = false;
    g->snd_explode_big = false;
    g->snd_bomb = false;
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

    // 1. 玩家自动开火 (根据三大弹道流派发射)
    g->shoot_timer++;
    if (g->shoot_timer >= THUNDER_SHOOT_INTERVAL) {
        g->shoot_timer = 0;

        if (g->weapon_style == WEAPON_STYLE_VULCAN) {
            // 突击神火流：直线穿甲激光
            g->snd_laser = true;
            if (g->weapon_level == 1) {
                spawn_bullet(g, g->player_x + 13, g->player_y - 2, 0, -13.0f, 4, 14, 2, BULLET_TRAJ_LINE, 0xFFD928);
                spawn_bullet(g, g->player_x + 2,  g->player_y,     0, -12.0f, 4, 12, 1, BULLET_TRAJ_LINE, 0x00E5FF);
                spawn_bullet(g, g->player_x + 24, g->player_y,     0, -12.0f, 4, 12, 1, BULLET_TRAJ_LINE, 0x00E5FF);
            } else if (g->weapon_level == 2) {
                spawn_bullet(g, g->player_x + 10, g->player_y - 3, -0.5f, -13.0f, 5, 14, 3, BULLET_TRAJ_LINE, 0xFFEA00);
                spawn_bullet(g, g->player_x + 16, g->player_y - 3,  0.5f, -13.0f, 5, 14, 3, BULLET_TRAJ_LINE, 0xFFEA00);
                spawn_bullet(g, g->player_x + 1,  g->player_y,     -2.0f, -12.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0x00E5FF);
                spawn_bullet(g, g->player_x + 25, g->player_y,      2.0f, -12.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0x00E5FF);
            } else if (g->weapon_level == 3) {
                spawn_bullet(g, g->player_x + 11, g->player_y - 4, 0, -14.0f, 8, 16, 4, BULLET_TRAJ_LINE, 0xFF00BB);
                spawn_bullet(g, g->player_x + 5,  g->player_y - 1, -1.2f, -13.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0xFFD928);
                spawn_bullet(g, g->player_x + 21, g->player_y - 1,  1.2f, -13.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0xFFD928);
                spawn_bullet(g, g->player_x + 0,  g->player_y + 2, -2.8f, -12.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0x00E5FF);
                spawn_bullet(g, g->player_x + 26, g->player_y + 2,  2.8f, -12.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0x00E5FF);
            } else {
                spawn_bullet(g, g->player_x + 10, g->player_y - 6, 0, -15.0f, 10, 18, 8, BULLET_TRAJ_LINE, 0xFFFFFF);
                spawn_bullet(g, g->player_x + 4,  g->player_y - 3, -0.8f, -14.0f, 6, 14, 4, BULLET_TRAJ_LINE, 0xFF00BB);
                spawn_bullet(g, g->player_x + 20, g->player_y - 3,  0.8f, -14.0f, 6, 14, 4, BULLET_TRAJ_LINE, 0xFF00BB);
                spawn_bullet(g, g->player_x + 0,  g->player_y,     -2.0f, -13.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0xFFEA00);
                spawn_bullet(g, g->player_x + 26, g->player_y,      2.0f, -13.0f, 4, 12, 2, BULLET_TRAJ_LINE, 0xFFEA00);
                spawn_bullet(g, g->player_x - 3,  g->player_y + 3, -3.6f, -12.0f, 4, 10, 2, BULLET_TRAJ_LINE, 0x00E5FF);
                spawn_bullet(g, g->player_x + 29, g->player_y + 3,  3.6f, -12.0f, 4, 10, 2, BULLET_TRAJ_LINE, 0x00E5FF);
            }
        } else if (g->weapon_style == WEAPON_STYLE_WAVE) {
            // 幻影波动流：S型蛇形回旋摆动弹幕
            g->snd_wave = true;
            int wave_dmg = (g->weapon_level >= 3) ? 4 : 2;
            spawn_bullet(g, g->player_x + 8,  g->player_y - 2, -0.6f, -12.0f, 6, 12, wave_dmg, BULLET_TRAJ_WAVE, 0x00FFCC);
            spawn_bullet(g, g->player_x + 18, g->player_y - 2,  0.6f, -12.0f, 6, 12, wave_dmg, BULLET_TRAJ_WAVE, 0x00FFCC);
            if (g->weapon_level >= 2) {
                spawn_bullet(g, g->player_x + 2,  g->player_y, -1.5f, -11.0f, 5, 10, 2, BULLET_TRAJ_WAVE, 0x88FF00);
                spawn_bullet(g, g->player_x + 24, g->player_y,  1.5f, -11.0f, 5, 10, 2, BULLET_TRAJ_WAVE, 0x88FF00);
            }
            if (g->weapon_level >= 4) {
                spawn_bullet(g, g->player_x + 13, g->player_y - 6, 0, -14.0f, 10, 16, 7, BULLET_TRAJ_WAVE, 0xFF00FF);
            }
        } else {
            // 炼狱烈焰流：狂暴爆轰火球
            g->snd_fire = true;
            int fire_dmg = (g->weapon_level >= 3) ? 5 : 3;
            spawn_bullet(g, g->player_x + 11, g->player_y - 4, 0, -11.0f, 8, 12, fire_dmg, BULLET_TRAJ_FIRE, 0xFF3300);
            if (g->weapon_level >= 2) {
                spawn_bullet(g, g->player_x + 3,  g->player_y - 1, -1.0f, -10.5f, 6, 10, 2, BULLET_TRAJ_FIRE, 0xFF6600);
                spawn_bullet(g, g->player_x + 21, g->player_y - 1,  1.0f, -10.5f, 6, 10, 2, BULLET_TRAJ_FIRE, 0xFF6600);
            }
            if (g->weapon_level >= 4) {
                spawn_bullet(g, g->player_x + 10, g->player_y - 8, 0, -13.0f, 12, 16, 8, BULLET_TRAJ_FIRE, 0xFFCC00);
            }
        }

        // 双子浮游僚机协同开火
        if (g->has_wingman) {
            spawn_bullet(g, g->player_x - 10, g->player_y + 8, -1.2f, -12.0f, 3, 8, 1, BULLET_TRAJ_LINE, 0x00E5FF);
            spawn_bullet(g, g->player_x + 36, g->player_y + 8,  1.2f, -12.0f, 3, 8, 1, BULLET_TRAJ_LINE, 0x00E5FF);
        }
    }

    // 2. 玩家子弹移动 (支持 S 型蛇形正弦摆动与烈焰粒子)
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (g->bullets[i].active) {
            bullet_t *b = &g->bullets[i];
            b->y += b->vy;
            if (b->traj == BULLET_TRAJ_WAVE) {
                b->phase += 0.32f;
                b->base_x += b->vx;
                b->x = b->base_x + sinf(b->phase) * 18.0f; // S型回旋
            } else {
                b->x += b->vx;
            }

            if (b->y < -20.0f || b->x < -20.0f || b->x > SCREEN_W + 20.0f || b->y > SCREEN_H + 20.0f) {
                b->active = false;
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
                    if (g->shield > 0) {
                        g->shield--;
                        g->invincible_timer = 40;
                        g->snd_shield = true;
                        create_explosion(g, g->player_x + g->player_w / 2.0f, g->player_y + g->player_h / 2.0f, 0x00E5FF, 16);
                    } else {
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
                if (g->items[i].type == ITEM_TYPE_POWER) {
                    if (g->weapon_level < 4) g->weapon_level++;
                } else if (g->items[i].type == ITEM_TYPE_WAVE) {
                    g->weapon_style = WEAPON_STYLE_WAVE;
                    if (g->weapon_level < 4) g->weapon_level++;
                } else if (g->items[i].type == ITEM_TYPE_FIRE) {
                    g->weapon_style = WEAPON_STYLE_FIRE;
                    if (g->weapon_level < 4) g->weapon_level++;
                } else if (g->items[i].type == ITEM_TYPE_SHIELD) {
                    if (g->shield < 2) g->shield++;
                } else if (g->items[i].type == ITEM_TYPE_BOMB) {
                    g->bombs++;
                } else if (g->items[i].type == ITEM_TYPE_HEAL && g->player_hp < g->player_max_hp) {
                    g->player_hp++;
                }
                g->items[i].active = false;
                g->score += 40;
            }
        }
    }

    // 5. 敌机编队刷新生成 (多机型：烈隼战机、机械飞龙、空中堡垒)
    if (!g->boss_active && (g->wave_tick % 45 == 0)) {
        for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
            if (!g->enemies[i].active) {
                g->enemies[i].active = true;
                int r = rand() % 100;
                if (r < 45) {
                    // 烈隼三角隐形战机 (26x24, 敏捷高速)
                    g->enemies[i].type = ENEMY_FALCON;
                    g->enemies[i].w = 26;
                    g->enemies[i].h = 24;
                    g->enemies[i].hp = 2;
                    g->enemies[i].vy = 2.4f;
                } else if (r < 75) {
                    // 幻翼机械飞龙 (32x28, 展翅扇动, 血量适中)
                    g->enemies[i].type = ENEMY_WYVERN;
                    g->enemies[i].w = 32;
                    g->enemies[i].h = 28;
                    g->enemies[i].hp = 4;
                    g->enemies[i].vy = 1.8f;
                } else {
                    // 空中重装巡洋堡垒 (36x30, 慢速厚血)
                    g->enemies[i].type = ENEMY_FORTRESS;
                    g->enemies[i].w = 36;
                    g->enemies[i].h = 30;
                    g->enemies[i].hp = 6;
                    g->enemies[i].vy = 1.2f;
                }
                g->enemies[i].x = 10 + (rand() % (SCREEN_W - g->enemies[i].w - 20));
                g->enemies[i].y = -35.0f;
                g->enemies[i].vx = ((rand() % 100) - 50) / 40.0f;
                g->enemies[i].max_hp = g->enemies[i].hp;
                g->enemies[i].shoot_timer = 20 + (rand() % 40);
                g->enemies[i].anim_tick = rand() % 30;
                break;
            }
        }
    }

    // 6. BOSS 刷新 (星海利维坦龙神)
    if (!g->boss_active && g->score >= 200 && g->wave_tick > 250) {
        for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
            if (!g->enemies[i].active) {
                g->enemies[i].active = true;
                g->enemies[i].type = ENEMY_BOSS;
                g->enemies[i].w = 68;
                g->enemies[i].h = 50;
                g->enemies[i].x = (SCREEN_W - 68) / 2.0f;
                g->enemies[i].y = -55.0f;
                g->enemies[i].vx = 1.2f;
                g->enemies[i].vy = 0.8f;
                g->enemies[i].hp = 60;
                g->enemies[i].max_hp = 60;
                g->enemies[i].shoot_timer = 30;
                g->enemies[i].anim_tick = 0;
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
            e->anim_tick++;

            if (e->type == ENEMY_BOSS) {
                if (e->y > 35.0f) e->vy = 0; // 停在屏幕上方巡游
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
                    e->shoot_timer = 38;
                    float spreads[4] = { -1.2f, -0.4f, 0.4f, 1.2f };
                    for (int s = 0; s < 4; s++) {
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
                } else if (e->type == ENEMY_WYVERN) {
                    // 飞龙喷吐等离子火球
                    e->shoot_timer = 50;
                    for (int b = 0; b < THUNDER_MAX_ENEMY_BULLETS; b++) {
                        if (!g->enemy_bullets[b].active) {
                            g->enemy_bullets[b].active = true;
                            g->enemy_bullets[b].x = e->x + e->w / 2.0f - 3;
                            g->enemy_bullets[b].y = e->y + e->h;
                            g->enemy_bullets[b].w = 6;
                            g->enemy_bullets[b].h = 6;
                            g->enemy_bullets[b].vx = (g->player_x > e->x) ? 0.8f : -0.8f;
                            g->enemy_bullets[b].vy = 2.6f;
                            break;
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
                        if (!pb->piercing) pb->active = false;
                        int dmg = pb->dmg > 0 ? pb->dmg : 1;
                        e->hp -= dmg;
                        g->snd_hit = true;
                        if (e->hp <= 0) {
                            e->active = false;
                            g->combo++;
                            g->combo_timer = 60;
                            int mult = (g->combo > 10) ? 3 : ((g->combo > 4) ? 2 : 1);
                            int base_score = (e->type == ENEMY_FORTRESS ? 40 : (e->type == ENEMY_BOSS ? 250 : 20));
                            g->score += base_score * mult;

                            if (e->type == ENEMY_BOSS) {
                                g->boss_active = false;
                                g->snd_explode_big = true;
                                create_explosion(g, e->x + e->w / 2.0f, e->y + e->h / 2.0f, 0xFF0055, 24);
                            } else if (e->type == ENEMY_WYVERN) {
                                g->snd_explode_big = true;
                                create_explosion(g, e->x + e->w / 2.0f, e->y + e->h / 2.0f, 0xAA00FF, 18);
                            } else {
                                g->snd_explode = true;
                                create_explosion(g, e->x + e->w / 2.0f, e->y + e->h / 2.0f, 0x00FFCC, 12);
                            }
                            if (rand() % 100 < 40) {
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
                    if (g->shield > 0) {
                        g->shield--;
                        g->invincible_timer = 40;
                        g->snd_shield = true;
                        create_explosion(g, g->player_x + g->player_w / 2.0f, g->player_y + g->player_h / 2.0f, 0x00E5FF, 16);
                    } else {
                        g->player_hp--;
                        g->invincible_timer = 40;
                        g->snd_hit = true;
                        create_explosion(g, g->player_x + g->player_w / 2.0f, g->player_y + g->player_h / 2.0f, 0xFF3344, 14);
                        if (g->player_hp <= 0) {
                            g->game_over = true;
                            g->snd_gameover = true;
                        }
                    }
                    if (e->type != ENEMY_BOSS) e->active = false;
                }
            }
        }
    }
}


