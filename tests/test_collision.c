#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "collision.h"

static const EIKHitbox player_hitbox = {
    .offset = { 18.0F, 26.0F },
    .size = { 11.0F, 22.0F },
    .radius = 0.0F,
};
static const float player_box_width = 48.0F;

typedef struct TestActor {
    Vector2 position;
    Vector2 velocity;
    bool grounded;
    EIKContactState contact;
    bool facing_right;
} TestActor;

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static bool nearly_equal(float left, float right)
{
    return fabsf(left - right) < 0.001F;
}

static EIKBlockSnapshot block(EIKBlockKind kind, Vector2 position, Vector2 size)
{
    return (EIKBlockSnapshot){
        .position = position,
        .size = size,
        .kind = kind,
    };
}

static TestActor actor(Vector2 position, Vector2 velocity)
{
    return (TestActor){
        .position = position,
        .velocity = velocity,
        .facing_right = true,
    };
}

static EIKActorBody actor_body(TestActor *actor_state)
{
    return (EIKActorBody){
        .position = &actor_state->position,
        .velocity = &actor_state->velocity,
        .grounded = &actor_state->grounded,
        .contact = &actor_state->contact,
        .hitbox = player_hitbox,
        .box_width = player_box_width,
        .facing_right = actor_state->facing_right,
    };
}

static bool solid_overlap_uses_the_hitbox_not_the_sprite(void)
{
    const EIKBlockSnapshot wall = block(EIK_BLOCK_SOLID,
        (Vector2){ 100.0F, 20.0F }, (Vector2){ 16.0F, 40.0F });

    CHECK(!eik_overlaps((Vector2){ 71.0F, 0.0F }, &player_hitbox,
        player_box_width, true, &wall));
    CHECK(eik_overlaps((Vector2){ 72.0F, 0.0F }, &player_hitbox,
        player_box_width, true, &wall));
    return true;
}

static bool facing_left_mirrors_the_hitbox_in_place(void)
{
    const Vector2 mirrored = eik_mirrored_pos((Vector2){ 81.0F, 0.0F },
        &player_hitbox, player_box_width, false);
    const EIKBlockSnapshot low_wall = block(EIK_BLOCK_SOLID,
        (Vector2){ 99.0F, 20.0F }, (Vector2){ 1.0F, 40.0F });
    const EIKBlockSnapshot high_wall = block(EIK_BLOCK_SOLID,
        (Vector2){ 110.0F, 20.0F }, (Vector2){ 1.0F, 40.0F });

    CHECK(eik_overlaps((Vector2){ 81.0F, 0.0F }, &player_hitbox,
        player_box_width, true, &low_wall));
    CHECK(!eik_overlaps((Vector2){ 81.0F, 0.0F }, &player_hitbox,
        player_box_width, true, &high_wall));
    CHECK(!eik_overlaps((Vector2){ 81.0F, 0.0F }, &player_hitbox,
        player_box_width, false, &low_wall));
    CHECK(eik_overlaps((Vector2){ 81.0F, 0.0F }, &player_hitbox,
        player_box_width, false, &high_wall));
    CHECK(nearly_equal(mirrored.x + player_hitbox.offset.x, 100.0F));
    return true;
}

static bool one_way_platform_only_catches_from_above(void)
{
    const EIKBlockSnapshot platform = block(EIK_BLOCK_PLATFORM,
        (Vector2){ 0.0F, 100.0F }, (Vector2){ 64.0F, 8.0F });

    CHECK(!eik_overlaps((Vector2){ 10.0F, 40.0F }, &player_hitbox,
        player_box_width, true, &platform));
    CHECK(eik_overlaps((Vector2){ 10.0F, 56.0F }, &player_hitbox,
        player_box_width, true, &platform));
    return true;
}

static bool landing_snaps_to_the_top_of_a_solid_block(void)
{
    const EIKBlockSnapshot blocks[] = { block(EIK_BLOCK_SOLID,
        (Vector2){ 0.0F, 200.0F }, (Vector2){ 64.0F, 16.0F }) };
    const EIKCollisionWorld world = { .blocks = blocks, .block_count = 1U };
    TestActor actor_state = actor((Vector2){ 10.0F, 160.0F }, (Vector2){ 0.0F, 120.0F });
    EIKActorBody body = actor_body(&actor_state);

    (void)eik_resolve_vertical(&body, &world);
    CHECK(actor_state.grounded);
    CHECK(nearly_equal(actor_state.velocity.y, 0.0F));
    CHECK(nearly_equal(actor_state.position.y, 152.0F));
    return true;
}

