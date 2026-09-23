#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flecs.h"
#include "mod_level.h"
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
    struct {
        int touch_count;
        Vector2 touch_position;
        bool gamepad_connected;
        float gamepad_horizontal;
    } input_frame;
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
}

static void update_input_frame(void)
{
    app.input_frame.touch_count = GetTouchPointCount();
    app.input_frame.touch_position = (Vector2){ 0.0F, 0.0F };
    if (app.input_frame.touch_count > 0) {
        app.input_frame.touch_position = GetTouchPosition(0);
    }
    app.input_frame.gamepad_connected = IsGamepadAvailable(0);
    app.input_frame.gamepad_horizontal = 0.0F;
    if (app.input_frame.gamepad_connected) {
        app.input_frame.gamepad_horizontal = GetGamepadAxisMovement(
            0, GAMEPAD_AXIS_LEFT_X);
    }
}

static void tick(void)
{
    static float accumulator = 0.0F;
    static int previous_touch_count = 0;
    const float real_dt = GetFrameTime() > 0.25F ? 0.25F : GetFrameTime();

    ecs_run_pipeline(app.world, app.pre_pipeline, real_dt);
    accumulator += real_dt;
    while (accumulator >= (1.0F / 60.0F)) {
        ecs_run_pipeline(app.world, app.fixed_pipeline, 1.0F / 60.0F);
        accumulator -= 1.0F / 60.0F;
    }
    ecs_run_pipeline(app.world, app.update_pipeline, real_dt);

    update_input_frame();
    if (app.sound_loaded && (IsKeyPressed(KEY_SPACE)
            || (app.input_frame.touch_count > 0 && previous_touch_count == 0))) {
        PlaySound(app.jump_sound);
    }
    previous_touch_count = app.input_frame.touch_count;

    if (IsKeyPressed(KEY_F1)) {
        app.show_collision = !app.show_collision;
    }
    eik_renderer_update_camera(&app.renderer, &app.level, real_dt);
    eik_renderer_draw(&app.renderer, &app.level, app.show_collision);

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
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--check-scaffold") == 0) {
        free(sprite_path);
        free(sound_path);
        free(tileset_path);
        free(sky_path);
        free(level_zero_path);
        free(level_one_path);
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
#if !defined(EIK_IOS_SIMULATOR)
        CloseAudioDevice();
#endif
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    free(tileset_path);
    free(sky_path);
    free(level_zero_path);
    free(level_one_path);
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
#if !defined(EIK_IOS_SIMULATOR)
    CloseAudioDevice();
#endif
    ecs_fini(app.world);
    CloseWindow();
#endif
    return EXIT_SUCCESS;
}
