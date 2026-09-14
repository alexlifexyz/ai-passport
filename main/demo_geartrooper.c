// main/demo_geartrooper.c —— 《齿轮骑兵：蒸汽过载》(Gear Cavalry: Steam Overdrive)
// 极速 40 FPS 纯矢量即时绘制，三实体键爽快操控，零堆分配防碎片，16kHz 蒸汽朋克工业合成音效。
#include "demo.h"
#include "geartrooper_logic.h"
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

static const char *TAG __attribute__((unused)) = "demo_geartrooper";

#define SCREEN_W 240
#define SCREEN_H 320

static gt_game_t    s_game;
static lv_obj_t    *s_scr = NULL;
static lv_obj_t    *s_playfield = NULL;
static lv_obj_t    *s_hud_score = NULL;
static lv_obj_t    *s_hud_hp = NULL;
static lv_obj_t    *s_hud_steam = NULL;
static lv_obj_t    *s_hud_hints = NULL;
static lv_obj_t    *s_gameover_box = NULL;
static lv_timer_t  *s_game_timer = NULL;

static QueueHandle_t s_snd_queue = NULL;
static TaskHandle_t  s_snd_task = NULL;
static volatile bool s_audio_running = false;
static uint32_t      s_frame_tick = 0;
static uint8_t       s_volume = 85;

static void send_sound(gt_sound_t snd)
{
    if (s_snd_queue && snd != GT_SND_NONE) {
        xQueueSend(s_snd_queue, &snd, 0);
    }
}

