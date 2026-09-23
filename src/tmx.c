#include "tmx.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "yxml.h"

enum {
    EIK_TMX_XML_STACK_SIZE = 1024,
    EIK_TMX_TAG_DEPTH = 16,
    EIK_TMX_TAG_SIZE = 32,
    EIK_TMX_VALUE_SIZE = 256,
};

typedef struct EikTmxParser {
    EikTmxMap *map;
    char *error;
    size_t error_size;
    char tags[EIK_TMX_TAG_DEPTH][EIK_TMX_TAG_SIZE];
    size_t depth;
    char attribute[EIK_TMX_TAG_SIZE];
    char property_name[32];
    char value[EIK_TMX_VALUE_SIZE];
    size_t value_length;
    char tile_number[16];
    size_t tile_number_length;
    bool reading_attribute;
    char group_name[32];
    float group_offset_x;
    float group_offset_y;
    bool reading_tiles;
    bool csv_encoding;
    bool failed;
} EikTmxParser;

static void set_error(EikTmxParser *parser, const char *format, ...)
{
    va_list arguments;

    if (parser->failed) {
        return;
    }
    parser->failed = true;
    va_start(arguments, format);
    (void)vsnprintf(parser->error, parser->error_size, format, arguments);
    va_end(arguments);
}

static void copy_string(char *destination, size_t destination_size, const char *source)
{
    (void)snprintf(destination, destination_size, "%s", source);
}

static bool parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0UL;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_float(const char *text, float *value)
{
    char *end = NULL;
    float parsed = 0.0F;

    errno = 0;
    parsed = strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    *value = parsed;
    return true;
}

static const char *current_tag(const EikTmxParser *parser)
{
    return parser->depth == 0U ? "" : parser->tags[parser->depth - 1U];
}

static EikTmxObject *current_object(EikTmxParser *parser)
{
    if (parser->map->object_count == 0U) {
        return NULL;
    }
    return &parser->map->objects[parser->map->object_count - 1U];
}

static bool append_object(EikTmxParser *parser)
{
    EikTmxMap *map = parser->map;
    EikTmxObject *objects = realloc(map->objects,
        (map->object_count + 1U) * sizeof(*map->objects));

    if (objects == NULL) {
        set_error(parser, "out of memory while reading TMX objects");
        return false;
    }
    map->objects = objects;
    map->objects[map->object_count] = (EikTmxObject){ 0 };
    copy_string(map->objects[map->object_count].layer,
        sizeof(map->objects[map->object_count].layer), parser->group_name);
    map->object_count++;
    return true;
}

static bool append_tile(EikTmxParser *parser, uint32_t tile)
{
    EikTmxMap *map = parser->map;
    uint32_t *tiles = realloc(map->tiles, (map->tile_count + 1U) * sizeof(*map->tiles));

    if (tiles == NULL) {
        set_error(parser, "out of memory while reading TMX tiles");
        return false;
    }
    map->tiles = tiles;
    map->tiles[map->tile_count] = tile;
    map->tile_count++;
    return true;
}

