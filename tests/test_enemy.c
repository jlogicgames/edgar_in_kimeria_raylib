#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "mod_enemy.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static EikEnemy make_enemy(ecs_world_t *world, EikEnemyKind kind, Vector2 position)
{
    EikEnemy enemy = {
        .entity = ecs_new(world),
        .kind = kind,
        .position = position,
        .spawn_position = position,
        .size = kind == EIK_ENEMY_BAT ? (Vector2){ 16.0F, 16.0F }
            : (Vector2){ 48.0F, 32.0F },
        .range_neg = -1000.0F,
        .range_pos = 1000.0F,
        .patrol_direction = 1.0F,
        .target_direction = -1.0F,
        .facing_right = true,
        .state = EIK_ACTOR_IDLE,
    };

    eik_anim_reset(&enemy.animation, EIK_ACTOR_IDLE);
    return enemy;
}

static bool bat_slows_physics_but_not_animation(void)
{
    ecs_world_t *world = ecs_init();
    EikEnemyWorld enemies = { 0 };
    EikPlayer player;

    eik_enemy_register(world);
    eik_player_spawn(&player, (Vector2){ 0.0F, 0.0F });
    enemies.enemies[0] = make_enemy(world, EIK_ENEMY_BAT, (Vector2){ 20.0F, 20.0F });
    enemies.count = 1U;
    CHECK(fabsf(eik_enemy_time_scale(&enemies, &player) - 0.5F) < 0.001F);
    eik_enemy_update(world, &enemies, 0.03F);
    CHECK(enemies.enemies[0].animation.frame == 1);
    enemies.enemies[0].position.x = 200.0F;
    CHECK(fabsf(eik_enemy_time_scale(&enemies, &player) - 1.0F) < 0.001F);
    ecs_fini(world);
    return true;
}

static bool red_mob_returns_to_its_spawn_point(void)
{
    ecs_world_t *world = ecs_init();
    EikEnemyWorld enemies = { 0 };
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    const EIKCollisionWorld collision = { 0 };

    eik_enemy_register(world);
    eik_player_spawn(&player, (Vector2){ 100.0F, 0.0F });
    enemies.enemies[0] = make_enemy(world, EIK_ENEMY_RED_MOB, (Vector2){ 100.0F, 0.0F });
    enemies.count = 1U;
    eik_enemy_fixed_step(world, &enemies, &player, &progress, &collision, 1.0F / 60.0F);
    CHECK(enemies.enemies[0].attacking);
    eik_enemy_update(world, &enemies, 0.81F);
    CHECK(enemies.enemies[0].returning_home);
    enemies.enemies[0].position.x = 140.0F;
    player.position.x = -500.0F;
    eik_enemy_fixed_step(world, &enemies, &player, &progress, &collision, 0.25F);
    CHECK(enemies.enemies[0].position.x < 140.0F);
    ecs_fini(world);
    return true;
}

static bool red_mob_can_be_stomped_and_bounces_player(void)
{
    ecs_world_t *world = ecs_init();
    EikEnemyWorld enemies = { 0 };
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    const EIKCollisionWorld collision = { 0 };

    eik_enemy_register(world);
    eik_player_spawn(&player, (Vector2){ 100.0F, 60.0F });
    player.velocity.y = 10.0F;
    enemies.enemies[0] = make_enemy(world, EIK_ENEMY_RED_MOB, (Vector2){ 100.0F, 100.0F });
    enemies.count = 1U;
    eik_enemy_fixed_step(world, &enemies, &player, &progress, &collision, 1.0F / 60.0F);
    CHECK(enemies.enemies[0].dying);
    CHECK(fabsf(player.velocity.y + 260.0F) < 0.001F);
    CHECK(player.routine == EIK_PLAYER_ACTIVE);
    ecs_fini(world);
    return true;
}

static bool sword_emits_a_stomp_and_kills_one_enemy(void)
{
    ecs_world_t *world = ecs_init();
    EikEnemyWorld enemies = { 0 };
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    const EIKCollisionWorld collision = { 0 };

    eik_enemy_register(world);
    eik_player_spawn(&player, (Vector2){ 100.0F, 100.0F });
    player.attacking = true;
    player.state = EIK_ACTOR_ATTACKING;
    enemies.enemies[0] = make_enemy(world, EIK_ENEMY_YELLOW_MOB, (Vector2){ 112.0F, 100.0F });
    enemies.count = 1U;
    eik_enemy_fixed_step(world, &enemies, &player, &progress, &collision, 1.0F / 60.0F);
    CHECK(enemies.enemies[0].dying);
    CHECK(player.routine == EIK_PLAYER_ACTIVE);
    ecs_fini(world);
    return true;
}

int main(void)
{
    return bat_slows_physics_but_not_animation()
            && red_mob_returns_to_its_spawn_point()
            && red_mob_can_be_stomped_and_bounces_player()
            && sword_emits_a_stomp_and_kills_one_enemy() ? 0 : 1;
}