// 独立后台音效合成任务 (16kHz 16-bit 蒸汽朋克工业音效)
static void geartrooper_audio_task(void *arg)
{
    (void)arg;
    gt_sound_t snd;
    static int16_t buf[256];

    bsp_audio_set_format(16000, 16, 1);
    bsp_audio_set_volume(s_volume);

    while (s_audio_running) {
        if (xQueueReceive(s_snd_queue, &snd, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (snd == GT_SND_NONE) continue;

            if (snd == GT_SND_JUMP) {
                // 蒸汽喷射起跳 (气压扫频白噪 40ms)
                const int total = 640;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    int noise = ((rand() % 4000) - 2000);
                    float env = (1.0f - t);
                    buf[i % 256] = (int16_t)(noise * env);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_STEAM_BLOW) {
                // 蒸汽泄压轰鸣 (低频急速跌落 300Hz -> 80Hz, 50ms)
                const int total = 800;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 300.0f - t * 220.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t * 0.5f) * 6500.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_SLIDE) {
                // 齿轮金属摩擦刮擦 (高频锯齿泛音, 45ms)
                const int total = 720;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    phase += 850.0f / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 2000) - 1000;
                    buf[i % 256] = (int16_t)((phase * 2.0f - 1.0f) * 4000.0f + noise);
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_LANCE) {
                // 骑枪机械活塞刺出 (金属穿透撞击, 50ms)
                const int total = 800;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 500.0f + t * 400.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t) * 7000.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_HIT) {
                // 重型撞击粉碎 (机械碎裂低音, 70ms)
                const int total = 1120;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = 220.0f - t * 140.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    int noise = (rand() % 3000) - 1500;
                    float amp = (1.0f - t) * 7500.0f;
                    buf[i % 256] = (int16_t)(((phase < 0.5f) ? amp : -amp) + noise * (1.0f - t));
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_OVERDRIVE) {
                // 蒸汽超压激活 (急促高亢双音鸣笛, 120ms)
                const int total = 1920;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    float freq = (i < total / 2) ? 880.0f : 1174.0f;
                    phase += freq / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    float amp = (1.0f - t * 0.3f) * 7000.0f;
                    buf[i % 256] = (phase < 0.5f) ? (int16_t)amp : -(int16_t)amp;
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_HURT) {
                // 装甲受损钝响 (40ms)
                const int total = 640;
                float phase = 0.0f;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    phase += 140.0f / 16000.0f;
                    if (phase >= 1.0f) phase -= 1.0f;
                    buf[i % 256] = (int16_t)((phase < 0.5f ? 5500.0f : -5500.0f) * (1.0f - t));
                    if ((i % 256) == 255 || i == total - 1) {
                        bsp_audio_write(buf, ((i % 256) + 1) * sizeof(int16_t));
                    }
                }
            } else if (snd == GT_SND_GAMEOVER) {
                // 泄气停机 (160ms)
                const int total = 2560;
                for (int i = 0; i < total; i++) {
                    float t = (float)i / (float)total;
                    int noise = ((rand() % 5000) - 2500);
                    buf[i % 256] = (int16_t)(noise * (1.0f - t));
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

// 绘制地表旋转机械齿轮 (根据角度实时渲染 8 颗外齿与中心轴孔)
static void draw_rotating_gear(lv_layer_t *layer, float cx, float cy, float r, float angle_deg, uint32_t body_color, uint32_t tooth_color)
{
    // 1. 齿轮主圆盘 (利用紧凑多边交叉矩形拟合)
    int ir = (int)r;
    draw_box(layer, (int)cx - ir + 2, (int)cy - ir, ir * 2 - 4, ir * 2, body_color);
    draw_box(layer, (int)cx - ir, (int)cy - ir + 2, ir * 2, ir * 2 - 4, body_color);

    // 2. 八方向凸轮齿 (根据当前旋转角绘制 4 对相对轮齿)
    float rad = angle_deg * (3.14159265f / 180.0f);
    int tooth_len = 4;
    int tooth_thick = 3;

    for (int t = 0; t < 4; t++) {
        float a = rad + t * (3.14159265f / 4.0f);
        float cos_a = cosf(a);
        float sin_a = sinf(a);

        // 正向齿
        int tx1 = (int)(cx + (r + tooth_len / 2) * cos_a);
        int ty1 = (int)(cy + (r + tooth_len / 2) * sin_a);
        draw_box(layer, tx1 - tooth_thick / 2, ty1 - tooth_thick / 2, tooth_thick, tooth_thick, tooth_color);

        // 反向相对齿
        int tx2 = (int)(cx - (r + tooth_len / 2) * cos_a);
        int ty2 = (int)(cy - (r + tooth_len / 2) * sin_a);
        draw_box(layer, tx2 - tooth_thick / 2, ty2 - tooth_thick / 2, tooth_thick, tooth_thick, tooth_color);
    }

    // 3. 中心圆芯轴孔
    int hole_r = (ir > 14) ? 4 : 3;
    draw_box(layer, (int)cx - hole_r, (int)cy - hole_r, hole_r * 2, hole_r * 2, 0x0A0F1D);
}

// 绘制战马与蒸汽骑兵
static void draw_horse_and_rider(lv_layer_t *layer, const gt_game_t *game)
{
    // 受伤无敌闪烁
    if (game->invuln_timer_ms > 0 && ((game->tick_count / 3) % 2 == 0)) {
        return;
    }

    int px = GT_HORSE_X;
    int py = (int)game->y;
    uint32_t armor_col = 0xB45309;  // 蒸汽黄铜金
    uint32_t steel_col = 0x475569;  // 锻铁钢板
    uint32_t glow_col  = 0xFBBF24;  // 活塞火光

    if (game->overdrive_active) {
        armor_col = 0x38BDF8; // 过载高能电浆蓝
        glow_col  = 0xFFFFFF; // 耀斑白
    }

    // A. 机械战马躯干与排气系统
    if (game->stance == STANCE_SLIDE) {
        // --- 滑铲俯身贴地姿态 ---
        // 压低马身
        draw_box(layer, px - 6, py + 10, 36, 12, steel_col);
        draw_box(layer, px - 2, py + 8, 30, 10, armor_col);
        // 马头向前伸平
        draw_box(layer, px + 26, py + 6, 12, 10, armor_col);
        draw_box(layer, px + 34, py + 8, 3, 3, glow_col); // 发光眼
        // 骑士俯身贴在马背
        draw_box(layer, px + 6, py + 4, 16, 7, 0x1E293B);
        // 滑铲支撑前腿
        draw_box(layer, px + 28, py + 16, 12, 4, steel_col);
    } else {
        // --- 正常奔跑 / 跳跃 / 下刺姿态 ---
        // 1. 马身锅炉主体
        draw_box(layer, px - 6, py - 4, 32, 18, steel_col);
        draw_box(layer, px - 2, py - 6, 26, 18, armor_col);

        // 2. 马背垂直排气管
        draw_box(layer, px - 8, py - 14, 5, 12, 0x334155);
        draw_box(layer, px - 9, py - 16, 7, 3, 0x94A3B8);

        // 3. 骑士身躯与重盔
        draw_box(layer, px + 2, py - 18, 14, 14, 0x1E293B);
        draw_box(layer, px + 6, py - 24, 10, 10, armor_col);
        draw_box(layer, px + 12, py - 21, 4, 2, glow_col); // 骑士目镜光

        // 4. 马颈与马首
        draw_box(layer, px + 20, py - 12, 10, 14, armor_col);
        draw_box(layer, px + 24, py - 18, 12, 12, armor_col);
        draw_box(layer, px + 32, py - 16, 3, 3, glow_col); // 机械战马眼

        // 5. 铰链机械四蹄动效
        int leg_tick = (game->tick_count / 3) % 4;
        int front_offset = (leg_tick == 0 || leg_tick == 2) ? 4 : -3;
        int rear_offset  = (leg_tick == 1 || leg_tick == 3) ? 4 : -3;

        if (game->stance == STANCE_JUMP) {
            // 跃空屈蹄
            front_offset = -4; rear_offset = 6;
        } else if (game->stance == STANCE_PLUNGE) {
            // 下刺垂直展蹄
            front_offset = 2; rear_offset = 2;
        }

        // 前蹄
        draw_box(layer, px + 18 + front_offset, py + 14, 5, 8, steel_col);
        // 后蹄
        draw_box(layer, px - 2 + rear_offset, py + 14, 5, 8, steel_col);
    }

    // B. 螺旋重装骑枪 (Lance)
    int lance_x = px + 28;
    int lance_y = py - 8;
    int lance_len = game->lance_reach_px;

    if (game->stance == STANCE_PLUNGE) {
        // 下刺：长矛朝右下方 45 度贯穿
        draw_box(layer, lance_x, lance_y + 4, 16, 16, 0xCBD5E1);
        draw_box(layer, lance_x + 8, lance_y + 12, 14, 14, glow_col);
        draw_box(layer, lance_x + 16, lance_y + 20, 12, 12, 0xFFFFFF);
    } else if (game->stance == STANCE_SLIDE) {
        // 滑铲：长矛向前低姿挺击
        draw_box(layer, px + 34, py + 10, lance_len, 4, 0xCBD5E1);
        draw_box(layer, px + 34 + lance_len, py + 9, 6, 6, glow_col);
    } else {
        // 正常持枪 / 突刺
        if (game->lance_active) {
            // 突刺延伸：枪身带有金黄过载电芒
            draw_box(layer, lance_x, lance_y, lance_len, 5, 0xE2E8F0);
            draw_box(layer, lance_x + 6, lance_y + 1, lance_len - 10, 3, glow_col);
            // 枪尖风刃
            draw_box(layer, lance_x + lance_len, lance_y - 1, 8, 7, 0xFFFFFF);
        } else {
            // 待机持枪
            draw_box(layer, lance_x, lance_y, 22, 4, 0x94A3B8);
            draw_box(layer, lance_x + 22, lance_y + 1, 4, 2, 0xE2E8F0);
        }
    }
}

// 绘制发条机械敌兵
static void draw_enemies(lv_layer_t *layer, const gt_game_t *game)
{
    for (int i = 0; i < GT_MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) continue;
        const gt_enemy_t *e = &game->enemies[i];
        int ex = (int)e->x;
        int ey = (int)e->y;

        if (e->type == ENEMY_FALCON) {
            // 1. 齿轮飞隼 (带双展翼与发光侦察眼)
            int wing_phase = (game->tick_count % 2 == 0) ? -2 : 2;
            // 机械双翼
            draw_box(layer, ex, ey - 4 + wing_phase, 18, 2, 0x94A3B8);
            // 鸟躯
            draw_box(layer, ex + 3, ey, 12, 10, 0xDC2626);
            // 发光黄铜目镜
            draw_box(layer, ex + 1, ey + 3, 3, 4, 0xFDE047);
        } else if (e->type == ENEMY_SPIDER) {
            // 2. 地面发条蜘蛛 (多足疾走)
            // 腹部机壳
            draw_box(layer, ex + 2, ey + 2, 14, 8, 0x475569);
            // 金色发条提钮
            draw_box(layer, ex + 7, ey - 3, 4, 5, 0xF59E0B);
            // 左右折叠足 (随步幅摆动)
            int leg_off = (game->tick_count % 4 < 2) ? 1 : -1;
            draw_box(layer, ex, ey + 8 + leg_off, 4, 5, 0x1E293B);
            draw_box(layer, ex + 14, ey + 8 - leg_off, 4, 5, 0x1E293B);
        } else if (e->type == ENEMY_GOLEM) {
            // 3. 重甲机械铁傀儡 (巨型高耐久要塞)
            // 头部
            draw_box(layer, ex + 6, ey, 12, 8, 0x334155);
            draw_box(layer, ex + 8, ey + 2, 8, 3, 0xEF4444); // 红色长条目镜
            // 躯干重盾
            draw_box(layer, ex, ey + 8, 24, 18, 0x1E293B);
            draw_box(layer, ex + 3, ey + 11, 18, 12, 0x78350F); // 核心黄铜护胸
            // 左右重腕
            draw_box(layer, ex - 3, ey + 10, 4, 14, 0x475569);
            draw_box(layer, ex + 23, ey + 10, 4, 14, 0x475569);
            // 双足
            draw_box(layer, ex + 3, ey + 26, 6, 6, 0x0F172A);
            draw_box(layer, ex + 15, ey + 26, 6, 6, 0x0F172A);
        }
    }
}

// 绘制粒子效果 (蒸汽气团与剧烈火花)
static void draw_particles(lv_layer_t *layer, const gt_game_t *game)
{
    for (int i = 0; i < GT_MAX_PARTICLES; i++) {
        if (!game->particles[i].active) continue;
        const gt_particle_t *p = &game->particles[i];
        int px = (int)p->x;
        int py = (int)p->y;

        if (p->type == PART_STEAM) {
            // 膨胀散开的蒸汽团
            int sz = (p->life > 0.5f) ? 4 : 6;
            draw_box(layer, px - sz / 2, py - sz / 2, sz, sz, p->color);
        } else if (p->type == PART_SPARK) {
            // 高亮火花小晶点
            draw_box(layer, px, py, 2, 2, p->color);
        }
    }
}

// 画面即时渲染回调 (LV_EVENT_DRAW_MAIN)
static void on_draw_playfield(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);

    // 1. 工业暗沉蒸汽工坊背景
    draw_box(layer, 0, 0, SCREEN_W, SCREEN_H, 0x14110E);

    // 2. 远景工业管道与烟囱剪影
    draw_box(layer, 20, 50, 16, 170, 0x1F1A15);
    draw_box(layer, 80, 70, 24, 150, 0x1A1612);
    draw_box(layer, 180, 40, 20, 180, 0x1F1A15);
    draw_box(layer, 0, 110, SCREEN_W, 6, 0x241F1A); // 远景横向通风管道

    // 3. 地表黄铜导轨与钢铁基座
    // 黄铜齿条地表 (Y = 220)
    draw_box(layer, 0, 220, SCREEN_W, 4, 0xD97706); // 黄金亮轨
    draw_box(layer, 0, 224, SCREEN_W, 2, 0x78350F); // 轨道暗边
    // 厚重机械地底基座
    draw_box(layer, 0, 226, SCREEN_W, 74, 0x1C1917);
    // 工业铆钉装饰线
    for (int rx = 8; rx < SCREEN_W; rx += 24) {
        draw_box(layer, rx, 230, 2, 2, 0x78716C);
    }

    // 4. 地底 4 组高速旋转啮合大齿轮
    for (int i = 0; i < GT_MAX_GEARS; i++) {
        draw_rotating_gear(layer,
                           s_game.gears[i].x,
                           s_game.gears[i].y,
                           s_game.gears[i].radius,
                           s_game.gears[i].angle_deg,
                           0x78350F, 0xD97706);
    }

    // 5. 敌兵
    draw_enemies(layer, &s_game);

    // 6. 骑兵本体与长矛
    draw_horse_and_rider(layer, &s_game);

    // 7. 粒子特效 (蒸汽、火花、残齿)
    draw_particles(layer, &s_game);

    // 8. 蒸汽过载全屏边缘光晕
    if (s_game.overdrive_active) {
        // 蓝白高能光环环绕四壁
        draw_box(layer, 0, 0, SCREEN_W, 2, 0x38BDF8);
        draw_box(layer, 0, SCREEN_H - 2, SCREEN_W, 2, 0x38BDF8);
        draw_box(layer, 0, 0, 2, SCREEN_H, 0x38BDF8);
        draw_box(layer, SCREEN_W - 2, 0, 2, SCREEN_H, 0x38BDF8);
    }
}

