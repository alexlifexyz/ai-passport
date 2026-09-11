// main/demo_thunder.c —— 《雷霆战机：大像素街机版》(Thunder Striker Arcade)
// 100% 还原 simulator/index.html 的全屏复古大像素画风、星空、战机火焰、光刃激光、粒子爆炸与街机音效。
#include "demo.h"
#include "thunder_logic.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG __attribute__((unused)) = "demo_thunder";

typedef enum {
    SND_NONE = 0,
    SND_LASER,
    SND_HIT,
    SND_EXPLODE,
    SND_EXPLODE_BIG,
    SND_BOMB,
    SND_POWERUP,
    SND_GAMEOVER
} thunder_snd_t;

static thunder_game_t s_game;
static lv_obj_t *s_scr;
static lv_obj_t *s_playfield;
static lv_obj_t *s_hud_score;
static lv_obj_t *s_hud_bomb;
static lv_obj_t *s_hud_boss;
static lv_obj_t *s_gameover_box;
static lv_obj_t *s_gameover_score;
static lv_timer_t *s_game_timer;

static QueueHandle_t s_snd_queue;
static TaskHandle_t s_snd_task;
static int s_flash_timer = 0;

static void send_sound(thunder_snd_t snd)
{
    if (s_snd_queue) {
        xQueueSend(s_snd_queue, &snd, 0);
    }
}

