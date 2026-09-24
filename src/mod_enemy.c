#include "mod_enemy.h"

#include <math.h>
#include <string.h>

#define EIK_TILE_SIZE 16.0F
#define EIK_BAT_SPEED 50.0F
#define EIK_MOB_SPEED 80.0F
#define EIK_BOUNCE_SPEED 260.0F
#define EIK_RED_ATTACK_RANGE 65.0F
#define EIK_BAT_SLOWDOWN_RANGE 50.0F

static const EIKHitbox bat_hitbox = {
    .offset = { 0.0F, 0.0F }, .size = { 16.0F, 16.0F }, .radius = 8.0F,
};
static const EIKHitbox mob_hitbox = {
    .offset = { 10.0F, 6.0F }, .size = { 14.0F, 26.0F }, .radius = 0.0F,
};
static const EIKGravity mob_gravity = {
    .acceleration = 588.0F, .terminal_velocity = 300.0F, .jump_force = 260.0F,
};

static EIKHitbox enemy_hitbox(const EikEnemy *enemy)
{
    return enemy->kind == EIK_ENEMY_BAT ? bat_hitbox : mob_hitbox;
}

static bool enemy_is_mob(const EikEnemy *enemy)
{
    return enemy->kind == EIK_ENEMY_YELLOW_MOB || enemy->kind == EIK_ENEMY_RED_MOB;
}

static void enemy_anim_reset(EikEnemy *enemy, EikActorState state)
{
    enemy->state = state;
    eik_anim_reset(&enemy->animation, state);
}

static void enemy_clip(const EikEnemy *enemy, int *origin_y, int *frames,
    float *step_time, bool *looping)
{
    *origin_y = 0;
    *frames = 4;
    *step_time = 0.1F;
    *looping = true;
    if (enemy->kind == EIK_ENEMY_BAT) {
        *origin_y = enemy->state == EIK_ACTOR_HIT ? 64
            : enemy->state == EIK_ACTOR_RUNNING ? 32 : 0;
        *frames = enemy->state == EIK_ACTOR_HIT ? 4 : 5;
        *step_time = 0.03F;
        *looping = enemy->state != EIK_ACTOR_HIT;
    } else {
        *origin_y = enemy->state == EIK_ACTOR_IDLE ? 5 * 32
            : enemy->state == EIK_ACTOR_RUNNING ? 32
            : enemy->state == EIK_ACTOR_ATTACKING ? 2 * 32 : 4 * 32;
        *step_time = enemy->kind == EIK_ENEMY_RED_MOB
            && enemy->state == EIK_ACTOR_ATTACKING ? 0.2F
            : enemy->kind == EIK_ENEMY_YELLOW_MOB ? 0.05F : 0.1F;
        *looping = enemy->state != EIK_ACTOR_HIT
            && enemy->state != EIK_ACTOR_ATTACKING;
    }
}

static void enemy_anim_advance(EikEnemy *enemy, float real_dt)
{
    int ignored_origin = 0;
    int frames = 0;
    float step_time = 0.0F;
    bool looping = false;

    enemy_clip(enemy, &ignored_origin, &frames, &step_time, &looping);
    if (enemy->animation.current != enemy->state) {
        eik_anim_reset(&enemy->animation, enemy->state);
    }
    if (enemy->animation.finished && !looping) {
        return;
    }
    enemy->animation.elapsed += real_dt;
    while (enemy->animation.elapsed >= step_time) {
        enemy->animation.elapsed -= step_time;
        if (enemy->animation.frame + 1 >= frames) {
            if (looping) {
                enemy->animation.frame = 0;
            } else {
                enemy->animation.frame = frames - 1;
                enemy->animation.finished = true;
                break;
            }
        } else {
            enemy->animation.frame++;
        }
    }
}

static Rectangle player_hitbox_rect(const EikPlayer *player)
{
    const EIKHitbox hitbox = {
        .offset = { 18.0F, 26.0F }, .size = { 11.0F, 22.0F }, .radius = 0.0F,
    };
    const Vector2 position = eik_mirrored_pos(player->position, &hitbox, 48.0F,
        player->facing_right);

    return (Rectangle){ position.x + hitbox.offset.x, position.y + hitbox.offset.y,
        hitbox.size.x, hitbox.size.y };
}

