#ifndef EIK_L10N_H
#define EIK_L10N_H

#include <stddef.h>

typedef enum EikLanguage {
    EIK_LANGUAGE_ENGLISH,
    EIK_LANGUAGE_UKRAINIAN,
    EIK_LANGUAGE_COUNT,
} EikLanguage;

typedef enum EikMsg {
    EIK_MSG_TITLE,
    EIK_MSG_PLAY,
    EIK_MSG_ABOUT,
    EIK_MSG_OPTIONS,
    EIK_MSG_EXIT,
    EIK_MSG_BACK,
    EIK_MSG_RESUME,
    EIK_MSG_EXIT_TO_MENU,
    EIK_MSG_PLAY_AGAIN,
    EIK_MSG_PAUSE_MENU,
    EIK_MSG_GAME_OVER,
    EIK_MSG_LANGUAGE,
    EIK_MSG_CONTROLS,
    EIK_MSG_TOUCH_CONTROLS,
    EIK_MSG_MENU_HINT,
    EIK_MSG_ABOUT_BODY,
    EIK_MSG_ABOUT_BODY_TOUCH,
    EIK_MSG_LIVES,
    EIK_MSG_DISPLAY_FULLSCREEN,
    EIK_MSG_DISPLAY_WINDOWED,
    EIK_MSG_COUNT,
} EikMsg;

const char *eik_l10n(EikLanguage language, EikMsg message);
const char *eik_language_native_name(EikLanguage language);
size_t eik_l10n_message_count(void);

#endif