static bool a_wall_hit_in_mid_air_starts_a_clamber(void)
{
    const EIKBlockSnapshot blocks[] = { block(EIK_BLOCK_WALL,
        (Vector2){ 100.0F, 0.0F }, (Vector2){ 16.0F, 64.0F }) };
    const EIKCollisionWorld world = { .blocks = blocks, .block_count = 1U };
    TestActor actor_state = actor((Vector2){ 80.0F, 0.0F }, (Vector2){ 60.0F, 0.0F });
    EIKActorBody body = actor_body(&actor_state);

    eik_resolve_horizontal(&body, &world);
    CHECK(nearly_equal(actor_state.velocity.x, 0.0F));
    CHECK(nearly_equal(actor_state.position.x, 71.0F));
    CHECK(actor_state.contact.clambering);
    return true;
}

static bool hitting_a_wall_while_moving_left_snaps_to_its_right_edge(void)
{
    const EIKBlockSnapshot blocks[] = { block(EIK_BLOCK_WALL,
        (Vector2){ 100.0F, 0.0F }, (Vector2){ 16.0F, 64.0F }) };
    const EIKCollisionWorld world = { .blocks = blocks, .block_count = 1U };
    TestActor actor_state = actor((Vector2){ 90.0F, 0.0F }, (Vector2){ -60.0F, 0.0F });
    EIKActorBody body = actor_body(&actor_state);

    actor_state.facing_right = false;
    body.facing_right = false;
    eik_resolve_horizontal(&body, &world);
    CHECK(nearly_equal(actor_state.velocity.x, 0.0F));
    CHECK(nearly_equal(actor_state.position.x, 97.0F));
    return true;
}

static bool the_same_wall_hit_while_grounded_does_not_clamber(void)
{
    const EIKBlockSnapshot blocks[] = { block(EIK_BLOCK_WALL,
        (Vector2){ 100.0F, 0.0F }, (Vector2){ 16.0F, 64.0F }) };
    const EIKCollisionWorld world = { .blocks = blocks, .block_count = 1U };
    TestActor actor_state = actor((Vector2){ 80.0F, 0.0F }, (Vector2){ 60.0F, 0.0F });
    EIKActorBody body = actor_body(&actor_state);

    actor_state.grounded = true;
    eik_resolve_horizontal(&body, &world);
    CHECK(nearly_equal(actor_state.position.x, 71.0F));
    CHECK(!actor_state.contact.clambering);
    return true;
}

static bool quicksand_is_passable_but_flags_contact(void)
{
    const EIKBlockSnapshot blocks[] = { block(EIK_BLOCK_QUICKSAND,
        (Vector2){ 100.0F, 0.0F }, (Vector2){ 64.0F, 32.0F }) };
    const EIKCollisionWorld world = { .blocks = blocks, .block_count = 1U };
    TestActor actor_state = actor((Vector2){ 100.0F, 0.0F }, (Vector2){ 60.0F, 0.0F });
    EIKActorBody body = actor_body(&actor_state);

    eik_resolve_horizontal(&body, &world);
    CHECK(actor_state.contact.in_quicksand);
    CHECK(nearly_equal(actor_state.velocity.x, 60.0F));
    CHECK(nearly_equal(actor_state.position.x, 100.0F));
    return true;
}

static bool standing_on_an_escalator_records_it_for_the_carry(void)
{
    const EIKSurfaceSnapshot escalators[] = {{
        .id = 7U,
        .position = { 0.0F, 200.0F },
        .size = { 32.0F, 16.0F },
        .velocity = { 50.0F, 0.0F },
    }};
    const EIKCollisionWorld world = {
        .escalators = escalators,
        .escalator_count = 1U,
    };
    TestActor actor_state = actor((Vector2){ 0.0F, 160.0F }, (Vector2){ 0.0F, 120.0F });
    EIKActorBody body = actor_body(&actor_state);

    (void)eik_resolve_vertical(&body, &world);
    CHECK(actor_state.grounded);
    CHECK(actor_state.contact.on_escalator);
    CHECK(actor_state.contact.escalator_id == 7U);
    return true;
}

static bool landing_on_an_untriggered_platform_asks_it_to_fall(void)
{
    const EIKSurfaceSnapshot platforms[] = {{
        .id = 9U,
        .position = { 0.0F, 200.0F },
        .size = { 32.0F, 16.0F },
    }};
    const EIKCollisionWorld world = {
        .falling_platforms = platforms,
        .falling_platform_count = 1U,
    };
    TestActor actor_state = actor((Vector2){ 0.0F, 160.0F }, (Vector2){ 0.0F, 120.0F });
    EIKActorBody body = actor_body(&actor_state);
    const EIKVerticalOutcome outcome = eik_resolve_vertical(&body, &world);

    CHECK(outcome.trigger_fall);
    CHECK(outcome.falling_platform_id == 9U);
    return true;
}

