#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flecs.h"
#include "raylib.h"

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
    bool running;
} App;

static App app;

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
}

static void tick(void)
{
    static float accumulator = 0.0F;
    const float real_dt = GetFrameTime() > 0.25F ? 0.25F : GetFrameTime();

    ecs_run_pipeline(app.world, app.pre_pipeline, real_dt);
    accumulator += real_dt;
    while (accumulator >= (1.0F / 60.0F)) {
        ecs_run_pipeline(app.world, app.fixed_pipeline, 1.0F / 60.0F);
        accumulator -= 1.0F / 60.0F;
    }
    ecs_run_pipeline(app.world, app.update_pipeline, real_dt);

    BeginDrawing();
    ClearBackground((Color){ 19, 30, 54, 255 });
    DrawTextureRec(app.sprite, (Rectangle){ 0.0F, 0.0F, 48.0F, 48.0F },
        (Vector2){ 616.0F, 336.0F }, WHITE);
    DrawText("Edgard in Kimeria", 32, 32, 30, RAYWHITE);
    EndDrawing();

    if (WindowShouldClose()) {
        app.running = false;
    }
}

int main(int argc, char **argv)
{
    char *sprite_path = asset_path("images/hero/Player.png");

    if (!FileExists(sprite_path)) {
        fail_asset_load(sprite_path);
    }
    if (argc == 2 && strcmp(argv[1], "--check-assets") == 0) {
        free(sprite_path);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--check-scaffold") == 0) {
        free(sprite_path);
        initialize_world();
        ecs_run_pipeline(app.world, app.pre_pipeline, 0.0F);
        ecs_run_pipeline(app.world, app.fixed_pipeline, 0.0F);
        ecs_run_pipeline(app.world, app.update_pipeline, 0.0F);
        ecs_fini(app.world);
        return EXIT_SUCCESS;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Edgard in Kimeria");
    SetTargetFPS(60);

    initialize_world();
    app.sprite = LoadTexture(sprite_path);
    if (app.sprite.id == 0U) {
        (void)fprintf(stderr, "Edgard in Kimeria: raylib rejected asset: %s\n", sprite_path);
        free(sprite_path);
        CloseWindow();
        ecs_fini(app.world);
        return EXIT_FAILURE;
    }
    free(sprite_path);

    app.running = true;
#ifdef PLATFORM_WEB
    emscripten_set_main_loop(tick, 0, 1);
#else
    while (app.running) {
        tick();
    }
    UnloadTexture(app.sprite);
    ecs_fini(app.world);
    CloseWindow();
#endif
    return EXIT_SUCCESS;
}
