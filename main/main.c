// main/main.c —— FoloToy AI Passport BSP 驱动参考示例:初始化 + 菜单 + 按键分发。
//
// 按键语义(全局统一):
//   上/下 短按   菜单中=移动选中项;演示页中=该页自定义
//   确定  短按   菜单中=进入选中项;演示页中=该页自定义
//   确定  长按   演示页中=返回菜单(由本文件统一拦截)
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"      // 错误日志里要打印 BSP_LCD_* 引脚号
#include "demo.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "main";

static const demo_entry_t DEMOS[] = {
    { "Flappy",    demo_flappy_enter,       demo_flappy_exit,       demo_flappy_key       },
    { "Tank 1990", demo_battlecity_enter,   demo_battlecity_exit,   demo_battlecity_key   },
    { "Pacman",    demo_pacman_enter,       demo_pacman_exit,       demo_pacman_key       },
    { "Striker",   demo_thunder_enter,      demo_thunder_exit,      demo_thunder_key      },
    { "Racer",     demo_thunderracer_enter, demo_thunderracer_exit, demo_thunderracer_key },
    { "Display",   demo_display_enter,      demo_display_exit,      demo_display_key      },
    { "Button",    demo_button_enter,       demo_button_exit,       demo_button_key       },
    { "Audio",     demo_audio_enter,        demo_audio_exit,        demo_audio_key        },
    { "Battery",   demo_battery_enter,      demo_battery_exit,      demo_battery_key      },
    { "Wi-Fi",     demo_wifi_enter,         demo_wifi_exit,         demo_wifi_key         },
    { "BLE",       demo_ble_enter,          demo_ble_exit,          demo_ble_key          },
    { "Low Power", demo_low_power_enter,    demo_low_power_exit,    demo_low_power_key    },
};
#define DEMO_COUNT (sizeof(DEMOS) / sizeof(DEMOS[0]))

// 各外设初始化结果:失败的项在菜单里标 [FAIL] 且不允许进入。
static bool s_ok[DEMO_COUNT];

static lv_obj_t *s_menu_scr;
static lv_obj_t *s_cards[DEMO_COUNT];
static lv_obj_t *s_rows[DEMO_COUNT];
static lv_obj_t *s_mascot;
static int  s_sel = 0;             // 当前选中项 (默认停在 Flappy)
static int  s_active = -1;         // 当前所在演示页;-1 = 在菜单

static void menu_refresh(void) {
    for (size_t i = 0; i < DEMO_COUNT; i++) {
        lv_label_set_text_fmt(s_rows[i], "%s%s",
                              DEMOS[i].name,
                              s_ok[i] ? "" : "  [FAIL]");
        ui_pixel_set_selected(s_cards[i], (int)i == s_sel, s_ok[i]);
        lv_obj_set_style_text_color(s_rows[i],
            s_ok[i] ? lv_color_hex(UI_INK) : lv_color_hex(0x7A2020), 0);
    }
}

static void menu_build(void) {
    s_menu_scr = ui_pixel_screen_create("ARCADE");

    for (size_t i = 0; i < DEMO_COUNT; i++) {
        int x = 11 + (int)(i % 2) * 112;
        int y = 42 + (int)(i / 2) * 36;
        s_cards[i] = ui_pixel_panel_create(s_menu_scr, x, y, 102, 32, UI_PAPER);
        s_rows[i] = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(s_rows[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_rows[i]);
    }

    s_mascot = ui_pixel_mascot_create(s_menu_scr, 101, 268);

    menu_refresh();
    lv_screen_load(s_menu_scr);
}

static void enter_menu(void) {
    s_active = -1;
    menu_build();
}

void bsp_demo_return_to_menu(void) {
    if (s_active >= 0) {
        DEMOS[s_active].exit();
        enter_menu();
    }
}

// 按键回调运行在 button 组件的任务里,操作 LVGL 必须加锁。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!bsp_lvgl_lock(500)) return;

    if (s_active >= 0) {
        // 全局长按任意键均可直接返回主菜单！
        if (ev == BSP_BTN_LONG) {
            bsp_demo_return_to_menu();
        } else {
            DEMOS[s_active].key(btn, ev);
        }
    } else {
        // 主菜单状态：按键按下 (PRESS) 或单击 (CLICK) 均即刻响应！
        if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK) {
            static uint32_t s_last_menu_tick = 0;
            uint32_t now = esp_log_timestamp();
            if (now - s_last_menu_tick >= 150) {
                s_last_menu_tick = now;
                if (btn == BSP_BTN_UP) {
                    s_sel = (s_sel + DEMO_COUNT - 1) % DEMO_COUNT;
                    menu_refresh();
                    ui_pixel_mascot_jump(s_mascot);
                } else if (btn == BSP_BTN_DOWN) {
                    s_sel = (s_sel + 1) % DEMO_COUNT;
                    menu_refresh();
                    ui_pixel_mascot_jump(s_mascot);
                } else if (btn == BSP_BTN_OK && s_ok[s_sel]) {
                    s_active = s_sel;
                    ui_pixel_mascot_jump(s_mascot);
                    lv_obj_delete(s_menu_scr);
                    s_menu_scr = NULL;
                    s_mascot = NULL;
                    DEMOS[s_active].enter();
                }
            }
        }
    }
    bsp_lvgl_unlock();
}

void app_main(void) {
    ESP_LOGI(TAG, "FoloToy AI Passport 街机掌机启动");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒原因: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    // 屏幕是本 demo 的 UI 载体,失败就没有菜单可言
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败,无法继续。"
                      "检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool btn_ok = (bsp_button_init(on_key, NULL) == ESP_OK);
    bool audio_ok = (bsp_audio_init() == ESP_OK);
    bool bat_ok = (bsp_battery_init() == ESP_OK);

    s_ok[0] = btn_ok;                                 // Flappy (像素飞鸟 HD)
    s_ok[1] = btn_ok;                                 // Tank 1990 (经典坦克大战 Neo)
    s_ok[2] = btn_ok;                                 // Pacman (吃豆人)
    s_ok[3] = btn_ok;                                 // Striker (雷霆战机)
    s_ok[4] = btn_ok;                                 // Racer (雷霆飞车)
    s_ok[5] = true;                                   // Display
    s_ok[6] = btn_ok;                                 // Button
    s_ok[7] = audio_ok;                               // Audio
    s_ok[8] = bat_ok;                                 // Battery
    s_ok[9] = true;                                   // Wi-Fi
    s_ok[10] = true;                                  // BLE
    s_ok[11] = true;                                  // Low Power

    if (bsp_lvgl_lock(1000)) {
        s_sel = 0; // 默认选中 Flappy
        enter_menu();
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "街机就绪，主菜单已载入，首项: %s", DEMOS[0].name);
}