static bool gravity_is_per_second_and_clamps_at_terminal_velocity(void)
{
    const EIKGravity gravity = {
        .acceleration = 588.0F,
        .terminal_velocity = 300.0F,
        .jump_force = 260.0F,
    };
    Vector2 velocity = { 0.0F, 0.0F };
    Vector2 position = { 0.0F, 0.0F };
    size_t index = 0U;

    eik_apply_gravity(&velocity, &position, &gravity, 1.0F / 60.0F);
    CHECK(nearly_equal(velocity.y, 9.8F));
    eik_apply_gravity(&velocity, &position, &gravity, 1.0F / 120.0F);
    CHECK(nearly_equal(velocity.y, 14.7F));
    for (index = 0U; index < 200U; ++index) {
        eik_apply_gravity(&velocity, &position, &gravity, 1.0F / 60.0F);
    }
    CHECK(nearly_equal(velocity.y, 300.0F));
    return true;
}

static bool falling_platform_is_solid_from_below_and_the_sides(void)
{
    const EIKSurfaceSnapshot platforms[] = {{
        .id = 9U,
        .position = { 100.0F, 100.0F },
        .size = { 32.0F, 16.0F },
    }};
    const EIKCollisionWorld world = {
        .falling_platforms = platforms,
        .falling_platform_count = 1U,
    };
    TestActor riser = actor((Vector2){ 100.0F, 80.0F }, (Vector2){ 0.0F, -120.0F });
    TestActor runner = actor((Vector2){ 72.0F, 80.0F }, (Vector2){ 60.0F, 0.0F });
    EIKActorBody rising_body = actor_body(&riser);
    EIKActorBody running_body = actor_body(&runner);

    (void)eik_resolve_vertical(&rising_body, &world);
    CHECK(nearly_equal(riser.velocity.y, 0.0F));
    CHECK(nearly_equal(riser.position.y, 90.0F));
    eik_resolve_horizontal(&running_body, &world);
    CHECK(nearly_equal(runner.velocity.x, 0.0F));
    CHECK(nearly_equal(runner.position.x, 71.0F));
    return true;
}

static bool circle_hitboxes_use_a_circle_not_their_bounding_box(void)
{
    const EIKHitbox circle = {
        .size = { 16.0F, 16.0F },
        .radius = 8.0F,
    };
    const EIKBlockSnapshot corner = block(EIK_BLOCK_SOLID,
        (Vector2){ 14.0F, 14.0F }, (Vector2){ 4.0F, 4.0F });

    CHECK(!eik_overlaps((Vector2){ 0.0F, 0.0F }, &circle, 16.0F, true, &corner));
    return true;
}

typedef bool (*CollisionTest)(void);

int main(void)
{
    static const struct {
        const char *name;
        CollisionTest run;
    } tests[] = {
        { "solid_overlap_uses_the_hitbox_not_the_sprite", solid_overlap_uses_the_hitbox_not_the_sprite },
        { "facing_left_mirrors_the_hitbox_in_place", facing_left_mirrors_the_hitbox_in_place },
        { "one_way_platform_only_catches_from_above", one_way_platform_only_catches_from_above },
        { "landing_snaps_to_the_top_of_a_solid_block", landing_snaps_to_the_top_of_a_solid_block },
        { "a_wall_hit_in_mid_air_starts_a_clamber", a_wall_hit_in_mid_air_starts_a_clamber },
        { "hitting_a_wall_while_moving_left_snaps_to_its_right_edge", hitting_a_wall_while_moving_left_snaps_to_its_right_edge },
        { "the_same_wall_hit_while_grounded_does_not_clamber", the_same_wall_hit_while_grounded_does_not_clamber },
        { "quicksand_is_passable_but_flags_contact", quicksand_is_passable_but_flags_contact },
        { "standing_on_an_escalator_records_it_for_the_carry", standing_on_an_escalator_records_it_for_the_carry },
        { "landing_on_an_untriggered_platform_asks_it_to_fall", landing_on_an_untriggered_platform_asks_it_to_fall },
        { "gravity_is_per_second_and_clamps_at_terminal_velocity", gravity_is_per_second_and_clamps_at_terminal_velocity },
        { "falling_platform_is_solid_from_below_and_the_sides", falling_platform_is_solid_from_below_and_the_sides },
        { "circle_hitboxes_use_a_circle_not_their_bounding_box", circle_hitboxes_use_a_circle_not_their_bounding_box },
    };
    size_t index = 0U;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].run()) {
            (void)fprintf(stderr, "failed: %s\n", tests[index].name);
            return 1;
        }
    }
    return 0;
}
