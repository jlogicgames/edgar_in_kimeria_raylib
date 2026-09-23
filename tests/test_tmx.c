#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tmx.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (false)

static bool nearly_equal(float left, float right)
{
    const float difference = left - right;
    return difference < 0.001F && difference > -0.001F;
}

static bool load_map(const char *directory, const char *name, EikTmxMap *map)
{
    char path[512];
    char error[256];

    (void)snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (!eik_tmx_load(path, map, error, sizeof(error))) {
        (void)fprintf(stderr, "%s\n", error);
        return false;
    }
    return true;
}

static bool forest_one_has_expected_objects(const char *directory)
{
    EikTmxMap map;

    CHECK(load_map(directory, "forest-1.tmx", &map));
    CHECK(map.width == 40U);
    CHECK(map.height == 23U);
    CHECK(map.tile_count == 920U);
    CHECK(map.tileset_columns == 20U);
    CHECK(strcmp(map.tileset_source, "Forest.tsx") == 0);
    CHECK(map.object_count == 20U);
    CHECK(strcmp(map.objects[0].class_name, "Player") == 0);
    CHECK(nearly_equal(map.objects[0].x, 32.0F));
    CHECK(strcmp(map.objects[3].actionable_type, "Torch") == 0);
    CHECK(nearly_equal(map.objects[3].intensity, 100.0F));
    CHECK(strcmp(map.objects[6].actionable_type, "Wall") == 0);
    CHECK(map.objects[8].is_vertical);
    CHECK(nearly_equal(map.objects[8].off_neg, 2.0F));
    CHECK(strcmp(map.objects[13].layer, "Collisions") == 0);
    CHECK(strcmp(map.objects[13].class_name, "") == 0);
    CHECK(strcmp(map.objects[17].class_name, "Wall") == 0);
    eik_tmx_unload(&map);
    return true;
}

static bool forest_has_expected_objects_and_spawn_order(const char *directory)
{
    EikTmxMap map;

    CHECK(load_map(directory, "forest.tmx", &map));
    CHECK(map.width == 80U);
    CHECK(map.height == 23U);
    CHECK(map.tile_count == 1840U);
    CHECK(map.object_count == 27U);
    CHECK(strcmp(map.objects[0].class_name, "Player") == 0);
    CHECK(nearly_equal(map.objects[0].x, 65.006F));
    CHECK(nearly_equal(map.objects[0].y, 270.811F));
    CHECK(strcmp(map.objects[1].class_name, "YellowMob") == 0);
    CHECK(strcmp(map.objects[2].name, "Coin") == 0);
    CHECK(strcmp(map.objects[14].layer, "Collisions") == 0);
    CHECK(strcmp(map.objects[22].class_name, "QuickSand") == 0);
    eik_tmx_unload(&map);
    return true;
}

static bool object_group_offsets_are_honoured(const char *fixture_path)
{
    EikTmxMap map;

    {
        char error[256];

        CHECK(eik_tmx_load(fixture_path, &map, error, sizeof(error)));
    }
    CHECK(map.object_count == 1U);
    CHECK(nearly_equal(map.objects[0].x, 11.0F));
    CHECK(nearly_equal(map.objects[0].y, -18.0F));
    eik_tmx_unload(&map);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        (void)fprintf(stderr, "usage: test_tmx <tiles-directory> <offset-fixture>\n");
        return 2;
    }
    if (!forest_one_has_expected_objects(argv[1])
            || !forest_has_expected_objects_and_spawn_order(argv[1])
            || !object_group_offsets_are_honoured(argv[2])) {
        return 1;
    }
    return 0;
}
