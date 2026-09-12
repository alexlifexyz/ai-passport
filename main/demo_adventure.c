// main/demo_adventure.c —— 《像素冒险岛 HD》(Pixel Adventure Island HD)
// 基于零 DRAM 开销的原生 LVGL 9.x 矢量绘制，带 16kHz 街机风音频合成、跑酷避障、战斧投掷、踩踏反弹与水果饱食。
#include "demo.h"
#include "adventure_logic.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG __attribute__((unused)) = "demo_adventure";

#define SCREEN_W 240
#define SCREEN_H 320

typedef enum {
    ADV_SND_NONE = 0,
    ADV_SND_JUMP,
    ADV_SND_THROW,
    ADV_SND_HIT,
    ADV_SND_KILL,
    ADV_SND_FRUIT,
    ADV_SND_HURT,
    ADV_SND_EGG,
    ADV_SND_GAMEOVER
} adv_snd_t;

static adventure_game_t s_game;
static lv_obj_t *s_scr;
static lv_obj_t *s_playfield;
static lv_obj_t *s_hud_score;
static lv_obj_t *s_hud_lives;
static lv_obj_t *s_hud_weapon;
static lv_obj_t *s_hud_stamina;
static lv_obj_t *s_hud_hints;
static lv_obj_t *s_gameover_box;
static lv_obj_t *s_pause_box = NULL;
static lv_obj_t *s_pause_vol_label = NULL;
static uint8_t s_volume = 80;
static bool s_paused = false;

static int s_up_hold_ticks = 0;
static int s_down_hold_ticks = 0;
static lv_timer_t *s_game_timer;

static QueueHandle_t s_snd_queue;
static TaskHandle_t s_snd_task;
static uint32_t s_frame_tick = 0;
static uint32_t s_spawn_enemy_timer = 0;
static uint32_t s_spawn_item_timer = 0;

static void send_adv_sound(adv_snd_t snd)
{
    if (s_snd_queue) {
        xQueueSend(s_snd_queue, &snd, 0);
    }
}

// 独立后台音效合成任务 (16kHz 16-bit 复古街机音效)
static void adv_audio_task(void *arg)
{
    (void)arg;
    adv_snd_t snd;
    int16_t buf[256];

    bsp_audio_set_format(16000, 16, 1);
    bsp_audio_set_volume(85);

    while (1) {
        if (xQueueReceive(s_snd_queue, &snd, portMAX_DELAY) == pdTRUE) {
            if (snd == ADV_SND_NONE) continue;

            if (snd == ADV_SND_JUMP) {
                // 跳跃音效 (260Hz -> 580Hz 快速上升滑音)
                const int total = 960;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 260.0f + t * 320.0f;
                    phase += (freq / 16000.0f);
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 6500.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == ADV_SND_THROW) {
                // 战斧投掷破空声 (520Hz 下降至 220Hz)
                const int total = 800;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 520.0f - t * 300.0f;
                    phase += (freq / 16000.0f);
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 5500.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == ADV_SND_KILL) {
                // 击杀敌人双音阶清脆提示 (587Hz -> 880Hz)
                const int freqs[] = { 587, 880 };
                for (int f = 0; f < 2; f++) {
                    const int total = 640;
                    float phase = 0.0f;
                    float freq = (float)freqs[f];
                    for (int i = 0; i < total; i++) {
                        float t = (float)i / (float)total;
                        phase += (freq / 16000.0f);
                        if (phase >= 1.0f) phase -= 1.0f;
                        float amp = (1.0f - t) * 7000.0f;
                        buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                        if ((i % 256) == 255 || i == total - 1) {
                            bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                        }
                    }
                }
            } else if (snd == ADV_SND_FRUIT) {
                // 捡起水果金币声 (988Hz -> 1318Hz)
                const int total = 800;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 988.0f + t * 330.0f;
                    phase += (freq / 16000.0f);
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 6000.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == ADV_SND_EGG) {
                // 恐龙蛋额外生命 (C大调 1UP 和弦 523 -> 659 -> 784 -> 1046)
                const int chord[] = { 523, 659, 784, 1046 };
                for (int c = 0; c < 4; c++) {
                    const int total = 500;
                    float phase = 0.0f;
                    float freq = (float)chord[c];
                    for (int i = 0; i < total; i++) {
                        float t = (float)i / (float)total;
                        phase += (freq / 16000.0f);
                        if (phase >= 1.0f) phase -= 1.0f;
                        float amp = (1.0f - t) * 7500.0f;
                        buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                        if ((i % 256) == 255 || i == total - 1) {
                            bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                        }
                    }
                }
            } else if (snd == ADV_SND_HURT) {
                // 受伤低沉蜂鸣
                const int total = 1600;
                int16_t last_sample = 0;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float amp = (1.0f - t) * 8000.0f;
                    int16_t raw_noise = (int16_t)(((rand() % 65536) - 32768) * amp / 32768.0f);
                    last_sample = (int16_t)((last_sample * 2 + raw_noise) / 3);
                    buf[i % 256] = last_sample;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == ADV_SND_GAMEOVER) {
                // 游戏结束悲伤下降音阶
                const int sad[] = { 392, 370, 349, 330 };
                for (int s = 0; s < 4; s++) {
                    const int total = 900;
                    float phase = 0.0f;
                    float freq = (float)sad[s];
                    for (int i = 0; i < total; i++) {
                        float t = (float)i / (float)total;
                        phase += (freq / 16000.0f);
                        if (phase >= 1.0f) phase -= 1.0f;
                        float amp = (1.0f - t) * 6000.0f;
                        buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                        if ((i % 256) == 255 || i == total - 1) {
                            bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                        }
                    }
                }
            }
        }
    }
}

