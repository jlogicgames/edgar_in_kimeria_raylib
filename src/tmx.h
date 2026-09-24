#ifndef EIK_TMX_H
#define EIK_TMX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct EikTmxObject {
    char layer[32];
    char name[32];
    char class_name[32];
    char actionable_type[32];
    float x;
    float y;
    float width;
    float height;
    float off_neg;
    float off_pos;
    float intensity;
    bool is_vertical;
} EikTmxObject;

typedef struct EikTmxMap {
    uint32_t width;
    uint32_t height;
    uint32_t tile_width;
    uint32_t tile_height;
    uint32_t tileset_first_gid;
    uint32_t tileset_columns;
    char tileset_source[128];
    uint32_t *tiles;
    size_t tile_count;
    EikTmxObject *objects;
    size_t object_count;
} EikTmxMap;

bool eik_tmx_load(const char *path, EikTmxMap *map, char *error, size_t error_size);
void eik_tmx_unload(EikTmxMap *map);

#endif
