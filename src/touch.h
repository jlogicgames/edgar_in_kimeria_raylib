#ifndef EIK_TOUCH_H
#define EIK_TOUCH_H

#include <stdbool.h>
#include <stddef.h>

#include "input.h"
#include "raylib.h"

typedef struct EikTouchSafeArea {
    float left;
    float top;
    float right;
    float bottom;
} EikTouchSafeArea;

typedef struct EikTouchControls {
    Texture2D joystick;
    Texture2D knob;
    Texture2D button;
    Vector2 joystick_centre;
    Vector2 knob_centre;
    Vector2 jump_centre;
    Vector2 attack_centre;
    Vector2 interact_centre;
    Vector2 pause_centre;
    float joystick_radius;
    float button_radius;
    int joystick_touch_id;
    int jump_touch_id;
    int attack_touch_id;
    int interact_touch_id;
    int pause_touch_id;
    bool initialized;
    bool visible;
    bool attack_pressed;
    bool interact_pressed;
    bool pause_pressed;
} EikTouchControls;

bool eik_touch_init(EikTouchControls *controls, const char *joystick_path, const char *knob_path,
    const char *button_path, char *error, size_t error_size);
void eik_touch_unload(EikTouchControls *controls);
void eik_touch_update(EikTouchControls *controls, bool playing, bool gamepad_connected,
    EikTouchSafeArea safe_area);
void eik_touch_apply(const EikTouchControls *controls, EikInputFrame *input);
void eik_touch_draw(const EikTouchControls *controls);

#endif
