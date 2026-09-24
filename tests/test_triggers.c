#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mod_items.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static EikItem *add_item(ecs_world_t *world, EikItemWorld *items, EikItemKind kind,
    const char *target_id)
{
    EikItem *item = &items->items[items->count++];

    *item = (EikItem){ .entity = ecs_new(world), .kind = kind, .active = true,
        .running = true, .direction = 1.0F };
    (void)snprintf(item->target_id, sizeof(item->target_id), "%s", target_id);
    return item;
}

static void fire(ecs_world_t *world, const EikItemWorld *items, const char *target_id)
{
    EikTriggerActivated event = { 0 };
    ecs_id_t dispatch_id = EikTriggerDispatch;
    const ecs_type_t ids = { .array = &dispatch_id, .count = 1 };

    (void)snprintf(event.target_id, sizeof(event.target_id), "%s", target_id);
    ecs_emit(world, &(ecs_event_desc_t){
        .event = items->trigger_event, .ids = &ids, .entity = items->trigger_dispatch,
        .const_param = &event,
    });
}

static bool matching_trigger_removes_wall(void)
{
    ecs_world_t *world = ecs_init();
    EikItemWorld items = { 0 };
    EikItem *wall = NULL;

    eik_items_register(world, &items);
    wall = add_item(world, &items, EIK_ITEM_WALL, "Wall1");
    fire(world, &items, "Wall1");
    CHECK(!wall->active);
    CHECK(!ecs_is_alive(world, wall->entity));
    ecs_fini(world);
    return true;
}

static bool another_trigger_leaves_wall_alone(void)
{
    ecs_world_t *world = ecs_init();
    EikItemWorld items = { 0 };
    EikItem *wall = NULL;

    eik_items_register(world, &items);
    wall = add_item(world, &items, EIK_ITEM_WALL, "Wall1");
    fire(world, &items, "SomeOtherDoor");
    CHECK(wall->active);
    CHECK(ecs_is_alive(world, wall->entity));
    ecs_fini(world);
    return true;
}

static bool trigger_toggles_escalator(void)
{
    ecs_world_t *world = ecs_init();
    EikItemWorld items = { 0 };
    EikItem *escalator = NULL;

    eik_items_register(world, &items);
    escalator = add_item(world, &items, EIK_ITEM_ESCALATOR, "Lift");
    fire(world, &items, "Lift");
    CHECK(!escalator->running);
    fire(world, &items, "Lift");
    CHECK(escalator->running);
    ecs_fini(world);
    return true;
}

static bool trigger_toggles_torch_at_fixed_relight_intensity(void)
{
    ecs_world_t *world = ecs_init();
    EikItemWorld items = { 0 };
    EikItem *torch = NULL;

    eik_items_register(world, &items);
    torch = add_item(world, &items, EIK_ITEM_TORCH, "Torch1");
    torch->intensity = 100.0F;
    fire(world, &items, "Torch1");
    CHECK(torch->intensity == 0.0F);
    fire(world, &items, "Torch1");
    CHECK(torch->intensity == 200.0F);
    ecs_fini(world);
    return true;
}

static bool trigger_only_reaches_its_own_torch(void)
{
    ecs_world_t *world = ecs_init();
    EikItemWorld items = { 0 };
    EikItem *mine = NULL;
    EikItem *theirs = NULL;

    eik_items_register(world, &items);
    mine = add_item(world, &items, EIK_ITEM_TORCH, "Torch1");
    theirs = add_item(world, &items, EIK_ITEM_TORCH, "Torch2");
    mine->intensity = 100.0F;
    theirs->intensity = 100.0F;
    fire(world, &items, "Torch1");
    CHECK(mine->intensity == 0.0F);
    CHECK(theirs->intensity == 100.0F);
    ecs_fini(world);
    return true;
}

static bool vertical_escalator_carries_rider_vertically(void)
{
    EikItemWorld items = { 0 };
    EIKSurfaceSnapshot escalators[1];
    EIKSurfaceSnapshot falling_platforms[1];
    EIKCollisionWorld world;
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    const EikInputFrame input = { 0 };

    items.count = 1U;
    items.items[0] = (EikItem){
        .kind = EIK_ITEM_ESCALATOR, .active = true, .running = true, .vertical = true,
        .direction = 1.0F, .position = { 0.0F, 200.0F }, .size = { 32.0F, 16.0F },
        .range_neg = 100.0F, .range_pos = 300.0F,
    };
    eik_items_virtual_step(&items, 1.0F / 60.0F);
    world = eik_items_collision_world(&items, escalators, 1U, falling_platforms, 1U);
    eik_player_spawn(&player, (Vector2){ 0.0F, 152.0F });
    player.grounded = true;
    player.contact.on_escalator = true;
    player.contact.escalator_id = 0U;
    (void)eik_player_fixed_step(&player, &progress, &input, &world, 1.0F / 60.0F);
    CHECK(player.position.y > 152.7F);
    return true;
}

static bool item_contact_queues_the_matching_visual_effects(void)
{
    ecs_world_t *world = ecs_init();
    EikItemWorld items = { 0 };
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    size_t index = 0U;

    eik_player_spawn(&player, (Vector2){ 0.0F, 0.0F });
    for (index = 0U; index < 3U; ++index) {
        items.items[index] = (EikItem){
            .entity = ecs_new(world), .active = true,
            .kind = index == 0U ? EIK_ITEM_COIN
                : (index == 1U ? EIK_ITEM_HEART : EIK_ITEM_BOMB),
            .position = { 18.0F, 26.0F }, .size = { 11.0F, 22.0F },
        };
    }
    items.count = 3U;
    eik_items_contact_step(world, &items, &player, &progress);
    CHECK(items.effect_count == 3U);
    CHECK(items.effects[0].kind == EIK_ITEM_EFFECT_RIPPLE);
    CHECK(items.effects[1].kind == EIK_ITEM_EFFECT_SHOCKWAVE);
    CHECK(items.effects[2].kind == EIK_ITEM_EFFECT_EXPLOSION);
    CHECK(items.effects[0].centre.x == 23.5F && items.effects[0].centre.y == 37.0F);
    ecs_fini(world);
    return true;
}

int main(void)
{
    return matching_trigger_removes_wall()
            && another_trigger_leaves_wall_alone()
            && trigger_toggles_escalator()
            && trigger_toggles_torch_at_fixed_relight_intensity()
            && trigger_only_reaches_its_own_torch()
            && vertical_escalator_carries_rider_vertically()
            && item_contact_queues_the_matching_visual_effects() ? 0 : 1;
}
