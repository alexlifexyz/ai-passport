#include "thunder_logic.h"
#include <stdlib.h>
#include <string.h>

void thunder_init(thunder_game_t *g)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));

    g->player_w = 30;
    g->player_h = 28;
    g->player_x = (SCREEN_W - g->player_w) / 2.0f;
    g->player_y = 260.0f;
    g->player_hp = 3;
    g->player_max_hp = 3;
    g->bombs = 2;
    g->weapon_level = 1;
    g->score = 0;
    g->game_over = false;
}

void thunder_move_left(thunder_game_t *g)
{
    if (!g || g->game_over) return;
    g->player_x -= 12.0f;
    if (g->player_x < 4.0f) g->player_x = 4.0f;
}

void thunder_move_right(thunder_game_t *g)
{
    if (!g || g->game_over) return;
    g->player_x += 12.0f;
    if (g->player_x > SCREEN_W - g->player_w - 4.0f) {
        g->player_x = SCREEN_W - g->player_w - 4.0f;
    }
}

bool thunder_use_bomb(thunder_game_t *g)
{
    if (!g || g->game_over || g->bombs <= 0) return false;
    g->bombs--;
    g->bomb_triggered = true;

    // 清空所有敌机弹幕
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        g->enemy_bullets[i].active = false;
    }

    // 对全体敌机造成重创
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        if (g->enemies[i].active) {
            g->enemies[i].hp -= 10;
            if (g->enemies[i].hp <= 0) {
                g->enemies[i].active = false;
                g->score += (g->enemies[i].type == ENEMY_BOSS) ? 200 : 20;
                if (g->enemies[i].type == ENEMY_BOSS) g->boss_active = false;
            }
        }
    }
    return true;
}

static void spawn_bullet(thunder_game_t *g, float x, float y, float vx, float vy, int w, int h, uint32_t color)
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

    // 1. 玩家自动开火
    g->shoot_timer++;
    if (g->shoot_timer >= 12) {
        g->shoot_timer = 0;
        if (g->weapon_level == 1) {
            spawn_bullet(g, g->player_x + 4, g->player_y, 0, -8.0f, 4, 10, 0x00E5FF);
            spawn_bullet(g, g->player_x + 22, g->player_y, 0, -8.0f, 4, 10, 0x00E5FF);
        } else if (g->weapon_level == 2) {
            spawn_bullet(g, g->player_x + 13, g->player_y - 2, 0, -8.5f, 4, 12, 0xFFD928);
            spawn_bullet(g, g->player_x + 2, g->player_y, -1.5f, -8.0f, 4, 10, 0x00E5FF);
            spawn_bullet(g, g->player_x + 24, g->player_y, 1.5f, -8.0f, 4, 10, 0x00E5FF);
        } else {
            spawn_bullet(g, g->player_x + 12, g->player_y - 4, 0, -9.0f, 6, 14, 0xFF00BB);
            spawn_bullet(g, g->player_x + 1, g->player_y, -2.0f, -8.0f, 4, 10, 0xFFD928);
            spawn_bullet(g, g->player_x + 25, g->player_y, 2.0f, -8.0f, 4, 10, 0xFFD928);
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
                    if (g->player_hp <= 0) {
                        g->game_over = true;
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
                if (g->items[i].type == ITEM_TYPE_POWER && g->weapon_level < 3) {
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
                g->enemies[i].h = is_bomber ? 28 : 20;
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
                e->shoot_timer = (e->type == ENEMY_BOSS) ? 40 : 60;
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

            // 玩家子弹打击敌机
            for (int b = 0; b < THUNDER_MAX_BULLETS; b++) {
                if (g->bullets[b].active) {
                    bullet_t *pb = &g->bullets[b];
                    if (pb->x < e->x + e->w && pb->x + pb->w > e->x &&
                        pb->y < e->y + e->h && pb->y + pb->h > e->y) {
                        pb->active = false;
                        e->hp--;
                        if (e->hp <= 0) {
                            e->active = false;
                            g->score += (e->type == ENEMY_BOMBER ? 30 : (e->type == ENEMY_BOSS ? 200 : 10));
                            if (e->type == ENEMY_BOSS) g->boss_active = false;
                            if (rand() % 100 < 30) {
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
                    if (e->type != ENEMY_BOSS) e->active = false;
                    if (g->player_hp <= 0) {
                        g->game_over = true;
                    }
                }
            }
        }
    }
}
