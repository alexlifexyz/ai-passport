#include <assert.h>
#include <stdio.h>
#include "thunder_logic.h"

int main(void)
{
    printf("[TEST] Testing Thunder Striker Logic...\n");

    thunder_game_t g;
    thunder_init(&g);

    // 1. 验证初始状态
    assert(g.player_hp == 3);
    assert(g.bombs == 2);
    assert(g.weapon_level == 1);
    assert(g.score == 0);
    assert(!g.game_over);
    printf("  ✓ Initialization OK\n");

    // 2. 验证左右移动与边界限制
    for (int i = 0; i < 30; i++) thunder_move_left(&g);
    assert(g.player_x >= 4.0f);

    for (int i = 0; i < 40; i++) thunder_move_right(&g);
    assert(g.player_x <= SCREEN_W - g.player_w - 4.0f);
    printf("  ✓ Movement & Screen Clamping OK\n");

    // 3. 验证自动开火与子弹生成
    g.shoot_timer = THUNDER_SHOOT_INTERVAL - 1;
    thunder_step(&g);
    // 应在 shoot_timer >= THUNDER_SHOOT_INTERVAL 时生成双路激光 (至少 2 发活跃子弹)
    int bullet_count = 0;
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (g.bullets[i].active) bullet_count++;
    }
    assert(bullet_count >= 2);
    printf("  ✓ Auto-fire Laser Spawning OK\n");

    // 4. 验证全屏核弹清屏效果
    g.enemy_bullets[0].active = true;
    g.enemy_bullets[1].active = true;
    g.enemies[0].active = true;
    g.enemies[0].hp = 5;
    assert(thunder_use_bomb(&g) == true);
    assert(g.bombs == 1);
    assert(!g.enemy_bullets[0].active);
    assert(!g.enemy_bullets[1].active);
    assert(!g.enemies[0].active); // 5点血被 10 点核弹伤害秒杀
    printf("  ✓ Screen-Clearing Mega Bomb OK\n");

    // 5. 验证玩家扣血与阵亡
    g.invincible_timer = 0;
    g.player_hp = 1;
    g.enemies[0].active = true;
    g.enemies[0].w = 20;
    g.enemies[0].h = 20;
    g.enemies[0].x = g.player_x;
    g.enemies[0].y = g.player_y;
    thunder_step(&g);
    assert(g.player_hp == 0);
    assert(g.game_over == true);
    printf("  ✓ Damage & Game Over State OK\n");

    printf("[PASS] All Thunder Striker Unit Tests Passed!\n");
    return 0;
}
