#ifndef EIK_UI_H
#define EIK_UI_H

#include <stdbool.h>
#include <stddef.h>

#include "l10n.h"
#include "raylib.h"

typedef enum EikAppState {
    EIK_APP_WEB_START,
    EIK_APP_MAIN_MENU,
    EIK_APP_ABOUT,
    EIK_APP_OPTIONS,
    EIK_APP_PLAYING,
    EIK_APP_PAUSED,
    EIK_APP_GAME_OVER,
} EikAppState;

typedef enum EikUiAction {
    EIK_UI_ACTION_NONE,
    EIK_UI_ACTION_PLAY,
    EIK_UI_ACTION_RESUME,
    EIK_UI_ACTION_EXIT_TO_MENU,
    EIK_UI_ACTION_QUIT,
    EIK_UI_ACTION_START_WEB,
    EIK_UI_ACTION_LANGUAGE_CHANGED,
    EIK_UI_ACTION_TOGGLE_DISPLAY,
} EikUiAction;

typedef struct EikUi {
    Font text_font;
    Font button_font;
    Texture2D items_texture;
    EikLanguage language;
    EikAppState state;
    int focus;
    float entered_at;
    bool fullscreen;
    bool initialized;
} EikUi;

bool eik_ui_init(EikUi *ui, const char *text_font_path, const char *button_font_path,
    const char *items_path, char *error, size_t error_size);
void eik_ui_unload(EikUi *ui);
void eik_ui_set_state(EikUi *ui, EikAppState state);
EikUiAction eik_ui_update(EikUi *ui, float real_dt);
void eik_ui_draw(const EikUi *ui, unsigned int coins, int lives);

#endif
