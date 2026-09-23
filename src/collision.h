#ifndef EIK_COLLISION_H
#define EIK_COLLISION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "raylib.h"

typedef enum EIKBlockKind {
    EIK_BLOCK_SOLID,
    EIK_BLOCK_PLATFORM,
    EIK_BLOCK_QUICKSAND,
    EIK_BLOCK_WALL,
} EIKBlockKind;

typedef struct EIKHitbox {
    Vector2 offset;
    Vector2 size;
    float radius;
} EIKHitbox;

typedef struct EIKBlockSnapshot {
    uint32_t id;
    Vector2 position;
    Vector2 size;
    EIKBlockKind kind;
} EIKBlockSnapshot;

typedef struct EIKSurfaceSnapshot {
    uint32_t id;
    Vector2 position;
    Vector2 size;
    Vector2 velocity;
    bool is_falling;
} EIKSurfaceSnapshot;

typedef struct EIKCollisionWorld {
    const EIKBlockSnapshot *blocks;
    size_t block_count;
    const EIKSurfaceSnapshot *escalators;
    size_t escalator_count;
    const EIKSurfaceSnapshot *falling_platforms;
    size_t falling_platform_count;
} EIKCollisionWorld;

typedef struct EIKContactState {
    bool in_quicksand;
    bool clambering;
    bool on_escalator;
    uint32_t escalator_id;
} EIKContactState;

typedef struct EIKGravity {
    float acceleration;
    float terminal_velocity;
    float jump_force;
} EIKGravity;

typedef struct EIKActorBody {
    Vector2 *position;
    Vector2 *velocity;
    bool *grounded;
    EIKContactState *contact;
    EIKHitbox hitbox;
    float box_width;
    bool facing_right;
} EIKActorBody;

typedef struct EIKVerticalOutcome {
    bool trigger_fall;
    uint32_t falling_platform_id;
} EIKVerticalOutcome;

float eik_hitbox_left_x(float actor_position_x, const EIKHitbox *hitbox,
    float box_width, bool facing_right);
Vector2 eik_mirrored_pos(Vector2 position, const EIKHitbox *hitbox,
    float box_width, bool facing_right);
bool eik_overlaps(Vector2 actor_position, const EIKHitbox *hitbox,
    float box_width, bool facing_right, const EIKBlockSnapshot *block);
void eik_resolve_horizontal(EIKActorBody *body, const EIKCollisionWorld *world);
void eik_apply_gravity(Vector2 *velocity, Vector2 *position,
    const EIKGravity *gravity, float dt);
EIKVerticalOutcome eik_resolve_vertical(EIKActorBody *body,
    const EIKCollisionWorld *world);

#endif
