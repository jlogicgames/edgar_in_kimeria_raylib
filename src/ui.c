#include "ui.h"

#include <stdio.h>
#include <string.h>

#define EIK_UI_EASE_RATE 14.0F
#define EIK_UI_APPEAR_DURATION 0.35F

typedef enum EikMenuButton {
    EIK_BUTTON_PLAY,
    EIK_BUTTON_ABOUT,
    EIK_BUTTON_OPTIONS,
    EIK_BUTTON_EXIT,
    EIK_BUTTON_BACK,
    EIK_BUTTON_ENGLISH,
    EIK_BUTTON_UKRAINIAN,
    EIK_BUTTON_RESUME,
    EIK_BUTTON_EXIT_TO_MENU,
    EIK_BUTTON_PLAY_AGAIN,
    EIK_BUTTON_DISPLAY,
} EikMenuButton;

static bool is_menu_state(EikAppState state)
{
    return state == EIK_APP_WEB_START || state == EIK_APP_MAIN_MENU || state == EIK_APP_ABOUT
        || state == EIK_APP_OPTIONS
        || state == EIK_APP_PAUSED || state == EIK_APP_GAME_OVER;
}

static int button_count(EikAppState state)
{
    switch (state) {
        case EIK_APP_WEB_START: return 1;
        case EIK_APP_MAIN_MENU:
#if defined(PLATFORM_WEB) || defined(EIK_IOS)
            return 3;
#else
            return 4;
#endif
        case EIK_APP_OPTIONS:
#if defined(PLATFORM_WEB) || defined(EIK_IOS)
            return 3;
#else
            return 4;
#endif
        case EIK_APP_ABOUT: return 1;
        case EIK_APP_PAUSED: return 2;
        case EIK_APP_GAME_OVER: return 1;
        case EIK_APP_PLAYING: return 0;
    }
    return 0;
}

static EikMenuButton button_at(const EikUi *ui, int index)
{
    static const EikMenuButton main_buttons[] = {
        EIK_BUTTON_PLAY, EIK_BUTTON_ABOUT, EIK_BUTTON_OPTIONS, EIK_BUTTON_EXIT,
    };
    static const EikMenuButton options_buttons[] = {
        EIK_BUTTON_ENGLISH, EIK_BUTTON_UKRAINIAN, EIK_BUTTON_BACK,
    };
    static const EikMenuButton about_buttons[] = { EIK_BUTTON_BACK };
    static const EikMenuButton pause_buttons[] = { EIK_BUTTON_RESUME, EIK_BUTTON_EXIT_TO_MENU };
    static const EikMenuButton game_over_buttons[] = { EIK_BUTTON_PLAY_AGAIN };

    switch (ui->state) {
        case EIK_APP_WEB_START: return EIK_BUTTON_PLAY;
        case EIK_APP_MAIN_MENU: return main_buttons[index];
        case EIK_APP_OPTIONS:
#if defined(PLATFORM_WEB) || defined(EIK_IOS)
            return options_buttons[index];
#else
            return index == 2 ? EIK_BUTTON_DISPLAY : options_buttons[index == 3 ? 2 : index];
#endif
        case EIK_APP_ABOUT: return about_buttons[index];
        case EIK_APP_PAUSED: return pause_buttons[index];
        case EIK_APP_GAME_OVER: return game_over_buttons[index];
        case EIK_APP_PLAYING: break;
    }
    return EIK_BUTTON_BACK;
}

