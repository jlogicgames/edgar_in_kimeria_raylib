#include "anim.h"

typedef struct EikClip {
    int origin_row;
    int frames;
    int per_row;
    float step_time;
    bool looping;
} EikClip;

static const EikClip player_clips[] = {
    [EIK_ACTOR_IDLE] = { 9, 4, 4, 0.1F, true },
    [EIK_ACTOR_RUNNING] = { 0, 4, 4, 0.1F, true },
    [EIK_ACTOR_JUMPING] = { 8, 1, 1, 0.1F, false },
    [EIK_ACTOR_FALLING] = { 4, 1, 1, 0.1F, false },
    [EIK_ACTOR_HIT] = { 4, 2, 2, 0.1F, false },
    [EIK_ACTOR_ATTACKING] = { 1, 7, 4, 0.1F, false },
    [EIK_ACTOR_APPEARING] = { 3, 4, 4, 0.1F, false },
    [EIK_ACTOR_DISAPPEARING] = { 6, 4, 4, 0.1F, true },
    [EIK_ACTOR_CLIMBING] = { 3, 1, 1, 0.1F, false },
};

void eik_anim_reset(EikAnimPlayer *player, EikActorState state)
{
    *player = (EikAnimPlayer){ .current = state };
}

void eik_anim_advance(EikAnimPlayer *player, EikActorState state, float real_dt)
{
    const EikClip *clip = &player_clips[state];

    if (player->current != state) {
        eik_anim_reset(player, state);
    }
    if (clip->frames <= 1 || (player->finished && !clip->looping)) {
        if (clip->frames <= 1) {
            player->finished = true;
        }
        return;
    }
    player->elapsed += real_dt;
    while (player->elapsed >= clip->step_time) {
        player->elapsed -= clip->step_time;
        if (player->frame + 1 >= clip->frames) {
            if (clip->looping) {
                player->frame = 0;
            } else {
                player->frame = clip->frames - 1;
                player->finished = true;
                break;
            }
        } else {
            player->frame++;
        }
    }
}

Rectangle eik_player_frame_rect(const EikAnimPlayer *player)
{
    const EikClip *clip = &player_clips[player->current];
    const int column = player->frame % clip->per_row;
    const int row = clip->origin_row + player->frame / clip->per_row;

    return (Rectangle){ (float)(column * 48), (float)(row * 48), 48.0F, 48.0F };
}
