#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flecs.h"
#include "input.h"
#include "mod_level.h"
#include "mod_enemy.h"
#include "mod_items.h"
#include "mod_player.h"
#include "raylib.h"
#include "render.h"

#if defined(EIK_IOS)
#include <SDL2/SDL.h>

void eik_ios_start_frame_loop(void *sdl_window, void (*frame)(void));
#endif

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

typedef struct App {
    ecs_world_t *world;
    ecs_entity_t pre_phase;
    ecs_entity_t fixed_phase;
    ecs_entity_t update_phase;
    ecs_entity_t pre_pipeline;
    ecs_entity_t fixed_pipeline;
    ecs_entity_t update_pipeline;
    Texture2D sprite;
    Shader tint_shader;
    Sound jump_sound;
    EikInputFrame input_frame;
    EikGameTime game_time;
    EikGameProgress progress;
    EikPlayer player;
    EikEnemyWorld enemies;
    EikItemWorld items;
    const char *level_paths[2];
    bool shader_loaded;
    bool sound_loaded;
    EikLevelState level;
    EikRenderer renderer;
    bool show_collision;
    bool running;
} App;

static App app;

#if defined(EIK_IOS)
static const char tint_fragment_shader[] =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 color = texture(texture0, fragTexCoord) * fragColor;\n"
    "    finalColor = vec4(color.rgb * vec3(0.75, 1.0, 0.85), color.a);\n"
    "}\n";
#elif defined(PLATFORM_WEB)
static const char tint_fragment_shader[] =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "void main() {\n"
    "    vec4 color = texture2D(texture0, fragTexCoord) * fragColor;\n"
    "    gl_FragColor = vec4(color.rgb * vec3(0.75, 1.0, 0.85), color.a);\n"
    "}\n";