static const char *button_label(const EikUi *ui, EikMenuButton button)
{
    switch (button) {
        case EIK_BUTTON_PLAY: return eik_l10n(ui->language, EIK_MSG_PLAY);
        case EIK_BUTTON_ABOUT: return eik_l10n(ui->language, EIK_MSG_ABOUT);
        case EIK_BUTTON_OPTIONS: return eik_l10n(ui->language, EIK_MSG_OPTIONS);
        case EIK_BUTTON_EXIT: return eik_l10n(ui->language, EIK_MSG_EXIT);
        case EIK_BUTTON_BACK: return eik_l10n(ui->language, EIK_MSG_BACK);
        case EIK_BUTTON_ENGLISH:
            return ui->language == EIK_LANGUAGE_ENGLISH
                ? TextFormat("> %s", eik_language_native_name(EIK_LANGUAGE_ENGLISH))
                : eik_language_native_name(EIK_LANGUAGE_ENGLISH);
        case EIK_BUTTON_UKRAINIAN:
            return ui->language == EIK_LANGUAGE_UKRAINIAN
                ? TextFormat("> %s", eik_language_native_name(EIK_LANGUAGE_UKRAINIAN))
                : eik_language_native_name(EIK_LANGUAGE_UKRAINIAN);
        case EIK_BUTTON_RESUME: return eik_l10n(ui->language, EIK_MSG_RESUME);
        case EIK_BUTTON_EXIT_TO_MENU: return eik_l10n(ui->language, EIK_MSG_EXIT_TO_MENU);
        case EIK_BUTTON_PLAY_AGAIN: return eik_l10n(ui->language, EIK_MSG_PLAY_AGAIN);
        case EIK_BUTTON_DISPLAY:
            return eik_l10n(ui->language, ui->fullscreen
                ? EIK_MSG_DISPLAY_FULLSCREEN : EIK_MSG_DISPLAY_WINDOWED);
    }
    return "";
}

static Rectangle button_bounds(int screen_width, int screen_height, int index, int count)
{
    const float width = 220.0F;
    const float height = 60.0F;
    const float gap = 14.0F;
    const float total_height = (float)count * height + (float)(count - 1) * gap;
    const float first_y = (float)screen_height * 0.5F - total_height * 0.5F + 35.0F;

    return (Rectangle){ ((float)screen_width - width) * 0.5F,
        first_y + (float)index * (height + gap), width, height };
}

static bool gamepad_pressed(int button)
{
    int index = 0;

    for (index = 0; index < 4; ++index) {
        if (IsGamepadAvailable(index) && IsGamepadButtonPressed(index, button)) {
            return true;
        }
    }
    return false;
}

static bool confirm_pressed(void)
{
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)
        || gamepad_pressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)
        || gamepad_pressed(GAMEPAD_BUTTON_MIDDLE_RIGHT);
}

static bool back_pressed(void)
{
    return IsKeyPressed(KEY_ESCAPE) || gamepad_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)
        || gamepad_pressed(GAMEPAD_BUTTON_MIDDLE_LEFT);
}

static EikUiAction activate(EikUi *ui, EikMenuButton button)
{
    switch (button) {
        case EIK_BUTTON_PLAY:
            return ui->state == EIK_APP_WEB_START ? EIK_UI_ACTION_START_WEB : EIK_UI_ACTION_PLAY;
        case EIK_BUTTON_PLAY_AGAIN: return EIK_UI_ACTION_PLAY;
        case EIK_BUTTON_ABOUT:
            eik_ui_set_state(ui, EIK_APP_ABOUT);
            return EIK_UI_ACTION_NONE;
        case EIK_BUTTON_OPTIONS:
            eik_ui_set_state(ui, EIK_APP_OPTIONS);
            return EIK_UI_ACTION_NONE;
        case EIK_BUTTON_EXIT: return EIK_UI_ACTION_QUIT;
        case EIK_BUTTON_BACK:
            eik_ui_set_state(ui, EIK_APP_MAIN_MENU);
            return EIK_UI_ACTION_NONE;
        case EIK_BUTTON_ENGLISH:
            ui->language = EIK_LANGUAGE_ENGLISH;
            eik_ui_set_state(ui, EIK_APP_MAIN_MENU);
            return EIK_UI_ACTION_LANGUAGE_CHANGED;
        case EIK_BUTTON_UKRAINIAN:
            ui->language = EIK_LANGUAGE_UKRAINIAN;
            eik_ui_set_state(ui, EIK_APP_MAIN_MENU);
            return EIK_UI_ACTION_LANGUAGE_CHANGED;
        case EIK_BUTTON_RESUME: return EIK_UI_ACTION_RESUME;
        case EIK_BUTTON_EXIT_TO_MENU: return EIK_UI_ACTION_EXIT_TO_MENU;
        case EIK_BUTTON_DISPLAY:
            ui->fullscreen = !ui->fullscreen;
            return EIK_UI_ACTION_TOGGLE_DISPLAY;
    }
    return EIK_UI_ACTION_NONE;
}

