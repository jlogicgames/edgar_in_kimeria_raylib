#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    EIK_LOGICAL_WIDTH = 640,
    EIK_LOGICAL_HEIGHT = 360,
    EIK_TILE_SIZE = 16,
};

static const float EIK_CAMERA_FOLLOW_SPEED = 500.0F;

static float effect_random(EikEffects *effects)
{
    uint32_t value = effects->random_state;

    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    effects->random_state = value;
    return (float)(value & 0x00FFFFFFU) / 16777215.0F;
}

static float clampf(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static Texture2D make_radial_texture(int size, float falloff)
{
    Image image = GenImageColor(size, size, BLANK);
    const float centre = ((float)size - 1.0F) * 0.5F;
    const float radius = (float)size * 0.5F;
    int y = 0;

    for (y = 0; y < size; ++y) {
        int x = 0;

        for (x = 0; x < size; ++x) {
            const float dx = ((float)x - centre) / radius;
            const float dy = ((float)y - centre) / radius;
            const float alpha = powf(clampf(1.0F - sqrtf(dx * dx + dy * dy), 0.0F, 1.0F),
                falloff);

            ImageDrawPixel(&image, x, y, (Color){ 255, 255, 255, (unsigned char)(alpha * 255.0F) });
        }
    }
    {
        Texture2D texture = LoadTextureFromImage(image);

        UnloadImage(image);
        return texture;
    }
}

static const char *shader_header(void)
{
#if defined(EIK_IOS)
    return "#version 300 es\nprecision mediump float;\n#define EIK_GLES 1\n";
#elif defined(PLATFORM_WEB)
    return "#version 100\nprecision mediump float;\n#define EIK_WEB 1\n";
#else
    return "#version 330\n#define EIK_DESKTOP 1\n";
#endif
}

static Shader load_effect_shader(const char *path)
{
    char *body = LoadFileText(path);
    Shader shader = { 0 };

    if (body == NULL) {
        TraceLog(LOG_WARNING, "Edgard in Kimeria: cannot read shader %s", path);
        return shader;
    }
    {
        const char *header = shader_header();
        const size_t size = strlen(header) + strlen(body) + 1U;
        char *source = MemAlloc((unsigned int)size);

        if (source != NULL) {
            (void)snprintf(source, size, "%s%s", header, body);
            shader = LoadShaderFromMemory(NULL, source);
            MemFree(source);
        }
    }
    UnloadFileText(body);
    if (shader.id == 0U) {
        TraceLog(LOG_WARNING, "Edgard in Kimeria: shader compilation failed for %s", path);
    }
    return shader;
}

static void get_effect_locations(EikEffects *effects)
{
    if (effects->fog_shader.id == 0U) {
        effects->fog_size = -1;
        effects->fog_time = -1;
    } else {
        effects->fog_size = GetShaderLocation(effects->fog_shader, "size");
        effects->fog_time = GetShaderLocation(effects->fog_shader, "time");
    }
    if (effects->shockwave_shader.id == 0U) {
        effects->shockwave_size = -1;
        effects->shockwave_center = -1;
        effects->shockwave_time = -1;
        effects->shockwave_progress = -1;
    } else {
        effects->shockwave_size = GetShaderLocation(effects->shockwave_shader, "size");
        effects->shockwave_center = GetShaderLocation(effects->shockwave_shader, "center");
        effects->shockwave_time = GetShaderLocation(effects->shockwave_shader, "time");
        effects->shockwave_progress = GetShaderLocation(effects->shockwave_shader, "progress");
    }
    if (effects->explosion_shader.id == 0U) {
        effects->explosion_time = -1;
        effects->explosion_progress = -1;
    } else {
        effects->explosion_time = GetShaderLocation(effects->explosion_shader, "time");
        effects->explosion_progress = GetShaderLocation(effects->explosion_shader, "progress");
    }
    if (effects->screen_shader.id == 0U) {
        effects->screen_ripple_center = -1;
        effects->screen_ripple_progress = -1;
        effects->screen_time = -1;
        effects->screen_aspect = -1;
        effects->screen_chroma_intensity = -1;
        effects->screen_chroma_shift = -1;
        return;
    }
    effects->screen_ripple_center = GetShaderLocation(effects->screen_shader, "rippleCenter");
    effects->screen_ripple_progress = GetShaderLocation(effects->screen_shader, "rippleProgress");
    effects->screen_time = GetShaderLocation(effects->screen_shader, "time");
    effects->screen_aspect = GetShaderLocation(effects->screen_shader, "aspect");
    effects->screen_chroma_intensity = GetShaderLocation(effects->screen_shader, "chromaIntensity");
    effects->screen_chroma_shift = GetShaderLocation(effects->screen_shader, "chromaShift");
}

void eik_renderer_load_effects(EikRenderer *renderer, const char *fog_path,
    const char *shockwave_path, const char *explosion_path, const char *screen_path)
{
    EikEffects *effects = &renderer->effects;

    effects->glow = make_radial_texture(32, 2.0F);
    effects->dot = make_radial_texture(8, 8.0F);
    SetTextureFilter(effects->glow, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(effects->dot, TEXTURE_FILTER_POINT);
    effects->fog_shader = load_effect_shader(fog_path);
    effects->shockwave_shader = load_effect_shader(shockwave_path);
    effects->explosion_shader = load_effect_shader(explosion_path);
    effects->screen_shader = load_effect_shader(screen_path);
    get_effect_locations(effects);
    effects->random_state = 0xC0FFEEU;
    effects->firefly_level = SIZE_MAX;
    effects->chroma_enabled = getenv("EIK_CHROMA_GLITCH") != NULL;
    effects->chroma_wait = 1.0F + effect_random(effects) * 2.0F;
    effects->initialized = effects->glow.id != 0U && effects->dot.id != 0U;
}

static void unload_effects(EikEffects *effects)
{
    if (effects->screen_shader.id != 0U) {
        UnloadShader(effects->screen_shader);
    }
    if (effects->explosion_shader.id != 0U) {
        UnloadShader(effects->explosion_shader);
    }
    if (effects->shockwave_shader.id != 0U) {
        UnloadShader(effects->shockwave_shader);
    }
    if (effects->fog_shader.id != 0U) {
        UnloadShader(effects->fog_shader);
    }
    if (effects->dot.id != 0U) {
        UnloadTexture(effects->dot);
    }
    if (effects->glow.id != 0U) {
        UnloadTexture(effects->glow);
    }
    *effects = (EikEffects){ 0 };
}

bool eik_renderer_init(EikRenderer *renderer, const char *tileset_path, const char *sky_path,
    char *error, size_t error_size)
{
    *renderer = (EikRenderer){ 0 };
    renderer->tileset = LoadTexture(tileset_path);
    renderer->sky = LoadTexture(sky_path);
    renderer->world_target = LoadRenderTexture(EIK_LOGICAL_WIDTH, EIK_LOGICAL_HEIGHT);
    if (renderer->tileset.id == 0U || renderer->sky.id == 0U || renderer->world_target.id == 0U) {
        (void)snprintf(error, error_size, "cannot load Phase 2 render assets");
        eik_renderer_unload(renderer);
        return false;
    }
    SetTextureFilter(renderer->tileset, TEXTURE_FILTER_POINT);
    SetTextureFilter(renderer->sky, TEXTURE_FILTER_POINT);
    renderer->initialized = true;
    return true;
}

void eik_renderer_unload(EikRenderer *renderer)
{
    unload_effects(&renderer->effects);
    if (renderer->red_mob.id != 0U) {
        UnloadTexture(renderer->red_mob);
    }
    if (renderer->yellow_mob.id != 0U) {
        UnloadTexture(renderer->yellow_mob);
    }
    if (renderer->bat.id != 0U) {
        UnloadTexture(renderer->bat);
    }
    if (renderer->world_target.id != 0U) {
        UnloadRenderTexture(renderer->world_target);
    }
    if (renderer->sky.id != 0U) {
        UnloadTexture(renderer->sky);
    }
    if (renderer->tileset.id != 0U) {
        UnloadTexture(renderer->tileset);
    }
    *renderer = (EikRenderer){ 0 };
}

bool eik_renderer_load_enemy_textures(EikRenderer *renderer, const char *bat_path,
    const char *yellow_mob_path, const char *red_mob_path, char *error, size_t error_size)
{
    renderer->bat = LoadTexture(bat_path);
    renderer->yellow_mob = LoadTexture(yellow_mob_path);
    renderer->red_mob = LoadTexture(red_mob_path);
    if (renderer->bat.id == 0U || renderer->yellow_mob.id == 0U || renderer->red_mob.id == 0U) {
        (void)snprintf(error, error_size, "cannot load Phase 4 enemy textures");
        if (renderer->red_mob.id != 0U) {
            UnloadTexture(renderer->red_mob);
        }
        if (renderer->yellow_mob.id != 0U) {
            UnloadTexture(renderer->yellow_mob);
        }
        if (renderer->bat.id != 0U) {
            UnloadTexture(renderer->bat);
        }
        renderer->bat = (Texture2D){ 0 };
        renderer->yellow_mob = (Texture2D){ 0 };
        renderer->red_mob = (Texture2D){ 0 };
        return false;
    }
    SetTextureFilter(renderer->bat, TEXTURE_FILTER_POINT);
    SetTextureFilter(renderer->yellow_mob, TEXTURE_FILTER_POINT);
    SetTextureFilter(renderer->red_mob, TEXTURE_FILTER_POINT);
    return true;
}

void eik_renderer_snap_camera(EikRenderer *renderer)
{
    renderer->camera_placed = false;
}

void eik_renderer_update_camera(EikRenderer *renderer, const EikLevelState *level, float dt)
{
    Vector2 target = { 0.0F, 0.0F };
    Vector2 delta = { 0.0F, 0.0F };
    float distance = 0.0F;
    const float max_step = EIK_CAMERA_FOLLOW_SPEED * dt;

    renderer->sky_scroll -= 5.0F * dt;
    if (renderer->sky_scroll <= -(float)renderer->sky.width) {
        renderer->sky_scroll += (float)renderer->sky.width;
    }
    if (!level->has_player) {
        return;
    }
    target.x = level->player_position.x - 211.0F;
    target.y = level->player_position.y - 200.0F;
    if (!renderer->camera_placed) {
        renderer->camera_top_left = target;
        renderer->camera_placed = true;
        return;
    }
    delta.x = target.x - renderer->camera_top_left.x;
    delta.y = target.y - renderer->camera_top_left.y;
    distance = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (distance <= max_step || distance == 0.0F) {
        renderer->camera_top_left = target;
    } else {
        renderer->camera_top_left.x += delta.x / distance * max_step;
        renderer->camera_top_left.y += delta.y / distance * max_step;
    }
}

static void spawn_particle(EikEffects *effects, Vector2 from, Vector2 to, float lifespan,
    float start_size, float end_size, Color color, float alpha, bool glow)
{
    size_t index = 0U;

    for (index = 0U; index < EIK_MAX_EFFECT_PARTICLES; ++index) {
        EikEffectParticle *particle = &effects->particles[index];

        if (!particle->active) {
            *particle = (EikEffectParticle){
                .from = from, .to = to, .lifespan = lifespan, .start_size = start_size,
                .end_size = end_size, .color = color, .start_alpha = alpha, .active = true,
                .glow = glow,
            };
            return;
        }
    }
}

static void update_particles(EikEffects *effects, float dt)
{
    size_t index = 0U;

    for (index = 0U; index < EIK_MAX_EFFECT_PARTICLES; ++index) {
        EikEffectParticle *particle = &effects->particles[index];

        if (!particle->active) {
            continue;
        }
        particle->age += dt;
        if (particle->age >= particle->lifespan) {
            particle->active = false;
        }
    }
}

static void drive_torch(EikEffects *effects, size_t index, const EikItem *torch, float dt)
{
    float *timers = effects->torch_timers[index];
    const Vector2 centre = { torch->position.x + torch->size.x * 0.5F,
        torch->position.y + torch->size.y * 0.5F };
    size_t burst = 0U;

    if (torch->intensity <= 0.0F) {
        return;
    }
    timers[0] -= dt;
    if (timers[0] <= 0.0F) {
        timers[0] = 0.04F + effect_random(effects) * 0.12F;
        spawn_particle(effects, centre, (Vector2){ centre.x, centre.y - 12.0F },
            0.14F + effect_random(effects) * 0.18F, 12.0F, 2.0F,
            (Color){ 179, 255, 77, 255 }, 0.8F, true);
    }
    timers[1] -= dt;
    if (timers[1] <= 0.0F) {
        timers[1] = 0.02F + effect_random(effects) * 0.12F;
        for (burst = 0U; burst < 3U + (size_t)(effect_random(effects) * 5.0F); ++burst) {
            const Vector2 from = { centre.x + (effect_random(effects) - 0.5F) * 8.0F,
                centre.y + (effect_random(effects) - 0.5F) * 6.0F };

            spawn_particle(effects, from, (Vector2){ from.x + (effect_random(effects) - 0.5F) * 28.0F,
                from.y - 40.0F - effect_random(effects) * 30.0F },
                0.35F + effect_random(effects) * 0.45F, 2.2F, 0.6F,
                (Color){ 153, 255, 102, 255 }, 0.94F, false);
        }
    }
    timers[2] -= dt;
    if (timers[2] <= 0.0F) {
        timers[2] = 0.08F + effect_random(effects) * 0.18F;
        burst = (size_t)clampf(torch->intensity * (0.8F + effect_random(effects) * 0.4F),
            10.0F, 40.0F);
        for (index = 0U; index < burst; ++index) {
            const Vector2 from = { centre.x + (effect_random(effects) - 0.5F) * 10.0F,
                centre.y + (effect_random(effects) - 0.5F) * 8.0F };

            spawn_particle(effects, from, (Vector2){ from.x + (effect_random(effects) - 0.5F) * 18.0F,
                from.y - 30.0F - effect_random(effects) * 60.0F },
                0.4F + effect_random(effects) * 0.8F, 1.8F, 0.4F,
                (Color){ 140, 255, 89, 255 }, 0.7F, false);
        }
    }
    timers[3] -= dt;
    if (timers[3] <= 0.0F) {
        const Vector2 from = { centre.x + (effect_random(effects) - 0.5F) * 6.0F,
            centre.y - 2.0F };

        timers[3] = 0.22F + effect_random(effects) * 0.5F;
        spawn_particle(effects, from, (Vector2){ from.x + (effect_random(effects) - 0.5F) * 8.0F,
            from.y - 50.0F - effect_random(effects) * 40.0F },
            2.0F + effect_random(effects) * 2.0F, 10.0F, 34.0F,
            (Color){ 82, 82, 87, 255 }, 0.78F, true);
    }
}

static void reset_fireflies(EikEffects *effects, const EikLevelState *level)
{
    size_t index = 0U;

    for (index = 0U; index < EIK_MAX_FIREFLIES; ++index) {
        effects->fireflies[index] = (EikFirefly){
            .timer = effect_random(effects), .hide_duration = 1.0F + effect_random(effects),
        };
    }
    effects->firefly_level = level->index;
}

static Vector2 random_point(EikEffects *effects, float width, float height)
{
    return (Vector2){ effect_random(effects) * width, effect_random(effects) * height };
}

static void update_fireflies(EikEffects *effects, const EikLevelState *level, float dt)
{
    const float width = (float)level->map.width * EIK_TILE_SIZE;
    const float height = (float)level->map.height * EIK_TILE_SIZE;
    size_t index = 0U;

    if (level->index != 1U) {
        effects->firefly_level = SIZE_MAX;
        return;
    }
    if (effects->firefly_level != level->index) {
        reset_fireflies(effects, level);
    }
    for (index = 0U; index < EIK_MAX_FIREFLIES; ++index) {
        EikFirefly *firefly = &effects->fireflies[index];

        firefly->timer += dt;
        if (!firefly->flying && firefly->timer >= firefly->hide_duration) {
            firefly->start = random_point(effects, width, height);
            firefly->control = (Vector2){ firefly->start.x + effect_random(effects) * 60.0F - 30.0F,
                firefly->start.y + effect_random(effects) * 60.0F - 30.0F };
            firefly->end = (Vector2){ firefly->start.x + effect_random(effects) * 40.0F - 20.0F,
                firefly->start.y + effect_random(effects) * 40.0F - 20.0F };
            firefly->duration = 3.0F + effect_random(effects) * 4.0F;
            firefly->timer = 0.0F;
            firefly->flying = true;
        } else if (firefly->flying && firefly->timer >= firefly->duration) {
            firefly->timer = 0.0F;
            firefly->hide_duration = 1.0F + effect_random(effects);
            firefly->flying = false;
        }
    }
}

void eik_renderer_update_effects(EikRenderer *renderer, const EikLevelState *level,
    const EikItemWorld *items, float real_dt)
{
    EikEffects *effects = &renderer->effects;
    size_t index = 0U;

    if (!effects->initialized) {
        return;
    }
    effects->time += real_dt;
    for (index = 0U; items != NULL && index < items->count; ++index) {
        const EikItem *item = &items->items[index];

        if (item->active && item->kind == EIK_ITEM_TORCH) {
            drive_torch(effects, index, item, real_dt);
        }
    }
    update_particles(effects, real_dt);
    update_fireflies(effects, level, real_dt);
    if (effects->shockwave.active) {
        effects->shockwave.elapsed += real_dt;
        effects->shockwave.active = effects->shockwave.elapsed < effects->shockwave.duration;
    }
    if (effects->explosion.active) {
        effects->explosion.elapsed += real_dt;
        effects->explosion.active = effects->explosion.elapsed < effects->explosion.duration;
    }
    if (effects->ripple.active) {
        effects->ripple.elapsed += real_dt;
        effects->ripple.active = effects->ripple.elapsed < effects->ripple.duration;
    }
    if (effects->chroma_enabled) {
        if (effects->chroma_elapsed > 0.0F) {
            effects->chroma_elapsed += real_dt;
        } else {
            effects->chroma_wait -= real_dt;
            if (effects->chroma_wait <= 0.0F) {
                effects->chroma_elapsed = 0.0001F;
                effects->chroma_duration = 0.2F + effect_random(effects) * 0.3F;
                effects->chroma_shift = 0.002F + effect_random(effects) * 0.008F;
            }
        }
        if (effects->chroma_elapsed >= effects->chroma_duration) {
            effects->chroma_elapsed = 0.0F;
            effects->chroma_wait = 1.0F + effect_random(effects) * 2.0F;
        }
    }
}

void eik_renderer_emit_item_effect(EikRenderer *renderer, EikItemEffectKind kind,
    Vector2 centre)
{
    EikEffects *effects = &renderer->effects;

    if (kind == EIK_ITEM_EFFECT_RIPPLE) {
        effects->ripple = (EikTransientEffect){ .centre = centre, .duration = 0.75F, .active = true };
    } else if (kind == EIK_ITEM_EFFECT_SHOCKWAVE) {
        effects->shockwave = (EikTransientEffect){ .centre = centre, .duration = 0.6F, .active = true };
    } else if (kind == EIK_ITEM_EFFECT_EXPLOSION) {
        effects->explosion = (EikTransientEffect){ .centre = centre, .duration = 0.7F, .active = true };
    }
}

void eik_renderer_emit_debug_effects(EikRenderer *renderer, Vector2 centre)
{
    eik_renderer_emit_item_effect(renderer, EIK_ITEM_EFFECT_SHOCKWAVE, centre);
    eik_renderer_emit_item_effect(renderer, EIK_ITEM_EFFECT_RIPPLE, centre);
}

static void draw_tiles(const EikRenderer *renderer, const EikTmxMap *map, Vector2 camera)
{
    const int start_x = (int)floorf(camera.x / EIK_TILE_SIZE);
    const int start_y = (int)floorf(camera.y / EIK_TILE_SIZE);
    const int end_x = (int)ceilf((camera.x + EIK_LOGICAL_WIDTH) / EIK_TILE_SIZE);
    const int end_y = (int)ceilf((camera.y + EIK_LOGICAL_HEIGHT) / EIK_TILE_SIZE);
    int tile_y = 0;

    for (tile_y = start_y; tile_y < end_y; tile_y++) {
        int tile_x = 0;

        if (tile_y < 0 || tile_y >= (int)map->height) {
            continue;
        }
        for (tile_x = start_x; tile_x < end_x; tile_x++) {
            const uint32_t raw_gid = tile_x < 0 || tile_x >= (int)map->width ? 0U
                : map->tiles[(size_t)tile_y * map->width + (size_t)tile_x];
            const uint32_t gid = raw_gid & 0x1FFFFFFFU;
            uint32_t tile_index = 0U;
            Rectangle source = { 0.0F, 0.0F, EIK_TILE_SIZE, EIK_TILE_SIZE };
            Rectangle destination = { (float)(tile_x * EIK_TILE_SIZE) - camera.x,
                (float)(tile_y * EIK_TILE_SIZE) - camera.y, EIK_TILE_SIZE, EIK_TILE_SIZE };

            if (gid == 0U || gid < map->tileset_first_gid) {
                continue;
            }
            tile_index = gid - map->tileset_first_gid;
            source.x = (float)((tile_index % map->tileset_columns) * EIK_TILE_SIZE);
            source.y = (float)((tile_index / map->tileset_columns) * EIK_TILE_SIZE);
            DrawTexturePro(renderer->tileset, source, destination, (Vector2){ 0.0F, 0.0F },
                0.0F, WHITE);
        }
    }
}

static void draw_collision_overlay(const EikLevelState *level, Vector2 camera)
{
    size_t index = 0U;

    for (index = 0U; index < level->map.object_count; index++) {
        const EikTmxObject *object = &level->map.objects[index];

        if (strcmp(object->layer, "Collisions") != 0) {
            continue;
        }
        DrawRectangleLinesEx((Rectangle){ object->x - camera.x, object->y - camera.y,
            object->width, object->height }, 1.0F, GREEN);
    }
    if (level->has_player) {
        DrawRectangleLinesEx((Rectangle){ level->player_position.x - camera.x + 18.0F,
            level->player_position.y - camera.y + 26.0F, 11.0F, 22.0F }, 1.0F, RED);
    }
}

static Texture2D enemy_texture(const EikRenderer *renderer, const EikEnemy *enemy)
{
    if (enemy->kind == EIK_ENEMY_BAT) {
        return renderer->bat;
    }
    return enemy->kind == EIK_ENEMY_YELLOW_MOB ? renderer->yellow_mob : renderer->red_mob;
}

static void draw_enemies(const EikRenderer *renderer, const EikEnemyWorld *enemies,
    Vector2 camera)
{
    size_t index = 0U;

    if (enemies == NULL) {
        return;
    }
    for (index = 0U; index < enemies->count; ++index) {
        const EikEnemy *enemy = &enemies->enemies[index];
        Texture2D texture = enemy_texture(renderer, enemy);
        Rectangle source = eik_enemy_frame_rect(enemy);
        const Rectangle destination = { enemy->position.x - camera.x,
            enemy->position.y - camera.y, enemy->size.x, enemy->size.y };

        if (texture.id == 0U) {
            continue;
        }
        if (!enemy->facing_right) {
            source.x += source.width;
            source.width = -source.width;
        }
        DrawTexturePro(texture, source, destination, (Vector2){ 0.0F, 0.0F }, 0.0F, WHITE);
    }
}

static void draw_items(const EikItemWorld *items, Vector2 camera, bool debug)
{
    size_t index = 0U;

    if (items == NULL) {
        return;
    }
    for (index = 0U; index < items->count; ++index) {
        const EikItem *item = &items->items[index];
        const Rectangle box = { item->position.x - camera.x, item->position.y - camera.y,
            item->size.x, item->size.y };
        const Vector2 centre = { box.x + box.width * 0.5F, box.y + box.height * 0.5F };

        if (!item->active) {
            continue;
        }
        switch (item->kind) {
        case EIK_ITEM_COIN:
            DrawCircleV(centre, box.width * 0.4F, GOLD);
            break;
        case EIK_ITEM_HEART:
            DrawRectangleRec(box, RED);
            break;
        case EIK_ITEM_BOMB:
            DrawCircleV(centre, box.width * 0.45F, DARKGRAY);
            break;
        case EIK_ITEM_TORCH:
            if (item->intensity > 0.0F) {
                DrawCircleV(centre, 3.0F + item->intensity * 0.03F, GREEN);
            }
            break;
        case EIK_ITEM_ESCALATOR:
            DrawRectangleRec(box, item->running ? GRAY : DARKGRAY);
            break;
        case EIK_ITEM_FALLING_PLATFORM:
            DrawRectangleRec(box, item->fall_phase == EIK_FALL_WARNING ? ORANGE : LIGHTGRAY);
            if (item->fall_phase == EIK_FALL_WARNING) {
                DrawCircleV(centre, 5.0F, GREEN);
            }
            break;
        case EIK_ITEM_CHECKPOINT:
        case EIK_ITEM_TRIGGER:
        case EIK_ITEM_WALL:
            if (debug) {
                DrawRectangleLinesEx(box, 1.0F, GREEN);
            }
            break;
        }
        if (debug && (item->kind == EIK_ITEM_CHECKPOINT || item->kind == EIK_ITEM_TRIGGER)) {
            DrawRectangleLinesEx(box, 1.0F, YELLOW);
        }
    }
}

static void draw_texture_centre(Texture2D texture, Vector2 centre, float size, Color tint)
{
    DrawTexturePro(texture, (Rectangle){ 0.0F, 0.0F, (float)texture.width, (float)texture.height },
        (Rectangle){ centre.x - size * 0.5F, centre.y - size * 0.5F, size, size },
        (Vector2){ 0.0F, 0.0F }, 0.0F, tint);
}

static Color with_alpha(Color color, float alpha)
{
    color.a = (unsigned char)(clampf(alpha, 0.0F, 1.0F) * 255.0F);
    return color;
}

static void draw_torch_glows(EikRenderer *renderer, const EikItemWorld *items, Vector2 camera)
{
    EikEffects *effects = &renderer->effects;
    size_t index = 0U;

    if (!effects->initialized || items == NULL) {
        return;
    }
    for (index = 0U; index < items->count; ++index) {
        const EikItem *item = &items->items[index];
        float flicker = effects->torch_flickers[index];
        const float base_radius = clampf(4.0F + item->intensity * 0.06F, 3.0F, 40.0F);
        const float base_alpha = clampf(18.0F + item->intensity * 0.22F, 8.0F, 220.0F) / 255.0F;
        Vector2 centre = { item->position.x + item->size.x * 0.5F - camera.x,
            item->position.y + item->size.y * 0.5F - camera.y };

        if (!item->active || item->kind != EIK_ITEM_TORCH || item->intensity <= 0.0F) {
            continue;
        }
        if (flicker == 0.0F) {
            flicker = 1.0F;
        }
        flicker += (0.75F + effect_random(effects) * 0.6F - flicker) * 0.13F;
        effects->torch_flickers[index] = flicker;
        draw_texture_centre(effects->glow, centre, base_radius * flicker * 4.0F,
            with_alpha((Color){ 102, 255, 153, 255 }, base_alpha * flicker));
    }
}

static void draw_particles(const EikEffects *effects, Vector2 camera)
{
    size_t index = 0U;

    for (index = 0U; index < EIK_MAX_EFFECT_PARTICLES; ++index) {
        const EikEffectParticle *particle = &effects->particles[index];
        const float t = particle->lifespan > 0.0F ? particle->age / particle->lifespan : 1.0F;
        const Vector2 position = { particle->from.x + (particle->to.x - particle->from.x) * t - camera.x,
            particle->from.y + (particle->to.y - particle->from.y) * t - camera.y };
        const float size = particle->start_size + (particle->end_size - particle->start_size) * t;

        if (!particle->active) {
            continue;
        }
        draw_texture_centre(particle->glow ? effects->glow : effects->dot, position, size,
            with_alpha(particle->color, particle->start_alpha * (1.0F - t)));
    }
}

static void draw_fireflies(const EikEffects *effects, Vector2 camera)
{
    size_t index = 0U;

    for (index = 0U; index < EIK_MAX_FIREFLIES; ++index) {
        const EikFirefly *firefly = &effects->fireflies[index];
        float t = 0.0F;
        float inv = 0.0F;
        Vector2 position = { 0.0F, 0.0F };

        if (!firefly->flying) {
            continue;
        }
        t = clampf(firefly->timer / firefly->duration, 0.0F, 1.0F);
        inv = 1.0F - t;
        position = (Vector2){ inv * inv * firefly->start.x + 2.0F * inv * t * firefly->control.x
                + t * t * firefly->end.x - camera.x,
            inv * inv * firefly->start.y + 2.0F * inv * t * firefly->control.y
                + t * t * firefly->end.y - camera.y };
        draw_texture_centre(effects->dot, position, 3.0F, with_alpha(BLACK,
            t < 0.5F ? t * 2.0F : (1.0F - t) * 2.0F));
    }
}

static void set_uniform(Shader shader, int location, const void *value, int type)
{
    if (shader.id != 0U && location >= 0) {
        SetShaderValue(shader, location, value, type);
    }
}

static void draw_transient_effects(EikRenderer *renderer, Vector2 camera)
{
    EikEffects *effects = &renderer->effects;
    const float size[] = { 128.0F, 128.0F };

    if (effects->shockwave.active && effects->shockwave_shader.id != 0U) {
        const float progress = effects->shockwave.elapsed / effects->shockwave.duration;
        const float center[] = { 0.5F, 0.5F };

        set_uniform(effects->shockwave_shader, effects->shockwave_size, size, SHADER_UNIFORM_VEC2);
        set_uniform(effects->shockwave_shader, effects->shockwave_center, center, SHADER_UNIFORM_VEC2);
        set_uniform(effects->shockwave_shader, effects->shockwave_time, &effects->time, SHADER_UNIFORM_FLOAT);
        set_uniform(effects->shockwave_shader, effects->shockwave_progress, &progress, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(effects->shockwave_shader);
        DrawRectangle((int)(effects->shockwave.centre.x - camera.x - 64.0F),
            (int)(effects->shockwave.centre.y - camera.y - 64.0F), 128, 128, WHITE);
        EndShaderMode();
    }
    if (effects->explosion.active && effects->explosion_shader.id != 0U) {
        const float progress = effects->explosion.elapsed / effects->explosion.duration;

        set_uniform(effects->explosion_shader, effects->explosion_time, &effects->time, SHADER_UNIFORM_FLOAT);
        set_uniform(effects->explosion_shader, effects->explosion_progress, &progress, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(effects->explosion_shader);
        DrawRectangle((int)(effects->explosion.centre.x - camera.x - 32.0F),
            (int)(effects->explosion.centre.y - camera.y - 32.0F), 64, 64, WHITE);
        EndShaderMode();
    }
}

static void draw_world_effects(EikRenderer *renderer, const EikItemWorld *items, Vector2 camera)
{
    draw_torch_glows(renderer, items, camera);
    draw_fireflies(&renderer->effects, camera);
    draw_particles(&renderer->effects, camera);
    draw_transient_effects(renderer, camera);
}

void eik_renderer_draw(EikRenderer *renderer, const EikLevelState *level,
    const EikPlayer *player, const EikEnemyWorld *enemies, const EikItemWorld *items,
    Texture2D player_texture,
    bool show_collision)
{
    const float scale_x = (float)GetScreenWidth() / EIK_LOGICAL_WIDTH;
    const float scale_y = (float)GetScreenHeight() / EIK_LOGICAL_HEIGHT;
    const float scale = scale_x < scale_y ? scale_x : scale_y;
    const float view_width = EIK_LOGICAL_WIDTH * scale;
    const float view_height = EIK_LOGICAL_HEIGHT * scale;
    const float view_x = ((float)GetScreenWidth() - view_width) * 0.5F;
    const float view_y = ((float)GetScreenHeight() - view_height) * 0.5F;
    int sky_x = (int)renderer->sky_scroll;

    BeginTextureMode(renderer->world_target);
    ClearBackground((Color){ 19, 30, 54, 255 });
    DrawTexture(renderer->sky, sky_x, 0, WHITE);
    DrawTexture(renderer->sky, sky_x + renderer->sky.width, 0, WHITE);
    draw_tiles(renderer, &level->map, renderer->camera_top_left);
    draw_items(items, renderer->camera_top_left, show_collision);
    draw_world_effects(renderer, items, renderer->camera_top_left);
    draw_enemies(renderer, enemies, renderer->camera_top_left);
    if (player != NULL && player_texture.id != 0U && level->has_player) {
        Rectangle source = eik_player_frame_rect(&player->animation);
        const Rectangle destination = { player->position.x - renderer->camera_top_left.x,
            player->position.y - renderer->camera_top_left.y, 48.0F, 48.0F };

        if (!player->facing_right) {
            source.x += source.width;
            source.width = -source.width;
        }
        DrawTexturePro(player_texture, source, destination, (Vector2){ 0.0F, 0.0F },
            0.0F, WHITE);
    }
    if (show_collision) {
        draw_collision_overlay(level, renderer->camera_top_left);
    }
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    if (renderer->effects.screen_shader.id != 0U) {
        EikEffects *effects = &renderer->effects;
        const float ripple_progress = effects->ripple.active
            ? effects->ripple.elapsed / effects->ripple.duration : 0.0F;
        const float ripple_center[] = { (effects->ripple.centre.x - renderer->camera_top_left.x)
                / EIK_LOGICAL_WIDTH,
            1.0F - (effects->ripple.centre.y - renderer->camera_top_left.y) / EIK_LOGICAL_HEIGHT };
        const float aspect = (float)EIK_LOGICAL_WIDTH / EIK_LOGICAL_HEIGHT;
        const float chroma_intensity = effects->chroma_elapsed > 0.0F ? 1.0F : 0.0F;
        const float chroma_shift = effects->chroma_shift;

        set_uniform(effects->screen_shader, effects->screen_ripple_center, ripple_center,
            SHADER_UNIFORM_VEC2);
        set_uniform(effects->screen_shader, effects->screen_ripple_progress, &ripple_progress,
            SHADER_UNIFORM_FLOAT);
        set_uniform(effects->screen_shader, effects->screen_time, &effects->time, SHADER_UNIFORM_FLOAT);
        set_uniform(effects->screen_shader, effects->screen_aspect, &aspect, SHADER_UNIFORM_FLOAT);
        set_uniform(effects->screen_shader, effects->screen_chroma_intensity, &chroma_intensity,
            SHADER_UNIFORM_FLOAT);
        set_uniform(effects->screen_shader, effects->screen_chroma_shift, &chroma_shift,
            SHADER_UNIFORM_FLOAT);
        BeginShaderMode(effects->screen_shader);
    }
    DrawTexturePro(renderer->world_target.texture,
        (Rectangle){ 0.0F, 0.0F, (float)renderer->world_target.texture.width,
            -(float)renderer->world_target.texture.height },
        (Rectangle){ view_x, view_y, view_width, view_height }, (Vector2){ 0.0F, 0.0F },
        0.0F, WHITE);
    if (renderer->effects.screen_shader.id != 0U) {
        EndShaderMode();
    }
    DrawText(level->index == 0U ? "forest-1" : "forest", (int)view_x + 12,
        (int)view_y + 12, 12, RAYWHITE);
    if (show_collision) {
        DrawText("F1 collision boxes", (int)view_x + 12, (int)view_y + 28, 12, GREEN);
    }
}
