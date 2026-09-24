#include "mod_items.h"

#include <stdio.h>
#include <string.h>

#define EIK_ESCALATOR_SPEED 50.0F
#define EIK_ESCALATOR_TILE_SIZE 32.0F
#define EIK_FALL_DELAY 1.0F
#define EIK_FALL_DISTANCE 200.0F
#define EIK_FALL_DURATION 1.5F

static const EIKHitbox player_hitbox = {
    .offset = { 18.0F, 26.0F }, .size = { 11.0F, 22.0F }, .radius = 0.0F,
};

static bool is_spawn_object(const EikTmxObject *object, const char *class_name)
{
    return strcmp(object->layer, "SpawnPoints") == 0
        && strcmp(object->class_name, class_name) == 0;
}

static EikItem *add_item(ecs_world_t *world, EikItemWorld *items, EikItemKind kind,
    const EikTmxObject *object)
{
    EikItem *item = NULL;

    if (items->count >= EIK_MAX_ITEMS) {
        (void)fprintf(stderr, "Edgard in Kimeria: too many Phase 5 objects\n");
        return NULL;
    }
    item = &items->items[items->count++];
    *item = (EikItem){
        .entity = ecs_new(world), .kind = kind,
        .position = { object->x, object->y }, .size = { object->width, object->height },
        .direction = 1.0F, .active = true, .running = true,
    };
    (void)snprintf(item->target_id, sizeof(item->target_id), "%s", object->name);
    ecs_add_id(world, item->entity, EikLevelEntity);
    return item;
}

static EikItemKind collectable_kind(const EikTmxObject *object)
{
    return strcmp(object->name, "Heart") == 0 ? EIK_ITEM_HEART : EIK_ITEM_COIN;
}

static void spawn_from_object(ecs_world_t *world, EikItemWorld *items,
    const EikTmxObject *object)
{
    EikItem *item = NULL;
    const float axis = object->is_vertical ? object->y : object->x;

    if (is_spawn_object(object, "Collectable")) {
        (void)add_item(world, items, collectable_kind(object), object);
    } else if (is_spawn_object(object, "Bomb")) {
        (void)add_item(world, items, EIK_ITEM_BOMB, object);
    } else if (is_spawn_object(object, "Checkpoint")) {
        (void)add_item(world, items, EIK_ITEM_CHECKPOINT, object);
    } else if (is_spawn_object(object, "Trigger")) {
        (void)add_item(world, items, EIK_ITEM_TRIGGER, object);
    } else if (is_spawn_object(object, "Torch")) {
        item = add_item(world, items, EIK_ITEM_TORCH, object);
        if (item != NULL) {
            item->intensity = object->intensity > 0.0F ? object->intensity : 80.0F;
        }
    } else if (is_spawn_object(object, "Actionable")) {
        if (strcmp(object->actionable_type, "Wall") == 0) {
            (void)add_item(world, items, EIK_ITEM_WALL, object);
        } else if (strcmp(object->actionable_type, "Torch") == 0) {
            item = add_item(world, items, EIK_ITEM_TORCH, object);
            if (item != NULL) {
                item->intensity = object->intensity;
            }
        }
    } else if (is_spawn_object(object, "Escalator")) {
        item = add_item(world, items, EIK_ITEM_ESCALATOR, object);
        if (item != NULL) {
            item->vertical = object->is_vertical;
            item->range_neg = axis - object->off_neg * EIK_ESCALATOR_TILE_SIZE;
            item->range_pos = axis + object->off_pos * EIK_ESCALATOR_TILE_SIZE;
        }
    } else if (is_spawn_object(object, "FallingPlatform")) {
        (void)add_item(world, items, EIK_ITEM_FALLING_PLATFORM, object);
    }
}