bool eik_ui_init(EikUi *ui, const char *text_font_path, const char *button_font_path,
    const char *items_path, char *error, size_t error_size)
{
    char text[4096] = "";
    size_t text_length = 0U;
    EikLanguage language = EIK_LANGUAGE_ENGLISH;
    EikMsg message = EIK_MSG_TITLE;
    int codepoint_count = 0;
    int *codepoints = NULL;

    *ui = (EikUi){ .language = EIK_LANGUAGE_ENGLISH, .state = EIK_APP_MAIN_MENU,
        .fullscreen = true };
    for (language = EIK_LANGUAGE_ENGLISH; language < EIK_LANGUAGE_COUNT; ++language) {
        for (message = EIK_MSG_TITLE; message < EIK_MSG_COUNT; ++message) {
            const int written = snprintf(text + text_length, sizeof(text) - text_length, "%s ",
                eik_l10n(language, message));

            if (written < 0 || (size_t)written >= sizeof(text) - text_length) {
                (void)snprintf(error, error_size, "UI message table exceeds font buffer");
                return false;
            }
            text_length += (size_t)written;
        }
        {
            const int written = snprintf(text + text_length, sizeof(text) - text_length, "%s ",
                eik_language_native_name(language));

            if (written < 0 || (size_t)written >= sizeof(text) - text_length) {
                (void)snprintf(error, error_size, "UI language names exceed font buffer");
                return false;
            }
            text_length += (size_t)written;
        }
    }
    codepoints = LoadCodepoints(text, &codepoint_count);
    if (codepoints == NULL || codepoint_count == 0) {
        (void)snprintf(error, error_size, "cannot collect UI font codepoints");
        return false;
    }
    ui->text_font = LoadFontEx(text_font_path, 96, codepoints, codepoint_count);
    ui->button_font = LoadFontEx(button_font_path, 96, codepoints, codepoint_count);
    UnloadCodepoints(codepoints);
    ui->items_texture = LoadTexture(items_path);
    if (ui->text_font.texture.id == 0U || ui->button_font.texture.id == 0U
            || ui->items_texture.id == 0U) {
        if (ui->text_font.texture.id != 0U) {
            UnloadFont(ui->text_font);
        }
        if (ui->button_font.texture.id != 0U) {
            UnloadFont(ui->button_font);
        }
        if (ui->items_texture.id != 0U) {
            UnloadTexture(ui->items_texture);
        }
        *ui = (EikUi){ 0 };
        (void)snprintf(error, error_size, "cannot load UI fonts");
        return false;
    }
    ui->entered_at = (float)GetTime();
    ui->initialized = true;
    return true;
}

void eik_ui_unload(EikUi *ui)
{
    if (ui->initialized) {
        UnloadFont(ui->text_font);
        UnloadFont(ui->button_font);
        UnloadTexture(ui->items_texture);
    }
    *ui = (EikUi){ 0 };
}

void eik_ui_set_state(EikUi *ui, EikAppState state)
{
    if (ui->state != state) {
        ui->state = state;
        ui->focus = 0;
        ui->entered_at = (float)GetTime();
    }
}

