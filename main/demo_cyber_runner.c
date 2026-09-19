// main/demo_cyber_runner.c —— 《霓虹疾行：影刃闪现》(Cyber Courier: Phantom Dash) 掌机固件驱动
// 极致流畅 40 FPS，零卡顿合批矢量图元与视差天际线，超轻量三键机动连招，16kHz 赛博合成器音效。
#include "demo.h"
#include "cyber_runner_logic.h"
#include "bsp_display.h"
#include "bsp_audio.h"
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

static const char *TAG __attribute__((unused)) = "demo_cyber_runner";

#define SCREEN_W 240
#define SCREEN_H 320

static cr_game_t    s_game;
static lv_obj_t    *s_scr = NULL;
static lv_obj_t    *s_playfield = NULL;
static lv_obj_t    *s_hud_score = NULL;
static lv_obj_t    *s_hud_hp = NULL;
static lv_obj_t    *s_hud_blink = NULL;
static lv_obj_t    *s_hud_combo = NULL;
static lv_obj_t    *s_gameover_box = NULL;
static lv_timer_t  *s_game_timer = NULL;

static QueueHandle_t s_snd_queue = NULL;
static TaskHandle_t  s_snd_task = NULL;
static volatile bool s_audio_running = false;
static uint8_t       s_volume = 85;

static void send_sound(cr_sound_t snd)
{
    if (s_snd_queue && snd != CR_SND_NONE) {
        xQueueSend(s_snd_queue, &snd, 0);
    }
}

