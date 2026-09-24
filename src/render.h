#ifndef EIK_RENDER_H
#define EIK_RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"

#include "mod_level.h"
#include "mod_enemy.h"
#include "mod_items.h"
#include "mod_player.h"

#define EIK_MAX_EFFECT_PARTICLES 1024U
#define EIK_MAX_FIREFLIES 24U

typedef struct EikEffectParticle {
    Vector2 from;
    Vector2 to;
    Color color;
    float age;
    float lifespan;
    float start_size;
    float end_size;
    float start_alpha;
    bool active;
    bool glow;
} EikEffectParticle;

typedef struct EikFirefly {
    Vector2 start;
    Vector2 control;
    Vector2 end;
    float timer;
    float duration;
    float hide_duration;
    bool flying;
} EikFirefly;

typedef struct EikTransientEffect {
    Vector2 centre;
    float elapsed;
    float duration;
    bool active;
} EikTransientEffect;

typedef struct EikEffects {
    Texture2D glow;
    Texture2D dot;
    Shader fog_shader;
    Shader shockwave_shader;
    Shader explosion_shader;
    Shader screen_shader;
    int fog_size;
    int fog_time;
    int shockwave_size;
    int shockwave_center;
    int shockwave_time;
    int shockwave_progress;
    int explosion_time;
    int explosion_progress;
    int screen_ripple_center;
    int screen_ripple_progress;
    int screen_time;
    int screen_aspect;
    int screen_chroma_intensity;
    int screen_chroma_shift;
    EikEffectParticle particles[EIK_MAX_EFFECT_PARTICLES];
    EikFirefly fireflies[EIK_MAX_FIREFLIES];
    EikTransientEffect shockwave;
    EikTransientEffect explosion;
    EikTransientEffect ripple;
    float torch_timers[EIK_MAX_ITEMS][4];
    float torch_flickers[EIK_MAX_ITEMS];
    float time;
    float chroma_wait;
    float chroma_elapsed;
    float chroma_duration;
    float chroma_shift;
    uint32_t random_state;
    size_t firefly_level;
    bool initialized;
    bool chroma_enabled;
} EikEffects;

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
    EikEffects effects;
} EikRenderer;

bool eik_renderer_init(EikRenderer *renderer, const char *tileset_path, const char *sky_path,
    char *error, size_t error_size);
bool eik_renderer_load_enemy_textures(EikRenderer *renderer, const char *bat_path,
    const char *yellow_mob_path, const char *red_mob_path, char *error, size_t error_size);
void eik_renderer_load_effects(EikRenderer *renderer, const char *fog_path,
    const char *shockwave_path, const char *explosion_path, const char *screen_path);
void eik_renderer_unload(EikRenderer *renderer);
void eik_renderer_snap_camera(EikRenderer *renderer);
void eik_renderer_update_camera(EikRenderer *renderer, const EikLevelState *level, float dt);
void eik_renderer_update_effects(EikRenderer *renderer, const EikLevelState *level,
    const EikItemWorld *items, float real_dt, bool paused);
void eik_renderer_emit_item_effect(EikRenderer *renderer, EikItemEffectKind kind,
    Vector2 centre);
void eik_renderer_emit_debug_effects(EikRenderer *renderer, Vector2 centre);
void eik_renderer_draw(EikRenderer *renderer, const EikLevelState *level,
    const EikPlayer *player, const EikEnemyWorld *enemies, const EikItemWorld *items,
    Texture2D player_texture,
    bool show_collision, bool paused);

#endif