// 刷新定时器 (每 25ms 推进一帧，40 FPS，零卡顿)
static void geartrooper_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_frame_tick++;

    // 推进纯 C 算法游戏核心
    geartrooper_step(&s_game, 25);

    // 触发合成音效
    if (s_game.pending_sound != GT_SND_NONE) {
        send_sound(s_game.pending_sound);
        s_game.pending_sound = GT_SND_NONE;
    }

    // 更新 HUD 标签
    if (s_hud_score) {
        lv_label_set_text_fmt(s_hud_score, "SCR:%ld  x%lu", (long)s_game.score, (unsigned long)(s_game.combo_count > 1 ? s_game.combo_count : 1));
    }
    if (s_hud_hp) {
        // 耐久度 5 格齿轮图标
        char hp_buf[16];
        int hp = s_game.hp;
        if (hp > 5) hp = 5;
        if (hp < 0) hp = 0;
        int idx = 0;
        hp_buf[idx++] = 'H'; hp_buf[idx++] = 'P'; hp_buf[idx++] = ':';
        for (int i = 0; i < 5; i++) {
            hp_buf[idx++] = (i < hp) ? '#' : '.';
        }
        hp_buf[idx] = '\0';
        lv_label_set_text(s_hud_hp, hp_buf);
    }
    if (s_hud_steam) {
        if (s_game.overdrive_active) {
            lv_label_set_text(s_hud_steam, ">>> OVERDRIVE! <<<");
            lv_obj_set_style_text_color(s_hud_steam, lv_color_hex((s_frame_tick % 2 == 0) ? 0x38BDF8 : 0xFFFFFF), 0);
        } else if (s_game.steam_psi >= 100) {
            lv_label_set_text(s_hud_steam, "[OK] OVERDRIVE READY!");
            lv_obj_set_style_text_color(s_hud_steam, lv_color_hex((s_frame_tick % 4 < 2) ? 0xFBBF24 : 0xEF4444), 0);
        } else {
            lv_label_set_text_fmt(s_hud_steam, "STEAM: %d PSI", s_game.steam_psi);
            lv_obj_set_style_text_color(s_hud_steam, lv_color_hex(0xCBD5E1), 0);
        }
    }

    // Game Over 弹窗提示
    if (s_game.game_over) {
        if (!s_gameover_box && s_scr) {
            s_gameover_box = lv_obj_create(s_scr);
            lv_obj_set_size(s_gameover_box, 200, 110);
            lv_obj_center(s_gameover_box);
            lv_obj_set_style_bg_color(s_gameover_box, lv_color_hex(0x1C1917), 0);
            lv_obj_set_style_border_color(s_gameover_box, lv_color_hex(0xD97706), 0);
            lv_obj_set_style_border_width(s_gameover_box, 2, 0);

            lv_obj_t *title = lv_label_create(s_gameover_box);
            lv_label_set_text(title, "STEAM VENTED");
            lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(title, lv_color_hex(0xEF4444), 0);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

            lv_obj_t *score_lbl = lv_label_create(s_gameover_box);
            lv_label_set_text_fmt(score_lbl, "FINAL SCORE: %ld", (long)s_game.score);
            lv_obj_set_style_text_font(score_lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(score_lbl, lv_color_hex(0xFBBF24), 0);
            lv_obj_align(score_lbl, LV_ALIGN_CENTER, 0, -4);

            lv_obj_t *sub = lv_label_create(s_gameover_box);
            lv_label_set_text(sub, "PRESS [OK] TO RESTART");
            lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(sub, lv_color_hex(0x94A3B8), 0);
            lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -2);
        }
    } else {
        if (s_gameover_box) {
            lv_obj_delete(s_gameover_box);
            s_gameover_box = NULL;
        }
    }

    if (s_playfield) {
        lv_obj_invalidate(s_playfield);
    }
}

