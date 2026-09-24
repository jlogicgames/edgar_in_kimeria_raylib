#include <stdbool.h>
#include <stdio.h>

#include "l10n.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        return false; \
    } \
} while (0)

static bool every_message_is_present_in_every_language(void)
{
    EikLanguage language = EIK_LANGUAGE_ENGLISH;
    EikMsg message = EIK_MSG_TITLE;

    CHECK(eik_l10n_message_count() == EIK_MSG_COUNT);
    for (language = EIK_LANGUAGE_ENGLISH; language < EIK_LANGUAGE_COUNT; ++language) {
        CHECK(eik_language_native_name(language)[0] != '\0');
        for (message = EIK_MSG_TITLE; message < EIK_MSG_COUNT; ++message) {
            CHECK(eik_l10n(language, message)[0] != '\0');
        }
    }
    return true;
}

int main(void)
{
    return every_message_is_present_in_every_language() ? 0 : 1;
}