// 零 DRAM 开销纯矩形快速绘制
static inline void draw_box(lv_layer_t *layer, int x, int y, int w, int h, uint32_t rgb)
{
    if (x >= SCREEN_W || y >= SCREEN_H || x + w <= 0 || y + h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(rgb);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;

    lv_area_t a;
    a.x1 = x;
    a.y1 = y;
    a.x2 = x + w - 1;
    a.y2 = y + h - 1;
    lv_draw_rect(layer, &dsc, &a);
}

// LVGL 9.x 矢量即时绘制回调
static void playfield_draw_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_MAIN) return;

    lv_layer_t *layer = lv_event_get_layer(e);
    if (!layer) return;

    // 1. 热带海岛蔚蓝天空
    draw_box(layer, 0, 0, SCREEN_W, (int)ADVENTURE_GROUND_Y, 0x38BDF8);

    // 远景白云与山脉
    int cloud_offset = (s_frame_tick / 2) % (SCREEN_W + 60);
    draw_box(layer, SCREEN_W - cloud_offset, 50, 48, 14, 0xE0F2FE);
    draw_box(layer, SCREEN_W - cloud_offset + 8, 44, 32, 10, 0xF0F9FF);

    // 椰子树随卷轴滚动
    int tree_scroll = (s_frame_tick * 2) % 180;
    for (int t = 0; t < 3; t++) {
        int tx = ((t * 90 - tree_scroll) % 270 + 270) % 270 - 20;
        // 树干
        draw_box(layer, tx + 14, 195, 6, 65, 0x78350F);
        // 椰子树冠 (深绿草木)
        draw_box(layer, tx, 175, 34, 20, 0x166534);
        draw_box(layer, tx + 6, 170, 22, 8, 0x15803D);
    }

    // 2. 地面与沙滩泥土
    draw_box(layer, 0, (int)ADVENTURE_GROUND_Y, SCREEN_W, 14, 0x22C55E); // 草原顶层
    draw_box(layer, 0, (int)ADVENTURE_GROUND_Y + 14, SCREEN_W, SCREEN_H - (int)ADVENTURE_GROUND_Y - 14, 0x854D0E); // 热带沙土

    // 3. 绘制补给道具 (香蕉/菠萝/恐龙蛋)
    for (int i = 0; i < ADVENTURE_MAX_ITEMS; i++) {
        adventure_item_t *it = &s_game.items[i];
        if (!it->active) continue;
        int ix = (int)it->x;
        int iy = (int)it->y;

        if (it->type == ADV_ITEM_BANANA) {
            // 香蕉：亮黄弧形水果
            draw_box(layer, ix + 2, iy + 2, 12, 6, 0xFACC15);
            draw_box(layer, ix, iy, 4, 4, 0xCA8A04);
        } else if (it->type == ADV_ITEM_PINEAPPLE) {
            // 菠萝：金黄果身 + 绿冠
            draw_box(layer, ix + 2, iy + 4, 12, 12, 0xF59E0B);
            draw_box(layer, ix + 4, iy, 8, 4, 0x15803D);
        } else if (it->type == ADV_ITEM_EGG) {
            // 恐龙蛋：金光巨蛋 (白金光芒)
            draw_box(layer, ix + 2, iy, 12, 16, 0xFEF08A);
            draw_box(layer, ix, iy + 4, 16, 10, 0xFDE047);
            draw_box(layer, ix + 4, iy + 2, 4, 4, 0xFFFFFF); // 高光
        }
    }

    // 4. 绘制敌人 (蜗牛/青蛙/飞鸟)
    for (int i = 0; i < ADVENTURE_MAX_ENEMIES; i++) {
        adventure_enemy_t *e = &s_game.enemies[i];
        if (!e->active) continue;
        int ex = (int)e->x;
        int ey = (int)e->y;

        if (e->type == ADV_ENEMY_SNAIL) {
            // 蜗牛：棕褐色坚硬螺旋壳 + 浅黄身躯
            draw_box(layer, ex + 4, ey, 14, 12, 0x92400E);
            draw_box(layer, ex, ey + 6, 8, 8, 0xFDE68A);
            draw_box(layer, ex + 1, ey + 4, 2, 3, 0x000000); // 触角
        } else if (e->type == ADV_ENEMY_FROG) {
            // 青蛙：翠绿跳跃体型 + 萌萌大眼
            draw_box(layer, ex + 2, ey + 4, 14, 12, 0x16A34A);
            draw_box(layer, ex, ey + 2, 5, 5, 0xFFFFFF);
            draw_box(layer, ex + 13, ey + 2, 5, 5, 0xFFFFFF);
            draw_box(layer, ex + 2, ey + 3, 2, 2, 0x000000);
            draw_box(layer, ex + 14, ey + 3, 2, 2, 0x000000);
        } else if (e->type == ADV_ENEMY_BIRD) {
            // 飞鸟：红色羽翼 + 金黄鸟嘴
            draw_box(layer, ex + 4, ey + 2, 12, 8, 0xEF4444);
            draw_box(layer, ex, ey + 4, 6, 4, 0xFBBF24); // 鸟喙
            int wing = ((s_frame_tick / 4) % 2 == 0) ? -4 : 4;
            draw_box(layer, ex + 8, ey + 4 + wing, 8, 4, 0xDC2626); // 翅膀
        }
    }

    // 5. 绘制投射物 (石斧/飞刀/月亮刃)
    for (int i = 0; i < ADVENTURE_MAX_PROJECTILES; i++) {
        adventure_projectile_t *p = &s_game.projectiles[i];
        if (!p->active) continue;
        int px = (int)p->x;
        int py = (int)p->y;

        if (p->type == ADV_WEAPON_AXE) {
            // 石斧：银灰石刃 + 木柄
            draw_box(layer, px, py, 6, 12, 0x78350F); // 柄
            draw_box(layer, px + 2, py + 2, 8, 8, 0x94A3B8); // 斧刃
        } else if (p->type == ADV_WEAPON_KNIFE) {
            // 飞刀：锐利银白直线刃
            draw_box(layer, px, py + 2, 14, 4, 0xF1F5F9);
            draw_box(layer, px, py + 2, 4, 4, 0x475569);
        } else if (p->type == ADV_WEAPON_MOON_BLADE) {
            // 月亮刃：霓虹炫紫半月贯穿轮
            draw_box(layer, px, py, 14, 14, 0xE879F9);
            draw_box(layer, px + 3, py + 3, 8, 8, 0x38BDF8);
        }
    }

    // 6. 绘制玩家 (冒险岛高桥名人：红白鸭舌帽 + 草裙)
    if (s_game.player.is_alive) {
        // 受伤无敌闪烁
        bool show_player = true;
        if (s_game.player.invincible_timer_ms > 0 && ((s_game.player.invincible_timer_ms / 60) % 2 == 0)) {
            show_player = false;
        }

        if (show_player) {
            int px = (int)s_game.player.x;
            int py = (int)s_game.player.y;

            // 红色鸭舌帽
            draw_box(layer, px + 2, py, 12, 5, 0xEF4444);
            draw_box(layer, px + 8, py + 3, 6, 2, 0xEF4444); // 鸭舌

            // 脸部与大眼
            draw_box(layer, px + 3, py + 5, 10, 7, 0xFDBA74);
            draw_box(layer, px + 8, py + 6, 2, 3, 0x000000);

            // 肌肉上身
            draw_box(layer, px + 3, py + 12, 10, 5, 0xFDBA74);

            // 草裙 / 短裤
            draw_box(layer, px + 2, py + 17, 12, 4, 0x15803D);

            // 奔跑小脚
            int leg_step = ((int)(s_game.player.x) / 6) % 2;
            if (s_game.player.on_ground) {
                draw_box(layer, px + (leg_step ? 2 : 8), py + 21, 4, 3, 0x78350F);
            } else {
                draw_box(layer, px + 3, py + 20, 4, 4, 0x78350F); // 腾空缩腿
            }
        }
    }
}

