#include "collision.h"

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

float eik_hitbox_left_x(float actor_position_x, const EIKHitbox *hitbox,
    float box_width, bool facing_right)
{
    if (facing_right) {
        return actor_position_x + hitbox->offset.x;
    }
    return actor_position_x + box_width - hitbox->offset.x - hitbox->size.x;
}

Vector2 eik_mirrored_pos(Vector2 position, const EIKHitbox *hitbox,
    float box_width, bool facing_right)
{
    if (facing_right) {
        return position;
    }
    return (Vector2){
        position.x + box_width - (2.0F * hitbox->offset.x) - hitbox->size.x,
        position.y,
    };
}

static bool circle_overlaps_block(float center_x, float center_y, float radius,
    const EIKBlockSnapshot *block)
{
    const float closest_x = clamp_float(center_x, block->position.x,
        block->position.x + block->size.x);
    const float closest_y = clamp_float(center_y, block->position.y,
        block->position.y + block->size.y);
    const float delta_x = center_x - closest_x;
    const float delta_y = center_y - closest_y;

    return (delta_x * delta_x) + (delta_y * delta_y) < radius * radius;
}

bool eik_overlaps(Vector2 actor_position, const EIKHitbox *hitbox,
    float box_width, bool facing_right, const EIKBlockSnapshot *block)
{
    const float actor_x = actor_position.x + hitbox->offset.x;
    const float actor_y = actor_position.y + hitbox->offset.y;
    const float actor_bottom = actor_y + hitbox->size.y;
    float fixed_x = eik_hitbox_left_x(actor_position.x, hitbox, box_width,
        facing_right);

    if (block->kind == EIK_BLOCK_QUICKSAND) {
        fixed_x = actor_x;
    }
    if (block->kind == EIK_BLOCK_PLATFORM) {
        return actor_bottom < block->position.y + block->size.y
            && actor_bottom > block->position.y
            && fixed_x < block->position.x + block->size.x
            && fixed_x + hitbox->size.x > block->position.x;
    }
    if (hitbox->radius > 0.0F) {
        return circle_overlaps_block(fixed_x + hitbox->radius,
            actor_y + hitbox->radius, hitbox->radius, block);
    }
    return actor_y < block->position.y + block->size.y
        && actor_bottom > block->position.y
        && fixed_x < block->position.x + block->size.x
        && fixed_x + hitbox->size.x > block->position.x;
}

static bool overlaps_surface(Vector2 actor_position, const EIKHitbox *hitbox,
    float box_width, bool facing_right, const EIKSurfaceSnapshot *surface)
{
    const EIKBlockSnapshot block = {
        .id = surface->id,
        .position = surface->position,
        .size = surface->size,
        .kind = EIK_BLOCK_SOLID,
    };

    return eik_overlaps(actor_position, hitbox, box_width, facing_right, &block);
}

static bool resolve_horizontal_contact(EIKActorBody *body, Vector2 position,
    Vector2 size, bool clamberable)
{
    if (body->velocity->x > 0.0F) {
        body->velocity->x = 0.0F;
        body->position->x = position.x - body->hitbox.offset.x - body->hitbox.size.x;
        if (clamberable && !*body->grounded) {
            body->contact->clambering = true;
        }
        return true;
    }
    if (body->velocity->x < 0.0F) {
        body->velocity->x = 0.0F;
        body->position->x = position.x + size.x - body->box_width
            + body->hitbox.offset.x + body->hitbox.size.x;
        if (clamberable && !*body->grounded) {
            body->contact->clambering = true;
        }
        return true;
    }
    return false;
}

