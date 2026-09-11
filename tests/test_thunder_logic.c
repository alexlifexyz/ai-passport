#include <assert.h>
#include <stdio.h>
#include "thunder_logic.h"

int main(void)
{
    printf("[TEST] Testing Thunder Striker Logic...\n");

    thunder_game_t g;
    thunder_init(&g);

    // 1. 验证初始状态 (6 HP, 4 核弹, Level 1 武器)
    assert(g.player_hp == 6);
    assert(g.player_max_hp == 6);
    assert(g.bombs == 4);
    assert(g.weapon_level == 1);
    assert(g.score == 0);
    assert(!g.game_over);
    printf("  ✓ Initialization OK (6 HP, 4 Bombs)\n");

    // 2. 验证左右移动与边界限制
    for (int i = 0; i < 30; i++) thunder_move_left(&g);
    assert(g.player_x >= 4.0f);

    for (int i = 0; i < 40; i++) thunder_move_right(&g);
    assert(g.player_x <= SCREEN_W - g.player_w - 4.0f);
    printf("  ✓ Movement & Screen Clamping OK\n");

    // 3. 验证开局高能自动开火 (初始 3 发强力光刃)
    g.shoot_timer = THUNDER_SHOOT_INTERVAL - 1;
    thunder_step(&g);
    int bullet_count = 0;
    for (int i = 0; i < THUNDER_MAX_BULLETS; i++) {
        if (g.bullets[i].active) bullet_count++;
    }
    assert(bullet_count >= 3);
    printf("  ✓ High-Energy 3-Way Laser Spawning OK\n");

    // 4. 验证高能子弹秒杀红煞轰炸机 (4 HP)
    // 模拟 Level 3 狂暴等离子主炮 (单发 dmg 4)
    g.weapon_level = 3;
    g.enemies[1].active = true;
    g.enemies[1].type = ENEMY_BOMBER;
    g.enemies[1].hp = 4;
    g.enemies[1].w = 32;
    g.enemies[1].h = 28;
    g.enemies[1].x = 100.0f;
    g.enemies[1].y = 100.0f;
    // 在其下方正中发射一发 dmg=4 等离子炮
    g.bullets[0].active = true;
    g.bullets[0].x = 110.0f;
    g.bullets[0].y = 105.0f;
    g.bullets[0].w = 8;
    g.bullets[0].h = 16;
    g.bullets[0].vx = 0;
    g.bullets[0].vy = -14.0f;
    g.bullets[0].dmg = 4;
    thunder_step(&g);
    // 敌机应被单发直接秒杀！
    assert(!g.enemies[1].active);
    printf("  ✓ High-Tier Bullet One-Shot Instakill Bomber (4 HP) OK\n");

    // 5. 验证全屏核弹清屏效果
    g.enemy_bullets[0].active = true;
    g.enemy_bullets[1].active = true;
    g.enemies[0].active = true;
    g.enemies[0].hp = 5;
    assert(thunder_use_bomb(&g) == true);
    assert(g.bombs == 3);
    assert(!g.enemy_bullets[0].active);
    assert(!g.enemy_bullets[1].active);
    assert(!g.enemies[0].active); // 5点血被 10 点核弹伤害秒杀
    printf("  ✓ Screen-Clearing Mega Bomb OK\n");

    // 6. 验证玩家扣血与阵亡
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
