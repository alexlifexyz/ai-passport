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

    // 1. 晚霞赛博天际线渐变 (黄昏紫橙色调，精确复刻参考图氛围)
    // 深靛紫 (Y: 0 ~ 45)
    draw_box(layer, 0, 0, SCREEN_W, 45, 0x1A102F);
    // 紫红晚霞 (Y: 45 ~ 85)
    draw_box(layer, 0, 45, SCREEN_W, 40, 0x3E1948);
    // 绛红暖霞 (Y: 85 ~ 125)
    draw_box(layer, 0, 85, SCREEN_W, 40, 0x701A58);
    // 霞光金橙 (Y: 125 ~ 165)
    draw_box(layer, 0, 125, SCREEN_W, 40, 0xA83250);
    // 近地黄昏暖金 (Y: 165 ~ 210)
    draw_box(layer, 0, 165, SCREEN_W, 45, 0xD9534F);

    // 2. 远景摩天大楼剪影 (极低速视差，错落大厦轮廓与零星发光窗户)
    int far_off = (int)s_game.bg_far_scroll_px;
    for (int bx = -60; bx < SCREEN_W + 60; bx += 70) {
        int sx = bx - (far_off % 70);
        int bw = 42;
        int bh = 140;
        int by = 50;
        draw_box(layer, sx, by, bw, bh, 0x221236);
        // 剪影窗户微光
        draw_box(layer, sx + 8, by + 18, 5, 4, 0xFDE047);
        draw_box(layer, sx + 22, by + 26, 5, 4, 0x38BDF8);
        draw_box(layer, sx + 14, by + 45, 5, 4, 0xEC4899);
        draw_box(layer, sx + 28, by + 65, 5, 4, 0xFDE047);
    }

    // 3. 中景大厦与天际全息广告牌 (中速视差)
    int mid_off = (int)s_game.bg_mid_scroll_px;
    for (int bx = -80; bx < SCREEN_W + 80; bx += 90) {
        int sx = bx - (mid_off % 90);
        draw_box(layer, sx, 100, 52, 160, 0x160B24);
        // 垂直霓虹招牌光条
        draw_box(layer, sx + 4, 115, 3, 28, 0x06B6D4);
        draw_box(layer, sx + 44, 130, 3, 22, 0xF43F5E);
    }

    // 4. 前景建筑屋顶平台 (实体与砖石纹理)
    for (int i = 0; i < CR_MAX_BUILDINGS; i++) {
        cr_building_t *b = &s_game.buildings[i];
        if (!b->active) continue;

        int bx = (int)b->x;
        int by = (int)b->y;
        int bw = (int)b->w;
        int bh = (int)b->h;

        if (bx + bw < 0 || bx >= SCREEN_W) continue;

        if (b->is_glass && b->crumbled) {
            // 已坍塌天窗不绘制表面实体，只留断壁
            draw_box(layer, bx, by + 12, bw, bh - 12, 0x14101E);
            continue;
        }

        // 大楼主体砖墙 (深紫暗砖色)
        uint32_t body_col = b->is_glass ? 0x1E293B : 0x221B32;
        draw_box(layer, bx, by + 3, bw, bh - 3, body_col);

        // 砖石横竖纹理 (营造像素砖墙质感)
        if (!b->is_glass) {
            for (int ly = by + 10; ly < SCREEN_H; ly += 12) {
                draw_box(layer, bx, ly, bw, 1, 0x181224);
            }
            // 窗户光点
            int seed = (int)b->win_seed;
            for (int wy = by + 16; wy < SCREEN_H - 20; wy += 22) {
                int wx1 = bx + 15 + ((seed >> 2) % 20);
                int wx2 = bx + bw - 30;
                if (wx1 + 8 < bx + bw) draw_box(layer, wx1, wy, 8, 12, 0x2E2544);
                if (wx2 > bx + 20)     draw_box(layer, wx2, wy, 8, 12, 0x38BDF8);
            }
        }

        // 屋顶发光边缘 (青色荧光边框，标志性赛博朋克特征)
        if (b->is_glass) {
            uint32_t glass_edge = (b->crumble_timer_ms > 0) ? 0xF43F5E : 0x38BDF8;
            draw_box(layer, bx, by, bw, 4, glass_edge);
            draw_box(layer, bx, by + 1, bw, 2, 0xE0F2FE);
        } else {
            // 顶级青色霓虹高光 (2层线)
            draw_box(layer, bx, by, bw, 2, 0x22D3EE);
            draw_box(layer, bx, by + 2, bw, 2, 0x0891B2);
            draw_box(layer, bx, by + 4, bw, 1, 0x164E63);
        }
    }

    // 5. 陷阱与机关渲染
    for (int i = 0; i < CR_MAX_HAZARDS; i++) {
        cr_hazard_t *h = &s_game.hazards[i];
        if (!h->active) continue;

        int hx = (int)h->x;
        int hy = (int)h->y;
        int hw = (int)h->w;
        int hh = (int)h->h;

        if (hx + hw < 0 || hx >= SCREEN_W) continue;

        if (h->type == CR_HAZARD_LASER_LOW || h->type == CR_HAZARD_LASER_HIGH) {
            // 脉冲激光束 (鲜红高光 + 左右极柱)
            draw_box(layer, hx, hy + 2, hw, 3, 0xF43F5E);
            draw_box(layer, hx + 1, hy + 3, hw - 2, 1, 0xFFE4E6);
            draw_box(layer, hx - 2, hy, 3, hh, 0x475569);
            draw_box(layer, hx + hw - 1, hy, 3, hh, 0x475569);
        } else if (h->type == CR_HAZARD_LASER_WALL) {
            // 全高激光光墙 (必须空中闪现虚化穿透)
            draw_box(layer, hx, hy, hw, hh, 0xBE123C);
            draw_box(layer, hx + 2, hy, hw - 4, hh, 0xF43F5E);
            draw_box(layer, hx + 3, hy, 2, hh, 0xFFFFFF);
            // 上下发射极
            draw_box(layer, hx - 2, hy, hw + 4, 4, 0x0F172A);
            draw_box(layer, hx - 2, hy + hh - 4, hw + 4, 4, 0x0F172A);
        } else if (h->type == CR_HAZARD_DRONE) {
            // 浮游侦察无人机
            draw_box(layer, hx + 2, hy + 2, hw - 4, hh - 4, 0x0F172A);
            // 闪烁红色警报眼
            draw_box(layer, hx + hw/2 - 2, hy + hh/2 - 2, 4, 4, 0xEF4444);
            // 两侧发光旋翼
            draw_box(layer, hx - 3, hy + 1, 4, 2, 0x06B6D4);
            draw_box(layer, hx + hw - 1, hy + 1, 4, 2, 0x06B6D4);
        } else if (h->type == CR_HAZARD_VENT) {
            // 超导排风口
            draw_box(layer, hx, hy, hw, hh, 0x334155);
            draw_box(layer, hx + 3, hy + 1, hw - 6, hh - 2, 0x06B6D4);
            // 向上喷气条纹
            if (s_game.tick_count % 2 == 0) {
                draw_box(layer, hx + 6, hy - 8, 2, 8, 0x67E8F9);
                draw_box(layer, hx + hw - 8, hy - 12, 2, 12, 0x67E8F9);
            }
        }
    }

    // 6. 收集道具渲染
    for (int i = 0; i < CR_MAX_ITEMS; i++) {
        cr_item_t *it = &s_game.items[i];
        if (!it->active) continue;

        int ix = (int)it->x;
        int iy = (int)(it->y + sinf(it->bob_phase) * 3.0f);
        if (ix < -10 || ix > SCREEN_W + 10) continue;

        if (it->type == CR_ITEM_DATA_GEM) {
            // 菱形数据晶体
            draw_box(layer, ix - 3, iy - 4, 6, 8, 0x06B6D4);
            draw_box(layer, ix - 1, iy - 2, 2, 4, 0xFFFFFF);
        } else if (it->type == CR_ITEM_BATTERY) {
            // 能量电池
            draw_box(layer, ix - 4, iy - 4, 8, 8, 0x10B981);
            draw_box(layer, ix - 2, iy - 2, 4, 4, 0x34D399);
        } else if (it->type == CR_ITEM_SHIELD) {
            // 磁暴护盾球
            draw_box(layer, ix - 5, iy - 5, 10, 10, 0x38BDF8);
            draw_box(layer, ix - 3, iy - 3, 6, 6, 0xBAE6FD);
        }
    }

    // 7. 幽灵闪现残影 (Afterimages)
    for (int i = 0; i < CR_MAX_AFTERIMAGES; i++) {
        cr_afterimage_t *img = &s_game.afterimages[i];
        if (img->alpha > 0.1f) {
            int ax = (int)img->x;
            int ay = (int)img->y - CR_PLAYER_H;
            draw_box(layer, ax, ay, CR_PLAYER_W, CR_PLAYER_H, img->color);
        }
    }

    // 8. 玩家主角 (Cyber Courier 信使)
    bool blink_hide = (s_game.invuln_timer_ms > 0 && (s_game.tick_count % 3 == 0));
    if (!blink_hide) {
        int px = (int)((float)CR_PLAYER_X + s_game.render_x_offset);
        int ph = (s_game.stance == CR_STANCE_SLIDE) ? CR_SLIDE_H : CR_PLAYER_H;
        int py = (int)(s_game.y - (float)ph);

        if (s_game.stance == CR_STANCE_SLIDE) {
            // 低姿态滑铲姿势
            draw_box(layer, px, py + 4, 26, 10, 0x0F172A);
            // 头部与前伸青色目镜
            draw_box(layer, px + 18, py + 3, 7, 5, 0x38BDF8);
            // 后部白靴
            draw_box(layer, px - 2, py + 6, 5, 6, 0xF8FAFC);
        } else {
            // 奔跑 / 跳跃姿势 (复刻参考图)
            // 躯干 (深蓝纳米作战服)
            draw_box(layer, px + 3, py + 10, 12, 14, 0x0F172A);
            // 头部战术头盔
            draw_box(layer, px + 2, py + 1, 14, 10, 0x0F172A);
            // 高亮发光青色面罩目镜 (Cyan Visor)
            draw_box(layer, px + 7, py + 4, 9, 4, 0x38BDF8);
            draw_box(layer, px + 9, py + 5, 5, 2, 0xFFFFFF);
            // 双腿与白色运动战靴
            if (s_game.stance == CR_STANCE_RUN) {
                int run_frame = (s_game.tick_count / 3) % 4;
                if (run_frame == 0 || run_frame == 2) {
                    draw_box(layer, px + 2, py + 22, 6, 8, 0x1E293B);
                    draw_box(layer, px + 10, py + 20, 6, 10, 0x1E293B);
                    draw_box(layer, px + 2, py + 27, 7, 3, 0xF8FAFC);
                    draw_box(layer, px + 10, py + 27, 7, 3, 0xF8FAFC);
                } else {
                    draw_box(layer, px + 5, py + 20, 6, 10, 0x1E293B);
                    draw_box(layer, px + 9, py + 22, 6, 8, 0x1E293B);
                    draw_box(layer, px + 5, py + 27, 7, 3, 0xF8FAFC);
                    draw_box(layer, px + 9, py + 27, 7, 3, 0xF8FAFC);
                }
            } else {
                // 跳跃收腿姿势
                draw_box(layer, px + 2, py + 21, 6, 7, 0x1E293B);
                draw_box(layer, px + 10, py + 19, 6, 8, 0x1E293B);
                draw_box(layer, px + 2, py + 25, 7, 4, 0xF8FAFC);
                draw_box(layer, px + 10, py + 24, 7, 4, 0xF8FAFC);
            }
        }

        // 磁暴护盾外圈
        if (s_game.has_shield) {
            draw_box(layer, px - 3, py - 3, CR_PLAYER_W + 6, 2, 0x38BDF8);
            draw_box(layer, px - 3, py + ph + 1, CR_PLAYER_W + 6, 2, 0x38BDF8);
            draw_box(layer, px - 3, py - 3, 2, ph + 6, 0x38BDF8);
            draw_box(layer, px + CR_PLAYER_W + 1, py - 3, 2, ph + 6, 0x38BDF8);
        }
    }

    // 9. 飘逸流光红围巾 (Trailing Scarf)
    // 渲染 5 个由物理拉动的质点
    uint32_t scarf_col = 0xF43F5E;
    for (int i = 0; i < CR_MAX_SCARF_NODES; i++) {
        int nx = (int)s_game.scarf[i].x;
        int ny = (int)s_game.scarf[i].y;
        int sw = (i == 0) ? 5 : (5 - i * 1);
        if (sw < 2) sw = 2;
        draw_box(layer, nx, ny, sw, 3, scarf_col);
    }

    // 10. 粒子绘制
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