// 游戏逻辑定时器 (30ms 刷新循环)
static void game_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_frame_tick++;

    if (!s_paused) {
        // 1. 物理步进
        adventure_logic_update(&s_game, 30);

        // 2. 自动生成敌人与道具逻辑
        s_spawn_enemy_timer += 30;
        if (s_spawn_enemy_timer >= 1800) {
            s_spawn_enemy_timer = 0;
            int r = rand() % 100;
            if (r < 40) {
                adventure_logic_spawn_enemy(&s_game, ADV_ENEMY_SNAIL, SCREEN_W + 10, ADVENTURE_GROUND_Y - 14);
            } else if (r < 75) {
                adventure_logic_spawn_enemy(&s_game, ADV_ENEMY_FROG, SCREEN_W + 10, ADVENTURE_GROUND_Y - 18);
            } else {
                adventure_logic_spawn_enemy(&s_game, ADV_ENEMY_BIRD, SCREEN_W + 10, 140 + (rand() % 40));
            }
        }

        s_spawn_item_timer += 30;
        if (s_spawn_item_timer >= 2800) {
            s_spawn_item_timer = 0;
            int r = rand() % 100;
            if (r < 50) {
                adventure_logic_spawn_item(&s_game, ADV_ITEM_BANANA, SCREEN_W + 10, ADVENTURE_GROUND_Y - 16);
            } else if (r < 80) {
                adventure_logic_spawn_item(&s_game, ADV_ITEM_PINEAPPLE, SCREEN_W + 10, ADVENTURE_GROUND_Y - 18);
            } else {
                adventure_logic_spawn_item(&s_game, ADV_ITEM_EGG, SCREEN_W + 10, ADVENTURE_GROUND_Y - 20);
            }
        }

        // 3. 事件音频响应
        if (s_game.events.jump)         send_adv_sound(ADV_SND_JUMP);
        if (s_game.events.throw_weapon) send_adv_sound(ADV_SND_THROW);
        if (s_game.events.enemy_killed) send_adv_sound(ADV_SND_KILL);
        if (s_game.events.pickup_fruit) send_adv_sound(ADV_SND_FRUIT);
        if (s_game.events.extra_life)   send_adv_sound(ADV_SND_EGG);
        if (s_game.events.player_hurt)  send_adv_sound(ADV_SND_HURT);
        if (s_game.events.game_over)    send_adv_sound(ADV_SND_GAMEOVER);

        // 4. 长按按键平滑移动支持
        if (s_game.player.is_alive) {
            int mv = bsp_button_read_mv();
            if (mv >= 0 && mv < 150) { // UP 键按住
                s_up_hold_ticks++;
                if (s_up_hold_ticks > 4) {
                    adventure_logic_move_left(&s_game);
                }
            } else {
                s_up_hold_ticks = 0;
            }

            if (mv >= 150 && mv < 447) { // DOWN 键按住
                s_down_hold_ticks++;
                if (s_down_hold_ticks > 4) {
                    adventure_logic_move_right(&s_game);
                }
            } else {
                s_down_hold_ticks = 0;
            }
        }
    }

    // 5. 更新 HUD
    if (s_hud_score) {
        char buf[32];
        if (s_game.player.combo > 1) {
            snprintf(buf, sizeof(buf), "x%d %d", s_game.player.combo, s_game.player.score);
        } else {
            snprintf(buf, sizeof(buf), "SC:%d", s_game.player.score);
        }
        lv_label_set_text(s_hud_score, buf);
        lv_obj_set_style_text_color(s_hud_score,
            s_game.player.combo > 1 ? lv_color_hex(0x00E5FF) : lv_color_hex(0xFFD700), 0);
    }

    if (s_hud_lives) {
        char buf[32];
        int lv = s_game.player.lives;
        if (lv <= 0) snprintf(buf, sizeof(buf), "DEAD");
        else if (lv == 1) snprintf(buf, sizeof(buf), "HP:*");
        else if (lv == 2) snprintf(buf, sizeof(buf), "HP:**");
        else snprintf(buf, sizeof(buf), "HP:***");
        lv_label_set_text(s_hud_lives, buf);
    }

    if (s_hud_stamina) {
        char buf[32];
        snprintf(buf, sizeof(buf), "FOOD:%d%%", (int)s_game.player.stamina);
        lv_label_set_text(s_hud_stamina, buf);
        lv_obj_set_style_text_color(s_hud_stamina,
            s_game.player.stamina > 30.0f ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);
    }

    // 6. 游戏结束弹窗
    if (s_game.game_over && !s_gameover_box) {
        s_gameover_box = lv_obj_create(s_scr);
        lv_obj_set_size(s_gameover_box, 200, 84);
        lv_obj_center(s_gameover_box);
        lv_obj_set_style_bg_color(s_gameover_box, lv_color_hex(0x0A0505), 0);
        lv_obj_set_style_bg_opa(s_gameover_box, LV_OPA_90, 0);
        lv_obj_set_style_border_color(s_gameover_box, lv_color_hex(0xEF4444), 0);
        lv_obj_set_style_border_width(s_gameover_box, 2, 0);
        lv_obj_clear_flag(s_gameover_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(s_gameover_box);
        lv_label_set_text(lbl, "ISLAND RUN OVER!\nPress OK to Restart");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFD700), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);
    } else if (!s_game.game_over && s_gameover_box) {
        lv_obj_delete(s_gameover_box);
        s_gameover_box = NULL;
    }

    lv_obj_invalidate(s_playfield);
}

