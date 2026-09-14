// tests/test_geartrooper_logic.c —— 《齿轮骑兵：蒸汽过载》核心逻辑单元测试
#include <assert.h>
#include <stdio.h>
#include "geartrooper_logic.h"

int main(void)
{
    printf("=======================================================\n");
    printf("  Starting Gear Cavalry (Steam Overdrive) Unit Tests   \n");
    printf("=======================================================\n");

    gt_game_t g;

    // [TEST 1] 初始化验证
    printf("[TEST 1] Testing Initialization & Gear System...\n");
    geartrooper_init(&g, 0x1234);
    assert(g.hp == 5);
    assert(g.max_hp == 5);
    assert(g.stance == STANCE_RUN);
    assert(g.y == GT_GROUND_Y);
    assert(g.vy == 0.0f);
    assert(!g.game_over);
    assert(!g.overdrive_active);
    assert(g.steam_psi == 0);
    for (int i = 0; i < GT_MAX_GEARS; i++) {
        assert(g.gears[i].radius > 0);
        assert(g.gears[i].speed_deg != 0);
    }
    printf("  ✓ Initialization values & 4 rotating ground gears OK\n");

    // [TEST 2] 三键操作与姿态转换
    printf("[TEST 2] Testing 3-Button Maneuvers & Stance Transitions...\n");
    // 1. 地面 UP -> JUMP
    geartrooper_input_up(&g);
    assert(g.stance == STANCE_JUMP);
    assert(g.vy < 0);
    assert(g.pending_sound == GT_SND_JUMP);

    // 2. 空中 UP -> PLUNGE 重碾
    g.y = GT_GROUND_Y - 50.0f; // 处于半空中
    geartrooper_input_up(&g);
    assert(g.stance == STANCE_PLUNGE);
    assert(g.vy > 0);
    assert(g.lance_active == true);

    // 落地重置
    g.y = GT_GROUND_Y;
    geartrooper_step(&g, 30);
    assert(g.stance == STANCE_RUN);

    // 3. 地面 DOWN -> SLIDE 滑铲
    geartrooper_input_down(&g);
    assert(g.stance == STANCE_SLIDE);
    assert(g.pending_sound == GT_SND_SLIDE);

    // 4. 地面 OK -> LANCE 突刺
    geartrooper_input_ok(&g);
    assert(g.lance_active == true);
    assert(g.pending_sound == GT_SND_LANCE);
    printf("  ✓ UP/DOWN/OK (Jump, Plunge, Slide, Lance) stances OK\n");

    // [TEST 3] 骑枪突刺击碎敌兵与蒸汽充能
    printf("[TEST 3] Testing Lance Collision Damage & Steam Charging...\n");
    geartrooper_init(&g, 0x5678);
    // 在骑兵骑枪范围内生成一个飞隼
    geartrooper_spawn_enemy(&g, ENEMY_FALCON, GT_HORSE_X + 40, GT_GROUND_Y - 20);
    assert(g.enemies[0].active == true);

    // 刺出骑枪
    geartrooper_input_ok(&g);
    uint32_t old_score = g.score;
    int old_psi = g.steam_psi;

    // 推进 30ms 命中敌兵
    geartrooper_step(&g, 30);
    assert(g.enemies[0].hp < g.enemies[0].max_hp);
    assert(g.pending_sound == GT_SND_HIT);

    // 再次命中消灭敌兵
    geartrooper_step(&g, 30);
    assert(g.enemies[0].active == false); // 被摧毁
    assert(g.score > old_score);
    assert(g.steam_psi > old_psi);
    assert(g.combo_count == 1);
    printf("  ✓ Lance penetration, enemy destroy & steam pressure build OK\n");

    // [TEST 4] 贴地滑铲碾碎机械蜘蛛
    printf("[TEST 4] Testing Gear Slide Spider Shredding...\n");
    geartrooper_init(&g, 0x9ABC);
    geartrooper_spawn_enemy(&g, ENEMY_SPIDER, GT_HORSE_X + 20, GT_GROUND_Y + 4);
    assert(g.enemies[0].active == true);

    // 按 DOWN 触发滑铲
    geartrooper_input_down(&g);
    geartrooper_step(&g, 30);
    // 蜘蛛应当被滑铲瞬间击碎
    assert(g.enemies[0].active == false);
    assert(g.score > 0);
    printf("  ✓ Low-stance gear slide spider destruction OK\n");

    // [TEST 5] 蒸汽满压与过载冲锋 (Overdrive)
    printf("[TEST 5] Testing Steam Overdrive & Invincible Ramming...\n");
    geartrooper_init(&g, 0xDEF0);
    g.steam_psi = 100; // 充满蒸汽

    // 按 OK 触发过载
    geartrooper_input_ok(&g);
    assert(g.overdrive_active == true);
    assert(g.pending_sound == GT_SND_OVERDRIVE);

    // 在过载期间直接撞击敌兵，玩家不受伤害，敌兵瞬间粉碎
    geartrooper_spawn_enemy(&g, ENEMY_GOLEM, GT_HORSE_X + 10, GT_GROUND_Y - 8);
    int p_hp = g.hp;
    geartrooper_step(&g, 30);
    assert(g.enemies[0].active == false);
    assert(g.hp == p_hp); // 未扣血
    printf("  ✓ 100 PSI Overdrive trigger & invincible smash verified OK\n");

    // [TEST 6] 伤害受创无敌与 Game Over 流程
    printf("[TEST 6] Testing Hurt Invulnerability & Game Over...\n");
    geartrooper_init(&g, 0x1111);
    g.hp = 1; // 仅剩 1 点血

    geartrooper_spawn_enemy(&g, ENEMY_SPIDER, GT_HORSE_X + 5, GT_GROUND_Y - 5);
    geartrooper_step(&g, 30);
    assert(g.hp == 0);
    assert(g.game_over == true);
    assert(g.pending_sound == GT_SND_GAMEOVER);

    // Game Over 状态下按 OK 重置游戏
    geartrooper_input_ok(&g);
    assert(g.game_over == false);
    assert(g.hp == 5);
    printf("  ✓ HP drain, game-over trigger & OK-key restart OK\n");

    // [TEST 7] 粒子发射与寿命衰减
    printf("[TEST 7] Testing Particle Pool & Physics Decay...\n");
    geartrooper_init(&g, 0x2222);
    for (int i = 0; i < GT_MAX_PARTICLES; i++) g.particles[i].active = false;
    geartrooper_emit_particle(&g, PART_STEAM, 100, 100, -10, -20, 0xFFFFFF);
    assert(g.particles[0].active == true);
    assert(g.particles[0].life == 1.0f);

    float last_life = g.particles[0].life;
    geartrooper_step(&g, 30);
    assert(g.particles[0].life < last_life);
    assert(g.particles[0].y < 100.0f); // 向上飘
    printf("  ✓ Particle pool recycling & smoke/spark physics OK\n");

    printf("=======================================================\n");
    printf("   ✓ [PASS] All 7 Gear Cavalry Unit Tests Passed!      \n");
    printf("=======================================================\n");

    return 0;
}