static void apply_attribute(EikTmxParser *parser)
{
    EikTmxMap *map = parser->map;
    EikTmxObject *object = current_object(parser);
    const char *tag = current_tag(parser);
    const char *key = parser->attribute;
    const char *value = parser->value;
    float parsed_float = 0.0F;
    uint32_t parsed_u32 = 0U;

    if (strcmp(tag, "map") == 0) {
        if (strcmp(key, "width") == 0 && parse_u32(value, &parsed_u32)) {
            map->width = parsed_u32;
        } else if (strcmp(key, "height") == 0 && parse_u32(value, &parsed_u32)) {
            map->height = parsed_u32;
        } else if (strcmp(key, "tilewidth") == 0 && parse_u32(value, &parsed_u32)) {
            map->tile_width = parsed_u32;
        } else if (strcmp(key, "tileheight") == 0 && parse_u32(value, &parsed_u32)) {
            map->tile_height = parsed_u32;
        }
    } else if (strcmp(tag, "tileset") == 0) {
        if (strcmp(key, "firstgid") == 0 && parse_u32(value, &parsed_u32)) {
            map->tileset_first_gid = parsed_u32;
        } else if (strcmp(key, "source") == 0) {
            copy_string(map->tileset_source, sizeof(map->tileset_source), value);
        }
    } else if (strcmp(tag, "objectgroup") == 0) {
        if (strcmp(key, "name") == 0) {
            copy_string(parser->group_name, sizeof(parser->group_name), value);
        } else if (strcmp(key, "offsetx") == 0 && parse_float(value, &parsed_float)) {
            parser->group_offset_x = parsed_float;
        } else if (strcmp(key, "offsety") == 0 && parse_float(value, &parsed_float)) {
            parser->group_offset_y = parsed_float;
        }
    } else if (strcmp(tag, "object") == 0 && object != NULL) {
        if (strcmp(key, "name") == 0) {
            copy_string(object->name, sizeof(object->name), value);
        } else if (strcmp(key, "type") == 0) {
            copy_string(object->class_name, sizeof(object->class_name), value);
        } else if (strcmp(key, "x") == 0 && parse_float(value, &parsed_float)) {
            object->x = parsed_float;
        } else if (strcmp(key, "y") == 0 && parse_float(value, &parsed_float)) {
            object->y = parsed_float;
        } else if (strcmp(key, "width") == 0 && parse_float(value, &parsed_float)) {
            object->width = parsed_float;
        } else if (strcmp(key, "height") == 0 && parse_float(value, &parsed_float)) {
            object->height = parsed_float;
        }
    } else if (strcmp(tag, "property") == 0 && object != NULL && strcmp(key, "value") == 0) {
        if (strcmp(parser->attribute, "name") == 0) {
            return;
        }
    }

    if (strcmp(tag, "property") == 0 && object != NULL) {
        if (strcmp(key, "name") == 0) {
            copy_string(parser->property_name, sizeof(parser->property_name), value);
        } else if (strcmp(key, "value") == 0) {
            if (strcmp(parser->property_name, "offNeg") == 0 && parse_float(value, &parsed_float)) {
                object->off_neg = parsed_float;
            } else if (strcmp(parser->property_name, "offPos") == 0 && parse_float(value, &parsed_float)) {
                object->off_pos = parsed_float;
            } else if (strcmp(parser->property_name, "Intensity") == 0 && parse_float(value, &parsed_float)) {
                object->intensity = parsed_float;
            } else if (strcmp(parser->property_name, "isVertical") == 0) {
                object->is_vertical = strcmp(value, "true") == 0;
            } else if (strcmp(parser->property_name, "type") == 0) {
                copy_string(object->actionable_type, sizeof(object->actionable_type), value);
            }
        }
    } else if (strcmp(tag, "data") == 0 && strcmp(key, "encoding") == 0) {
        parser->csv_encoding = strcmp(value, "csv") == 0;
        if (!parser->csv_encoding) {
            set_error(parser, "TMX data encoding must be CSV, got '%s'", value);
        }
    }
}

static void append_csv_character(EikTmxParser *parser, char character)
{
    uint32_t tile = 0U;

    if (!parser->reading_tiles || parser->failed) {
        return;
    }
    if (character >= '0' && character <= '9') {
        if (parser->tile_number_length + 1U >= sizeof(parser->tile_number)) {
            set_error(parser, "TMX tile id is too long");
            return;
        }
        parser->tile_number[parser->tile_number_length++] = character;
        return;
    }
    if (character == ',' || character == '\n' || character == '\r' || character == ' ' || character == '\t') {
        if (parser->tile_number_length != 0U) {
            parser->tile_number[parser->tile_number_length] = '\0';
            if (!parse_u32(parser->tile_number, &tile)) {
                set_error(parser, "invalid TMX tile id '%s'", parser->tile_number);
            } else {
                (void)append_tile(parser, tile);
            }
            parser->tile_number_length = 0U;
        }
        return;
    }
    set_error(parser, "invalid character in TMX CSV data");
}