void eik_resolve_horizontal(EIKActorBody *body, const EIKCollisionWorld *world)
{
    size_t index = 0U;

    body->contact->in_quicksand = false;
    body->contact->clambering = false;
    for (index = 0U; index < world->block_count; ++index) {
        const EIKBlockSnapshot *block = &world->blocks[index];

        if (block->kind == EIK_BLOCK_QUICKSAND) {
            body->contact->in_quicksand = eik_overlaps(*body->position,
                &body->hitbox, body->box_width, body->facing_right, block);
            continue;
        }
        if (eik_overlaps(*body->position, &body->hitbox, body->box_width,
                body->facing_right, block)
            && resolve_horizontal_contact(body, block->position, block->size,
                block->kind == EIK_BLOCK_WALL)) {
            return;
        }
    }
    for (index = 0U; index < world->escalator_count; ++index) {
        const EIKSurfaceSnapshot *surface = &world->escalators[index];

        if (overlaps_surface(*body->position, &body->hitbox, body->box_width,
                body->facing_right, surface)
            && resolve_horizontal_contact(body, surface->position, surface->size, false)) {
            return;
        }
    }
    for (index = 0U; index < world->falling_platform_count; ++index) {
        const EIKSurfaceSnapshot *surface = &world->falling_platforms[index];

        if (overlaps_surface(*body->position, &body->hitbox, body->box_width,
                body->facing_right, surface)) {
            (void)resolve_horizontal_contact(body, surface->position, surface->size, false);
            return;
        }
    }
}

void eik_apply_gravity(Vector2 *velocity, Vector2 *position,
    const EIKGravity *gravity, float dt)
{
    velocity->y += gravity->acceleration * dt;
    velocity->y = clamp_float(velocity->y, -gravity->jump_force,
        gravity->terminal_velocity);
    position->y += velocity->y * dt;
}

static void land_on_surface(EIKActorBody *body, Vector2 position)
{
    body->velocity->y = 0.0F;
    body->position->y = position.y - body->hitbox.size.y - body->hitbox.offset.y;
    *body->grounded = true;
}

static void hit_surface_from_below(EIKActorBody *body, Vector2 position, Vector2 size)
{
    body->velocity->y = 0.0F;
    body->position->y = position.y + size.y - body->hitbox.offset.y;
}

EIKVerticalOutcome eik_resolve_vertical(EIKActorBody *body,
    const EIKCollisionWorld *world)
{
    EIKVerticalOutcome outcome = { 0 };
    size_t index = 0U;

    *body->grounded = false;
    body->contact->on_escalator = false;
    body->contact->escalator_id = 0U;
    for (index = 0U; index < world->block_count; ++index) {
        const EIKBlockSnapshot *block = &world->blocks[index];

        if (!eik_overlaps(*body->position, &body->hitbox, body->box_width,
                body->facing_right, block)) {
            continue;
        }
        if (block->kind == EIK_BLOCK_QUICKSAND && body->velocity->y > 0.0F) {
            body->velocity->y = 0.0F;
            *body->grounded = true;
            break;
        }
        if ((block->kind == EIK_BLOCK_SOLID || block->kind == EIK_BLOCK_WALL
                || block->kind == EIK_BLOCK_PLATFORM)
            && body->velocity->y > 0.0F) {
            land_on_surface(body, block->position);
            break;
        }
        if ((block->kind == EIK_BLOCK_SOLID || block->kind == EIK_BLOCK_WALL)
            && body->velocity->y < 0.0F) {
            hit_surface_from_below(body, block->position, block->size);
        }
    }
    for (index = 0U; index < world->escalator_count; ++index) {
        const EIKSurfaceSnapshot *surface = &world->escalators[index];

        if (overlaps_surface(*body->position, &body->hitbox, body->box_width,
                body->facing_right, surface)
            && body->velocity->y > 0.0F) {
            land_on_surface(body, surface->position);
            body->contact->on_escalator = true;
            body->contact->escalator_id = surface->id;
            break;
        }
    }
    for (index = 0U; index < world->falling_platform_count; ++index) {
        const EIKSurfaceSnapshot *surface = &world->falling_platforms[index];

        if (!overlaps_surface(*body->position, &body->hitbox, body->box_width,
                body->facing_right, surface)) {
            continue;
        }
        if (body->velocity->y > 0.0F) {
            land_on_surface(body, surface->position);
            if (!surface->is_falling) {
                outcome.trigger_fall = true;
                outcome.falling_platform_id = surface->id;
            }
            break;
        }
        if (body->velocity->y < 0.0F) {
            hit_surface_from_below(body, surface->position, surface->size);
        }
    }
    return outcome;
}
