#ifndef EIK_ANIM_H
#define EIK_ANIM_H

#include <stdbool.h>

#include "raylib.h"

typedef enum EikActorState {
    EIK_ACTOR_IDLE,
    EIK_ACTOR_RUNNING,
    EIK_ACTOR_JUMPING,
    EIK_ACTOR_FALLING,
    EIK_ACTOR_HIT,
    EIK_ACTOR_ATTACKING,
    EIK_ACTOR_APPEARING,
    EIK_ACTOR_DISAPPEARING,
    EIK_ACTOR_CLIMBING,
} EikActorState;

typedef struct EikAnimPlayer {
    EikActorState current;
    int frame;
    float elapsed;
    bool finished;
} EikAnimPlayer;

void eik_anim_reset(EikAnimPlayer *player, EikActorState state);
void eik_anim_advance(EikAnimPlayer *player, EikActorState state, float real_dt);
Rectangle eik_player_frame_rect(const EikAnimPlayer *player);

#endif
