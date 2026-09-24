#ifndef EIK_MOD_PLAYER_H
#define EIK_MOD_PLAYER_H

#include <stdbool.h>

#include "anim.h"
#include "collision.h"
#include "input.h"
#include "mod_level.h"

typedef enum EikPlayerRoutine {
    EIK_PLAYER_ACTIVE,
    EIK_PLAYER_DYING,
    EIK_PLAYER_REAPPEARING,
    EIK_PLAYER_LEAVING_LEVEL,
} EikPlayerRoutine;

typedef struct EikGameTime {
    float real_dt;
    float virtual_dt;
    float time_scale;
    float fixed_dt;
} EikGameTime;

typedef struct EikGameProgress {
    int lives;
    unsigned int coins_collected;
    size_t current_level;
    bool game_over;
} EikGameProgress;

typedef struct EikPlayer {
    Vector2 position;
    Vector2 start_position;
    Vector2 velocity;
    EIKContactState contact;
    EikAnimPlayer animation;
    EikActorState state;
    EikPlayerRoutine routine;
    float routine_elapsed;
    float wall_jump_timer;
    bool grounded;
    bool facing_right;
    bool attacking;
    bool invulnerable;
} EikPlayer;

void eik_game_time_begin_frame(EikGameTime *time, float real_dt);
void eik_player_spawn(EikPlayer *player, Vector2 start_position);
void eik_player_kill(EikPlayer *player, EikGameProgress *progress);
void eik_player_update(EikPlayer *player, EikGameProgress *progress, float real_dt);
void eik_player_fixed_step(EikPlayer *player, EikGameProgress *progress,
    const EikInputFrame *input, const EIKCollisionWorld *world, float fixed_dt);
Rectangle eik_player_attack_rect(const EikPlayer *player);

#endif
