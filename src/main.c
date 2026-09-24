#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flecs.h"
#include "audio.h"
#include "dev_tools.h"
#include "input.h"
#include "mod_level.h"
#include "mod_enemy.h"
#include "mod_items.h"
#include "mod_player.h"
#include "raylib.h"
#include "render.h"
#include "settings.h"
#include "touch.h"
#include "ui.h"

#if defined(EIK_IOS)
#include <SDL2/SDL.h>

void eik_ios_start_frame_loop(void *sdl_window, void (*frame)(void));
void eik_ios_configure_audio_session(void);
void eik_ios_set_audio_lifecycle_callbacks(void (*pause_callback)(void),
    void (*resume_callback)(void));
EikTouchSafeArea eik_ios_safe_area(void *sdl_window);
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
    EikAudio audio;
    EikAudioPaths audio_paths;
    EikSettings settings;
    EikInputFrame input_frame;
    EikGameTime game_time;
    EikGameProgress progress;
    EikPlayer player;
    EikEnemyWorld enemies;
    EikItemWorld items;
    const char *level_paths[2];
    bool shader_loaded;
    bool audio_device_initialized;
    EikLevelState level;
    EikRenderer renderer;
    EikUi ui;
    EikTouchControls touch;
    EikCapture capture;
#if defined(EIK_DEV)
    ecs_entity_t rest_dequeue_system;
#endif
    bool show_collision;
    bool show_fps;
    bool running;
} App;

static App app;

#if defined(EIK_IOS)
static void pause_audio(void);
static void resume_audio(void);
#endif

static void app_set_state(EikAppState state)
{
    eik_ui_set_state(&app.ui, state);
    if (state == EIK_APP_MAIN_MENU || state == EIK_APP_ABOUT || state == EIK_APP_OPTIONS) {
        eik_audio_set_state(&app.audio, state == EIK_APP_ABOUT ? EIK_AUDIO_ABOUT
            : state == EIK_APP_OPTIONS ? EIK_AUDIO_OPTIONS : EIK_AUDIO_MAIN_MENU);
    } else if (state == EIK_APP_PLAYING) {
        eik_audio_set_state(&app.audio, EIK_AUDIO_PLAYING);
    }
}

static void close_audio_device(void);