#else
static const char tint_fragment_shader[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 color = texture(texture0, fragTexCoord) * fragColor;\n"
    "    finalColor = vec4(color.rgb * vec3(0.75, 1.0, 0.85), color.a);\n"
    "}\n";
#endif

static void fail_asset_load(const char *path)
{
    (void)fprintf(stderr, "Edgard in Kimeria: cannot load asset: %s\n", path);
    exit(EXIT_FAILURE);
}

static char *asset_path(const char *relative_path)
{
    const char *configured_root = getenv("EIK_ASSET_ROOT");
    const char *root = configured_root;
    char *path = NULL;
    size_t path_length = 0U;

    if (root == NULL || root[0] == '\0') {
        root = GetApplicationDirectory();
        if (root == NULL || root[0] == '\0') {
            root = ".";
        }
        path_length = strlen(root) + strlen("assets/") + strlen(relative_path) + 1U;
        path = malloc(path_length);
        if (path != NULL) {
            (void)snprintf(path, path_length, "%sassets/%s", root, relative_path);
        }
    } else {
        path_length = strlen(root) + 1U + strlen(relative_path) + 1U;
        path = malloc(path_length);
        if (path != NULL) {
            (void)snprintf(path, path_length, "%s/%s", root, relative_path);
        }
    }

    if (path == NULL) {
        (void)fprintf(stderr, "Edgard in Kimeria: out of memory building an asset path\n");
        exit(EXIT_FAILURE);
    }
    return path;
}

static ecs_entity_t make_phase(ecs_world_t *world, const char *name)
{
    return ecs_entity_init(world, &(ecs_entity_desc_t){
        .name = name,
        .add = ecs_ids(EcsPhase),
    });
}

static ecs_entity_t make_pipeline(
    ecs_world_t *world,
    const char *name,
    ecs_entity_t phase)
{
    return ecs_pipeline_init(world, &(ecs_pipeline_desc_t){
        .entity = ecs_entity(world, { .name = name }),
        .query = {
            .terms = {
                { .id = EcsSystem },
                { .id = EcsPhase, .src.id = EcsCascade, .trav = EcsDependsOn },
                { .id = ecs_dependson(phase), .trav = EcsDependsOn },
                { .id = EcsDisabled, .src.id = EcsUp, .trav = EcsDependsOn, .oper = EcsNot },
                { .id = EcsDisabled, .src.id = EcsUp, .trav = EcsChildOf, .oper = EcsNot },
            },
        },
    });
}

static void initialize_world(void)
{
    app.world = ecs_init();
    app.pre_phase = make_phase(app.world, "PrePhase");
    app.fixed_phase = make_phase(app.world, "FixedPhase");
    app.update_phase = make_phase(app.world, "UpdatePhase");
    app.pre_pipeline = make_pipeline(app.world, "PrePipeline", app.pre_phase);
    app.fixed_pipeline = make_pipeline(app.world, "FixedPipeline", app.fixed_phase);
    app.update_pipeline = make_pipeline(app.world, "UpdatePipeline", app.update_phase);
    eik_level_register(app.world);
    eik_enemy_register(app.world);
    eik_items_register(app.world, &app.items);
}

static EIKBlockKind block_kind_from_object(const EikTmxObject *object)
{
    if (strcmp(object->class_name, "Platform") == 0) {
        return EIK_BLOCK_PLATFORM;
    }
    if (strcmp(object->class_name, "QuickSand") == 0) {
        return EIK_BLOCK_QUICKSAND;
    }
    if (strcmp(object->class_name, "Wall") == 0) {
        return EIK_BLOCK_WALL;
    }
    return EIK_BLOCK_SOLID;
}

static EIKCollisionWorld collision_snapshot(const EikLevelState *level,
    const EikItemWorld *items, EIKBlockSnapshot *blocks, size_t capacity,
    EIKSurfaceSnapshot *escalators, size_t escalator_capacity,
    EIKSurfaceSnapshot *falling_platforms, size_t falling_platform_capacity)
{
    size_t object_index = 0U;
    size_t block_count = 0U;

    for (object_index = 0U; object_index < level->map.object_count; ++object_index) {
        const EikTmxObject *object = &level->map.objects[object_index];

        if (strcmp(object->layer, "Collisions") != 0 || block_count >= capacity) {
            continue;
        }
        blocks[block_count++] = (EIKBlockSnapshot){
            .id = (uint32_t)object_index,
            .position = { object->x, object->y },
            .size = { object->width, object->height },
            .kind = block_kind_from_object(object),
        };
    }
    for (object_index = 0U; object_index < items->count && block_count < capacity;
            ++object_index) {
        const EikItem *item = &items->items[object_index];

        if (item->active && item->kind == EIK_ITEM_WALL) {
            blocks[block_count++] = (EIKBlockSnapshot){
                .id = (uint32_t)object_index, .position = item->position, .size = item->size,
                .kind = EIK_BLOCK_WALL,
            };
        }
    }
    {
        EIKCollisionWorld item_world = eik_items_collision_world(items, escalators,
            escalator_capacity, falling_platforms, falling_platform_capacity);

        item_world.blocks = blocks;
        item_world.block_count = block_count;
        return item_world;
    }
}

static bool advance_level(void)
{
    const size_t next_index = (app.progress.current_level + 1U) % 2U;
    char error[256];

    if (!eik_level_load(app.world, &app.level, next_index, app.level_paths[next_index], error,
            sizeof(error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", error);
        return false;
    }
    app.progress.current_level = next_index;
    eik_player_spawn(&app.player, app.level.player_position);
    eik_enemy_world_load(app.world, &app.enemies, &app.level);
    eik_items_world_load(app.world, &app.items, &app.level);
    eik_renderer_snap_camera(&app.renderer);
    return true;
}

static void tick(void)
{
    static float accumulator = 0.0F;
    static bool capture_fx_emitted = false;
    EIKBlockSnapshot blocks[96];
    EIKSurfaceSnapshot escalators[16];
    EIKSurfaceSnapshot falling_platforms[16];
    EIKCollisionWorld world;
    const float raw_dt = GetFrameTime();

    app.input_frame = eik_input_read();
    app.game_time.time_scale = eik_enemy_time_scale(&app.enemies, &app.player);
    eik_game_time_begin_frame(&app.game_time, raw_dt);
    if (app.input_frame.pause_pressed) {
        app.running = false;
    }
    ecs_run_pipeline(app.world, app.pre_pipeline, app.game_time.real_dt);
    eik_items_virtual_step(&app.items, app.game_time.virtual_dt);
    world = collision_snapshot(&app.level, &app.items, blocks, sizeof(blocks) / sizeof(blocks[0]),
        escalators, sizeof(escalators) / sizeof(escalators[0]), falling_platforms,
        sizeof(falling_platforms) / sizeof(falling_platforms[0]));
    accumulator += app.game_time.virtual_dt;
    while (accumulator >= app.game_time.fixed_dt) {
        const EIKVerticalOutcome outcome = eik_player_fixed_step(&app.player, &app.progress,
            &app.input_frame, &world, app.game_time.fixed_dt);

        if (outcome.trigger_fall) {
            eik_items_trigger_fall(&app.items, outcome.falling_platform_id);
        }
        eik_enemy_fixed_step(app.world, &app.enemies, &app.player, &app.progress, &world,
            app.game_time.fixed_dt);
        ecs_run_pipeline(app.world, app.fixed_pipeline, app.game_time.fixed_dt);
        accumulator -= app.game_time.fixed_dt;
    }
    eik_player_update(&app.player, &app.progress, app.game_time.real_dt);
    eik_enemy_update(app.world, &app.enemies, app.game_time.real_dt);
    eik_items_contact_step(app.world, &app.items, &app.player, &app.progress);
    {
        size_t effect_index = 0U;

        for (effect_index = 0U; effect_index < app.items.effect_count; ++effect_index) {
            const EikItemEffect *effect = &app.items.effects[effect_index];

            eik_renderer_emit_item_effect(&app.renderer, effect->kind, effect->centre);
        }
    }
    if (app.input_frame.interact_pressed) {
        eik_items_activate_trigger(app.world, &app.items);
    }
    if (app.player.routine == EIK_PLAYER_LEAVING_LEVEL
            && app.player.routine_elapsed >= 3.0F && !advance_level()) {
        app.running = false;
    }
    app.level.player_position = app.player.position;
    app.level.has_player = !app.progress.game_over;
    ecs_run_pipeline(app.world, app.update_pipeline, app.game_time.real_dt);

    if (IsKeyPressed(KEY_F1)) {
        app.show_collision = !app.show_collision;
    }
    if (IsKeyPressed(KEY_F3) || (!capture_fx_emitted && getenv("EIK_CAPTURE_INPUT") != NULL
            && strcmp(getenv("EIK_CAPTURE_INPUT"), "fx") == 0)) {
        eik_renderer_emit_debug_effects(&app.renderer, (Vector2){ app.player.position.x + 24.0F,
            app.player.position.y + 24.0F });
        capture_fx_emitted = true;
    }
    eik_renderer_update_camera(&app.renderer, &app.level, app.game_time.real_dt);
    eik_renderer_update_effects(&app.renderer, &app.level, &app.items, app.game_time.real_dt);
    eik_renderer_draw(&app.renderer, &app.level, &app.player, &app.enemies, &app.items,
        app.sprite,
        app.show_collision);

    if (WindowShouldClose()) {
        app.running = false;
    }
}

int main(int argc, char **argv)
{
    char *sprite_path = asset_path("images/hero/Player.png");
    char *sound_path = asset_path("audio/jump.wav");
    char *tileset_path = asset_path("images/Tileset/Tileset.png");
    char *sky_path = asset_path("images/background/sky.png");
    char *bat_path = asset_path("images/enemy/Bat.png");
    char *yellow_mob_path = asset_path("images/enemy/yellow_mob.png");
    char *red_mob_path = asset_path("images/enemy/Mobs.png");
    char *level_zero_path = asset_path("tiles/forest-1.tmx");
    char *level_one_path = asset_path("tiles/forest.tmx");
    const char *level_path = level_zero_path;
    size_t level_index = 0U;
    char level_error[256];

    if (!FileExists(sprite_path)) {
        fail_asset_load(sprite_path);
    }
    if (!FileExists(sound_path)) {
        free(sprite_path);
        fail_asset_load(sound_path);
    }
    if (!FileExists(tileset_path)) {
        free(sprite_path);
        free(sound_path);
        fail_asset_load(tileset_path);
    }
    if (!FileExists(sky_path)) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(level_zero_path);
        free(level_one_path);
        fail_asset_load(sky_path);
    }
    if (!FileExists(bat_path)) {
        fail_asset_load(bat_path);
    }
    if (!FileExists(yellow_mob_path)) {
        fail_asset_load(yellow_mob_path);
    }
    if (!FileExists(red_mob_path)) {
        fail_asset_load(red_mob_path);
    }
    if (!FileExists(level_zero_path)) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_one_path);
        fail_asset_load(level_zero_path);
    }
    if (!FileExists(level_one_path)) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        fail_asset_load(level_one_path);
    }
    if (argc == 2 && strcmp(argv[1], "--check-assets") == 0) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
        free(bat_path);
        free(yellow_mob_path);
        free(red_mob_path);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--check-scaffold") == 0) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
        free(bat_path);
        free(yellow_mob_path);
        free(red_mob_path);
        initialize_world();
        ecs_run_pipeline(app.world, app.pre_pipeline, 0.0F);
        ecs_run_pipeline(app.world, app.fixed_pipeline, 0.0F);
        ecs_run_pipeline(app.world, app.update_pipeline, 0.0F);
        ecs_fini(app.world);
        return EXIT_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "--check-level") == 0) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(bat_path);
        free(yellow_mob_path);
        free(red_mob_path);
        initialize_world();
        if (!eik_level_load(app.world, &app.level, 0U, level_zero_path,
                level_error, sizeof(level_error))) {
            (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
            free(level_zero_path);
            free(level_one_path);
            ecs_fini(app.world);
            return EXIT_FAILURE;
        }
        eik_level_unload(app.world, &app.level);
        free(level_zero_path);
        free(level_one_path);
        ecs_fini(app.world);
        return EXIT_SUCCESS;
    }

    if (getenv("EIK_CAPTURE_LEVEL") != NULL && strcmp(getenv("EIK_CAPTURE_LEVEL"), "1") == 0) {
        level_path = level_one_path;
        level_index = 1U;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Edgard in Kimeria");
    SetTargetFPS(60);
#if !defined(EIK_IOS_SIMULATOR)
    InitAudioDevice();
#endif

    initialize_world();
    if (!eik_level_load(app.world, &app.level, level_index, level_path,
            level_error, sizeof(level_error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
#if !defined(EIK_IOS_SIMULATOR)
        CloseAudioDevice();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    app.game_time.time_scale = 1.0F;
    app.progress = (EikGameProgress){ .lives = 3, .current_level = level_index };
    eik_player_spawn(&app.player, app.level.player_position);
    eik_enemy_world_load(app.world, &app.enemies, &app.level);
    eik_items_world_load(app.world, &app.items, &app.level);
    app.player.invulnerable = getenv("EIK_INVULNERABLE") != NULL;
    eik_renderer_snap_camera(&app.renderer);
    if (!eik_renderer_init(&app.renderer, tileset_path, sky_path,
            level_error, sizeof(level_error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
        eik_level_unload(app.world, &app.level);
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
        free(bat_path);
        free(yellow_mob_path);
        free(red_mob_path);
#if !defined(EIK_IOS_SIMULATOR)
        CloseAudioDevice();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    if (!eik_renderer_load_enemy_textures(&app.renderer, bat_path, yellow_mob_path,
            red_mob_path, level_error, sizeof(level_error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
        free(bat_path);
        free(yellow_mob_path);
        free(red_mob_path);
        eik_renderer_unload(&app.renderer);
        eik_level_unload(app.world, &app.level);
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
#if !defined(EIK_IOS_SIMULATOR)
        CloseAudioDevice();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    free(bat_path);
    free(yellow_mob_path);
    free(red_mob_path);
    free(tileset_path);
    free(sky_path);
    {
        char *fog_path = asset_path("shaders/fog.fs");
        char *shockwave_path = asset_path("shaders/shockwave.fs");
        char *explosion_path = asset_path("shaders/bomb_explosion.fs");
        char *screen_path = asset_path("shaders/screen_effects.fs");

        eik_renderer_load_effects(&app.renderer, fog_path, shockwave_path, explosion_path,
            screen_path);
        free(fog_path);
        free(shockwave_path);
        free(explosion_path);
        free(screen_path);
    }
    app.level_paths[0] = level_zero_path;
    app.level_paths[1] = level_one_path;
    app.sprite = LoadTexture(sprite_path);
    if (app.sprite.id == 0U) {
        (void)fprintf(stderr, "Edgard in Kimeria: raylib rejected asset: %s\n", sprite_path);
        free(sprite_path);
        free(sound_path);
#if !defined(EIK_IOS_SIMULATOR)
        CloseAudioDevice();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    free(sprite_path);
    app.tint_shader = LoadShaderFromMemory(NULL, tint_fragment_shader);
    if (app.tint_shader.id == 0U) {
        (void)fprintf(stderr, "Edgard in Kimeria: tint shader failed to compile\n");
        free(sound_path);
        UnloadTexture(app.sprite);
#if !defined(EIK_IOS_SIMULATOR)
        CloseAudioDevice();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    app.shader_loaded = true;
#if !defined(EIK_IOS_SIMULATOR)
    app.jump_sound = LoadSound(sound_path);
    if (app.jump_sound.frameCount == 0U) {
        (void)fprintf(stderr, "Edgard in Kimeria: raylib rejected jump sound\n");
        free(sound_path);
        UnloadShader(app.tint_shader);
        UnloadTexture(app.sprite);
        CloseAudioDevice();
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    app.sound_loaded = true;
#endif
    free(sound_path);

    app.running = true;
#if defined(EIK_IOS)
    eik_ios_start_frame_loop(GetWindowHandle(), tick);
#elif defined(PLATFORM_WEB)
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (app.running) {
        tick();
    }
    if (app.sound_loaded) {
        UnloadSound(app.jump_sound);
    }
    if (app.shader_loaded) {
        UnloadShader(app.tint_shader);
    }
    UnloadTexture(app.sprite);
    eik_renderer_unload(&app.renderer);
    eik_level_unload(app.world, &app.level);
    free(level_zero_path);
    free(level_one_path);
#if !defined(EIK_IOS_SIMULATOR)
    CloseAudioDevice();
#endif
    ecs_fini(app.world);
    CloseWindow();
#endif
    return EXIT_SUCCESS;
}