static void thunder_audio_task(void *arg)
{
    (void)arg;
    thunder_snd_t snd;
    int16_t buf[256];

    while (1) {
        if (xQueueReceive(s_snd_queue, &snd, portMAX_DELAY) == pdTRUE) {
            if (snd == SND_NONE) continue;
            bsp_audio_set_format(16000, 16, 1);
            bsp_audio_set_volume(90);

            if (snd == SND_LASER) {
                for (int i = 0; i < 180; i++) {
                    int period = 8 + (i * 24 / 180);
                    buf[i] = (i % period < period / 2) ? 5000 : -5000;
                }
                bsp_audio_write(buf, 180 * sizeof(int16_t));
            } else if (snd == SND_HIT) {
                for (int i = 0; i < 80; i++) buf[i] = (i % 6 < 3) ? 4000 : -4000;
                bsp_audio_write(buf, 80 * sizeof(int16_t));
            } else if (snd == SND_EXPLODE) {
                for (int c = 0; c < 8; c++) {
                    int amp = 7000 - c * 800;
                    for (int i = 0; i < 256; i++) {
                        buf[i] = (int16_t)(((rand() % 65536) - 32768) * amp / 32768);
                    }
                    bsp_audio_write(buf, 256 * sizeof(int16_t));
                }
            } else if (snd == SND_EXPLODE_BIG) {
                for (int c = 0; c < 16; c++) {
                    int amp = 9000 - c * 500;
                    for (int i = 0; i < 256; i++) {
                        buf[i] = (int16_t)(((rand() % 65536) - 32768) * amp / 32768);
                    }
                    bsp_audio_write(buf, 256 * sizeof(int16_t));
                }
            } else if (snd == SND_BOMB) {
                for (int round = 0; round < 3; round++) {
                    for (int c = 0; c < 12; c++) {
                        int amp = 9500 - c * 700;
                        for (int i = 0; i < 256; i++) {
                            buf[i] = (int16_t)(((rand() % 65536) - 32768) * amp / 32768);
                        }
                        bsp_audio_write(buf, 256 * sizeof(int16_t));
                    }
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
            } else if (snd == SND_POWERUP) {
                int freqs[] = { 440, 554, 659, 880 };
                for (int f = 0; f < 4; f++) {
                    int period = 16000 / freqs[f];
                    for (int i = 0; i < 180; i++) {
                        buf[i] = (i % period < period / 2) ? 5000 : -5000;
                    }
                    bsp_audio_write(buf, 180 * sizeof(int16_t));
                    vTaskDelay(pdMS_TO_TICKS(35));
                }
            } else if (snd == SND_GAMEOVER) {
                int freqs[] = { 392, 349, 311, 261 };
                for (int f = 0; f < 4; f++) {
                    int period = 16000 / freqs[f];
                    for (int i = 0; i < 220; i++) {
                        buf[i] = (i % period < period / 2) ? 6000 : -6000;
                    }
                    bsp_audio_write(buf, 220 * sizeof(int16_t));
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
            }
        }
    }
}

// 快速屏幕像素色块填充 (基于 LVGL v9 渲染上下文，零额外堆内存消耗)
static inline void draw_box(lv_layer_t *layer, int x, int y, int w, int h, uint32_t hex_color)
{
    if (x >= SCREEN_W || y >= SCREEN_H || x + w <= 0 || y + h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    lv_area_t coords;
    coords.x1 = x;
    coords.y1 = y;
    coords.x2 = x + w - 1;
    coords.y2 = y + h - 1;

    lv_draw_fill_dsc_t dsc;
    lv_draw_fill_dsc_init(&dsc);
    dsc.color = lv_color_hex(hex_color);
    dsc.opa = LV_OPA_COVER;
    dsc.radius = 0;
    lv_draw_fill(layer, &dsc, &coords);
}

// 绘制红心生命图标 (7x6 经典像素心)
static void draw_heart(lv_layer_t *layer, int x, int y, bool filled)
{
    uint32_t col = filled ? 0xFF2244 : 0x441122;
    draw_box(layer, x + 1, y, 2, 1, col);
    draw_box(layer, x + 4, y, 2, 1, col);
    draw_box(layer, x, y + 1, 7, 2, col);
    draw_box(layer, x + 1, y + 3, 5, 1, col);
    draw_box(layer, x + 2, y + 4, 3, 1, col);
    draw_box(layer, x + 3, y + 5, 1, 1, col);
}

// 游戏画布主绘制回调 (每帧直接渲染)
static void on_playfield_draw(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    if (!layer) return;

    int ox = 0, oy = 0;
    if (s_game.screen_shake > 0) {
        ox = (rand() % s_game.screen_shake) - (s_game.screen_shake / 2);
        oy = (rand() % s_game.screen_shake) - (s_game.screen_shake / 2);
    }

    // 0. 全屏核弹白光
    if (s_flash_timer > 0) {
        draw_box(layer, 0, 0, SCREEN_W, SCREEN_H, 0xFFFFFF);
        return;
    }

    // 1. 深邃星空背景与滚动星辰
    draw_box(layer, 0, 0, SCREEN_W, SCREEN_H, 0x080C16);
    for (int i = 0; i < THUNDER_MAX_STARS; i++) {
        draw_box(layer, (int)s_game.stars[i].x + ox, (int)s_game.stars[i].y + oy,
                 s_game.stars[i].size, s_game.stars[i].size, s_game.stars[i].color);
    }

    // 2. 玩家战机 (30x28 像素，带双喷射火焰、机翼、座舱光晕)
    if (!s_game.game_over) {
        bool flash_hide = (s_game.invincible_timer > 0 && (s_game.invincible_timer / 4) % 2 == 0);
        if (!flash_hide) {
            int px = (int)s_game.player_x + ox;
            int py = (int)s_game.player_y + oy;

            // 双尾喷动态尾焰
            int flame_h = 6 + (s_game.wave_tick % 3) * 3;
            draw_box(layer, px + 9, py + 26, 4, flame_h, 0xFF9900);
            draw_box(layer, px + 17, py + 26, 4, flame_h, 0xFF9900);
            draw_box(layer, px + 10, py + 26, 2, flame_h - 2, 0xFFEA00);
            draw_box(layer, px + 18, py + 26, 2, flame_h - 2, 0xFFEA00);

            // 主机身脊背与尖端整流鼻锥
            draw_box(layer, px + 11, py + 2, 8, 24, 0x0077AA);
            draw_box(layer, px + 13, py + 0, 4, 16, 0x00E5FF);

            // 战机掠翼
            draw_box(layer, px + 2, py + 14, 26, 8, 0x00AACC);
            draw_box(layer, px + 0, py + 18, 30, 6, 0x0088BB);

            // 翼尖金色双联激光炮口
            draw_box(layer, px + 0, py + 12, 3, 6, 0xFFD928);
            draw_box(layer, px + 27, py + 12, 3, 6, 0xFFD928);

            // 晶透橙色座舱盖与反光高光
            draw_box(layer, px + 12, py + 8, 6, 8, 0xFFAA00);
            draw_box(layer, px + 14, py + 9, 2, 4, 0xFFFFFF);
        }
    }

    // 3. 玩家子弹 (高能激光与等离子火球)
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (s_game.bullets[i].active) {
            int bx = (int)s_game.bullets[i].x + ox;
            int by = (int)s_game.bullets[i].y + oy;
            draw_box(layer, bx, by, s_game.bullets[i].w, s_game.bullets[i].h, s_game.bullets[i].color);
        }
    }

    // 4. 敌机编队 (绿蜂快艇、红煞轰炸机、巨型战列要塞)
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        if (s_game.enemies[i].active) {
            enemy_t *e = &s_game.enemies[i];
            int ex = (int)e->x + ox;
            int ey = (int)e->y + oy;

            if (e->type == ENEMY_SCOUT) {
                // 绿蜂快艇 (24x22)
                draw_box(layer, ex + 8, ey + 0, 8, 18, 0x22CC44);
                draw_box(layer, ex + 2, ey + 6, 20, 8, 0x22CC44);
                draw_box(layer, ex + 10, ey + 4, 4, 6, 0x88FF33);
                // 红色复眼
                draw_box(layer, ex + 4, ey + 8, 3, 4, 0xFF2222);
                draw_box(layer, ex + 17, ey + 8, 3, 4, 0xFF2222);
            } else if (e->type == ENEMY_BOMBER) {
                // 红煞轰炸机 (32x28)
                draw_box(layer, ex + 10, ey + 0, 12, 26, 0xCC2233);
                draw_box(layer, ex + 2, ey + 8, 28, 12, 0xCC2233);
                draw_box(layer, ex + 0, ey + 14, 32, 6, 0xCC2233);
                draw_box(layer, ex + 12, ey + 10, 8, 8, 0xFF6677);
                // 护甲血条
                draw_box(layer, ex + 4, ey - 6, 24, 3, 0x000000);
                if (e->max_hp > 0) {
                    int hp_w = (e->hp * 24) / e->max_hp;
                    if (hp_w > 0) draw_box(layer, ex + 4, ey - 6, hp_w, 3, 0xFF3344);
                }
            } else {
                // 巨型战列要塞 BOSS (64x44)
                draw_box(layer, ex + 16, ey + 0, 32, 40, 0x442266);
                draw_box(layer, ex + 4, ey + 10, 56, 20, 0x7733AA);
                draw_box(layer, ex + 0, ey + 18, 64, 12, 0x7733AA);
                // 核心主炮
                draw_box(layer, ex + 24, ey + 30, 16, 12, 0xFF0055);
                draw_box(layer, ex + 28, ey + 34, 8, 6, 0xFFFFFF);
                // 双侧等离子副炮
                draw_box(layer, ex + 6, ey + 26, 6, 8, 0xFFBB00);
                draw_box(layer, ex + 52, ey + 26, 6, 8, 0xFFBB00);
            }
        }
    }

    // 5. 敌机子弹 (发光红光球，红外壳 + 亮白内芯)
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        if (s_game.enemy_bullets[i].active) {
            int bx = (int)s_game.enemy_bullets[i].x + ox;
            int by = (int)s_game.enemy_bullets[i].y + oy;
            int bw = s_game.enemy_bullets[i].w;
            int bh = s_game.enemy_bullets[i].h;
            draw_box(layer, bx - 1, by - 1, bw + 2, bh + 2, 0xFF2255);
            draw_box(layer, bx, by, bw, bh, 0xFFFFFF);
        }
    }

    // 6. 掉落补给道具 [P] [B] [H]
    for (int i = 0; i < THUNDER_MAX_ITEMS; i++) {
        if (s_game.items[i].active) {
            int ix = (int)s_game.items[i].x + ox;
            int iy = (int)s_game.items[i].y + oy;
            uint32_t col = (s_game.items[i].type == ITEM_TYPE_POWER) ? 0xFFD928 :
                           ((s_game.items[i].type == ITEM_TYPE_BOMB) ? 0xFF3344 : 0x00E5FF);
            // 白框 + 色块底
            draw_box(layer, ix, iy, 14, 14, 0xFFFFFF);
            draw_box(layer, ix + 2, iy + 2, 10, 10, col);

            // 内部像素字母
            if (s_game.items[i].type == ITEM_TYPE_POWER) { // 'P'
                draw_box(layer, ix + 4, iy + 4, 2, 6, 0x000000);
                draw_box(layer, ix + 6, iy + 4, 3, 2, 0x000000);
                draw_box(layer, ix + 7, iy + 5, 2, 2, 0x000000);
                draw_box(layer, ix + 6, iy + 6, 3, 2, 0x000000);
            } else if (s_game.items[i].type == ITEM_TYPE_BOMB) { // 'B'
                draw_box(layer, ix + 4, iy + 4, 2, 6, 0x000000);
                draw_box(layer, ix + 6, iy + 4, 3, 2, 0x000000);
                draw_box(layer, ix + 6, iy + 6, 3, 2, 0x000000);
                draw_box(layer, ix + 6, iy + 8, 3, 2, 0x000000);
            } else { // 'H'
                draw_box(layer, ix + 4, iy + 4, 2, 6, 0x000000);
                draw_box(layer, ix + 8, iy + 4, 2, 6, 0x000000);
                draw_box(layer, ix + 6, iy + 6, 2, 2, 0x000000);
            }
        }
    }

    // 7. 爆炸碎屑与火花粒子
    for (int i = 0; i < THUNDER_MAX_PARTICLES; i++) {
        if (s_game.particles[i].life > 0) {
            int px = (int)s_game.particles[i].x + ox;
            int py = (int)s_game.particles[i].y + oy;
            int ps = s_game.particles[i].size;
            draw_box(layer, px, py, ps, ps, s_game.particles[i].color);
        }
    }

    // 8. 顶部半透 HUD 背景栏
    draw_box(layer, 0, 0, SCREEN_W, 22, 0x050812);

    // 8.1 玩家红心生命绘制 (在 HUD 中间)
    for (int h = 0; h < s_game.player_max_hp; h++) {
        draw_heart(layer, 108 + h * 12, 8, h < s_game.player_hp);
    }

    // 8.2 BOSS 预警血条
    if (s_game.boss_active && s_game.boss_idx >= 0 && s_game.enemies[s_game.boss_idx].active) {
        enemy_t *b = &s_game.enemies[s_game.boss_idx];
        draw_box(layer, 20, 26, 200, 8, 0x000000);
        if (b->max_hp > 0) {
            int bar_w = (b->hp * 196) / b->max_hp;
            if (bar_w > 0) draw_box(layer, 22, 28, bar_w, 4, 0xFF0055);
        }
    }
}

