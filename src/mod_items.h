#ifndef EIK_MOD_ITEMS_H
#define EIK_MOD_ITEMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "flecs.h"

#include "collision.h"
#include "mod_level.h"
#include "mod_player.h"

#define EIK_MAX_ITEMS 64U

typedef enum EikItemKind {
    EIK_ITEM_COIN,
    EIK_ITEM_HEART,
    EIK_ITEM_BOMB,
    EIK_ITEM_CHECKPOINT,
    EIK_ITEM_TRIGGER,
    EIK_ITEM_TORCH,
    EIK_ITEM_WALL,
    EIK_ITEM_ESCALATOR,
    EIK_ITEM_FALLING_PLATFORM,
} EikItemKind;

typedef enum EikFallingPhase {
    EIK_FALL_IDLE,
    EIK_FALL_WARNING,
    EIK_FALL_DROPPING,
} EikFallingPhase;

typedef struct EikTriggerActivated {
    char target_id[32];
} EikTriggerActivated;

ECS_COMPONENT_DECLARE(EikTriggerActivated);
ECS_TAG_DECLARE(EikTriggerDispatch);

typedef struct EikItem {
    ecs_entity_t entity;
    EikItemKind kind;
    Vector2 position;
    Vector2 size;
    char target_id[32];
    float range_neg;
    float range_pos;
    float direction;
    float intensity;
    float elapsed;
    float drop_start_y;
    bool vertical;
    bool active;
    bool running;
    EikFallingPhase fall_phase;
} EikItem;

typedef struct EikItemWorld {
    EikItem items[EIK_MAX_ITEMS];
    size_t count;
    char overlapped_trigger[32];
    ecs_entity_t trigger_event;
    ecs_entity_t trigger_dispatch;
} EikItemWorld;

void eik_items_register(ecs_world_t *world, EikItemWorld *items);
void eik_items_world_load(ecs_world_t *world, EikItemWorld *items,
    const EikLevelState *level);
void eik_items_virtual_step(EikItemWorld *items, float virtual_dt);
void eik_items_contact_step(ecs_world_t *world, EikItemWorld *items,
    EikPlayer *player, EikGameProgress *progress);
void eik_items_activate_trigger(ecs_world_t *world, const EikItemWorld *items);
void eik_items_trigger_fall(EikItemWorld *items, uint32_t id);
EIKCollisionWorld eik_items_collision_world(const EikItemWorld *items,
    EIKSurfaceSnapshot *escalators, size_t escalator_capacity,
    EIKSurfaceSnapshot *falling_platforms, size_t falling_platform_capacity);

#endif
