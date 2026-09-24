#include "settings.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static bool make_directory(const char *path)
{
    if (mkdir(path, 0700) == 0 || errno == EEXIST) {
        return true;
    }
    return false;
}

void eik_settings_defaults(EikSettings *settings)
{
    *settings = (EikSettings){
        .display = EIK_DISPLAY_FULLSCREEN,
        .language = EIK_LANGUAGE_ENGLISH,
    };
}

bool eik_settings_parse(EikSettings *settings, const char *contents)
{
    char display[16] = "";
    char language[8] = "";
    char expected[64];
    int matched = 0;

    if (contents == NULL) {
        return false;
    }
    matched = sscanf(contents, "display=%15[^\n]\nlanguage=%7[^\n]\n", display, language);
    if (matched != 2 || (strcmp(display, "fullscreen") != 0 && strcmp(display, "windowed") != 0)
            || (strcmp(language, "en") != 0 && strcmp(language, "uk") != 0)) {
        return false;
    }
    if (snprintf(expected, sizeof(expected), "display=%s\nlanguage=%s\n", display, language)
            >= (int)sizeof(expected) || strcmp(contents, expected) != 0) {
        return false;
    }
    settings->display = strcmp(display, "windowed") == 0
        ? EIK_DISPLAY_WINDOWED : EIK_DISPLAY_FULLSCREEN;
    settings->language = strcmp(language, "uk") == 0
        ? EIK_LANGUAGE_UKRAINIAN : EIK_LANGUAGE_ENGLISH;
    return true;
}

void eik_settings_load(EikSettings *settings, const char *home_directory)
{
    FILE *file = NULL;
    char contents[128] = "";
    size_t read_count = 0U;

    eik_settings_defaults(settings);
#if defined(PLATFORM_WEB)
    (void)home_directory;
    settings->language = EM_ASM_INT({
        return localStorage.getItem("edgard_in_kimeria.language") === "uk" ? 1 : 0;
    }) == 1 ? EIK_LANGUAGE_UKRAINIAN : EIK_LANGUAGE_ENGLISH;
    return;
#else
    if (home_directory == NULL || home_directory[0] == '\0'
            || snprintf(settings->path, sizeof(settings->path),
                "%s/Library/Application Support/edgard_in_kimeria/settings.ini", home_directory)
                >= (int)sizeof(settings->path)) {
        return;
    }
    settings->persistent = true;
    file = fopen(settings->path, "r");
    if (file == NULL) {
        return;
    }
    read_count = fread(contents, 1U, sizeof(contents) - 1U, file);
    (void)fclose(file);
    contents[read_count] = '\0';
    if (!eik_settings_parse(settings, contents)) {
        eik_settings_defaults(settings);
        (void)snprintf(settings->path, sizeof(settings->path),
            "%s/Library/Application Support/edgard_in_kimeria/settings.ini", home_directory);
        settings->persistent = true;
    }
#endif
}

bool eik_settings_save(const EikSettings *settings)
{
#if defined(PLATFORM_WEB)
    EM_ASM({
        localStorage.setItem("edgard_in_kimeria.language", $0 ? "uk" : "en");
    }, settings->language == EIK_LANGUAGE_UKRAINIAN ? 1 : 0);
    return true;
#else
    char library[EIK_SETTINGS_PATH_MAX];
    char support[EIK_SETTINGS_PATH_MAX];
    const char *marker = "/edgard_in_kimeria/settings.ini";
    char *game_directory = NULL;
    FILE *file = NULL;

    if (!settings->persistent || settings->path[0] == '\0'
            || strlen(settings->path) >= sizeof(library)) {
        return false;
    }
    (void)strncpy(library, settings->path, sizeof(library) - 1U);
    library[sizeof(library) - 1U] = '\0';
    game_directory = strstr(library, marker);
    if (game_directory == NULL) {
        return false;
    }
    *game_directory = '\0';
    if (snprintf(support, sizeof(support), "%s/Library/Application Support", library)
            >= (int)sizeof(support) || !make_directory(library)) {
        return false;
    }
    {
        char library_directory[EIK_SETTINGS_PATH_MAX];

        if (snprintf(library_directory, sizeof(library_directory), "%s/Library", library)
                >= (int)sizeof(library_directory) || !make_directory(library_directory)
                || !make_directory(support)) {
            return false;
        }
    }
    {
        char game_directory_path[EIK_SETTINGS_PATH_MAX];

        if (snprintf(game_directory_path, sizeof(game_directory_path),
                "%s/edgard_in_kimeria", support) >= (int)sizeof(game_directory_path)
                || !make_directory(game_directory_path)) {
            return false;
        }
    }
    file = fopen(settings->path, "w");
    if (file == NULL) {
        return false;
    }
    if (fprintf(file, "display=%s\nlanguage=%s\n",
            settings->display == EIK_DISPLAY_WINDOWED ? "windowed" : "fullscreen",
            settings->language == EIK_LANGUAGE_UKRAINIAN ? "uk" : "en") < 0) {
        (void)fclose(file);
        return false;
    }
    return fclose(file) == 0;
#endif
}
