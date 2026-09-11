#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 240
#define SCREEN_H 320

#define THUNDER_MAX_BULLETS       20
#define THUNDER_MAX_ENEMY_BULLETS 16
#define THUNDER_MAX_ENEMIES       8
#define THUNDER_MAX_ITEMS         4
#define THUNDER_MAX_STARS         32
#define THUNDER_MAX_PARTICLES     24
#define THUNDER_SHOOT_INTERVAL    5

typedef enum {
    ENEMY_SCOUT = 0,  // 绿蜂快艇 (24x22)
    ENEMY_BOMBER = 1, // 红煞轰炸机 (32x28)
    ENEMY_BOSS = 2,   // 巨型战列要塞 (64x44)
} enemy_type_t;

typedef enum {
    ITEM_TYPE_POWER = 0, // [P] 升级激光
    ITEM_TYPE_BOMB = 1,  // [B] 补充全屏核弹
    ITEM_TYPE_HEAL = 2,  // [H] 回复 1 点生命
} item_type_t;

typedef struct {
    float x, y;
    float vx, vy;
    int w, h;
    bool active;
    uint32_t color;
} bullet_t;

typedef struct {
    float x, y;
    float vx, vy;
    int w, h;
    int hp;
    int max_hp;
    enemy_type_t type;
    int shoot_timer;
    bool active;
} enemy_t;

typedef struct {
    float x, y;
    int w, h;
    item_type_t type;
    bool active;
} thunder_item_t;

typedef struct {
    float x, y;
    float speed;
    uint8_t size;
    uint32_t color;
} star_t;

typedef struct {
    float x, y;
    float vx, vy;
    int life;
    int max_life;
    int size;
    uint32_t color;
} particle_t;

typedef struct {
    // 玩家数据
    float player_x;
    float player_y;
    int player_w;
    int player_h;
    int player_hp;
    int player_max_hp;
    int bombs;
    int weapon_level; // 1: 双发激光, 2: 三联激光, 3: 狂暴等离子
    int invincible_timer;
    int shoot_timer;

    // 实体池
    bullet_t bullets[THUNDER_MAX_BULLETS];
    bullet_t enemy_bullets[THUNDER_MAX_ENEMY_BULLETS];
    enemy_t enemies[THUNDER_MAX_ENEMIES];
    thunder_item_t items[THUNDER_MAX_ITEMS];

    // 星空与爆炸粒子
    star_t stars[THUNDER_MAX_STARS];
    particle_t particles[THUNDER_MAX_PARTICLES];
    int screen_shake;

    // BOSS
    bool boss_active;
    int boss_idx;

    // 游戏统计
    int score;
    int wave_tick;
    bool game_over;
    bool bomb_triggered; // 用于触发屏幕全屏白光

    // 音效触发事件
    bool snd_laser;
    bool snd_hit;
    bool snd_explode;
    bool snd_explode_big;
    bool snd_bomb;
    bool snd_powerup;
    bool snd_gameover;
} thunder_game_t;

void thunder_init(thunder_game_t *g);
void thunder_move_left(thunder_game_t *g);
void thunder_move_right(thunder_game_t *g);
bool thunder_use_bomb(thunder_game_t *g);
void thunder_step(thunder_game_t *g);