static Rectangle enemy_rect(const EikEnemy *enemy)
{
    const EIKHitbox hitbox = enemy_hitbox(enemy);
    const Vector2 position = eik_mirrored_pos(enemy->position, &hitbox, enemy->size.x,
        enemy->facing_right);

    return (Rectangle){ position.x + hitbox.offset.x, position.y + hitbox.offset.y,
        hitbox.size.x, hitbox.size.y };
}

static bool enemy_touches_player(const EikEnemy *enemy, Rectangle player_box)
{
    const EIKHitbox hitbox = enemy_hitbox(enemy);

    if (hitbox.radius > 0.0F) {
        const Vector2 position = eik_mirrored_pos(enemy->position, &hitbox, enemy->size.x,
            enemy->facing_right);
        return CheckCollisionCircleRec((Vector2){ position.x + hitbox.radius,
            position.y + hitbox.radius }, hitbox.radius, player_box);
    }
    return CheckCollisionRecs(enemy_rect(enemy), player_box);
}

static float rect_distance(Rectangle left, Rectangle right)
{
    const float horizontal = fmaxf(fmaxf(right.x - (left.x + left.width),
        left.x - (right.x + right.width)), 0.0F);
    const float vertical = fmaxf(fmaxf(right.y - (left.y + left.height),
        left.y - (right.y + right.height)), 0.0F);

    return sqrtf(horizontal * horizontal + vertical * vertical);
}

static void spawn_enemy(ecs_world_t *world, EikEnemyWorld *enemies,
    const EikTmxObject *object)
{
    EikEnemy *enemy = NULL;
    const bool is_bat = strcmp(object->class_name, "Bat") == 0;
    const bool is_yellow = strcmp(object->class_name, "YellowMob") == 0;
    const float axis = is_bat && object->is_vertical ? object->y : object->x;

    if (enemies->count >= EIK_MAX_ENEMIES || (!is_bat && !is_yellow
            && strcmp(object->class_name, "RedMob") != 0)) {
        return;
    }
    enemy = &enemies->enemies[enemies->count++];
    *enemy = (EikEnemy){
        .entity = ecs_new(world),
        .kind = is_bat ? EIK_ENEMY_BAT : is_yellow ? EIK_ENEMY_YELLOW_MOB
            : EIK_ENEMY_RED_MOB,
        .position = { object->x, object->y },
        .spawn_position = { object->x, object->y },
        .size = { object->width, object->height },
        .range_neg = axis - object->off_neg * EIK_TILE_SIZE,
        .range_pos = axis + object->off_pos * EIK_TILE_SIZE,
        .patrol_direction = 1.0F,
        .target_direction = -1.0F,
        .facing_right = true,
        .vertical = object->is_vertical,
        .state = EIK_ACTOR_IDLE,
    };
    ecs_add_id(world, enemy->entity, EikLevelEntity);
    eik_anim_reset(&enemy->animation, EIK_ACTOR_IDLE);
}

void eik_enemy_register(ecs_world_t *world)
{
    ECS_COMPONENT_DEFINE(world, EikEnemyStomped);
}

void eik_enemy_world_load(ecs_world_t *world, EikEnemyWorld *enemies,
    const EikLevelState *level)
{
    size_t index = 0U;

    *enemies = (EikEnemyWorld){ 0 };
    for (index = 0U; index < level->map.object_count; ++index) {
        const EikTmxObject *object = &level->map.objects[index];

        if (strcmp(object->layer, "SpawnPoints") == 0) {
            spawn_enemy(world, enemies, object);
        }
    }
}

static void emit_enemy_stomped(ecs_world_t *world, const EikEnemy *enemy,
    uint32_t enemy_index, bool by_attack)
{
    const EikEnemyStomped event = { .enemy_index = enemy_index, .by_attack = by_attack };

    ecs_emit(world, &(ecs_event_desc_t){
        .event = ecs_id(EikEnemyStomped), .entity = enemy->entity, .const_param = &event,
    });
}

