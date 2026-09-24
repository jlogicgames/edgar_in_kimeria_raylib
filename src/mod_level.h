#ifndef EIK_MOD_LEVEL_H
#define EIK_MOD_LEVEL_H

#include <stdbool.h>
#include <stddef.h>

#include "flecs.h"
#include "raylib.h"

#include "tmx.h"

typedef struct EikLevelPosition {
    Vector2 value;
} EikLevelPosition;

typedef struct EikLevelBoxSize {
    Vector2 value;
} EikLevelBoxSize;

typedef struct EikSpawnIndex {
    size_t value;
} EikSpawnIndex;

ECS_TAG_DECLARE(EikLevelEntity);
ECS_TAG_DECLARE(EikCollisionBlock);
ECS_TAG_DECLARE(EikPlatformBlock);
ECS_TAG_DECLARE(EikQuickSandBlock);
ECS_TAG_DECLARE(EikWallBlock);
ECS_COMPONENT_DECLARE(EikLevelPosition);
ECS_COMPONENT_DECLARE(EikLevelBoxSize);
ECS_COMPONENT_DECLARE(EikSpawnIndex);

typedef struct EikLevelState {
    EikTmxMap map;
    size_t index;
    Vector2 size;
    Vector2 player_position;
    bool has_player;
} EikLevelState;

void eik_level_register(ecs_world_t *world);
bool eik_level_load(ecs_world_t *world, EikLevelState *state, size_t index,
    const char *path, char *error, size_t error_size);
void eik_level_unload(ecs_world_t *world, EikLevelState *state);

#endif