static void game_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_game.game_over) {
        thunder_step(&s_game);

        if (s_flash_timer > 0) s_flash_timer--;

        // 音频事件派发
        if (s_game.snd_bomb) {
            s_flash_timer = 3;
            send_sound(SND_BOMB);
        } else if (s_game.snd_explode_big) {
            send_sound(SND_EXPLODE_BIG);
        } else if (s_game.snd_explode) {
            send_sound(SND_EXPLODE);
        }

        if (s_game.snd_laser) send_sound(SND_LASER);
        if (s_game.snd_hit) send_sound(SND_HIT);
        if (s_game.snd_powerup) send_sound(SND_POWERUP);

        // 刷新 HUD 文本
        lv_label_set_text_fmt(s_hud_score, "SCORE:%d", s_game.score);
        lv_label_set_text_fmt(s_hud_bomb, "BOMB:x%d", s_game.bombs);

        if (s_game.boss_active) {
            lv_obj_remove_flag(s_hud_boss, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_hud_boss, LV_OBJ_FLAG_HIDDEN);
        }

        if (s_game.game_over) {
            send_sound(SND_GAMEOVER);
            lv_label_set_text_fmt(s_gameover_score, "FINAL SCORE: %d", s_game.score);
            lv_obj_remove_flag(s_gameover_box, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 触发画布重绘
    if (s_playfield) {
        lv_obj_invalidate(s_playfield);
    }
}

void demo_thunder_enter(void)
{
    thunder_init(&s_game);
    s_flash_timer = 0;

    // 音频任务与队列
    if (!s_snd_queue) {
        s_snd_queue = xQueueCreate(8, sizeof(thunder_snd_t));
        xTaskCreate(thunder_audio_task, "thunder_snd", 3072, NULL, 5, &s_snd_task);
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x080C16), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    // 1. 全屏游戏主画板 (240x320)
    s_playfield = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_playfield);
    lv_obj_set_size(s_playfield, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_playfield, 0, 0);
    lv_obj_add_event_cb(s_playfield, on_playfield_draw, LV_EVENT_DRAW_MAIN, NULL);

    // 2. HUD 顶栏标签
    s_hud_score = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_score, 6, 4);
    lv_label_set_text(s_hud_score, "SCORE:0");
    lv_obj_set_style_text_font(s_hud_score, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_score, lv_color_hex(0xFFFFFF), 0);

    s_hud_bomb = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_bomb, 172, 4);
    lv_label_set_text(s_hud_bomb, "BOMB:x2");
    lv_obj_set_style_text_font(s_hud_bomb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_bomb, lv_color_hex(0x00E5FF), 0);

    s_hud_boss = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_boss, 46, 22);
    lv_label_set_text(s_hud_boss, "WARNING: CYBER BOSS");
    lv_obj_set_style_text_font(s_hud_boss, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_boss, lv_color_hex(0xFF0055), 0);
    lv_obj_add_flag(s_hud_boss, LV_OBJ_FLAG_HIDDEN);

    // 3. GAME OVER 结算遮罩容器
    s_gameover_box = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_gameover_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_gameover_box, 200, 140);
    lv_obj_center(s_gameover_box);
    lv_obj_set_style_bg_color(s_gameover_box, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_gameover_box, LV_OPA_80, 0);
    lv_obj_set_style_border_color(s_gameover_box, lv_color_hex(0xFF3344), 0);
    lv_obj_set_style_border_width(s_gameover_box, 2, 0);
    lv_obj_set_style_radius(s_gameover_box, 10, 0);

    lv_obj_t *title = lv_label_create(s_gameover_box);
    lv_label_set_text(title, "MISSION FAILED");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF3344), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_gameover_score = lv_label_create(s_gameover_box);
    lv_label_set_text(s_gameover_score, "FINAL SCORE: 0");
    lv_obj_set_style_text_color(s_gameover_score, lv_color_hex(0xFFD928), 0);
    lv_obj_set_style_text_font(s_gameover_score, &lv_font_montserrat_14, 0);
    lv_obj_align(s_gameover_score, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *hint = lv_label_create(s_gameover_box);
    lv_label_set_text(hint, "PRESS [OK] RETRY");
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_add_flag(s_gameover_box, LV_OBJ_FLAG_HIDDEN);

    s_game_timer = lv_timer_create(game_timer_cb, 35, NULL);
    lv_screen_load(s_scr);
}

void demo_thunder_exit(void)
{
    if (s_game_timer) {
        lv_timer_delete(s_game_timer);
        s_game_timer = NULL;
    }
    if (s_snd_task) {
        vTaskDelete(s_snd_task);
        s_snd_task = NULL;
    }
    if (s_snd_queue) {
        vQueueDelete(s_snd_queue);
        s_snd_queue = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_playfield = s_hud_score = s_hud_bomb = s_hud_boss = s_gameover_box = s_gameover_score = NULL;
    }
}

void demo_thunder_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_PRESS && ev != BSP_BTN_CLICK) return;

    if (s_game.game_over) {
        if (btn == BSP_BTN_OK) {
            thunder_init(&s_game);
            s_flash_timer = 0;
            if (s_gameover_box) {
                lv_obj_add_flag(s_gameover_box, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return;
    }

    if (btn == BSP_BTN_UP) {
        thunder_move_left(&s_game);
    } else if (btn == BSP_BTN_DOWN) {
        thunder_move_right(&s_game);
    } else if (btn == BSP_BTN_OK) {
        thunder_use_bomb(&s_game);
    }
}
