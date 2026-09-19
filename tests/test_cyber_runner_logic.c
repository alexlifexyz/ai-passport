// tests/test_cyber_runner_logic.c —— 《霓虹疾行：影刃闪现》核心逻辑单元测试
#include "cyber_runner_logic.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

int main(void)
{
    printf("=======================================================\n");
    printf("  Starting Cyber Runner (Phantom Dash) Unit Tests     \n");
    printf("=======================================================\n");

    cr_game_t g;

    // [TEST 1] 初始化系统验证
    printf("[TEST 1] Testing Initialization & Rooftop Systems...\n");
    cyber_runner_init(&g, 0x12345678);
    assert(g.stance == CR_STANCE_RUN);
    assert(g.y == g.buildings[0].y);
    assert(g.hp == 3);
    assert(g.max_hp == 3);
    assert(g.blink_charges == 3);
    assert(g.air_jumps_left == 1);
    assert(!g.phase_shift);
    assert(!g.game_over);
    printf("  ✓ Initial values, 3-charge blink & buildings ready OK\n");

    // [TEST 2] 地面跳跃与空中二段喷气腾空
    printf("[TEST 2] Testing Ground Jump & Mid-Air Double Jump...\n");
    cyber_runner_init(&g, 0x1111);
    cyber_runner_input_up(&g);
    assert(g.stance == CR_STANCE_JUMP);
    assert(g.vy < -400.0f);
    assert(g.pending_sound == CR_SND_JUMP);

    // 步进数帧腾空
    cyber_runner_step(&g, 25);
    cyber_runner_step(&g, 25);
    assert(g.y < g.buildings[0].y);

    // 空中再次按 UP -> 触发二段跳
    cyber_runner_input_up(&g);
    assert(g.stance == CR_STANCE_DOUBLE_JUMP);
    assert(g.air_jumps_left == 0);
    assert(g.vy < -300.0f);
    assert(g.pending_sound == CR_SND_AIR_BOOST);
    printf("  ✓ Ground jump and mid-air double jump OK\n");

    // [TEST 3] 空中幽灵闪现 (Phantom Blink) 与相位虚化
    printf("[TEST 3] Testing Phantom Blink & Phase Shift...\n");
    cyber_runner_init(&g, 0x2222);
    // 先起跳进入空中
    cyber_runner_input_up(&g);
    cyber_runner_step(&g, 50);

    // 在半空中按下 OK 触发闪现
    cyber_runner_input_ok(&g);
    assert(g.stance == CR_STANCE_BLINK);
    assert(g.blink_charges == 2);
    assert(g.phase_shift == true);
    assert(g.vy == 0.0f); // 闪现时重力滞空
    assert(g.render_x_offset > 20.0f); // 瞬移突进视觉位移
    assert(g.afterimages[0].alpha > 0.5f); // 残影生成
    assert(g.pending_sound == CR_SND_BLINK);
    printf("  ✓ Phantom blink zero-gravity flash & phase shift OK\n");

    // [TEST 4] 贴地滑铲与空中极速下坠
    printf("[TEST 4] Testing Slide (Crouch) & Dive Plunge...\n");
    cyber_runner_init(&g, 0x3333);
    // 地面按 DOWN -> 滑铲
    cyber_runner_input_down(&g);
    assert(g.stance == CR_STANCE_SLIDE);
    assert(g.stance_timer_ms > 300);
    assert(g.pending_sound == CR_SND_SLIDE);

    // 起跳后在空中按 DOWN -> 俯冲砸地
    cyber_runner_input_up(&g);
    cyber_runner_step(&g, 50);
    cyber_runner_input_down(&g);
    assert(g.stance == CR_STANCE_DIVE);
    assert(g.vy > 600.0f); // 超速下坠
    printf("  ✓ Ground slide low stance & mid-air plunge OK\n");

    // [TEST 5] 幽灵闪现虚化穿爆浮游无人机与穿透激光墙
    printf("[TEST 5] Testing Phase Shift Drone Annihilation & Laser Pierce...\n");
    cyber_runner_init(&g, 0x4444);
    // 生成一个激光墙在闪现位移命中范围内
    g.hazards[0].active = true;
    g.hazards[0].type = CR_HAZARD_LASER_WALL;
    g.hazards[0].x = (float)CR_PLAYER_X + 25.0f;
    g.hazards[0].y = g.y - 40.0f;
    g.hazards[0].w = 20.0f;
    g.hazards[0].h = 45.0f;

    // 开启闪现虚化状态穿透激光墙
    cyber_runner_input_up(&g);
    cyber_runner_input_ok(&g);
    assert(g.phase_shift == true);
    int prev_hp = g.hp;
    uint32_t prev_score = g.score;
    cyber_runner_step(&g, 25);
    // 毫发无损且加分
    assert(g.hp == prev_hp);
    assert(g.score > prev_score);

    // 放置无人机，闪现直接穿爆
    g.hazards[1].active = true;
    g.hazards[1].type = CR_HAZARD_DRONE;
    g.hazards[1].x = (float)CR_PLAYER_X + 25.0f;
    g.hazards[1].y = g.y - 25.0f;
    g.hazards[1].w = 20.0f;
    g.hazards[1].h = 20.0f;

    cyber_runner_step(&g, 25);
    assert(g.hazards[1].active == false); // 无人机被穿爆解体
    assert(g.pending_sound == CR_SND_DRONE_POP);
    printf("  ✓ Laser phase-through & drone instant annihilation OK\n");

    // [TEST 6] 超导排风口强力腾空弹射
    printf("[TEST 6] Testing Superconductor Vent Boost...\n");
    cyber_runner_init(&g, 0x5555);
    g.hazards[0].active = true;
    g.hazards[0].type = CR_HAZARD_VENT;
    g.hazards[0].x = (float)CR_PLAYER_X;
    g.hazards[0].y = g.y - 6.0f;
    g.hazards[0].w = 20.0f;
    g.hazards[0].h = 8.0f;

    cyber_runner_step(&g, 25);
    assert(g.vy < -500.0f); // 暴风冲天
    assert(g.pending_sound == CR_SND_VENT_BOOST);
    printf("  ✓ Superconductor vent super leap launch OK\n");

    // [TEST 7] 道具收集 (电池瞬间充满闪现格) 与坠落重开
    printf("[TEST 7] Testing Battery Recharging & Fall Reset...\n");
    cyber_runner_init(&g, 0x6666);
    g.blink_charges = 0; // 耗尽能量

    g.items[0].active = true;
    g.items[0].type = CR_ITEM_BATTERY;
    g.items[0].x = (float)CR_PLAYER_X + 2.0f;
    g.items[0].y = g.y - 10.0f;

    cyber_runner_step(&g, 25);
    assert(g.items[0].active == false);
    assert(g.blink_charges == 3); // 满格回满
    assert(g.pending_sound == CR_SND_GEM);

    // 掉落深渊死亡
    g.y = (float)CR_SCREEN_H + 20.0f;
    cyber_runner_step(&g, 25);
    assert(g.game_over == true);
    assert(g.pending_sound == CR_SND_GAMEOVER);

    // 按 OK 重新开始
    cyber_runner_input_ok(&g);
    assert(g.game_over == false);
    assert(g.hp == 3);
    printf("  ✓ Battery recharge, fatal fall & instant reboot OK\n");

    printf("=======================================================\n");
    printf("   ✓ [PASS] All 7 Cyber Runner Unit Tests Passed!      \n");
    printf("=======================================================\n");
    return 0;
}
