// main/demo_thunder.c —— 《雷霆战机：大像素街机版》(Thunder Striker Arcade)
#include "demo.h"
#include "thunder_logic.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "demo_thunder";

typedef enum {
    SND_NONE = 0,
    SND_LASER,
    SND_HIT,
    SND_EXPLODE,
    SND_BOMB,
    SND_POWERUP,
    SND_GAMEOVER
} thunder_snd_t;

static thunder_game_t s_game;
static lv_obj_t *s_scr;
static lv_obj_t *s_hud_score;
static lv_obj_t *s_hud_hp;
static lv_obj_t *s_hud_bomb;
static lv_obj_t *s_canvas;
static lv_obj_t *s_guide_label;
static lv_timer_t *s_game_timer;

static QueueHandle_t s_snd_queue;
static TaskHandle_t s_snd_task;
static int s_flash_timer = 0;

// 240x260 游戏绘制画布缓冲 (RGB565 格式)
#define CANVAS_W 240
#define CANVAS_H 250
static uint8_t s_canvas_buf[LV_CANVAS_BUF_SIZE_TRUE_COLOR(CANVAS_W, CANVAS_H)];

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
                // 激光开火音效 (快速扫频)
                for (int i = 0; i < 180; i++) {
                    int period = 8 + (i * 24 / 180);
                    buf[i] = (i % period < period / 2) ? 5000 : -5000;
                }
                bsp_audio_write(buf, 180 * sizeof(int16_t));
            } else if (snd == SND_HIT) {
                for (int i = 0; i < 80; i++) buf[i] = (i % 6 < 3) ? 4000 : -4000;
                bsp_audio_write(buf, 80 * sizeof(int16_t));
            } else if (snd == SND_EXPLODE) {
                // 击毁爆炸爆破音
                for (int c = 0; c < 8; c++) {
                    int amp = 7000 - c * 800;
                    for (int i = 0; i < 256; i++) {
                        buf[i] = (int16_t)(((rand() % 65536) - 32768) * amp / 32768);
                    }
                    bsp_audio_write(buf, 256 * sizeof(int16_t));
                }
            } else if (snd == SND_BOMB) {
                // 核弹全屏轰炸 (三次连续震荡)
                for (int round = 0; round < 3; round++) {
                    for (int c = 0; c < 12; c++) {
                        int amp = 9000 - c * 700;
                        for (int i = 0; i < 256; i++) {
                            buf[i] = (int16_t)(((rand() % 65536) - 32768) * amp / 32768);
                        }
                        bsp_audio_write(buf, 256 * sizeof(int16_t));
                    }
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
            } else if (snd == SND_POWERUP) {
                // 欢快拾取音
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

static void draw_rect_canvas(int x, int y, int w, int h, lv_color_t color)
{
    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.border_width = 0;
    dsc.radius = 0;

    lv_area_t coords;
    coords.x1 = x;
    coords.y1 = y;
    coords.x2 = x + w - 1;
    coords.y2 = y + h - 1;

    lv_draw_rect(&layer, &dsc, &coords);
    lv_canvas_finish_layer(s_canvas, &layer);
}

static void render_game_scene(void)
{
    if (!s_canvas) return;

    // 清空背景 (深邃夜空黑)
    lv_color_t bg_col = (s_flash_timer > 0) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x060913);
    lv_canvas_fill_bg(s_canvas, bg_col, LV_OPA_COVER);
    if (s_flash_timer > 0) {
        s_flash_timer--;
        return;
    }

    // 1. 绘制玩家机体 (大尺寸 30x28 像素)
    if (!s_game.game_over) {
        int px = (int)s_game.player_x;
        int py = (int)s_game.player_y - 30; // 适配画布高度

        // 喷射火焰动画
        int flame_h = 5 + (s_game.wave_tick % 3) * 3;
        draw_rect_canvas(px + 9, py + 24, 4, flame_h, lv_color_hex(0xFF9900));
        draw_rect_canvas(px + 17, py + 24, 4, flame_h, lv_color_hex(0xFF9900));

        // 战机机翼与机体
        draw_rect_canvas(px + 2, py + 12, 26, 8, lv_color_hex(0x00AACC));
        draw_rect_canvas(px, py + 16, 30, 6, lv_color_hex(0x0077AA));
        draw_rect_canvas(px + 11, py + 2, 8, 22, lv_color_hex(0x00E5FF));
        // 机鼻与座舱
        draw_rect_canvas(px + 13, py, 4, 12, lv_color_hex(0xFFFFFF));
        draw_rect_canvas(px + 12, py + 8, 6, 6, lv_color_hex(0xFFAA00));
        // 翼尖加农炮
        draw_rect_canvas(px, py + 10, 3, 6, lv_color_hex(0xFFD928));
        draw_rect_canvas(px + 27, py + 10, 3, 6, lv_color_hex(0xFFD928));
    }

    // 2. 绘制玩家激光 (大号粗光束)
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (s_game.bullets[i].active) {
            bullet_t *b = &s_game.bullets[i];
            draw_rect_canvas((int)b->x, (int)b->y - 30, b->w, b->h, lv_color_hex(b->color));
        }
    }

    // 3. 绘制敌机子弹
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        if (s_game.enemy_bullets[i].active) {
            bullet_t *eb = &s_game.enemy_bullets[i];
            draw_rect_canvas((int)eb->x, (int)eb->y - 30, eb->w, eb->h, lv_color_hex(0xFF2244));
        }
    }

    // 4. 绘制敌机
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        if (s_game.enemies[i].active) {
            enemy_t *e = &s_game.enemies[i];
            int ex = (int)e->x;
            int ey = (int)e->y - 30;

            if (e->type == ENEMY_SCOUT) { // 绿蜂侦察机 (24x20)
                draw_rect_canvas(ex + 8, ey, 8, 16, lv_color_hex(0x22CC44));
                draw_rect_canvas(ex + 2, ey + 4, 20, 8, lv_color_hex(0x88FF33));
                draw_rect_canvas(ex + 4, ey + 6, 4, 4, lv_color_hex(0xFF2222));
                draw_rect_canvas(ex + 16, ey + 6, 4, 4, lv_color_hex(0xFF2222));
            } else if (e->type == ENEMY_BOMBER) { // 红色重巡 (32x28)
                draw_rect_canvas(ex + 10, ey, 12, 24, lv_color_hex(0xCC2233));
                draw_rect_canvas(ex + 2, ey + 8, 28, 12, lv_color_hex(0xFF4455));
                draw_rect_canvas(ex + 12, ey + 8, 8, 8, lv_color_hex(0xFFD928));
            } else { // 巨型 BOSS (64x44)
                draw_rect_canvas(ex + 16, ey, 32, 40, lv_color_hex(0x552277));
                draw_rect_canvas(ex + 4, ey + 10, 56, 20, lv_color_hex(0x8833BB));
                draw_rect_canvas(ex + 24, ey + 28, 16, 12, lv_color_hex(0xFF0055));
                draw_rect_canvas(ex + 6, ey + 24, 6, 8, lv_color_hex(0xFFBB00));
                draw_rect_canvas(ex + 52, ey + 24, 6, 8, lv_color_hex(0xFFBB00));
            }
        }
    }

    // 5. 绘制道具 (带彩色边框的大块)
    for (int i = 0; i < THUNDER_MAX_ITEMS; i++) {
        if (s_game.items[i].active) {
            thunder_item_t *it = &s_game.items[i];
            lv_color_t c = (it->type == ITEM_TYPE_POWER) ? lv_color_hex(0xFFD928) :
                           ((it->type == ITEM_TYPE_BOMB) ? lv_color_hex(0xFF3344) : lv_color_hex(0x00E5FF));
            draw_rect_canvas((int)it->x, (int)it->y - 30, it->w, it->h, c);
        }
    }
}