// 演示页进入
void demo_adventure_enter(void)
{
    ESP_LOGI(TAG, "启动《像素冒险岛 HD》");
    adventure_logic_init(&s_game);
    s_paused = false;

    if (!s_snd_queue) {
        s_snd_queue = xQueueCreate(16, sizeof(adv_snd_t));
        xTaskCreate(adv_audio_task, "adv_audio", 4096, NULL, 5, &s_snd_task);
    }

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x060A14), 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_playfield = lv_obj_create(s_scr);
    lv_obj_set_size(s_playfield, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_playfield, 0, 0);
    lv_obj_set_style_bg_opa(s_playfield, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_playfield, 0, 0);
    lv_obj_clear_flag(s_playfield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_playfield, playfield_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    // 顶部 HUD
    lv_obj_t *hud_bar = lv_obj_create(s_scr);
    lv_obj_set_size(hud_bar, SCREEN_W, 26);
    lv_obj_set_pos(hud_bar, 0, 4);
    lv_obj_set_style_bg_color(hud_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hud_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(hud_bar, 0, 0);
    lv_obj_clear_flag(hud_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_hud_score = lv_label_create(hud_bar);
    lv_label_set_text(s_hud_score, "SC:0");
    lv_obj_set_style_text_font(s_hud_score, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_score, lv_color_hex(0xFFD700), 0);
    lv_obj_set_pos(s_hud_score, 8, 4);

    s_hud_lives = lv_label_create(hud_bar);
    lv_label_set_text(s_hud_lives, "HP:***");
    lv_obj_set_style_text_font(s_hud_lives, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_lives, lv_color_hex(0xFF4466), 0);
    lv_obj_set_pos(s_hud_lives, 95, 4);

    s_hud_stamina = lv_label_create(hud_bar);
    lv_label_set_text(s_hud_stamina, "FOOD:100%");
    lv_obj_set_style_text_font(s_hud_stamina, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_stamina, lv_color_hex(0x22C55E), 0);
    lv_obj_set_pos(s_hud_stamina, 160, 4);

    // 底部操作栏提示
    s_hud_hints = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_hints, 0, 303);
    lv_obj_set_size(s_hud_hints, 240, 16);
    lv_obj_set_style_text_align(s_hud_hints, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_hud_hints, "UP:BACK  DN:RUN  OK:JUMP/THROW");
    lv_obj_set_style_text_font(s_hud_hints, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_hints, lv_color_hex(0xFFD700), 0);

    // 暂停半透明弹窗
    s_pause_box = lv_obj_create(s_scr);
    lv_obj_set_size(s_pause_box, 180, 110);
    lv_obj_center(s_pause_box);
    lv_obj_set_style_bg_color(s_pause_box, lv_color_hex(0x0A1020), 0);
    lv_obj_set_style_bg_opa(s_pause_box, LV_OPA_90, 0);
    lv_obj_set_style_border_color(s_pause_box, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(s_pause_box, 2, 0);
    lv_obj_set_style_pad_all(s_pause_box, 8, 0);
    lv_obj_add_flag(s_pause_box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *pt = lv_label_create(s_pause_box);
    lv_label_set_text(pt, "-- PAUSED --");
    lv_obj_set_style_text_font(pt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pt, lv_color_hex(0xFFD700), 0);
    lv_obj_align(pt, LV_ALIGN_TOP_MID, 0, 2);

    s_pause_vol_label = lv_label_create(s_pause_box);
    lv_label_set_text_fmt(s_pause_vol_label, "VOLUME: %d%%", s_volume);
    lv_obj_set_style_text_font(s_pause_vol_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_pause_vol_label, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(s_pause_vol_label, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t *ph = lv_label_create(s_pause_box);
    lv_label_set_text(ph, "UP/DN: VOL\nOK: RESUME");
    lv_obj_set_style_text_font(ph, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ph, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_align(ph, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ph, LV_ALIGN_BOTTOM_MID, 0, 0);

    s_gameover_box = NULL;
    lv_screen_load(s_scr);

    s_game_timer = lv_timer_create(game_timer_cb, 30, NULL);
}

// 演示页退出
void demo_adventure_exit(void)
{
    ESP_LOGI(TAG, "退出《像素冒险岛 HD》");
    if (s_game_timer) {
        lv_timer_delete(s_game_timer);
        s_game_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_playfield = NULL;
        s_hud_score = NULL;
        s_hud_lives = NULL;
        s_hud_weapon = NULL;
        s_hud_stamina = NULL;
        s_hud_hints = NULL;
        s_pause_box = NULL;
        s_pause_vol_label = NULL;
        s_gameover_box = NULL;
    }
    if (s_snd_queue) {
        vQueueDelete(s_snd_queue);
        s_snd_queue = NULL;
    }
    if (s_snd_task) {
        vTaskDelete(s_snd_task);
        s_snd_task = NULL;
    }
}

// 硬件按键分发
void demo_adventure_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    // 游戏结束状态：按 OK 重生重开
    if (s_game.game_over) {
        if (btn == BSP_BTN_OK && (ev == BSP_BTN_CLICK || ev == BSP_BTN_PRESS)) {
            adventure_logic_restart(&s_game);
            if (s_gameover_box) {
                lv_obj_delete(s_gameover_box);
                s_gameover_box = NULL;
            }
        }
        return;
    }

    // 暂停状态
    if (s_paused) {
        if (btn == BSP_BTN_UP && (ev == BSP_BTN_CLICK || ev == BSP_BTN_PRESS)) {
            if (s_volume <= 90) s_volume += 10; else s_volume = 100;
            bsp_audio_set_volume(s_volume);
            if (s_pause_vol_label) {
                lv_label_set_text_fmt(s_pause_vol_label, "VOLUME: %d%%", s_volume);
            }
        } else if (btn == BSP_BTN_DOWN && (ev == BSP_BTN_CLICK || ev == BSP_BTN_PRESS)) {
            if (s_volume >= 10) s_volume -= 10; else s_volume = 0;
            bsp_audio_set_volume(s_volume);
            if (s_pause_vol_label) {
                lv_label_set_text_fmt(s_pause_vol_label, "VOLUME: %d%%", s_volume);
            }
        } else if (btn == BSP_BTN_OK && (ev == BSP_BTN_CLICK || ev == BSP_BTN_PRESS)) {
            s_paused = false;
            if (s_pause_box) {
                lv_obj_add_flag(s_pause_box, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return;
    }

    // 正常游玩输入
    if (btn == BSP_BTN_OK) {
        if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK) {
            adventure_logic_action(&s_game, ADV_ACTION_OK);
        }
        return;
    }

    if (btn == BSP_BTN_UP) {
        if (ev == BSP_BTN_PRESS) {
            adventure_logic_move_left(&s_game);
        } else if (ev == BSP_BTN_DOUBLE) {
            s_paused = true;
            if (s_pause_box) {
                if (s_pause_vol_label) {
                    lv_label_set_text_fmt(s_pause_vol_label, "VOLUME: %d%%", s_volume);
                }
                lv_obj_remove_flag(s_pause_box, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else if (btn == BSP_BTN_DOWN) {
        if (ev == BSP_BTN_PRESS) {
            adventure_logic_move_right(&s_game);
        }
    }
}
