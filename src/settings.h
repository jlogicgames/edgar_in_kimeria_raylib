#ifndef EIK_SETTINGS_H
#define EIK_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

#include "l10n.h"

#define EIK_SETTINGS_PATH_MAX 1024U

typedef enum EikDisplayMode {
    EIK_DISPLAY_FULLSCREEN,
    EIK_DISPLAY_WINDOWED,
} EikDisplayMode;

typedef struct EikSettings {
    EikDisplayMode display;
    EikLanguage language;
    char path[EIK_SETTINGS_PATH_MAX];
    bool persistent;
} EikSettings;

void eik_settings_defaults(EikSettings *settings);
bool eik_settings_parse(EikSettings *settings, const char *contents);
void eik_settings_load(EikSettings *settings, const char *home_directory);
bool eik_settings_save(const EikSettings *settings);

#endif