static void game_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_game.game_over) {
        thunder_step(&s_game);

        if (s_game.bomb_triggered) {
            s_game.bomb_triggered = false;
            s_flash_timer = 3;
            send_sound(SND_BOMB);
        }
        if (s_game.shoot_timer == 0) {
            send_sound(SND_LASER);
        }

        // 刷新 HUD
        lv_label_set_text_fmt(s_hud_score, "SCORE:%d", s_game.score);

        char hp_str[16] = {0};
        for (int i = 0; i < s_game.player_max_hp; i++) {
            strcat(hp_str, (i < s_game.player_hp) ? "[*]" : "[ ]");
        }
        lv_label_set_text_fmt(s_hud_hp, "HP:%s", hp_str);
        lv_label_set_text_fmt(s_hud_bomb, "BOMB:x%d", s_game.bombs);

        if (s_game.game_over) {
            send_sound(SND_GAMEOVER);
            lv_label_set_text(s_guide_label, "MISSION FAILED! PRESS [OK] RETRY");
            lv_obj_set_style_text_color(s_guide_label, lv_color_hex(0xFF3333), 0);
        }
    }

    render_game_scene();
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
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x060913), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    // 1. 顶部 HUD
    s_hud_score = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_score, 6, 6);
    lv_label_set_text(s_hud_score, "SCORE:0");
    lv_obj_set_style_text_font(s_hud_score, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_score, lv_color_hex(0xFFFFFF), 0);

    s_hud_hp = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_hp, 110, 6);
    lv_label_set_text(s_hud_hp, "HP:[*][*][*]");
    lv_obj_set_style_text_font(s_hud_hp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_hp, lv_color_hex(0x33FF66), 0);

    s_hud_bomb = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_bomb, 182, 6);
    lv_label_set_text(s_hud_bomb, "BOMB:x2");
    lv_obj_set_style_text_font(s_hud_bomb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_bomb, lv_color_hex(0x00E5FF), 0);

    // 2. 游戏画布 (240 x 250)
    s_canvas = lv_canvas_create(s_scr);
    lv_canvas_set_buffer(s_canvas, s_canvas_buf, CANVAS_W, CANVAS_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_canvas, 0, 26);

    // 3. 底部按键说明
    s_guide_label = lv_label_create(s_scr);
    lv_obj_set_pos(s_guide_label, 0, 282);
    lv_obj_set_size(s_guide_label, 240, 36);
    lv_obj_set_style_text_align(s_guide_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_guide_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_guide_label, "UP: LEFT | DOWN: RIGHT\nOK: MEGA BOMB (CLEAR SCREEN)");
    lv_obj_set_style_text_color(s_guide_label, lv_color_hex(0xFFD928), 0);

    s_game_timer = lv_timer_create(game_timer_cb, 40, NULL); // 25 FPS 高速流畅刷新
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
        s_hud_score = s_hud_hp = s_hud_bomb = s_canvas = s_guide_label = NULL;
    }
}

void demo_thunder_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_PRESS && ev != BSP_BTN_CLICK) return;

    if (s_game.game_over) {
        demo_thunder_enter();
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
