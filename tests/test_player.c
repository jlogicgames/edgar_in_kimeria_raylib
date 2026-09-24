#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "mod_player.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static bool nearly_equal(float left, float right)
{
    return fabsf(left - right) < 0.01F;
}

static bool attack_wraps_after_the_fourth_cell(void)
{
    EikAnimPlayer animation;
    Rectangle frame;

    eik_anim_reset(&animation, EIK_ACTOR_ATTACKING);
    eik_anim_advance(&animation, EIK_ACTOR_ATTACKING, 0.41F);
    frame = eik_player_frame_rect(&animation);
    CHECK(animation.frame == 4);
    CHECK(nearly_equal(frame.x, 0.0F));
    CHECK(nearly_equal(frame.y, 96.0F));
    return true;
}

static bool third_death_ends_the_run(void)
{
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    int death = 0;

    eik_player_spawn(&player, (Vector2){ 0.0F, 0.0F });
    for (death = 0; death < 3; ++death) {
        eik_player_kill(&player, &progress);
        eik_player_update(&player, &progress, 1.0F);
        eik_player_update(&player, &progress, 1.0F);
    }
    CHECK(progress.lives == 0);
    CHECK(progress.game_over);
    return true;
}

static bool gravity_uses_seconds_not_steps(void)
{
    EikPlayer player;
    EikGameProgress progress = { .lives = 3 };
    const EikInputFrame input = { 0 };
    const EIKCollisionWorld world = { 0 };

    eik_player_spawn(&player, (Vector2){ 0.0F, 0.0F });
    eik_player_fixed_step(&player, &progress, &input, &world, 1.0F / 60.0F);
    CHECK(nearly_equal(player.velocity.y, 9.8F));
    return true;
}

int main(void)
{
    return attack_wraps_after_the_fourth_cell()
            && third_death_ends_the_run()
            && gravity_uses_seconds_not_steps() ? 0 : 1;
}
