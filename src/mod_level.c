#include "mod_level.h"

#include <stdio.h>
#include <string.h>

static bool is_collision_object(const EikTmxObject *object)
{
    return strcmp(object->layer, "Collisions") == 0
        || (strcmp(object->layer, "SpawnPoints") == 0
            && strcmp(object->class_name, "Actionable") == 0
            && strcmp(object->actionable_type, "Wall") == 0);
}

static void add_collision_kind(ecs_world_t *world, ecs_entity_t entity,
    const EikTmxObject *object)
{
    const char *class_name = object->class_name;

    if (strcmp(object->layer, "SpawnPoints") == 0) {
        class_name = "Wall";
    }
    if (strcmp(class_name, "Platform") == 0) {
        ecs_add_id(world, entity, EikPlatformBlock);
    } else if (strcmp(class_name, "QuickSand") == 0) {
        ecs_add_id(world, entity, EikQuickSandBlock);
    } else if (strcmp(class_name, "Wall") == 0) {
        ecs_add_id(world, entity, EikWallBlock);
    }
}

static bool is_known_spawn_class(const char *class_name)
{
    static const char *const known[] = {
        "Player", "Collectable", "Bat", "YellowMob", "RedMob", "Checkpoint",
        "Bomb", "Torch", "Trigger", "Escalator", "FallingPlatform", "Actionable",
    };
    size_t index = 0U;

    for (index = 0U; index < sizeof(known) / sizeof(known[0]); index++) {
        if (strcmp(class_name, known[index]) == 0) {
            return true;
        }
    }
    return false;
}

void eik_level_register(ecs_world_t *world)
{
    ECS_TAG_DEFINE(world, EikLevelEntity);
    ECS_TAG_DEFINE(world, EikCollisionBlock);
    ECS_TAG_DEFINE(world, EikPlatformBlock);
    ECS_TAG_DEFINE(world, EikQuickSandBlock);
    ECS_TAG_DEFINE(world, EikWallBlock);
    ECS_COMPONENT_DEFINE(world, EikLevelPosition);
    ECS_COMPONENT_DEFINE(world, EikLevelBoxSize);
    ECS_COMPONENT_DEFINE(world, EikSpawnIndex);
}

void eik_level_unload(ecs_world_t *world, EikLevelState *state)
{
    ecs_delete_with(world, EikLevelEntity);
    eik_tmx_unload(&state->map);
    *state = (EikLevelState){ 0 };
}

bool eik_level_load(ecs_world_t *world, EikLevelState *state, size_t index,
    const char *path, char *error, size_t error_size)
{
    size_t object_index = 0U;

    eik_level_unload(world, state);
    if (!eik_tmx_load(path, &state->map, error, error_size)) {
        return false;
    }
    state->index = index;
    state->size = (Vector2){
        (float)(state->map.width * state->map.tile_width),
        (float)(state->map.height * state->map.tile_height),
    };
    for (object_index = 0U; object_index < state->map.object_count; object_index++) {
        const EikTmxObject *object = &state->map.objects[object_index];
        const ecs_entity_t entity = ecs_new(world);

        ecs_add_id(world, entity, EikLevelEntity);
        ecs_set(world, entity, EikLevelPosition,
            { .value = { object->x, object->y } });
        ecs_set(world, entity, EikLevelBoxSize,
            { .value = { object->width, object->height } });
        ecs_set(world, entity, EikSpawnIndex, { .value = object_index });
        if (is_collision_object(object)) {
            ecs_add_id(world, entity, EikCollisionBlock);
            add_collision_kind(world, entity, object);
        } else if (strcmp(object->layer, "SpawnPoints") == 0
                && object->class_name[0] != '\0' && !is_known_spawn_class(object->class_name)) {
            (void)fprintf(stderr, "Edgard in Kimeria: unhandled SpawnPoints class '%s'\n",
                object->class_name);
        }
        if (strcmp(object->layer, "SpawnPoints") == 0
                && strcmp(object->class_name, "Player") == 0) {
            state->player_position = (Vector2){ object->x, object->y };
            state->has_player = true;
        }
    }
    return true;
}