static void handle_enemy_stomped(ecs_world_t *world, EikEnemyWorld *enemies,
    uint32_t enemy_index, bool by_attack, EikPlayer *player, EikGameProgress *progress)
{
    EikEnemy *enemy = NULL;
    bool falling_onto = false;

    if (enemy_index >= enemies->count) {
        return;
    }
    enemy = &enemies->enemies[enemy_index];
    falling_onto = player->velocity.y > 0.0F
        && player->position.y + 48.0F > enemy->position.y;
    if (enemy->dying) {
        return;
    }
    emit_enemy_stomped(world, enemy, enemy_index, by_attack);
    if (by_attack || falling_onto) {
        if (!by_attack && enemy_is_mob(enemy)) {
            player->velocity.y = -EIK_BOUNCE_SPEED;
        }
        enemy->dying = true;
        enemy_anim_reset(enemy, EIK_ACTOR_HIT);
    } else {
        eik_player_kill(player, progress);
    }
}

static void move_bat(EikEnemy *enemy, float fixed_dt)
{
    const float axis = enemy->vertical ? enemy->position.y : enemy->position.x;

    if (axis >= enemy->range_pos) {
        enemy->patrol_direction = -1.0F;
    } else if (axis <= enemy->range_neg) {
        enemy->patrol_direction = 1.0F;
    }
    if (enemy->vertical) {
        enemy->position.y += enemy->patrol_direction * EIK_BAT_SPEED * fixed_dt;
    } else {
        enemy->position.x += enemy->patrol_direction * EIK_BAT_SPEED * fixed_dt;
    }
}

static bool vertically_overlaps(const EikEnemy *enemy, const EikPlayer *player)
{
    return player->position.y + 48.0F > enemy->position.y
        && player->position.y < enemy->position.y + enemy->size.y;
}

static bool red_attack_in_range(const EikEnemy *enemy, const EikPlayer *player)
{
    const float reference_x = enemy->position.x + (enemy->facing_right ? 0.0F : enemy->size.x);

    return player->position.x >= reference_x - EIK_RED_ATTACK_RANGE
        && player->position.x + 48.0F <= reference_x + EIK_RED_ATTACK_RANGE
        && vertically_overlaps(enemy, player);
}

static void move_mob(EikEnemy *enemy, const EikPlayer *player,
    const EIKCollisionWorld *collision, float fixed_dt)
{
    const bool in_range = player->position.x >= enemy->range_neg
        && player->position.x <= enemy->range_pos && vertically_overlaps(enemy, player);
    EIKActorBody body;

    if (!enemy->attacking) {
        enemy->velocity.x = 0.0F;
        if (enemy->kind == EIK_ENEMY_RED_MOB && enemy->returning_home) {
            const float distance = enemy->spawn_position.x - enemy->position.x;

            if (fabsf(distance) <= EIK_MOB_SPEED * fixed_dt) {
                enemy->position.x = enemy->spawn_position.x;
                enemy->returning_home = false;
            } else {
                enemy->target_direction = distance < 0.0F ? -1.0F : 1.0F;
                enemy->velocity.x = enemy->target_direction * EIK_MOB_SPEED;
            }
        } else if (enemy->kind == EIK_ENEMY_RED_MOB && red_attack_in_range(enemy, player)
                && player->routine == EIK_PLAYER_ACTIVE) {
            enemy->attacking = true;
            enemy_anim_reset(enemy, EIK_ACTOR_ATTACKING);
        } else if (in_range) {
            enemy->target_direction = player->position.x < enemy->position.x ? -1.0F : 1.0F;
            enemy->velocity.x = enemy->target_direction * EIK_MOB_SPEED;
        }
        enemy->move_direction += (enemy->target_direction - enemy->move_direction) * 0.1F;
        if (enemy->move_direction < 0.0F) {
            enemy->facing_right = false;
        } else if (enemy->move_direction > 0.0F) {
            enemy->facing_right = true;
        }
        if (!enemy->attacking) {
            enemy->state = enemy->velocity.x == 0.0F ? EIK_ACTOR_IDLE : EIK_ACTOR_RUNNING;
        }
        enemy->position.x += enemy->velocity.x * fixed_dt;
    }
    body = (EIKActorBody){
        .position = &enemy->position, .velocity = &enemy->velocity,
        .grounded = &enemy->grounded, .contact = &enemy->contact,
        .hitbox = mob_hitbox, .box_width = enemy->size.x, .facing_right = enemy->facing_right,
    };
    eik_resolve_horizontal(&body, collision);
    eik_apply_gravity(&enemy->velocity, &enemy->position, &mob_gravity, fixed_dt);
    (void)eik_resolve_vertical(&body, collision);
}

