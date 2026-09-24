#ifndef EIK_RENDER_H
#define EIK_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

#include "mod_level.h"
#include "mod_enemy.h"
#include "mod_player.h"

typedef struct EikRenderer {
    Texture2D tileset;
    Texture2D sky;
    Texture2D bat;
    Texture2D yellow_mob;
    Texture2D red_mob;
    RenderTexture2D world_target;
    Vector2 camera_top_left;
    float sky_scroll;
    bool camera_placed;
    bool initialized;
} EikRenderer;

bool eik_renderer_init(EikRenderer *renderer, const char *tileset_path, const char *sky_path,
    char *error, size_t error_size);
bool eik_renderer_load_enemy_textures(EikRenderer *renderer, const char *bat_path,
    const char *yellow_mob_path, const char *red_mob_path, char *error, size_t error_size);
void eik_renderer_unload(EikRenderer *renderer);
void eik_renderer_snap_camera(EikRenderer *renderer);
void eik_renderer_update_camera(EikRenderer *renderer, const EikLevelState *level, float dt);
void eik_renderer_draw(EikRenderer *renderer, const EikLevelState *level,
    const EikPlayer *player, const EikEnemyWorld *enemies, Texture2D player_texture,
    bool show_collision);

#endif