EikUiAction eik_ui_update(EikUi *ui, float real_dt)
{
    const int count = button_count(ui->state);
    int index = 0;
    Vector2 pointer = GetMousePosition();
    bool pointer_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    (void)real_dt;
    if (!is_menu_state(ui->state)) {
        return EIK_UI_ACTION_NONE;
    }
    if (GetTouchPointCount() > 0) {
        pointer = GetTouchPosition(0);
        pointer_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsGestureDetected(GESTURE_TAP);
    }
    if (back_pressed()) {
        if (ui->state == EIK_APP_PAUSED) {
            return EIK_UI_ACTION_RESUME;
        }
        if (ui->state == EIK_APP_ABOUT || ui->state == EIK_APP_OPTIONS) {
            eik_ui_set_state(ui, EIK_APP_MAIN_MENU);
        }
        return EIK_UI_ACTION_NONE;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_TAB)
            || gamepad_pressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
        ui->focus = (ui->focus + 1) % count;
    } else if (IsKeyPressed(KEY_UP) || gamepad_pressed(GAMEPAD_BUTTON_LEFT_FACE_UP)) {
        ui->focus = (ui->focus + count - 1) % count;
    }
    for (index = 0; index < count; ++index) {
        if (CheckCollisionPointRec(pointer, button_bounds(GetScreenWidth(), GetScreenHeight(),
                index, count))) {
            ui->focus = index;
            if (pointer_pressed) {
                return activate(ui, button_at(ui, index));
            }
        }
    }
    if (confirm_pressed()) {
        return activate(ui, button_at(ui, ui->focus));
    }
    return EIK_UI_ACTION_NONE;
}

static void draw_centered(Font font, const char *text, float y, float size, Color tint)
{
    const Vector2 measured = MeasureTextEx(font, text, size, 1.0F);

    DrawTextEx(font, text, (Vector2){ ((float)GetScreenWidth() - measured.x) * 0.5F, y },
        size, 1.0F, tint);
}

static void draw_multiline_centered(Font font, const char *text, float y, float size, Color tint)
{
    const char *line = text;
    const char *next = NULL;
    char buffer[512];

    while (line[0] != '\0') {
        size_t length = 0U;

        next = strchr(line, '\n');
        length = next == NULL ? strlen(line) : (size_t)(next - line);
        if (length >= sizeof(buffer)) {
            length = sizeof(buffer) - 1U;
        }
        (void)memcpy(buffer, line, length);
        buffer[length] = '\0';
        draw_centered(font, buffer, y, size, tint);
        y += size * 1.35F;
        line = next == NULL ? line + length : next + 1;
    }
}