// 快速后台音效合成任务 (16kHz 16-bit 赛博合成器音效，清脆低延迟)
static void cyber_runner_audio_task(void *arg)
{
    (void)arg;
    cr_sound_t snd;
    static int16_t buf[256];

    bsp_audio_set_format(16000, 16, 1);
    bsp_audio_set_volume(s_volume);

    while (s_audio_running) {
        if (xQueueReceive(s_snd_queue, &snd, pdMS_TO_TICKS(40)) == pdTRUE) {
            if (snd == CR_SND_NONE) continue;

            if (snd == CR_SND_JUMP) {
                // 离子起跳 (350Hz -> 850Hz 快速升频方波, 25ms)
                const int total = 400;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 350.0f + t * 500.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 6000.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_AIR_BOOST) {
                // 二段喷气腾空 (双频调制短脉冲, 25ms)
                const int total = 400;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 600.0f + t * 400.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 1600) - 800;
                    float amp = (1.0f - t) * 6500.0f;
                    buf[i % 256] = (int16_t)(((phase < 0.5f) ? amp : -amp) + noise);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_BLINK) {
                // 幽灵闪现电子虚化滑音 (1300Hz -> 300Hz 强混响方波, 38ms)
                const int total = 600;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 1300.0f - t * 1000.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t * 0.7f) * 7000.0f;
                    buf[i % 256] = (phase < 0.4f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_SLIDE) {
                // 贴地滑铲摩擦 (高频白噪切削, 30ms)
                const int total = 480;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    int noise = (rand() % 5000) - 2500;
                    buf[i % 256] = (int16_t)(noise * (1.0f - t));
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_WALL_KICK) {
                // 蹬墙反弹跳金属铮鸣 (450Hz -> 900Hz 快速升频, 25ms)
                const int total = 400;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 450.0f + t * 450.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 7500.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_SLASH_HIT) {
                // 影刃斩爆目标 (爽脆金属切削+双频炸裂, 40ms)
                const int total = 640;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 1200.0f - t * 800.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 4000) - 2000;
                    float amp = (1.0f - t * 0.8f) * 8500.0f;
                    buf[i % 256] = (int16_t)(((phase < 0.4f) ? amp : -amp) + noise);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_DIVE_SLAM) {
                // 俯冲砸地震地轰鸣 (低频低通爆震, 45ms)
                const int total = 720;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 180.0f - t * 100.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 4500) - 2250;
                    float amp = (1.0f - t) * 8000.0f;
                    buf[i % 256] = (int16_t)(((phase < 0.5f) ? amp : -amp) + noise);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_DRONE_POP) {
                // 闪现穿爆无人机炸裂音 (高低频爆裂, 35ms)
                const int total = 560;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 300.0f - t * 200.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 3600) - 1800;
                    float amp = (1.0f - t) * 8000.0f;
                    buf[i % 256] = (int16_t)(((phase < 0.5f) ? amp : -amp) + noise);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_VENT_BOOST) {
                // 排风口暴风喷射音 (低沉上升风鸣, 40ms)
                const int total = 640;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 180.0f + t * 450.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 2400) - 1200;
                    float amp = (1.0f - t) * 6500.0f;
                    buf[i % 256] = (int16_t)(((phase < 0.5f) ? amp : -amp) + noise);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_GEM) {
                // 拾取数据晶体双音和弦 (880Hz + 1320Hz, 30ms)
                const int total = 480;
                float p1 = 0.0f, p2 = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    p1 += 880.0f / 16000.0f; if (p1 >= 1.0f) p1 -= 1.0f;
                    p2 += 1320.0f / 16000.0f; if (p2 >= 1.0f) p2 -= 1.0f;
                    float amp = (1.0f - t) * 4000.0f;
                    int16_t s1 = (p1 < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    int16_t s2 = (p2 < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    buf[i % 256] = (s1 + s2) / 2;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_HURT) {
                // 受创警报音 (160Hz 尖锐锯齿音, 35ms)
                const int total = 560;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    phase += 160.0f / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 7000.0f;
                    buf[i % 256] = (int16_t)((phase * 2.0f - 1.0f) * amp);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == CR_SND_GAMEOVER) {
                // 坠落停机 (下行滑音 600Hz -> 80Hz, 80ms)
                const int total = 1280;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 600.0f - t * 520.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 7000.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}

// 快速绘制填充矩形
static inline void draw_box(lv_layer_t *layer, int x, int y, int w, int h, uint32_t color_hex)
{
    if (w <= 0 || h <= 0) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(color_hex);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    lv_area_t coords = {
        .x1 = x,
        .y1 = y,
        .x2 = x + w - 1,
        .y2 = y + h - 1,
    };
    lv_draw_rect(layer, &dsc, &coords);
}

// 高性能合批渲染回调 (LV_EVENT_DRAW_MAIN)
static void on_draw_playfield(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);

    // 1. 赛博黄昏天际线全屏垂直渐变 (精确还原 attract-cyberrunner.jpg 赛博晚霞与夕阳霞光)
    // 顶部深夜星空靛紫 (Y: 0 ~ 70)
    draw_box(layer, 0, 0,   SCREEN_W, 35, 0x160826);
    draw_box(layer, 0, 35,  SCREEN_W, 35, 0x2A0D3D);
    // 中高空晚霞绛紫与玫瑰玫红 (Y: 70 ~ 140)
    draw_box(layer, 0, 70,  SCREEN_W, 35, 0x48114E);
    draw_box(layer, 0, 105, SCREEN_W, 35, 0x761756);
    // 中空暖霞绯红与金橙 (Y: 140 ~ 195)
    draw_box(layer, 0, 140, SCREEN_W, 30, 0xAA2B4A);
    draw_box(layer, 0, 170, SCREEN_W, 25, 0xD84D2E);
    // 璀璨金色落日地平线霞光 (Y: 195 ~ 240) —— 正好处于大厦屋顶奔跑纵深区间，形成极具冲击力的背光剪影对比！
    draw_box(layer, 0, 195, SCREEN_W, 25, 0xEE7928);
    draw_box(layer, 0, 220, SCREEN_W, 20, 0xF6A330);
    // 大厦峡谷深处暖色暮霭 (Y: 240 ~ 320) —— 彻底消除纯黑/断层，建筑物间隙透出深邃都市霞光
    draw_box(layer, 0, 240, SCREEN_W, 40, 0x881E48);
    draw_box(layer, 0, 280, SCREEN_W, 40, 0x3E0C32);

    // 2. 晚霞像素积云 (带有玫瑰粉霞光边缘与暗紫云影)
    int far_off = (int)s_game.bg_far_scroll_px;
    int cloud_off = (far_off / 2) % (SCREEN_W + 80);
    for (int cx = -80; cx < SCREEN_W + 80; cx += 110) {
        int x = cx - cloud_off;
        draw_box(layer, x + 6, 58, 48, 12, 0x320E40);
        draw_box(layer, x + 16, 52, 32, 6, 0x320E40);
        draw_box(layer, x + 2, 68, 54, 3, 0x8A1F5E);
        draw_box(layer, x + 12, 71, 36, 2, 0xBA3468);
    }

    // 3. 远景摩天大楼剪影 (深靛黑剪影耸入金色霞光，带有天线与闪烁信标)
    for (int bx = -60; bx < SCREEN_W + 60; bx += 64) {
        int sx = bx - (far_off % 64);
        int bw = 38;
        int bh = 155;
        int by = 60;
        // 大厦剪影主体 (深靛冷夜紫)
        draw_box(layer, sx, by, bw, bh, 0x1B0E2E);
        // 楼顶通信尖塔天线
        draw_box(layer, sx + 2, by - 12, 2, 12, 0x1B0E2E);
        if ((s_game.tick_count / 10) % 2 == 0) {
            draw_box(layer, sx + 1, by - 14, 4, 2, 0xFF0055); // 红色防撞航标灯
        }
        // 剪影散落发光窗户 (暖金、电光青与霓虹粉，营造生动大都会)
        draw_box(layer, sx + 6,  by + 22, 4, 3, 0xFDE047);
        draw_box(layer, sx + 20, by + 30, 4, 3, 0x00FFFF);
        draw_box(layer, sx + 12, by + 52, 4, 3, 0xFDE047);
        draw_box(layer, sx + 26, by + 74, 4, 3, 0x38BDF8);
        draw_box(layer, sx + 8,  by + 96, 4, 3, 0xFF2A6D);
    }

    // 4. 中景大厦与天际全息广告牌 (中速视差)
    int mid_off = (int)s_game.bg_mid_scroll_px;
    for (int bx = -80; bx < SCREEN_W + 80; bx += 88) {
        int sx = bx - (mid_off % 88);
        draw_box(layer, sx, 110, 48, 150, 0x140B22);
        // 垂直全息霓虹广告灯条
        draw_box(layer, sx + 4, 124, 3, 28, 0x00F5FF);
        draw_box(layer, sx + 40, 138, 3, 22, 0xFF0066);
    }

    // 5. 前景建筑屋顶平台 (深紫夜蓝砖墙 + 经典错落砖缝 + 超高对比度纯白电光青平台发光边)
    for (int i = 0; i < CR_MAX_BUILDINGS; i++) {
        cr_building_t *b = &s_game.buildings[i];
        if (!b->active) continue;

        int bx = (int)b->x;
        int by = (int)b->y;
        int bw = (int)b->w;
        int bh = (int)b->h;

        if (bx + bw < 0 || bx >= SCREEN_W) continue;

        if (b->is_glass && b->crumbled) {
            // 已坍塌天窗不绘制表面实体，只留残破断壁
            draw_box(layer, bx, by + 12, bw, bh - 12, 0x0E0B1A);
            continue;
        }

        // 大楼主体砖墙 (深靛夜蓝基底 0x1A1634，完美还原 attract-cyberrunner.jpg 的砖石质感)
        uint32_t body_col = b->is_glass ? 0x121724 : 0x1A1634;
        draw_box(layer, bx, by + 4, bw, bh - 4, body_col);

        // 砖石横向缝隙与纵向错落砖纹
        if (!b->is_glass) {
            // 左右侧边光影修饰 (左深阴影 0x0E0B1C，右微光 0x2A244E)
            draw_box(layer, bx, by + 4, 2, bh - 4, 0x0E0B1C);
            draw_box(layer, bx + bw - 2, by + 4, 2, bh - 4, 0x2A244E);

            // 经典水平砖缝 (每隔 11 像素一条暗黑砂浆线)
            for (int ly = by + 12; ly < SCREEN_H; ly += 11) {
                draw_box(layer, bx + 2, ly, bw - 4, 1, 0x0F0C20);
            }
            // 错落砖面微光高光块 (复刻参考图的细腻砖块肌理)
            int seed = (int)b->win_seed;
            for (int r = 0; r < 4; r++) {
                int py_blk = by + 16 + r * 22;
                if (py_blk + 8 < SCREEN_H) {
                    int px1 = bx + 8 + ((seed >> (r * 3)) % (bw > 40 ? bw - 30 : 5));
                    int px2 = bx + bw - 18 - ((seed >> (r * 2 + 1)) % 15);
                    if (px1 + 10 < bx + bw) draw_box(layer, px1, py_blk, 10, 6, 0x272248);
                    if (px2 > bx + 15)     draw_box(layer, px2, py_blk + 11, 10, 6, 0x272248);
                }
            }
        }

        // 屋顶平台发光边缘 (4px 超高识别度霓虹边缘：纯白高光线 + 电光青晶体 + 深青过渡)
        // 这一圈边缘在任何复杂背景与光照下，均能以最高优先级标示落脚点
        if (b->is_glass) {
            // 易碎天窗：白色预警边 + 烈焰霓虹玫红
            uint32_t glass_edge = (b->crumble_timer_ms > 0) ? 0xFF0055 : 0xFF2A6D;
            draw_box(layer, bx, by, bw, 2, 0xFFFFFF);
            draw_box(layer, bx, by + 2, bw, 3, glass_edge);
            draw_box(layer, bx, by + 5, bw, 1, 0x880033);
        } else {
            // 经典屋顶：1px 纯白顶高光 + 2px 极光电光青 + 1px 深青底边
            draw_box(layer, bx, by, bw, 1, 0xFFFFFF);
            draw_box(layer, bx, by + 1, bw, 2, 0x00FFFF);
            draw_box(layer, bx, by + 3, bw, 1, 0x007799);
        }
    }

    // 6. 俯冲震荡波渲染 (Shockwave on Ground)
    if (s_game.shockwave_active && s_game.shockwave_timer_ms > 0) {
        int swx = (int)s_game.shockwave_x;
        int swy = (int)s_game.shockwave_y;
        int swr = (int)s_game.shockwave_radius;
        draw_box(layer, swx - swr, swy - 3, swr * 2, 5, 0x00FFFF);
        draw_box(layer, swx - swr + 2, swy - 2, (swr - 2) * 2, 3, 0xFFFFFF);
    }

    // 7. 陷阱与机关渲染
    for (int i = 0; i < CR_MAX_HAZARDS; i++) {
        cr_hazard_t *h = &s_game.hazards[i];
        if (!h->active) continue;

        int hx = (int)h->x;
        int hy = (int)h->y;
        int hw = (int)h->w;
        int hh = (int)h->h;

        if (hx + hw < 0 || hx >= SCREEN_W) continue;

        if (h->type == CR_HAZARD_LASER_LOW || h->type == CR_HAZARD_LASER_HIGH) {
            // 脉冲激光束 (白色高能芯 + 艳红外焰 + 左右金属发射极)
            draw_box(layer, hx, hy + 1, hw, 4, 0xFF0055);
            draw_box(layer, hx, hy + 2, hw, 2, 0xFFFFFF);
            draw_box(layer, hx - 2, hy, 3, hh, 0x64748B);
            draw_box(layer, hx + hw - 1, hy, 3, hh, 0x64748B);
        } else if (h->type == CR_HAZARD_LASER_WALL) {
            // 全高阻断激光墙 (纯白能量芯 + 烈红高光)
            draw_box(layer, hx, hy, hw, hh, 0xBE123C);
            draw_box(layer, hx + 1, hy, hw - 2, hh, 0xFF0055);
            draw_box(layer, hx + 2, hy, 2, hh, 0xFFFFFF);
            // 上下发射极
            draw_box(layer, hx - 2, hy, hw + 4, 4, 0x020617);
            draw_box(layer, hx - 2, hy + hh - 4, hw + 4, 4, 0x020617);
        } else if (h->type == CR_HAZARD_DRONE) {
            // 浮游侦察无人机 (亮黄警示机壳 + 闪烁红眼 + 旋转青翼)
            draw_box(layer, hx, hy, hw, hh, 0x020617); // 黑色外框
            draw_box(layer, hx + 2, hy + 2, hw - 4, hh - 4, 0xFACC15);
            // 闪烁红色侦测眼
            draw_box(layer, hx + hw/2 - 2, hy + hh/2 - 2, 4, 4, 0xFF0000);
            draw_box(layer, hx + hw/2 - 1, hy + hh/2 - 1, 2, 2, 0xFFFFFF);
            // 两侧发光旋翼
            draw_box(layer, hx - 3, hy + 1, 4, 2, 0x00FFFF);
            draw_box(layer, hx + hw - 1, hy + 1, 4, 2, 0x00FFFF);

            // ★ 影刃锁定指示框 (进入 75px 斩杀范围高亮菱形标记)
            if (hx >= CR_PLAYER_X - 10 && hx <= CR_PLAYER_X + 75) {
                draw_box(layer, hx - 2, hy - 2, hw + 4, 1, 0x00FFFF);
                draw_box(layer, hx - 2, hy + hh + 1, hw + 4, 1, 0x00FFFF);
                draw_box(layer, hx - 2, hy - 2, 1, hh + 4, 0x00FFFF);
                draw_box(layer, hx + hw + 1, hy - 2, 1, hh + 4, 0x00FFFF);
            }
        } else if (h->type == CR_HAZARD_VENT) {
            // 超导排风口
            draw_box(layer, hx, hy, hw, hh, 0x334155);
            draw_box(layer, hx + 2, hy + 1, hw - 4, hh - 2, 0x00FFFF);
            // 向上喷气气流
            if (s_game.tick_count % 2 == 0) {
                draw_box(layer, hx + 5, hy - 10, 2, 10, 0xFFFFFF);
                draw_box(layer, hx + hw - 7, hy - 14, 2, 14, 0x67E8F9);
            }
        }
    }

    // 8. 收集道具渲染
    for (int i = 0; i < CR_MAX_ITEMS; i++) {
        cr_item_t *it = &s_game.items[i];
        if (!it->active) continue;

        int ix = (int)it->x;
        int iy = (int)(it->y + sinf(it->bob_phase) * 3.0f);
        if (ix < -10 || ix > SCREEN_W + 10) continue;

        if (it->type == CR_ITEM_DATA_GEM) {
            // 菱形数据晶体 (璀璨电光青 + 白核心)
            draw_box(layer, ix - 3, iy - 4, 6, 8, 0x00FFFF);
            draw_box(layer, ix - 1, iy - 2, 2, 4, 0xFFFFFF);
        } else if (it->type == CR_ITEM_BATTERY) {
            // 能量电池 (翡翠绿 + 荧光充能条)
            draw_box(layer, ix - 4, iy - 4, 8, 8, 0x10B981);
            draw_box(layer, ix - 2, iy - 2, 4, 4, 0x6EE7B7);
        } else if (it->type == CR_ITEM_SHIELD) {
            // 磁暴护盾球
            draw_box(layer, ix - 5, iy - 5, 10, 10, 0x00FFFF);
            draw_box(layer, ix - 3, iy - 3, 6, 6, 0xFFFFFF);
        }
    }

    // 9. 影刃瞬影飞斩轨迹 (Blade Slash Line)
    if (s_game.slash_active) {
        int sx = (int)s_game.slash_start_x;
        int sy = (int)s_game.slash_start_y;
        int tx = (int)s_game.slash_target_x;
        int ty = (int)s_game.slash_target_y;

        int steps = 6;
        for (int s = 0; s <= steps; s++) {
            float t = (float)s / (float)steps;
            int cx = (int)(sx + (tx - sx) * t);
            int cy = (int)(sy + (ty - sy) * t);
            draw_box(layer, cx - 2, cy - 2, 5, 5, 0x00FFFF);
            draw_box(layer, cx - 1, cy - 1, 3, 3, 0xFFFFFF);
        }
        // 斩击爆发十字星
        draw_box(layer, tx - 8, ty - 1, 17, 3, 0xFFFFFF);
        draw_box(layer, tx - 1, ty - 8, 3, 17, 0xFFFFFF);
        draw_box(layer, tx - 12, ty, 25, 1, 0x00FFFF);
        draw_box(layer, tx, ty - 12, 1, 25, 0x00FFFF);
    }

    // 10. 幽灵闪现残影 (Afterimages)
    for (int i = 0; i < CR_MAX_AFTERIMAGES; i++) {
        cr_afterimage_t *img = &s_game.afterimages[i];
        if (img->alpha > 0.1f) {
            int ax = (int)img->x;
            int ay = (int)img->y - CR_PLAYER_H;
            draw_box(layer, ax, ay, CR_PLAYER_W, CR_PLAYER_H, img->color);
        }
    }

    // 11. 玩家主角 (Cyber Courier 信使) —— 极致灵动赛博信使 (完美复刻 attract-cyberrunner.jpg)
    // 纯正午夜蓝战衣 + 灵动流光绯红围巾 + 璀璨电光青目镜 + 高反差纯白运动战靴
    bool blink_hide = (s_game.invuln_timer_ms > 0 && (s_game.tick_count % 3 == 0));
    if (!blink_hide) {
        int px = (int)((float)CR_PLAYER_X + s_game.render_x_offset);
        int ph = (s_game.stance == CR_STANCE_SLIDE) ? CR_SLIDE_H : CR_PLAYER_H;
        int py = (int)(s_game.y - (float)ph);

        // 统一配色常量
        const uint32_t COL_SUIT_DARK  = 0x14112E; // 午夜深靛潜行服底色
        const uint32_t COL_SUIT_MID   = 0x282454; // 装甲主体蓝紫
        const uint32_t COL_SUIT_LIGHT = 0x484382; // 肩背与关节高光
        const uint32_t COL_VISOR_CYAN = 0x00FFFF; // 电光青面罩目镜
        const uint32_t COL_VISOR_WHT  = 0xFFFFFF; // 目镜反光白芯
        const uint32_t COL_BOOT_WHT   = 0xFFFFFF; // 纯白高帮运动跑鞋
        const uint32_t COL_BOOT_SOLE  = 0x0F172A; // 鞋底抓地深色纹

        if (s_game.stance == CR_STANCE_SLIDE) {
            // 1) 地面超低空极速滑铲姿势 (贴地飞驰，火花四溅)
            // 头部与前倾目镜
            draw_box(layer, px + 15, py + 2, 8, 8, COL_SUIT_DARK);
            draw_box(layer, px + 18, py + 4, 6, 3, COL_VISOR_CYAN);
            draw_box(layer, px + 20, py + 4, 3, 2, COL_VISOR_WHT);
            // 水平流线型躯干
            draw_box(layer, px + 5, py + 4, 12, 7, COL_SUIT_MID);
            draw_box(layer, px + 7, py + 5, 8, 3, COL_SUIT_LIGHT);
            // 贴地蹬踏的纯白跑鞋
            draw_box(layer, px - 2, py + 6, 7, 5, COL_BOOT_WHT);
            draw_box(layer, px - 2, py + 10, 7, 2, COL_BOOT_SOLE);
            // 尾部喷气推进火花
            draw_box(layer, px - 4, py + 7, 3, 4, 0x38BDF8);
            draw_box(layer, px + 6, py + 12, 5, 2, 0xFFFFFF); // 地面摩擦火花
        } else if (s_game.stance == CR_STANCE_WALL_SLIDE) {
            // 2) 贴墙下滑姿态 (单手贴墙减速，身躯紧贴墙体)
            // 头部侧视
            draw_box(layer, px + 4, py + 1, 9, 8, COL_SUIT_DARK);
            draw_box(layer, px + 8, py + 3, 6, 3, COL_VISOR_CYAN);
            draw_box(layer, px + 10, py + 3, 3, 2, COL_VISOR_WHT);
            // 紧凑纵向身躯
            draw_box(layer, px + 2, py + 9, 10, 10, COL_SUIT_MID);
            draw_box(layer, px + 4, py + 10, 6, 6, COL_SUIT_LIGHT);
            // 贴墙支撑手臂
            draw_box(layer, px + 11, py + 9, 5, 4, COL_SUIT_LIGHT);
            // 垂挂脚部与蹬墙白鞋
            draw_box(layer, px + 4, py + 19, 6, 5, COL_SUIT_DARK);
            draw_box(layer, px + 8, py + 22, 6, 5, COL_BOOT_WHT);
            draw_box(layer, px + 12, py + 22, 2, 5, COL_BOOT_SOLE);

            // 蹬墙剧烈摩擦火花 (白色 + 青色 + 金黄)
            int spk_x = px + CR_PLAYER_W - 1;
            draw_box(layer, spk_x, py + 15, 3, 3, 0xFFFFFF);
            draw_box(layer, spk_x + 1, py + 10, 2, 4, 0x00FFFF);
            draw_box(layer, spk_x, py + 22, 2, 3, 0xFDE047);
        } else if (s_game.stance == CR_STANCE_RUN) {
            // 3) 楼顶极速疾跑姿态 (经典 4 帧灵动步态，双足白鞋清晰交代触地与步频)
            int run_frame = (s_game.tick_count / 3) % 4;

            // 头盔头部 (微微前倾，带流线型头盔高光)
            draw_box(layer, px + 3, py + 1, 10, 9, COL_SUIT_DARK);
            draw_box(layer, px + 5, py + 1, 6, 2, COL_SUIT_LIGHT);
            // 发光青色横向面罩目镜 (极富未来科技感)
            draw_box(layer, px + 8, py + 4, 7, 3, COL_VISOR_CYAN);
            draw_box(layer, px + 10, py + 4, 3, 2, COL_VISOR_WHT);

            // 躯干 (健美身形，前胸装甲板微光)
            draw_box(layer, px + 4, py + 10, 9, 9, COL_SUIT_MID);
            draw_box(layer, px + 6, py + 11, 5, 5, COL_SUIT_LIGHT);
            // 战术腰带与能量指示灯
            draw_box(layer, px + 4, py + 18, 9, 2, 0x0F172A);
            draw_box(layer, px + 8, py + 18, 2, 1, 0x00FFFF);

            // 双腿与纯白战靴 (4 帧跑动：两腿分明，白鞋无论在亮空还是暗楼均清晰可见)
            if (run_frame == 0) {
                // 前蹬后扬
                draw_box(layer, px + 8, py + 19, 4, 5, COL_SUIT_DARK);
                draw_box(layer, px + 9, py + 23, 6, 5, COL_BOOT_WHT);
                draw_box(layer, px + 9, py + 27, 6, 2, COL_BOOT_SOLE);

                draw_box(layer, px + 2, py + 19, 4, 4, COL_SUIT_DARK);
                draw_box(layer, px,     py + 22, 5, 4, COL_BOOT_WHT);
                draw_box(layer, px,     py + 25, 5, 2, COL_BOOT_SOLE);
            } else if (run_frame == 1 || run_frame == 3) {
                // 腾空交汇
                draw_box(layer, px + 4, py + 19, 5, 5, COL_SUIT_DARK);
                draw_box(layer, px + 5, py + 24, 7, 4, COL_BOOT_WHT);
                draw_box(layer, px + 5, py + 27, 7, 2, COL_BOOT_SOLE);
            } else {
                // 后蹬前扬 (左右反向)
                draw_box(layer, px + 1, py + 19, 4, 5, COL_SUIT_DARK);
                draw_box(layer, px - 1, py + 23, 5, 4, COL_BOOT_WHT);
                draw_box(layer, px - 1, py + 26, 5, 2, COL_BOOT_SOLE);

                draw_box(layer, px + 7, py + 19, 4, 4, COL_SUIT_DARK);
                draw_box(layer, px + 8, py + 22, 6, 5, COL_BOOT_WHT);
                draw_box(layer, px + 8, py + 26, 6, 2, COL_BOOT_SOLE);
            }
        } else {
            // 4) 空中腾空/跳跃/俯冲姿势 (完美复刻 attract-cyberrunner.jpg 飞跃剪影)
            // 头部前视
            draw_box(layer, px + 3, py + 1, 10, 9, COL_SUIT_DARK);
            draw_box(layer, px + 5, py + 1, 6, 2, COL_SUIT_LIGHT);
            draw_box(layer, px + 8, py + 4, 7, 3, COL_VISOR_CYAN);
            draw_box(layer, px + 10, py + 4, 3, 2, COL_VISOR_WHT);

            // 躯干倾斜
            draw_box(layer, px + 4, py + 10, 9, 9, COL_SUIT_MID);
            draw_box(layer, px + 6, py + 11, 5, 5, COL_SUIT_LIGHT);

            // 忍者腾跃收腿姿势：前脚向前舒展，后脚自然勾起
            draw_box(layer, px + 8, py + 18, 4, 4, COL_SUIT_DARK);
            draw_box(layer, px + 9, py + 21, 6, 5, COL_BOOT_WHT);
            draw_box(layer, px + 9, py + 25, 6, 2, COL_BOOT_SOLE);

            draw_box(layer, px + 1, py + 18, 4, 5, COL_SUIT_DARK);
            draw_box(layer, px + 1, py + 22, 5, 4, COL_BOOT_WHT);
            draw_box(layer, px + 1, py + 25, 5, 2, COL_BOOT_SOLE);

            // 二段跳 / 俯冲离子推进尾迹
            if (s_game.stance == CR_STANCE_DOUBLE_JUMP || s_game.stance == CR_STANCE_DIVE) {
                draw_box(layer, px + 10, py + 27, 4, 3, 0x00FFFF);
                draw_box(layer, px + 2,  py + 27, 4, 3, 0x00FFFF);
                draw_box(layer, px + 11, py + 28, 2, 2, 0xFFFFFF);
                draw_box(layer, px + 3,  py + 28, 2, 2, 0xFFFFFF);
            }
        }

        // 磁暴护盾外圈
        if (s_game.has_shield) {
            draw_box(layer, px - 3, py - 3, CR_PLAYER_W + 6, 2, 0x00FFFF);
            draw_box(layer, px - 3, py + ph + 1, CR_PLAYER_W + 6, 2, 0x00FFFF);
            draw_box(layer, px - 3, py - 3, 2, ph + 6, 0x00FFFF);
            draw_box(layer, px + CR_PLAYER_W + 1, py - 3, 2, ph + 6, 0x00FFFF);
        }
    }

    // 12. 飘逸流光绯红围巾 (Vivid Fluorescent Scarf) —— 强烈视觉引导与灵动信标
    uint32_t scarf_col  = 0xFF1760; // 鲜艳荧光玫红外圈
    uint32_t scarf_core = 0xFFA8C4; // 耀眼粉白内芯
    for (int i = 0; i < CR_MAX_SCARF_NODES; i++) {
        int nx = (int)s_game.scarf[i].x;
        int ny = (int)s_game.scarf[i].y;
        int sw = (i == 0) ? 6 : (6 - i * 1);
        if (sw < 3) sw = 3;
        draw_box(layer, nx, ny - 1, sw, 4, scarf_col);
        draw_box(layer, nx + 1, ny, sw - 2, 2, scarf_core);
    }

    // 12. 粒子微粒绘制
    for (int i = 0; i < CR_MAX_PARTICLES; i++) {
        cr_particle_t *p = &s_game.particles[i];
        if (!p->active) continue;

        int px = (int)p->x;
        int py = (int)p->y;
        if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
            int sz = (p->type == CR_PART_EXPLOSION) ? 3 : 2;
            draw_box(layer, px, py, sz, sz, p->color);
        }
    }
}