// 进入演示页
void demo_geartrooper_enter(void)
{
    geartrooper_init(&s_game, 0x778899);

    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x14110E), 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // 主绘图表面
    s_playfield = lv_obj_create(s_scr);
    lv_obj_set_size(s_playfield, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_opa(s_playfield, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_playfield, 0, 0);
    lv_obj_clear_flag(s_playfield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_playfield, on_draw_playfield, LV_EVENT_DRAW_MAIN, NULL);

    // 顶部 HUD 信息条
    lv_obj_t *hud_bar = lv_obj_create(s_scr);
    lv_obj_set_size(hud_bar, SCREEN_W, 36);
    lv_obj_set_pos(hud_bar, 0, 0);
    lv_obj_set_style_bg_color(hud_bar, lv_color_hex(0x1C1917), 0);
    lv_obj_set_style_bg_opa(hud_bar, LV_OPA_90, 0);
    lv_obj_set_style_border_color(hud_bar, lv_color_hex(0x78350F), 0);
    lv_obj_set_style_border_width(hud_bar, 1, 0);
    lv_obj_clear_flag(hud_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_hud_score = lv_label_create(hud_bar);
    lv_label_set_text(s_hud_score, "SCR:0  x1");
    lv_obj_set_style_text_font(s_hud_score, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_score, lv_color_hex(0xF59E0B), 0);
    lv_obj_set_pos(s_hud_score, 4, 2);

    s_hud_hp = lv_label_create(hud_bar);
    lv_label_set_text(s_hud_hp, "HP:#####");
    lv_obj_set_style_text_font(s_hud_hp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_hp, lv_color_hex(0x22C55E), 0);
    lv_obj_set_pos(s_hud_hp, 140, 2);

    s_hud_steam = lv_label_create(hud_bar);
    lv_label_set_text(s_hud_steam, "STEAM: 0 PSI");
    lv_obj_set_style_text_font(s_hud_steam, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_steam, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_pos(s_hud_steam, 4, 18);

    // 底部按键指引
    s_hud_hints = lv_label_create(s_scr);
    lv_obj_set_pos(s_hud_hints, 0, 302);
    lv_obj_set_size(s_hud_hints, SCREEN_W, 16);
    lv_obj_set_style_text_align(s_hud_hints, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_hud_hints, "UP:JUMP/PLUNGE  DN:SLIDE  OK:LANCE");
    lv_obj_set_style_text_font(s_hud_hints, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_hud_hints, lv_color_hex(0xD97706), 0);

    // 启动音频合成任务 (栈分配 4096 字节，防止溢出)
    s_audio_running = true;
    s_snd_queue = xQueueCreate(8, sizeof(gt_sound_t));
    xTaskCreate(geartrooper_audio_task, "gt_audio", 4096, NULL, 5, &s_snd_task);

    // 启动 40 FPS 超高灵敏刷新定时器 (25ms)
    s_game_timer = lv_timer_create(geartrooper_timer_cb, 25, NULL);

    lv_screen_load(s_scr);
}

// 退出演示页
void demo_geartrooper_exit(void)
{
    if (s_game_timer) {
        lv_timer_delete(s_game_timer);
        s_game_timer = NULL;
    }

    s_audio_running = false;
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
        s_playfield = NULL;
        s_hud_score = NULL;
        s_hud_hp = NULL;
        s_hud_steam = NULL;
        s_hud_hints = NULL;
        s_gameover_box = NULL;
    }
}

// 硬件按键分发：0ms 触底响应 + 40ms 防抖
void demo_geartrooper_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK) {
        static uint32_t s_last_press_tick = 0;
        uint32_t now = esp_log_timestamp();
        if (now - s_last_press_tick < 40) return;
        s_last_press_tick = now;

        if (btn == BSP_BTN_UP) {
            geartrooper_input_up(&s_game);
        } else if (btn == BSP_BTN_DOWN) {
            geartrooper_input_down(&s_game);
        } else if (btn == BSP_BTN_OK) {
            geartrooper_input_ok(&s_game);
        }

        if (s_game.pending_sound != GT_SND_NONE) {
            send_sound(s_game.pending_sound);
            s_game.pending_sound = GT_SND_NONE;
        }

        if (s_playfield) {
            lv_obj_invalidate(s_playfield);
        }
    }
}
