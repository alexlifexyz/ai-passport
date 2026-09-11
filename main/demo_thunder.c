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

static const char *TAG __attribute__((unused)) = "demo_thunder";

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
static lv_obj_t *s_playfield;
static lv_obj_t *s_guide_label;
static lv_timer_t *s_game_timer;

static lv_obj_t *s_player_obj;
static lv_obj_t *s_player_flame;
static lv_obj_t *s_bullets[THUNDER_MAX_BULLETS];
static lv_obj_t *s_ebullets[THUNDER_MAX_ENEMY_BULLETS];
static lv_obj_t *s_enemies[THUNDER_MAX_ENEMIES];
static lv_obj_t *s_items[THUNDER_MAX_ITEMS];

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

static void update_render(void)
{
    if (!s_playfield) return;

    if (s_flash_timer > 0) {
        s_flash_timer--;
        lv_obj_set_style_bg_color(s_playfield, lv_color_hex(0xFFFFFF), 0);
    } else {
        lv_obj_set_style_bg_color(s_playfield, lv_color_hex(0x060913), 0);
    }

    // 1. 玩家战机
    if (!s_game.game_over) {
        lv_obj_remove_flag(s_player_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_player_flame, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_player_obj, (int)s_game.player_x, (int)s_game.player_y - 24);
        int flame_h = 4 + (s_game.wave_tick % 3) * 3;
        lv_obj_set_size(s_player_flame, 8, flame_h);
        lv_obj_set_pos(s_player_flame, (int)s_game.player_x + 11, (int)s_game.player_y - 24 + 26);
    } else {
        lv_obj_add_flag(s_player_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_player_flame, LV_OBJ_FLAG_HIDDEN);
    }

    // 2. 玩家子弹
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (s_game.bullets[i].active) {
            lv_obj_remove_flag(s_bullets[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(s_bullets[i], (int)s_game.bullets[i].x, (int)s_game.bullets[i].y - 24);
            lv_obj_set_size(s_bullets[i], s_game.bullets[i].w, s_game.bullets[i].h);
            lv_obj_set_style_bg_color(s_bullets[i], lv_color_hex(s_game.bullets[i].color), 0);
        } else {
            lv_obj_add_flag(s_bullets[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 3. 敌机子弹
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        if (s_game.enemy_bullets[i].active) {
            lv_obj_remove_flag(s_ebullets[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(s_ebullets[i], (int)s_game.enemy_bullets[i].x, (int)s_game.enemy_bullets[i].y - 24);
            lv_obj_set_size(s_ebullets[i], s_game.enemy_bullets[i].w, s_game.enemy_bullets[i].h);
        } else {
            lv_obj_add_flag(s_ebullets[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 4. 敌机
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        if (s_game.enemies[i].active) {
            enemy_t *e = &s_game.enemies[i];
            lv_obj_remove_flag(s_enemies[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(s_enemies[i], (int)e->x, (int)e->y - 24);
            lv_obj_set_size(s_enemies[i], e->w, e->h);

            if (e->type == ENEMY_SCOUT) {
                lv_obj_set_style_bg_color(s_enemies[i], lv_color_hex(0x22CC44), 0);
                lv_obj_set_style_border_color(s_enemies[i], lv_color_hex(0x88FF33), 0);
            } else if (e->type == ENEMY_BOMBER) {
                lv_obj_set_style_bg_color(s_enemies[i], lv_color_hex(0xCC2233), 0);
                lv_obj_set_style_border_color(s_enemies[i], lv_color_hex(0xFFD928), 0);
            } else {
                lv_obj_set_style_bg_color(s_enemies[i], lv_color_hex(0x7722AA), 0);
                lv_obj_set_style_border_color(s_enemies[i], lv_color_hex(0xFF0055), 0);
            }
        } else {
            lv_obj_add_flag(s_enemies[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 5. 掉落道具
    for (int i = 0; i < THUNDER_MAX_ITEMS; i++) {
        if (s_game.items[i].active) {
            thunder_item_t *it = &s_game.items[i];
            lv_obj_remove_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(s_items[i], (int)it->x, (int)it->y - 24);
            lv_color_t col = (it->type == ITEM_TYPE_POWER) ? lv_color_hex(0xFFD928) :
                             ((it->type == ITEM_TYPE_BOMB) ? lv_color_hex(0xFF3344) : lv_color_hex(0x00E5FF));
            lv_obj_set_style_bg_color(s_items[i], col, 0);
        } else {
            lv_obj_add_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);
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

    update_render();
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

    // 2. 战场容器 (240x250)
    s_playfield = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_playfield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_playfield, 0, 26);
    lv_obj_set_size(s_playfield, 240, 252);
    lv_obj_set_style_bg_color(s_playfield, lv_color_hex(0x060913), 0);
    lv_obj_set_style_border_width(s_playfield, 0, 0);
    lv_obj_set_style_pad_all(s_playfield, 0, 0);

    // 2.1 玩家战机
    s_player_obj = lv_obj_create(s_playfield);
    lv_obj_remove_flag(s_player_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_player_obj, 30, 26);
    lv_obj_set_style_bg_color(s_player_obj, lv_color_hex(0x00AACC), 0);
    lv_obj_set_style_border_color(s_player_obj, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s_player_obj, 2, 0);
    lv_obj_set_style_radius(s_player_obj, 4, 0);

    s_player_flame = lv_obj_create(s_playfield);
    lv_obj_remove_flag(s_player_flame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_player_flame, 8, 6);
    lv_obj_set_style_bg_color(s_player_flame, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_border_width(s_player_flame, 0, 0);

    // 2.2 玩家子弹池
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        s_bullets[i] = lv_obj_create(s_playfield);
        lv_obj_remove_flag(s_bullets[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_bullets[i], 4, 14);
        lv_obj_set_style_bg_color(s_bullets[i], lv_color_hex(0xFFD928), 0);
        lv_obj_set_style_border_width(s_bullets[i], 0, 0);
        lv_obj_set_style_radius(s_bullets[i], 2, 0);
        lv_obj_add_flag(s_bullets[i], LV_OBJ_FLAG_HIDDEN);
    }

    // 2.3 敌机子弹池
    for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) {
        s_ebullets[i] = lv_obj_create(s_playfield);
        lv_obj_remove_flag(s_ebullets[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_ebullets[i], 4, 8);
        lv_obj_set_style_bg_color(s_ebullets[i], lv_color_hex(0xFF2244), 0);
        lv_obj_set_style_border_width(s_ebullets[i], 0, 0);
        lv_obj_set_style_radius(s_ebullets[i], 2, 0);
        lv_obj_add_flag(s_ebullets[i], LV_OBJ_FLAG_HIDDEN);
    }

    // 2.4 敌机池
    for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) {
        s_enemies[i] = lv_obj_create(s_playfield);
        lv_obj_remove_flag(s_enemies[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_enemies[i], 24, 20);
        lv_obj_set_style_border_width(s_enemies[i], 2, 0);
        lv_obj_set_style_radius(s_enemies[i], 4, 0);
        lv_obj_add_flag(s_enemies[i], LV_OBJ_FLAG_HIDDEN);
    }

    // 2.5 道具池
    for (int i = 0; i < THUNDER_MAX_ITEMS; i++) {
        s_items[i] = lv_obj_create(s_playfield);
        lv_obj_remove_flag(s_items[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_items[i], 14, 14);
        lv_obj_set_style_border_color(s_items[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(s_items[i], 2, 0);
        lv_obj_set_style_radius(s_items[i], 3, 0);
        lv_obj_add_flag(s_items[i], LV_OBJ_FLAG_HIDDEN);
    }

    // 3. 底部操作指引
    s_guide_label = lv_label_create(s_scr);
    lv_obj_set_pos(s_guide_label, 0, 282);
    lv_obj_set_size(s_guide_label, 240, 36);
    lv_obj_set_style_text_align(s_guide_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_guide_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_guide_label, "UP: LEFT | DOWN: RIGHT\nOK: MEGA BOMB (CLEAR SCREEN)");
    lv_obj_set_style_text_color(s_guide_label, lv_color_hex(0xFFD928), 0);

    s_game_timer = lv_timer_create(game_timer_cb, 40, NULL);
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
        s_hud_score = s_hud_hp = s_hud_bomb = s_playfield = s_guide_label = NULL;
        s_player_obj = s_player_flame = NULL;
        for (int i = 0; i < THUNDER_MAX_BULLETS; i++) s_bullets[i] = NULL;
        for (int i = 0; i < THUNDER_MAX_ENEMY_BULLETS; i++) s_ebullets[i] = NULL;
        for (int i = 0; i < THUNDER_MAX_ENEMIES; i++) s_enemies[i] = NULL;
        for (int i = 0; i < THUNDER_MAX_ITEMS; i++) s_items[i] = NULL;
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