// 40 FPS 主游戏循环
static void on_timer(lv_timer_t *timer)
{
    (void)timer;

    // 1. 步进核心算法 (25ms = 40FPS)
    cyber_runner_step(&s_game, 25);

    // 2. 音效队列分发
    if (s_game.pending_sound != CR_SND_NONE) {
        send_sound(s_game.pending_sound);
        s_game.pending_sound = CR_SND_NONE;
    }

    // 3. 刷新 HUD 显示
    if (s_hud_score) {
        lv_label_set_text_fmt(s_hud_score, "%ldm  %ld", (long)s_game.distance_m, (long)s_game.score);
    }

    if (s_hud_hp) {
        char hp_buf[32] = {0};
        int pos = 0;
        for (int i = 0; i < s_game.max_hp; i++) {
            pos += snprintf(hp_buf + pos, sizeof(hp_buf) - pos, "%s",
                            (i < s_game.hp) ? "♥" : "♡");
        }
        if (s_game.has_shield) {
            snprintf(hp_buf + pos, sizeof(hp_buf) - pos, " [S]");
        }
        lv_label_set_text(s_hud_hp, hp_buf);
    }

    if (s_hud_blink) {
        // 闪现充能电量显示
        char blk_buf[16] = {0};
        snprintf(blk_buf, sizeof(blk_buf), "⚡%d", s_game.blink_charges);
        lv_label_set_text(s_hud_blink, blk_buf);
    }

    if (s_hud_combo) {
        if (s_game.combo_count > 1) {
            lv_obj_clear_flag(s_hud_combo, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(s_hud_combo, "x%d ECHO", (int)s_game.combo_count);
        } else {
            lv_obj_add_flag(s_hud_combo, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 4. Game Over 弹窗
    if (s_game.game_over) {
        if (!s_gameover_box) {
            s_gameover_box = lv_obj_create(s_scr);
            lv_obj_set_size(s_gameover_box, 200, 100);
            lv_obj_center(s_gameover_box);
            lv_obj_set_style_bg_color(s_gameover_box, lv_color_hex(0x0F172A), 0);
            lv_obj_set_style_border_color(s_gameover_box, lv_color_hex(0xF43F5E), 0);
            lv_obj_set_style_border_width(s_gameover_box, 2, 0);

            lv_obj_t *l1 = lv_label_create(s_gameover_box);
            lv_label_set_text(l1, "SIGNAL LOST");
            lv_obj_set_style_text_color(l1, lv_color_hex(0xF43F5E), 0);
            lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, 4);

            lv_obj_t *l2 = lv_label_create(s_gameover_box);
            lv_label_set_text_fmt(l2, "%ldm / %ld PTS", (long)s_game.distance_m, (long)s_game.score);
            lv_obj_set_style_text_color(l2, lv_color_hex(0x38BDF8), 0);
            lv_obj_align(l2, LV_ALIGN_CENTER, 0, 0);

            lv_obj_t *l3 = lv_label_create(s_gameover_box);
            lv_label_set_text(l3, "PRESS OK TO REBOOT");
            lv_obj_set_style_text_color(l3, lv_color_hex(0x94A3B8), 0);
            lv_obj_align(l3, LV_ALIGN_BOTTOM_MID, 0, -2);
        }
    } else {
        if (s_gameover_box) {
            lv_obj_del(s_gameover_box);
            s_gameover_box = NULL;
        }
    }

    // 5. 触发画布重绘
    if (s_playfield) {
        lv_obj_invalidate(s_playfield);
    }
}

// 统一演示入口：建屏、启动后台任务与定时器
void demo_cyber_runner_enter(void)
{
    // 初始化算法逻辑
    cyber_runner_init(&s_game, 0x20260919);

    // 启动音频队列与任务
    s_snd_queue = xQueueCreate(16, sizeof(cr_sound_t));
    s_audio_running = true;
    xTaskCreate(cyber_runner_audio_task, "cr_audio", 3072, NULL, 5, &s_snd_task);

    // 构建界面
    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x1A102F), 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // 全屏画布
    s_playfield = lv_obj_create(s_scr);
    lv_obj_set_size(s_playfield, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_playfield, 0, 0);
    lv_obj_clear_flag(s_playfield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_playfield, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_playfield, 0, 0);
    lv_obj_add_event_cb(s_playfield, on_draw_playfield, LV_EVENT_DRAW_MAIN, NULL);

    // 顶部 HUD 标签
    s_hud_score = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_hud_score, lv_color_hex(0xF8FAFC), 0);
    lv_obj_align(s_hud_score, LV_ALIGN_TOP_LEFT, 8, 6);
    lv_label_set_text(s_hud_score, "0m  0");

    s_hud_blink = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_hud_blink, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(s_hud_blink, LV_ALIGN_TOP_MID, -20, 6);
    lv_label_set_text(s_hud_blink, "⚡3");

    s_hud_hp = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_hud_hp, lv_color_hex(0xF43F5E), 0);
    lv_obj_align(s_hud_hp, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_label_set_text(s_hud_hp, "♥♥♥");

    s_hud_combo = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_hud_combo, lv_color_hex(0xFDE047), 0);
    lv_obj_align(s_hud_combo, LV_ALIGN_TOP_RIGHT, -8, 22);
    lv_obj_add_flag(s_hud_combo, LV_OBJ_FLAG_HIDDEN);

    // 载入屏幕
    lv_disp_load_scr(s_scr);

    // 启动 40 FPS 驱动定时器
    s_game_timer = lv_timer_create(on_timer, 25, NULL);
}