bool eik_tmx_load(const char *path, EikTmxMap *map, char *error, size_t error_size)
{
    FILE *file = NULL;
    yxml_t xml;
    char xml_stack[EIK_TMX_XML_STACK_SIZE];
    EikTmxParser parser = { .map = map, .error = error, .error_size = error_size };
    int character = 0;

    if (path == NULL || map == NULL || error == NULL || error_size == 0U) {
        return false;
    }
    *map = (EikTmxMap){ 0 };
    error[0] = '\0';
    file = fopen(path, "rb");
    if (file == NULL) {
        (void)snprintf(error, error_size, "cannot open TMX '%s'", path);
        return false;
    }
    yxml_init(&xml, xml_stack, sizeof(xml_stack));
    while (!parser.failed && (character = fgetc(file)) != EOF) {
        const yxml_ret_t token = yxml_parse(&xml, character);

        if (token < YXML_OK) {
            set_error(&parser, "invalid TMX XML at line %u", xml.line);
        } else if (token == YXML_ELEMSTART) {
            if (parser.depth == EIK_TMX_TAG_DEPTH) {
                set_error(&parser, "TMX nesting is too deep");
            } else {
                copy_string(parser.tags[parser.depth], EIK_TMX_TAG_SIZE, xml.elem);
                parser.depth++;
                if (strcmp(current_tag(&parser), "objectgroup") == 0) {
                    parser.group_name[0] = '\0';
                    parser.group_offset_x = 0.0F;
                    parser.group_offset_y = 0.0F;
                } else if (strcmp(current_tag(&parser), "object") == 0) {
                    (void)append_object(&parser);
                } else if (strcmp(current_tag(&parser), "data") == 0) {
                    parser.csv_encoding = false;
                    parser.tile_number_length = 0U;
                }
            }
        } else if (token == YXML_ATTRSTART) {
            copy_string(parser.attribute, sizeof(parser.attribute), xml.attr);
            parser.value_length = 0U;
            parser.value[0] = '\0';
            parser.reading_attribute = true;
        } else if (token == YXML_DATA) {
            if (parser.reading_attribute && parser.value_length + 1U < sizeof(parser.value)) {
                parser.value[parser.value_length++] = xml.data;
                parser.value[parser.value_length] = '\0';
            } else if (parser.reading_attribute) {
                set_error(&parser, "TMX attribute value is too long");
            }
            append_csv_character(&parser, xml.data);
        } else if (token == YXML_ATTREND) {
            apply_attribute(&parser);
            parser.reading_attribute = false;
        } else if (token == YXML_CONTENT && strcmp(current_tag(&parser), "data") == 0) {
            parser.reading_tiles = parser.csv_encoding;
        } else if (token == YXML_ELEMEND) {
            if (strcmp(current_tag(&parser), "data") == 0) {
                append_csv_character(&parser, ',');
                parser.reading_tiles = false;
            } else if (strcmp(current_tag(&parser), "object") == 0) {
                EikTmxObject *object = current_object(&parser);
                if (object != NULL) {
                    object->x += parser.group_offset_x;
                    object->y += parser.group_offset_y;
                }
            }
            if (parser.depth != 0U) {
                parser.depth--;
            }
        }
    }
    if (!parser.failed && yxml_eof(&xml) < YXML_OK) {
        set_error(&parser, "unexpected end of TMX XML");
    }
    (void)fclose(file);
    if (!parser.failed && (map->width == 0U || map->height == 0U || map->tile_width == 0U
            || map->tile_height == 0U || map->tileset_first_gid == 0U
            || strcmp(map->tileset_source, "Forest.tsx") != 0
            || map->tile_count != (size_t)map->width * map->height)) {
        set_error(&parser, "TMX is missing the expected Forest CSV tile layer");
    }
    if (parser.failed) {
        eik_tmx_unload(map);
        return false;
    }
    map->tileset_columns = 20U;
    return true;
}

void eik_tmx_unload(EikTmxMap *map)
{
    if (map == NULL) {
        return;
    }
    free(map->tiles);
    free(map->objects);
    *map = (EikTmxMap){ 0 };
}
