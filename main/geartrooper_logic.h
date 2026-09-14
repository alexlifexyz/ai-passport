// main/geartrooper_logic.h —— 《齿轮骑兵：蒸汽过载》(Gear Cavalry: Steam Overdrive) 核心独立算法与状态机
// 专为 ESP32-C3 极简三键(UP/DOWN/OK)与零动态堆分配(Zero malloc)设计。
// 涵盖行星齿轮咬合轨、链锯骑枪突刺、蒸汽压力过载与多兵种机械碰撞判定。
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GT_SCREEN_W 240
#define GT_SCREEN_H 320

#define GT_GROUND_Y     220   // 地面基准高度
#define GT_HORSE_X      56    // 骑兵横向锚点
#define GT_MAX_ENEMIES  6     // 静态敌兵池上限
#define GT_MAX_PARTICLES 24   // 静态粒子池上限
#define GT_MAX_GEARS    4     // 地面咬合齿轮数

// 骑兵动作姿态
typedef enum {
    STANCE_RUN = 0,    // 正常战马冲刺奔跑
    STANCE_JUMP,       // 腾空跳跃
    STANCE_SLIDE,      // 俯身贴地齿轮滑铲
    STANCE_PLUNGE      // 空中斜向下 45° 重碾下刺
} gt_stance_t;

// 敌兵类型
typedef enum {
    ENEMY_NONE = 0,
    ENEMY_SPIDER,      // 发条机械蜘蛛 (地面贴地爬行)
    ENEMY_FALCON,      // 齿轮飞隼 (空中俯冲)
    ENEMY_GOLEM        // 蒸汽重型铜偶 (带盾装甲)
} gt_enemy_type_t;

// 粒子类型
typedef enum {
    PART_NONE = 0,
    PART_STEAM,        // 蒸汽喷射气团 (向上飘散消隐)
    PART_SPARK         // 金属碰撞火花 (重力抛物线)
} gt_part_type_t;

// 音效事件触发 (完全解耦供音频任务播放)
typedef enum {
    GT_SND_NONE = 0,
    GT_SND_JUMP,       // 机械跃起弹性上弦
    GT_SND_SLIDE,      // 贴地齿轮摩擦滑铲
    GT_SND_LANCE,      // 骑枪高速破空旋转
    GT_SND_HIT,        // 击碎敌兵金属铁砧重响
    GT_SND_STEAM_BLOW, // 蒸汽泄压喷气
    GT_SND_OVERDRIVE,  // 蒸汽过载狂暴音阶
    GT_SND_HURT,       // 受到伤害受损警报
    GT_SND_GAMEOVER    // 动力耗尽停转
} gt_sound_t;

// 敌兵实体
typedef struct {
    bool active;
    gt_enemy_type_t type;
    float x;
    float y;
    float vx;
    float vy;
    int hp;
    int max_hp;
    uint32_t anim_tick;
} gt_enemy_t;

// 粒子实体
typedef struct {
    bool active;
    gt_part_type_t type;
    float x;
    float y;
    float vx;
    float vy;
    float life;      // 0.0 ~ 1.0
    float decay;     // 衰减速率
    uint32_t color;  // 十六进制颜色
} gt_particle_t;

// 地面啮合齿轮
typedef struct {
    float x;
    float y;
    float radius;
    float angle_deg;
    float speed_deg; // 旋转角速度 (正为顺时针，负为逆时针)
    int teeth_count;
} gt_gear_t;

// 核心游戏上下文
typedef struct {
    // 玩家骑兵状态
    float y;
    float vy;
    gt_stance_t stance;
    uint32_t stance_timer_ms;

    // 骑枪突刺系统
    bool lance_active;
    uint32_t lance_timer_ms;
    int lance_reach_px; // 突刺延伸长度

    // 战马发条齿轮与蒸汽压力
    int gear_rpm;           // 战马转速
    int steam_psi;          // 蒸汽压力 (0 ~ 100)
    bool overdrive_active;  // 过载狂暴状态
    uint32_t overdrive_ms;  // 过载剩余时间

    // 生命与积分
    int hp;
    int max_hp;
    uint32_t score;
    uint32_t combo_count;
    uint32_t invuln_timer_ms; // 受创无敌闪烁

    // 游戏总体状态
    bool game_over;
    bool stage_cleared;
    uint32_t distance_m;    // 行进距离米数

    // 实体对象池
    gt_gear_t gears[GT_MAX_GEARS];
    gt_enemy_t enemies[GT_MAX_ENEMIES];
    gt_particle_t particles[GT_MAX_PARTICLES];

    // 待播放音效
    gt_sound_t pending_sound;

    // 随机数种子与节拍
    uint32_t rng_state;
    uint32_t tick_count;
    uint32_t enemy_spawn_timer_ms;
} gt_game_t;

// API 接口声明
void geartrooper_init(gt_game_t *game, uint32_t seed);
void geartrooper_step(gt_game_t *game, uint32_t dt_ms);

// 三键输入处理
void geartrooper_input_up(gt_game_t *game);
void geartrooper_input_down(gt_game_t *game);
void geartrooper_input_ok(gt_game_t *game);

// 核心算法与物理函数 (公开供 Host 单元测试)
bool geartrooper_check_collision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);
void geartrooper_spawn_enemy(gt_game_t *game, gt_enemy_type_t type, float x, float y);
void geartrooper_emit_particle(gt_game_t *game, gt_part_type_t type, float x, float y, float vx, float vy, uint32_t color);

#ifdef __cplusplus
}
#endif
