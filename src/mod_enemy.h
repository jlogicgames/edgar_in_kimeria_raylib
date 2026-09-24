#ifndef EIK_MOD_ENEMY_H
#define EIK_MOD_ENEMY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "flecs.h"

#include "anim.h"
#include "collision.h"
#include "mod_level.h"
#include "mod_player.h"

#define EIK_MAX_ENEMIES 32U

typedef enum EikEnemyKind {
    EIK_ENEMY_BAT,
    EIK_ENEMY_YELLOW_MOB,
    EIK_ENEMY_RED_MOB,
} EikEnemyKind;

/* Payload for the synchronous Flecs event emitted when a sword or stomp hits. */
typedef struct EikEnemyStomped {
    uint32_t enemy_index;
    bool by_attack;
} EikEnemyStomped;

ECS_COMPONENT_DECLARE(EikEnemyStomped);

typedef struct EikEnemy {
    ecs_entity_t entity;
    EikEnemyKind kind;
    Vector2 position;
    Vector2 spawn_position;
    Vector2 size;
    Vector2 velocity;
    EIKContactState contact;
    EikAnimPlayer animation;
    EikActorState state;
    float range_neg;
    float range_pos;
    float patrol_direction;
    float target_direction;
    float move_direction;
    bool vertical;
    bool grounded;
    bool facing_right;
    bool attacking;
    bool returning_home;
    bool dying;
} EikEnemy;

typedef struct EikEnemyWorld {
    EikEnemy enemies[EIK_MAX_ENEMIES];
    size_t count;
} EikEnemyWorld;

void eik_enemy_register(ecs_world_t *world);
void eik_enemy_world_load(ecs_world_t *world, EikEnemyWorld *enemies,
    const EikLevelState *level);
void eik_enemy_fixed_step(ecs_world_t *world, EikEnemyWorld *enemies,
    EikPlayer *player, EikGameProgress *progress, const EIKCollisionWorld *collision,
    float fixed_dt);
void eik_enemy_update(ecs_world_t *world, EikEnemyWorld *enemies, float real_dt);
float eik_enemy_time_scale(const EikEnemyWorld *enemies, const EikPlayer *player);
Rectangle eik_enemy_frame_rect(const EikEnemy *enemy);

#endif