static void trigger_observer(ecs_iter_t *iterator)
{
    EikItemWorld *items = iterator->ctx;
    const EikTriggerActivated *event = iterator->param;
    size_t index = 0U;

    if (items == NULL || event == NULL) {
        return;
    }
    for (index = 0U; index < items->count; ++index) {
        EikItem *item = &items->items[index];

        if (!item->active || strcmp(item->target_id, event->target_id) != 0) {
            continue;
        }
        if (item->kind == EIK_ITEM_WALL) {
            item->active = false;
            ecs_delete(iterator->world, item->entity);
        } else if (item->kind == EIK_ITEM_ESCALATOR) {
            item->running = !item->running;
        } else if (item->kind == EIK_ITEM_TORCH) {
            item->intensity = item->intensity > 0.0F ? 0.0F : 200.0F;
        }
    }
}

void eik_items_register(ecs_world_t *world, EikItemWorld *items)
{
    ECS_COMPONENT_DEFINE(world, EikTriggerActivated);
    ECS_TAG_DEFINE(world, EikTriggerDispatch);
    items->trigger_event = ecs_new(world);
    items->trigger_dispatch = ecs_new_w_id(world, EikTriggerDispatch);
    (void)ecs_observer_init(world, &(ecs_observer_desc_t){
        .query = { .terms = {{ .id = EikTriggerDispatch }} },
        .events = { items->trigger_event }, .callback = trigger_observer, .ctx = items,
    });
}

void eik_items_world_load(ecs_world_t *world, EikItemWorld *items,
    const EikLevelState *level)
{
    size_t index = 0U;
    const ecs_entity_t trigger_event = items->trigger_event;
    const ecs_entity_t trigger_dispatch = items->trigger_dispatch;

    *items = (EikItemWorld){ 0 };
    items->trigger_event = trigger_event;
    items->trigger_dispatch = trigger_dispatch;
    for (index = 0U; index < level->map.object_count; ++index) {
        spawn_from_object(world, items, &level->map.objects[index]);
    }
}

void eik_items_virtual_step(EikItemWorld *items, float virtual_dt)
{
    size_t index = 0U;

    for (index = 0U; index < items->count; ++index) {
        EikItem *item = &items->items[index];
        float *axis = NULL;

        if (!item->active) {
            continue;
        }
        if (item->kind == EIK_ITEM_ESCALATOR) {
            axis = item->vertical ? &item->position.y : &item->position.x;
            if (*axis >= item->range_pos) {
                item->direction = -1.0F;
            } else if (*axis <= item->range_neg) {
                item->direction = 1.0F;
            }
            *axis += item->direction * EIK_ESCALATOR_SPEED * virtual_dt;
        } else if (item->kind == EIK_ITEM_FALLING_PLATFORM) {
            if (item->fall_phase == EIK_FALL_WARNING) {
                item->elapsed += virtual_dt;
                if (item->elapsed >= EIK_FALL_DELAY) {
                    item->fall_phase = EIK_FALL_DROPPING;
                    item->elapsed = 0.0F;
                    item->drop_start_y = item->position.y;
                }
            } else if (item->fall_phase == EIK_FALL_DROPPING) {
                item->elapsed += virtual_dt;
                item->position.y = item->drop_start_y + EIK_FALL_DISTANCE
                    * (item->elapsed / EIK_FALL_DURATION);
                if (item->elapsed >= EIK_FALL_DURATION) {
                    item->active = false;
                }
            }
        }
    }
}

static Rectangle player_box(const EikPlayer *player)
{
    const Vector2 position = eik_mirrored_pos(player->position, &player_hitbox, 48.0F,
        player->facing_right);

    return (Rectangle){ position.x + player_hitbox.offset.x, position.y + player_hitbox.offset.y,
        player_hitbox.size.x, player_hitbox.size.y };
}

