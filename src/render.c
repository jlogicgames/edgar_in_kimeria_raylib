#include "render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    EIK_LOGICAL_WIDTH = 640,
    EIK_LOGICAL_HEIGHT = 360,
    EIK_TILE_SIZE = 16,
};

static const float EIK_CAMERA_FOLLOW_SPEED = 500.0F;

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
    DrawTexturePro(renderer->world_target.texture,
        (Rectangle){ 0.0F, 0.0F, (float)renderer->world_target.texture.width,
            -(float)renderer->world_target.texture.height },
        (Rectangle){ view_x, view_y, view_width, view_height }, (Vector2){ 0.0F, 0.0F },
        0.0F, WHITE);
    DrawText(level->index == 0U ? "forest-1" : "forest", (int)view_x + 12,
        (int)view_y + 12, 12, RAYWHITE);
    if (show_collision) {
        DrawText("F1 collision boxes", (int)view_x + 12, (int)view_y + 28, 12, GREEN);
    }
    EndDrawing();
}