static void resolve_player_contacts(ecs_world_t *world, EikEnemyWorld *enemies,
    EikPlayer *player, EikGameProgress *progress)
{
    const Rectangle player_box = player_hitbox_rect(player);
    const Rectangle sword_box = eik_player_attack_rect(player);
    size_t index = 0U;

    if (player->routine != EIK_PLAYER_ACTIVE) {
        return;
    }
    if (player->attacking) {
        for (index = 0U; index < enemies->count; ++index) {
            EikEnemy *enemy = &enemies->enemies[index];

            if (!enemy->dying && CheckCollisionRecs(sword_box, enemy_rect(enemy))) {
                handle_enemy_stomped(world, enemies, (uint32_t)index, true, player, progress);
                break;
            }
        }
    }
    for (index = 0U; index < enemies->count; ++index) {
        EikEnemy *enemy = &enemies->enemies[index];

        if (enemy->dying || !enemy_touches_player(enemy, player_box)) {
            continue;
        }
        if (enemy->kind == EIK_ENEMY_BAT) {
            if (!player->attacking) {
                eik_player_kill(player, progress);
            }
            return;
        }
        if (!player->attacking) {
            handle_enemy_stomped(world, enemies, (uint32_t)index, false, player, progress);
            return;
        }
    }
}

void eik_enemy_fixed_step(ecs_world_t *world, EikEnemyWorld *enemies,
    EikPlayer *player, EikGameProgress *progress, const EIKCollisionWorld *collision,
    float fixed_dt)
{
    size_t index = 0U;

    for (index = 0U; index < enemies->count; ++index) {
        EikEnemy *enemy = &enemies->enemies[index];

        if (enemy->dying) {
            continue;
        }
        if (enemy->kind == EIK_ENEMY_BAT) {
            move_bat(enemy, fixed_dt);
        } else {
            const bool swinging = enemy->kind == EIK_ENEMY_RED_MOB && enemy->attacking
                && red_attack_in_range(enemy, player) && player->routine == EIK_PLAYER_ACTIVE;

            move_mob(enemy, player, collision, fixed_dt);
            if (swinging) {
                eik_player_kill(player, progress);
            }
        }
    }
    resolve_player_contacts(world, enemies, player, progress);
}

void eik_enemy_update(ecs_world_t *world, EikEnemyWorld *enemies, float real_dt)
{
    size_t index = 0U;

    while (index < enemies->count) {
        EikEnemy *enemy = &enemies->enemies[index];

        enemy_anim_advance(enemy, real_dt);
        if (enemy->dying && enemy->animation.finished) {
            ecs_delete(world, enemy->entity);
            enemies->enemies[index] = enemies->enemies[enemies->count - 1U];
            enemies->count--;
            continue;
        }
        if (enemy->kind == EIK_ENEMY_RED_MOB && enemy->attacking
                && enemy->animation.finished) {
            enemy->attacking = false;
            enemy->returning_home = true;
            enemy_anim_reset(enemy, EIK_ACTOR_IDLE);
        }
        index++;
    }
}

float eik_enemy_time_scale(const EikEnemyWorld *enemies, const EikPlayer *player)
{
    const Rectangle player_box = player_hitbox_rect(player);
    size_t index = 0U;

    for (index = 0U; index < enemies->count; ++index) {
        const EikEnemy *enemy = &enemies->enemies[index];

        if (enemy->kind == EIK_ENEMY_BAT && !enemy->dying
                && rect_distance(player_box, enemy_rect(enemy)) < EIK_BAT_SLOWDOWN_RANGE) {
            return 0.5F;
        }
    }
    return 1.0F;
}

Rectangle eik_enemy_frame_rect(const EikEnemy *enemy)
{
    int origin_y = 0;
    int frames = 0;
    float ignored_step = 0.0F;
    bool ignored_loop = false;
    const float width = enemy->kind == EIK_ENEMY_BAT ? 16.0F : 48.0F;
    const float height = enemy->kind == EIK_ENEMY_BAT ? 16.0F : 32.0F;

    enemy_clip(enemy, &origin_y, &frames, &ignored_step, &ignored_loop);
    (void)frames;
    return (Rectangle){ (float)(enemy->animation.frame * (int)width), (float)origin_y,
        width, height };
}