void eik_ui_draw(const EikUi *ui, unsigned int coins, int lives)
{
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const float entered = ((float)GetTime() - ui->entered_at) / EIK_UI_APPEAR_DURATION;
    const float progress = entered < 0.0F ? 0.0F : (entered > 1.0F ? 1.0F : entered);
    const float ease = progress * progress * (3.0F - 2.0F * progress);
    const Color white = { 255, 255, 255, (unsigned char)(255.0F * ease) };
    int count = 0;
    int index = 0;

    if (ui->state == EIK_APP_PLAYING || ui->state == EIK_APP_PAUSED) {
        DrawTexturePro(ui->items_texture, (Rectangle){ 0.0F, 0.0F, 16.0F, 16.0F },
            (Rectangle){ 10.0F, 10.0F, 32.0F, 32.0F }, (Vector2){ 0.0F, 0.0F }, 0.0F, WHITE);
        DrawTextEx(ui->text_font, TextFormat("%u", coins), (Vector2){ 45.0F, 12.0F }, 24.0F,
            1.0F, RAYWHITE);
        DrawTextEx(ui->text_font, eik_l10n(ui->language, EIK_MSG_LIVES),
            (Vector2){ 10.0F, 47.0F }, 16.0F, 1.0F, RAYWHITE);
        for (index = 0; index < lives; ++index) {
            DrawTexturePro(ui->items_texture, (Rectangle){ 0.0F, 16.0F, 16.0F, 16.0F },
                (Rectangle){ 10.0F + (float)index * 36.0F, 68.0F, 32.0F, 32.0F },
                (Vector2){ 0.0F, 0.0F }, 0.0F, WHITE);
        }
    }
    if (!is_menu_state(ui->state)) {
        return;
    }
    if (ui->state == EIK_APP_WEB_START) {
        ClearBackground(BLACK);
    } else if (ui->state == EIK_APP_MAIN_MENU) {
        DrawRectangle(0, 0, width, height, (Color){ 0, 0, 0, 90 });
    } else {
        DrawRectangle(0, 0, width, height, (Color){ 0, 0, 0, 145 });
    }
    if (ui->state == EIK_APP_ABOUT) {
        const Rectangle panel = { ((float)width - 580.0F) * 0.5F,
            ((float)height - 460.0F) * 0.5F, 580.0F, 460.0F };

        DrawRectangleRounded(panel, 0.07F, 12, BLACK);
        draw_centered(ui->text_font, eik_l10n(ui->language, EIK_MSG_ABOUT), panel.y + 30.0F,
            42.0F, white);
        draw_multiline_centered(ui->text_font, eik_l10n(ui->language,
#if defined(EIK_IOS)
            EIK_MSG_ABOUT_BODY_TOUCH),
#else
            EIK_MSG_ABOUT_BODY),
#endif
            panel.y + 92.0F, 20.0F, white);
    } else {
        const Rectangle panel = { ((float)width - 400.0F) * 0.5F,
            ((float)height - 300.0F) * 0.5F, 400.0F, 300.0F };
        const EikMsg heading = ui->state == EIK_APP_PAUSED ? EIK_MSG_PAUSE_MENU
            : ui->state == EIK_APP_GAME_OVER ? EIK_MSG_GAME_OVER
            : ui->state == EIK_APP_OPTIONS ? EIK_MSG_OPTIONS : EIK_MSG_TITLE;

        if (ui->state != EIK_APP_MAIN_MENU) {
            DrawRectangleRounded(panel, 0.10F, 12, BLACK);
        }
        if (ui->state != EIK_APP_WEB_START) {
            draw_centered(ui->text_font, eik_l10n(ui->language, heading),
            ui->state == EIK_APP_MAIN_MENU ? 75.0F : panel.y + 25.0F,
            ui->state == EIK_APP_MAIN_MENU ? (float)width * 0.075F : 30.0F, white);
        }
        if (ui->state == EIK_APP_OPTIONS) {
            draw_centered(ui->text_font, eik_l10n(ui->language, EIK_MSG_LANGUAGE), panel.y + 65.0F,
                18.0F, white);
        }
    }
    count = button_count(ui->state);
    for (index = 0; index < count; ++index) {
        const Rectangle bounds = button_bounds(width, height, index, count);
        const bool focused = index == ui->focus;
        const float scale = focused ? 1.0F + 0.08F * (1.0F - 1.0F / (1.0F + EIK_UI_EASE_RATE
            * (float)GetFrameTime())) : 1.0F;
        const Rectangle scaled = { bounds.x - bounds.width * (scale - 1.0F) * 0.5F,
            bounds.y - bounds.height * (scale - 1.0F) * 0.5F, bounds.width * scale,
            bounds.height * scale };
        const char *label = button_label(ui, button_at(ui, index));
        const float font_size = button_at(ui, index) == EIK_BUTTON_PLAY ? 34.0F : 24.0F;
        const Vector2 label_size = MeasureTextEx(ui->button_font, label, font_size, 1.0F);

        DrawRectangleRounded(scaled, 0.133F, 8, focused ? (Color){ 217, 217, 217, 255 } : WHITE);
        DrawTextEx(ui->button_font, label, (Vector2){ scaled.x + (scaled.width - label_size.x) * 0.5F,
            scaled.y + (scaled.height - label_size.y) * 0.5F }, font_size, 1.0F, BLACK);
    }
    if (ui->state != EIK_APP_WEB_START) {
        draw_centered(ui->text_font, eik_l10n(ui->language, EIK_MSG_MENU_HINT),
            (float)height - 28.0F, 12.0F, white);
    }
    if (ui->state == EIK_APP_PAUSED) {
        draw_multiline_centered(ui->text_font, eik_l10n(ui->language,
#if defined(EIK_IOS)
            EIK_MSG_TOUCH_CONTROLS),
#else
            EIK_MSG_CONTROLS),
#endif
            (float)height - 88.0F, 12.0F, white);
    }
}