void eik_items_contact_step(ecs_world_t *world, EikItemWorld *items,
    EikPlayer *player, EikGameProgress *progress)
{
    const Rectangle actor = player_box(player);
    size_t index = 0U;

    items->overlapped_trigger[0] = '\0';
    if (player->routine != EIK_PLAYER_ACTIVE) {
        return;
    }
    for (index = 0U; index < items->count; ++index) {
        EikItem *item = &items->items[index];
        const Rectangle object = { item->position.x, item->position.y, item->size.x, item->size.y };

        if (!item->active || !CheckCollisionRecs(actor, object)) {
            continue;
        }
        if (item->kind == EIK_ITEM_COIN) {
            progress->coins_collected++;
            item->active = false;
            ecs_delete(world, item->entity);
        } else if (item->kind == EIK_ITEM_HEART) {
            if (progress->lives < 3) {
                progress->lives++;
            }
            item->active = false;
            ecs_delete(world, item->entity);
        } else if (item->kind == EIK_ITEM_BOMB) {
            item->active = false;
            ecs_delete(world, item->entity);
            eik_player_kill(player, progress);
        } else if (item->kind == EIK_ITEM_CHECKPOINT) {
            eik_player_reach_checkpoint(player);
        } else if (item->kind == EIK_ITEM_TRIGGER && items->overlapped_trigger[0] == '\0') {
            (void)snprintf(items->overlapped_trigger, sizeof(items->overlapped_trigger), "%s",
                item->target_id);
        }
    }
}

void eik_items_activate_trigger(ecs_world_t *world, const EikItemWorld *items)
{
    EikTriggerActivated event = { 0 };
    ecs_id_t dispatch_id = EikTriggerDispatch;
    const ecs_type_t ids = { .array = &dispatch_id, .count = 1 };

    if (items->overlapped_trigger[0] == '\0') {
        return;
    }
    (void)snprintf(event.target_id, sizeof(event.target_id), "%s", items->overlapped_trigger);
    ecs_emit(world, &(ecs_event_desc_t){
        .event = items->trigger_event, .ids = &ids, .entity = items->trigger_dispatch,
        .const_param = &event,
    });
}

void eik_items_trigger_fall(EikItemWorld *items, uint32_t id)
{
    size_t index = 0U;

    for (index = 0U; index < items->count; ++index) {
        EikItem *item = &items->items[index];

        if (item->kind == EIK_ITEM_FALLING_PLATFORM && item->active
            && (uint32_t)index == id && item->fall_phase == EIK_FALL_IDLE) {
            item->fall_phase = EIK_FALL_WARNING;
            item->elapsed = 0.0F;
        }
    }
}

EIKCollisionWorld eik_items_collision_world(const EikItemWorld *items,
    EIKSurfaceSnapshot *escalators, size_t escalator_capacity,
    EIKSurfaceSnapshot *falling_platforms, size_t falling_platform_capacity)
{
    size_t index = 0U;
    size_t escalator_count = 0U;
    size_t platform_count = 0U;

    for (index = 0U; index < items->count; ++index) {
        const EikItem *item = &items->items[index];

        if (!item->active) {
            continue;
        }
        if (item->kind == EIK_ITEM_ESCALATOR && escalator_count < escalator_capacity) {
            escalators[escalator_count++] = (EIKSurfaceSnapshot){
                .id = (uint32_t)index, .position = item->position, .size = item->size,
                .velocity = item->running ? (item->vertical
                    ? (Vector2){ 0.0F, item->direction * EIK_ESCALATOR_SPEED }
                    : (Vector2){ item->direction * EIK_ESCALATOR_SPEED, 0.0F })
                    : (Vector2){ 0.0F, 0.0F },
            };
        } else if (item->kind == EIK_ITEM_FALLING_PLATFORM
                && platform_count < falling_platform_capacity) {
            falling_platforms[platform_count++] = (EIKSurfaceSnapshot){
                .id = (uint32_t)index, .position = item->position, .size = item->size,
                .is_falling = item->fall_phase != EIK_FALL_IDLE,
            };
        }
    }
    return (EIKCollisionWorld){
        .escalators = escalators, .escalator_count = escalator_count,
        .falling_platforms = falling_platforms, .falling_platform_count = platform_count,
    };
}
