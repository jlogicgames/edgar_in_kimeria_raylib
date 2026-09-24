#include "mod_player.h"

#include <stdlib.h>

#define EIK_FIXED_DT (1.0F / 60.0F)
#define EIK_GRAVITY 588.0F
#define EIK_TERMINAL_VELOCITY 300.0F
#define EIK_JUMP_FORCE 260.0F
#define EIK_MOVE_SPEED 100.0F
#define EIK_COYOTE_THRESHOLD 147.0F
#define EIK_DEATH_PLANE_Y 380.0F
#define EIK_WALL_JUMP_LOCKOUT 0.1F

static const EIKHitbox player_hitbox = {
    .offset = { 18.0F, 26.0F }, .size = { 11.0F, 22.0F }, .radius = 0.0F,
};
static const EIKGravity player_gravity = {
    .acceleration = EIK_GRAVITY, .terminal_velocity = EIK_TERMINAL_VELOCITY,
    .jump_force = EIK_JUMP_FORCE,
};

void eik_game_time_begin_frame(EikGameTime *time, float real_dt)
{
    time->real_dt = real_dt > 0.25F ? 0.25F : real_dt;
    time->fixed_dt = EIK_FIXED_DT;
    time->virtual_dt = time->real_dt * time->time_scale;
}

void eik_player_spawn(EikPlayer *player, Vector2 start_position)
{
    *player = (EikPlayer){
        .position = start_position,
        .start_position = start_position,
        .facing_right = true,
        .state = EIK_ACTOR_IDLE,
        .routine = EIK_PLAYER_ACTIVE,
    };
    eik_anim_reset(&player->animation, player->state);
}

void eik_player_kill(EikPlayer *player, EikGameProgress *progress)
{
    if (player->invulnerable || player->routine != EIK_PLAYER_ACTIVE) {
        return;
    }
    player->routine = EIK_PLAYER_DYING;
    player->routine_elapsed = 0.0F;
    player->attacking = false;
    player->state = EIK_ACTOR_HIT;
    eik_anim_reset(&player->animation, player->state);
    if (progress->lives > 0) {
        progress->lives--;
    }
}

void eik_player_update(EikPlayer *player, EikGameProgress *progress, float real_dt)
{
    eik_anim_advance(&player->animation, player->state, real_dt);
    if (player->routine == EIK_PLAYER_DYING && player->animation.finished) {
        if (progress->lives == 0) {
            progress->game_over = true;
            return;
        }
        player->position = player->start_position;
        player->velocity = (Vector2){ 0.0F, 0.0F };
        player->contact = (EIKContactState){ 0 };
        player->facing_right = true;
        player->routine = EIK_PLAYER_REAPPEARING;
        player->state = EIK_ACTOR_APPEARING;
        eik_anim_reset(&player->animation, player->state);
    } else if (player->routine == EIK_PLAYER_REAPPEARING && player->animation.finished) {
        player->routine = EIK_PLAYER_ACTIVE;
        player->state = EIK_ACTOR_IDLE;
        eik_anim_reset(&player->animation, player->state);
    } else if (player->routine == EIK_PLAYER_LEAVING_LEVEL) {
        player->routine_elapsed += real_dt;
    }
    if (player->attacking && player->animation.finished) {
        player->attacking = false;
    }
}

static void update_state(EikPlayer *player, const EikInputFrame *input)
{
    EikActorState state = EIK_ACTOR_IDLE;

    if (input->horizontal < 0.0F) {
        player->facing_right = false;
    } else if (input->horizontal > 0.0F) {
        player->facing_right = true;
    }
    if (input->horizontal != 0.0F) {
        state = EIK_ACTOR_RUNNING;
    }
    if (player->velocity.y > 0.0F) {
        state = EIK_ACTOR_FALLING;
    } else if (player->velocity.y < 0.0F) {
        state = EIK_ACTOR_JUMPING;
    }
    if (player->contact.clambering) {
        state = EIK_ACTOR_CLIMBING;
    }
    if (player->attacking) {
        state = EIK_ACTOR_ATTACKING;
    }
    player->state = state;
}

void eik_player_fixed_step(EikPlayer *player, EikGameProgress *progress,
    const EikInputFrame *input, const EIKCollisionWorld *world, float fixed_dt)
{
    EIKActorBody body;

    if (player->wall_jump_timer > 0.0F) {
        player->wall_jump_timer -= fixed_dt;
    }
    if (player->routine != EIK_PLAYER_ACTIVE || progress->game_over) {
        return;
    }
    if (input->attack_pressed && !player->attacking && player->grounded
            && !input->jump_held && !player->contact.clambering) {
        player->attacking = true;
        player->state = EIK_ACTOR_ATTACKING;
        eik_anim_reset(&player->animation, player->state);
    }
    update_state(player, input);
    if (input->jump_held && (player->grounded || player->contact.clambering)) {
        if (player->contact.clambering) {
            const bool push_left = input->horizontal != 0.0F
                ? input->horizontal > 0.0F : player->facing_right;
            player->velocity.y = -EIK_JUMP_FORCE * 0.7F;
            player->velocity.x = push_left ? -EIK_JUMP_FORCE * 0.5F : EIK_JUMP_FORCE * 0.5F;
            player->contact.clambering = false;
            player->wall_jump_timer = EIK_WALL_JUMP_LOCKOUT;
        } else {
            player->velocity.y = -EIK_JUMP_FORCE
                * (player->contact.in_quicksand ? 0.1F : 1.0F);
        }
        player->position.y += player->velocity.y * fixed_dt;
        player->grounded = false;
    }
    if (player->velocity.y > EIK_COYOTE_THRESHOLD) {
        player->grounded = false;
    }
    if (player->wall_jump_timer <= 0.0F) {
        player->velocity.x = player->attacking ? 0.0F : input->horizontal * EIK_MOVE_SPEED;
        if (player->contact.in_quicksand) {
            player->velocity.x *= 0.1F;
        }
    }
    player->position.x += player->velocity.x * fixed_dt;
    if (player->contact.clambering) {
        player->velocity.y *= 0.1F;
    }
    if (player->position.y > EIK_DEATH_PLANE_Y) {
        eik_player_kill(player, progress);
        return;
    }
    body = (EIKActorBody){
        .position = &player->position, .velocity = &player->velocity,
        .grounded = &player->grounded, .contact = &player->contact,
        .hitbox = player_hitbox, .box_width = 48.0F, .facing_right = player->facing_right,
    };
    eik_resolve_horizontal(&body, world);
    eik_apply_gravity(&player->velocity, &player->position, &player_gravity, fixed_dt);
    (void)eik_resolve_vertical(&body, world);
}

Rectangle eik_player_attack_rect(const EikPlayer *player)
{
    const float offset_x = player->facing_right ? 16.0F - player_hitbox.offset.x
        + player_hitbox.size.x : player_hitbox.offset.x - 20.0F + player_hitbox.size.x;

    return (Rectangle){ player->position.x + offset_x,
        player->position.y + player_hitbox.offset.y - 14.0F, 37.0F,
        player_hitbox.size.y + 14.0F };
}