static bool initialize_audio(void)
{
#if defined(EIK_IOS_SIMULATOR)
    return true;
#else
    char error[256];

    if (app.audio.initialized) {
        return true;
    }
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        (void)fprintf(stderr, "Edgard in Kimeria: cannot initialize audio device\n");
        return false;
    }
    app.audio_device_initialized = true;
    if (!eik_audio_init(&app.audio, &app.audio_paths, error, sizeof(error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", error);
        close_audio_device();
        return false;
    }
#if defined(EIK_IOS)
    eik_ios_configure_audio_session();
    eik_ios_set_audio_lifecycle_callbacks(pause_audio, resume_audio);
#endif
    return true;
#endif
}

static void apply_display_mode(EikDisplayMode display)
{
#if !defined(PLATFORM_WEB) && !defined(EIK_IOS)
    if (display == EIK_DISPLAY_FULLSCREEN && !IsWindowFullscreen()) {
        ToggleBorderlessWindowed();
    } else if (display == EIK_DISPLAY_WINDOWED && IsWindowFullscreen()) {
        const int width = 1280;
        const int height = 720;

        ToggleBorderlessWindowed();
        SetWindowSize(width, height);
        SetWindowPosition((GetMonitorWidth(GetCurrentMonitor()) - width) / 2,
            (GetMonitorHeight(GetCurrentMonitor()) - height) / 2);
    }
#else
    (void)display;
#endif
}

static void close_audio_device(void)
{
    if (app.audio_device_initialized) {
        CloseAudioDevice();
        app.audio_device_initialized = false;
    }
}

#if defined(EIK_IOS)
static void pause_audio(void)
{
    eik_audio_pause(&app.audio);
}

static void resume_audio(void)
{
    eik_audio_resume(&app.audio);
}
#endif

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

static void free_audio_paths(char *jump_path, char *hit_path, char *collect_path,
    char *bounce_path, char *disappear_path, char *button_click_path, char *menu_music_path)
{
    free(jump_path);
    free(hit_path);
    free(collect_path);
    free(bounce_path);
    free(disappear_path);
    free(button_click_path);
    free(menu_music_path);
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
#if defined(__APPLE__) && !defined(EIK_IOS)
        /* A Finder-launched .app keeps its executable in Contents/MacOS while CMake
         * packages assets in Contents/Resources. Command-line builds retain the
         * adjacent assets/ fallback below. */
        path_length = strlen(root) + strlen("../Resources/assets/")
            + strlen(relative_path) + 1U;
        path = malloc(path_length);
        if (path != NULL) {
            (void)snprintf(path, path_length, "%s../Resources/assets/%s", root,
                relative_path);
            if (FileExists(path)) {
                return path;
            }
            free(path);
            path = NULL;
        }
#endif
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
#if defined(EIK_DEV)
    ECS_IMPORT(app.world, FlecsRest);
    ECS_IMPORT(app.world, FlecsStats);
    ecs_set(app.world, EcsWorld, EcsRest, { .port = ECS_REST_DEFAULT_PORT,
        .ipaddr = "127.0.0.1" });
    app.rest_dequeue_system = ecs_lookup(app.world, "flecs.rest.DequeueRest");
    (void)fprintf(stderr, "Edgard in Kimeria: Flecs Explorer listening at http://127.0.0.1:%d\n",
        ECS_REST_DEFAULT_PORT);
#endif
}

static void log_tilemap_debug(const EikLevelState *level)
{
    if (getenv("EIK_DEBUG_TILEMAP") == NULL) {
        return;
    }
    (void)fprintf(stderr, "TILEDBG level=%zu size=%ux%u tiles=%zu objects=%zu tileset=%s\n",
        level->index, level->map.width, level->map.height, level->map.tile_count,
        level->map.object_count, level->map.tileset_source);
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
    log_tilemap_debug(&app.level);
    eik_player_spawn(&app.player, app.level.player_position);
    eik_enemy_world_load(app.world, &app.enemies, &app.level);
    eik_items_world_load(app.world, &app.items, &app.level);
    eik_renderer_snap_camera(&app.renderer);
    return true;
}

static bool start_new_run(size_t level_index)
{
    char error[256];

    if (!eik_level_load(app.world, &app.level, level_index, app.level_paths[level_index], error,
            sizeof(error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", error);
        return false;
    }
    app.progress = (EikGameProgress){ .lives = 3, .current_level = level_index };
    log_tilemap_debug(&app.level);
    eik_player_spawn(&app.player, app.level.player_position);
    app.player.invulnerable = getenv("EIK_INVULNERABLE") != NULL;
    eik_enemy_world_load(app.world, &app.enemies, &app.level);
    eik_items_world_load(app.world, &app.items, &app.level);
    eik_renderer_snap_camera(&app.renderer);
    app_set_state(EIK_APP_PLAYING);
    return true;
}

static void handle_ui_action(EikUiAction action)
{
    switch (action) {
        case EIK_UI_ACTION_PLAY:
            if (!start_new_run(0U)) {
                app.running = false;
            }
            break;
        case EIK_UI_ACTION_RESUME:
            app_set_state(EIK_APP_PLAYING);
            break;
        case EIK_UI_ACTION_EXIT_TO_MENU:
            app_set_state(EIK_APP_MAIN_MENU);
            break;
        case EIK_UI_ACTION_QUIT:
            app.running = false;
            break;
        case EIK_UI_ACTION_START_WEB:
            if (!initialize_audio()) {
                app.running = false;
            } else {
                app_set_state(EIK_APP_MAIN_MENU);
            }
            break;
        case EIK_UI_ACTION_LANGUAGE_CHANGED:
            app.settings.language = app.ui.language;
            if (!eik_settings_save(&app.settings)) {
                (void)fprintf(stderr, "Edgard in Kimeria: cannot save settings\n");
            }
            break;
        case EIK_UI_ACTION_TOGGLE_DISPLAY:
            app.settings.display = app.ui.fullscreen ? EIK_DISPLAY_FULLSCREEN
                : EIK_DISPLAY_WINDOWED;
            apply_display_mode(app.settings.display);
            if (!eik_settings_save(&app.settings)) {
                (void)fprintf(stderr, "Edgard in Kimeria: cannot save settings\n");
            }
            break;
        case EIK_UI_ACTION_NONE:
            break;
    }
}

static void tick(void)
{
    static float accumulator = 0.0F;
    EIKBlockSnapshot blocks[96];
    EIKSurfaceSnapshot escalators[16];
    EIKSurfaceSnapshot falling_platforms[16];
    EIKCollisionWorld world;
    const float raw_dt = GetFrameTime();
    const EikPlayerRoutine routine_before = app.player.routine;

    app.input_frame = eik_input_read();
    {
        EikTouchSafeArea safe_area = { 0 };

#if defined(EIK_IOS)
        safe_area = eik_ios_safe_area(GetWindowHandle());
#endif
        eik_touch_update(&app.touch, app.ui.state == EIK_APP_PLAYING,
            eik_input_gamepad_connected(), safe_area);
        eik_touch_apply(&app.touch, &app.input_frame);
    }
    if (app.ui.state == EIK_APP_PLAYING && app.input_frame.pause_pressed) {
        app_set_state(EIK_APP_PAUSED);
    }
    app.game_time.time_scale = app.ui.state == EIK_APP_PLAYING
        ? eik_enemy_time_scale(&app.enemies, &app.player) : 0.0F;
    eik_game_time_begin_frame(&app.game_time, raw_dt);
    if (app.ui.state != EIK_APP_PLAYING) {
        handle_ui_action(eik_ui_update(&app.ui, app.game_time.real_dt));
    }
    ecs_run_pipeline(app.world, app.pre_pipeline, app.game_time.real_dt);
    if (app.ui.state == EIK_APP_PLAYING) {
        eik_items_virtual_step(&app.items, app.game_time.virtual_dt);
    }
    world = collision_snapshot(&app.level, &app.items, blocks, sizeof(blocks) / sizeof(blocks[0]),
        escalators, sizeof(escalators) / sizeof(escalators[0]), falling_platforms,
        sizeof(falling_platforms) / sizeof(falling_platforms[0]));
    accumulator += app.game_time.virtual_dt;
    while (app.ui.state == EIK_APP_PLAYING && accumulator >= app.game_time.fixed_dt) {
        const float velocity_before = app.player.velocity.y;
        const EIKVerticalOutcome outcome = eik_player_fixed_step(&app.player, &app.progress,
            &app.input_frame, &world, app.game_time.fixed_dt);

        if (velocity_before >= 0.0F && app.player.velocity.y < 0.0F
                && app.player.routine == EIK_PLAYER_ACTIVE) {
            eik_audio_play(&app.audio, EIK_AUDIO_JUMP);
        }

        if (outcome.trigger_fall) {
            eik_items_trigger_fall(&app.items, outcome.falling_platform_id);
        }
        eik_enemy_fixed_step(app.world, &app.enemies, &app.player, &app.progress, &world,
            app.game_time.fixed_dt);
        ecs_run_pipeline(app.world, app.fixed_pipeline, app.game_time.fixed_dt);
        accumulator -= app.game_time.fixed_dt;
    }
    if (app.ui.state == EIK_APP_PLAYING) {
        eik_player_update(&app.player, &app.progress, app.game_time.real_dt);
        eik_enemy_update(app.world, &app.enemies, app.game_time.real_dt);
        eik_items_contact_step(app.world, &app.items, &app.player, &app.progress);
    }
    {
        size_t effect_index = 0U;

        for (effect_index = 0U; effect_index < app.items.effect_count; ++effect_index) {
            const EikItemEffect *effect = &app.items.effects[effect_index];

            eik_renderer_emit_item_effect(&app.renderer, effect->kind, effect->centre);
            if (effect->kind == EIK_ITEM_EFFECT_EXPLOSION) {
                eik_audio_play(&app.audio, EIK_AUDIO_BOUNCE);
            } else {
                eik_audio_play(&app.audio, EIK_AUDIO_COLLECT);
            }
        }
    }
    if (app.ui.state == EIK_APP_PLAYING && routine_before == EIK_PLAYER_ACTIVE
            && app.player.routine == EIK_PLAYER_DYING) {
        eik_audio_play(&app.audio, EIK_AUDIO_HIT);
    } else if (routine_before == EIK_PLAYER_ACTIVE
            && app.player.routine == EIK_PLAYER_LEAVING_LEVEL) {
        eik_audio_play(&app.audio, EIK_AUDIO_DISAPPEAR);
    }
    if (app.ui.state == EIK_APP_PLAYING && app.input_frame.interact_pressed) {
        eik_items_activate_trigger(app.world, &app.items);
    }
    if (app.ui.state == EIK_APP_PLAYING && app.player.routine == EIK_PLAYER_LEAVING_LEVEL
            && app.player.routine_elapsed >= 3.0F && !advance_level()) {
        app.running = false;
    }
    app.level.player_position = app.player.position;
    app.level.has_player = !app.progress.game_over;
    if (app.ui.state == EIK_APP_PLAYING && app.progress.game_over) {
        app_set_state(EIK_APP_GAME_OVER);
    }
    ecs_run_pipeline(app.world, app.update_pipeline, app.game_time.real_dt);
#if defined(EIK_DEV)
    if (app.rest_dequeue_system != 0U) {
        ecs_run(app.world, app.rest_dequeue_system, app.game_time.real_dt, NULL);
    }
#endif

    if (IsKeyPressed(KEY_F1)) {
        app.show_collision = !app.show_collision;
    }
    if (IsKeyPressed(KEY_F2)) {
        app.player.invulnerable = !app.player.invulnerable;
    }
    if (IsKeyPressed(KEY_F3) || app.input_frame.debug_fx_pressed) {
        eik_renderer_emit_debug_effects(&app.renderer, (Vector2){ app.player.position.x + 24.0F,
            app.player.position.y + 24.0F });
    }
    if ((IsKeyPressed(KEY_F4) || app.input_frame.debug_advance_level_pressed)
            && app.ui.state == EIK_APP_PLAYING && !advance_level()) {
        app.running = false;
    }
    if ((IsKeyPressed(KEY_F5) || app.input_frame.debug_checkpoint_pressed)
            && app.ui.state == EIK_APP_PLAYING) {
        eik_player_reach_checkpoint(&app.player);
    }
    if (IsKeyPressed(KEY_F6)) {
        app.show_fps = !app.show_fps;
    }
    eik_renderer_update_camera(&app.renderer, &app.level, app.game_time.real_dt);
    eik_renderer_update_effects(&app.renderer, &app.level, &app.items, app.game_time.real_dt,
        app.ui.state == EIK_APP_PAUSED);
    eik_renderer_draw(&app.renderer, &app.level, &app.player, &app.enemies, &app.items,
        app.sprite, app.show_collision, app.ui.state == EIK_APP_PAUSED);
    eik_ui_draw(&app.ui, app.progress.coins_collected, app.progress.lives);
    eik_touch_draw(&app.touch);
    if (app.show_fps) {
        const int fps = GetFPS();
        const char *label = TextFormat("%d FPS", fps);

        DrawText(label, GetScreenWidth() - MeasureText(label, 20) - 10, 10, 20, LIME);
    }
    EndDrawing();
    if (eik_capture_should_take(&app.capture, app.game_time.real_dt)) {
        char capture_path[EIK_CAPTURE_PATH_MAX];

        if (!eik_capture_path(&app.capture, capture_path, sizeof(capture_path))) {
            (void)fprintf(stderr, "Edgard in Kimeria: capture path is too long\n");
            app.running = false;
        } else {
            TakeScreenshot(capture_path);
        }
    }
    if (eik_capture_should_start(&app.capture)) {
        eik_capture_mark_started(&app.capture);
        if (app.capture.about) {
            app_set_state(EIK_APP_ABOUT);
        } else if (!start_new_run(app.capture.level)) {
            app.running = false;
        }
    }
    if (app.capture.enabled && app.capture.started && app.capture.remaining == 0U) {
        app.running = false;
    }
    eik_audio_update(&app.audio, app.game_time.real_dt);

    if (WindowShouldClose()) {
        app.running = false;
    }
}

int main(int argc, char **argv)
{
    char *sprite_path = asset_path("images/hero/Player.png");
    char *jump_path = asset_path("audio/jump.wav");
    char *hit_path = asset_path("audio/hit.wav");
    char *collect_path = asset_path("audio/collect.wav");
    char *bounce_path = asset_path("audio/bounce.wav");
    char *disappear_path = asset_path("audio/disappear.wav");
    char *button_click_path = asset_path("audio/button_click.wav");
    char *menu_music_path = asset_path("audio/main_menu.mp3");
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

    if (!eik_capture_configure(&app.capture, level_error, sizeof(level_error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
        return EXIT_FAILURE;
    }

    if (!FileExists(sprite_path)) {
        fail_asset_load(sprite_path);
    }
    if (!FileExists(jump_path)) { fail_asset_load(jump_path); }
    if (!FileExists(hit_path)) { fail_asset_load(hit_path); }
    if (!FileExists(collect_path)) { fail_asset_load(collect_path); }
    if (!FileExists(bounce_path)) { fail_asset_load(bounce_path); }
    if (!FileExists(disappear_path)) { fail_asset_load(disappear_path); }
    if (!FileExists(button_click_path)) { fail_asset_load(button_click_path); }
    if (!FileExists(menu_music_path)) { fail_asset_load(menu_music_path); }
    if (!FileExists(tileset_path)) {
        free(sprite_path);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        fail_asset_load(tileset_path);
    }
    if (!FileExists(sky_path)) {
        free(sprite_path);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
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
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        free(tileset_path);
        free(sky_path);
        free(level_one_path);
        fail_asset_load(level_zero_path);
    }
    if (!FileExists(level_one_path)) {
        free(sprite_path);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        fail_asset_load(level_one_path);
    }
    if (argc == 2 && strcmp(argv[1], "--check-assets") == 0) {
        free(sprite_path);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
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
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
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
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
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

    if (app.capture.enabled && app.capture.level == 1U) {
        level_path = level_one_path;
        level_index = 1U;
    }

    eik_settings_load(&app.settings, getenv("HOME"));
#if defined(EIK_IOS)
    app.settings.display = EIK_DISPLAY_FULLSCREEN;
#endif
    app.audio_paths = (EikAudioPaths){
        .sounds = { jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path },
        .menu_music = menu_music_path,
    };
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 720, "Edgard in Kimeria");
    SetTargetFPS(60);
    apply_display_mode(app.settings.display);

    initialize_world();
    if (!eik_level_load(app.world, &app.level, level_index, level_path,
            level_error, sizeof(level_error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
        free(sprite_path);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
#if !defined(EIK_IOS_SIMULATOR)
        close_audio_device();
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
    app.show_collision = getenv("EIK_DEBUG_DRAW") != NULL;
    app.show_fps = getenv("EIK_SHOW_FPS") != NULL;
    log_tilemap_debug(&app.level);
    eik_renderer_snap_camera(&app.renderer);
    if (!eik_renderer_init(&app.renderer, tileset_path, sky_path,
            level_error, sizeof(level_error))) {
        (void)fprintf(stderr, "Edgard in Kimeria: %s\n", level_error);
        eik_level_unload(app.world, &app.level);
        free(sprite_path);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
        free(bat_path);
        free(yellow_mob_path);
        free(red_mob_path);
#if !defined(EIK_IOS_SIMULATOR)
        close_audio_device();
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
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
#if !defined(EIK_IOS_SIMULATOR)
        close_audio_device();
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
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
#if !defined(EIK_IOS_SIMULATOR)
        close_audio_device();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    free(sprite_path);
    app.tint_shader = LoadShaderFromMemory(NULL, tint_fragment_shader);
    if (app.tint_shader.id == 0U) {
        (void)fprintf(stderr, "Edgard in Kimeria: tint shader failed to compile\n");
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        UnloadTexture(app.sprite);
#if !defined(EIK_IOS_SIMULATOR)
        close_audio_device();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    app.shader_loaded = true;
    {
        char *text_font_path = asset_path("fonts/QuestSquare.ttf");
        char *button_font_path = asset_path("fonts/NanoPlus.ttf");
        char *items_path = asset_path("images/Items.png");
        char *joystick_path = asset_path("images/HUD/Joystick.png");
        char *knob_path = asset_path("images/HUD/Knob.png");
        char *jump_button_path = asset_path("images/HUD/JumpButton.png");

        if (!FileExists(text_font_path) || !FileExists(button_font_path) || !FileExists(items_path)
                || !FileExists(joystick_path) || !FileExists(knob_path)
                || !FileExists(jump_button_path) || !eik_ui_init(&app.ui, text_font_path,
                    button_font_path, items_path,
                    level_error, sizeof(level_error))
                || !eik_touch_init(&app.touch, joystick_path, knob_path, jump_button_path,
                    level_error, sizeof(level_error))) {
            (void)fprintf(stderr, "Edgard in Kimeria: cannot load UI or touch-control assets\n");
            free(text_font_path);
            free(button_font_path);
            free(items_path);
            free(joystick_path);
            free(knob_path);
            free(jump_button_path);
            eik_ui_unload(&app.ui);
            eik_touch_unload(&app.touch);
            eik_audio_unload(&app.audio);
            UnloadShader(app.tint_shader);
            UnloadTexture(app.sprite);
            eik_renderer_unload(&app.renderer);
            eik_level_unload(app.world, &app.level);
            free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
                button_click_path, menu_music_path);
            free(level_zero_path);
            free(level_one_path);
#if !defined(EIK_IOS_SIMULATOR)
            close_audio_device();
#endif
            ecs_fini(app.world);
            CloseWindow();
            return EXIT_FAILURE;
        }
        free(text_font_path);
        free(button_font_path);
        free(items_path);
        free(joystick_path);
        free(knob_path);
        free(jump_button_path);
    }
    app.ui.language = app.settings.language;
    app.ui.fullscreen = app.settings.display == EIK_DISPLAY_FULLSCREEN;
#if defined(PLATFORM_WEB)
    app_set_state(EIK_APP_WEB_START);
#else
    if (!initialize_audio()) {
        eik_ui_unload(&app.ui);
        eik_touch_unload(&app.touch);
        UnloadShader(app.tint_shader);
        UnloadTexture(app.sprite);
        eik_renderer_unload(&app.renderer);
        eik_level_unload(app.world, &app.level);
        free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
            button_click_path, menu_music_path);
        free(level_zero_path);
        free(level_one_path);
        ecs_fini(app.world);
        CloseWindow();
        return EXIT_FAILURE;
    }
    app_set_state(EIK_APP_MAIN_MENU);
#endif

    app.running = true;
#if defined(EIK_IOS)
    eik_ios_start_frame_loop(GetWindowHandle(), tick);
#elif defined(PLATFORM_WEB)
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (app.running) {
        tick();
    }
    eik_ui_unload(&app.ui);
    eik_touch_unload(&app.touch);
    eik_audio_unload(&app.audio);
    if (app.shader_loaded) {
        UnloadShader(app.tint_shader);
    }
    UnloadTexture(app.sprite);
    eik_renderer_unload(&app.renderer);
    eik_level_unload(app.world, &app.level);
    free_audio_paths(jump_path, hit_path, collect_path, bounce_path, disappear_path,
        button_click_path, menu_music_path);
    free(level_zero_path);
    free(level_one_path);
    close_audio_device();
    ecs_fini(app.world);
    CloseWindow();
#endif
    return EXIT_SUCCESS;
}
