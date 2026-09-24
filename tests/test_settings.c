#include <stdio.h>
#include <string.h>

#include "settings.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "check failed: %s\n", #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    EikSettings settings;

    eik_settings_defaults(&settings);
    CHECK(settings.display == EIK_DISPLAY_FULLSCREEN);
    CHECK(settings.language == EIK_LANGUAGE_ENGLISH);
    CHECK(eik_settings_parse(&settings, "display=windowed\nlanguage=uk\n"));
    CHECK(settings.display == EIK_DISPLAY_WINDOWED);
    CHECK(settings.language == EIK_LANGUAGE_UKRAINIAN);
    CHECK(!eik_settings_parse(&settings, "display=exclusive\nlanguage=en\n"));
    CHECK(!eik_settings_parse(&settings, "display=fullscreen\nlanguage=fr\n"));
    CHECK(!eik_settings_parse(&settings, "display=fullscreen\n"));
    CHECK(!eik_settings_parse(&settings, "display=fullscreen\nlanguage=en\nextra"));
    CHECK(strcmp(eik_l10n(settings.language, EIK_MSG_PLAY), "Грати") == 0);
    return 0;
}