// 统一退出接口：清理全部定时器、任务与图层
void demo_cyber_runner_exit(void)
{
    // 停止定时器
    if (s_game_timer) {
        lv_timer_del(s_game_timer);
        s_game_timer = NULL;
    }

    // 停止音频后台任务
    s_audio_running = false;
    if (s_snd_queue) {
        cr_sound_t stop = CR_SND_NONE;
        xQueueSend(s_snd_queue, &stop, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
        vQueueDelete(s_snd_queue);
        s_snd_queue = NULL;
    }
    s_snd_task = NULL;

    // 清空对象句柄
    s_playfield = NULL;
    s_hud_score = NULL;
    s_hud_hp = NULL;
    s_hud_blink = NULL;
    s_hud_combo = NULL;
    s_gameover_box = NULL;

    // 删除主屏
    if (s_scr) {
        lv_obj_del(s_scr);
        s_scr = NULL;
    }
}

// 三键输入响应 (非阻塞)
void demo_cyber_runner_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_PRESS && ev != BSP_BTN_CLICK) return;

    if (btn == BSP_BTN_UP) {
        cyber_runner_input_up(&s_game);
    } else if (btn == BSP_BTN_DOWN) {
        cyber_runner_input_down(&s_game);
    } else if (btn == BSP_BTN_OK) {
        cyber_runner_input_ok(&s_game);
    }
}
