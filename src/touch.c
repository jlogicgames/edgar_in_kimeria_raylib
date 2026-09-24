#include "touch.h"

#include <stdio.h>

#define EIK_TOUCH_ALPHA 102U
#define EIK_TOUCH_DEADZONE 0.3F
#define EIK_TOUCH_UNASSIGNED -1

#if defined(EIK_IOS)

static float clampf(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    return value > maximum ? maximum : value;
}

static float minimum(float left, float right)
{
    return left < right ? left : right;
}

static Color touch_tint(Color color)
{
    color.a = EIK_TOUCH_ALPHA;
    return color;
}

static void reset_touch_ids(EikTouchControls *controls)
{
    controls->joystick_touch_id = EIK_TOUCH_UNASSIGNED;
    controls->jump_touch_id = EIK_TOUCH_UNASSIGNED;
    controls->attack_touch_id = EIK_TOUCH_UNASSIGNED;
    controls->interact_touch_id = EIK_TOUCH_UNASSIGNED;
    controls->pause_touch_id = EIK_TOUCH_UNASSIGNED;
}

static void layout_controls(EikTouchControls *controls, EikTouchSafeArea safe_area)
{
    const float width = (float)GetScreenWidth();
    const float height = (float)GetScreenHeight();
    const float inset_left = clampf(safe_area.left, 0.0F, width * 0.4F);
    const float inset_right = clampf(safe_area.right, 0.0F, width * 0.4F);
    const float inset_top = clampf(safe_area.top, 0.0F, height * 0.4F);
    const float inset_bottom = clampf(safe_area.bottom, 0.0F, height * 0.4F);
    const float usable_width = width - inset_left - inset_right;
    const float usable_height = height - inset_top - inset_bottom;
    const float margin = 18.0F;
    const float joystick_size = clampf(minimum(usable_width, usable_height) * 0.24F,
        88.0F, 152.0F);
    const float button_size = clampf(joystick_size * 0.67F, 60.0F, 102.0F);
    const float right = width - inset_right - margin;
    const float bottom = height - inset_bottom - margin;

    controls->joystick_radius = joystick_size * 0.5F;
    controls->button_radius = button_size * 0.5F;
    controls->joystick_centre = (Vector2){ inset_left + margin + controls->joystick_radius,
        bottom - controls->joystick_radius };
    controls->jump_centre = (Vector2){ right - controls->button_radius,
        bottom - controls->button_radius };
    controls->attack_centre = (Vector2){ controls->jump_centre.x - button_size * 0.92F,
        controls->jump_centre.y };
    controls->interact_centre = (Vector2){ controls->jump_centre.x,
        controls->jump_centre.y - button_size * 0.92F };
    controls->pause_centre = (Vector2){ right - controls->button_radius * 0.72F,
        inset_top + margin + controls->button_radius * 0.72F };
    controls->knob_centre = controls->joystick_centre;
}

static int touch_index_for_id(int touch_id)
{
    int index = 0;

    if (touch_id == EIK_TOUCH_UNASSIGNED) {
        return -1;
    }
    for (index = 0; index < GetTouchPointCount(); ++index) {
        if (GetTouchPointId(index) == touch_id) {
            return index;
        }
    }
    return -1;
}

static bool touch_id_is_assigned(const EikTouchControls *controls, int touch_id)
{
    return controls->joystick_touch_id == touch_id || controls->jump_touch_id == touch_id
        || controls->attack_touch_id == touch_id || controls->interact_touch_id == touch_id
        || controls->pause_touch_id == touch_id;
}

static void release_finished_touches(EikTouchControls *controls)
{
    if (touch_index_for_id(controls->joystick_touch_id) < 0) {
        controls->joystick_touch_id = EIK_TOUCH_UNASSIGNED;
    }
    if (touch_index_for_id(controls->jump_touch_id) < 0) {
        controls->jump_touch_id = EIK_TOUCH_UNASSIGNED;
    }
    if (touch_index_for_id(controls->attack_touch_id) < 0) {
        controls->attack_touch_id = EIK_TOUCH_UNASSIGNED;
    }
    if (touch_index_for_id(controls->interact_touch_id) < 0) {
        controls->interact_touch_id = EIK_TOUCH_UNASSIGNED;
    }
    if (touch_index_for_id(controls->pause_touch_id) < 0) {
        controls->pause_touch_id = EIK_TOUCH_UNASSIGNED;
    }
}

static void assign_new_touches(EikTouchControls *controls)
{
    int index = 0;

    for (index = 0; index < GetTouchPointCount(); ++index) {
        const int touch_id = GetTouchPointId(index);
        const Vector2 position = GetTouchPosition(index);

        if (touch_id_is_assigned(controls, touch_id)) {
            continue;
        }
        if (controls->joystick_touch_id == EIK_TOUCH_UNASSIGNED
                && CheckCollisionPointCircle(position, controls->joystick_centre,
                    controls->joystick_radius)) {
            controls->joystick_touch_id = touch_id;
        } else if (controls->jump_touch_id == EIK_TOUCH_UNASSIGNED
                && CheckCollisionPointCircle(position, controls->jump_centre,
                    controls->button_radius)) {
            controls->jump_touch_id = touch_id;
        } else if (controls->attack_touch_id == EIK_TOUCH_UNASSIGNED
                && CheckCollisionPointCircle(position, controls->attack_centre,
                    controls->button_radius)) {
            controls->attack_touch_id = touch_id;
            controls->attack_pressed = true;
        } else if (controls->interact_touch_id == EIK_TOUCH_UNASSIGNED
                && CheckCollisionPointCircle(position, controls->interact_centre,
                    controls->button_radius)) {
            controls->interact_touch_id = touch_id;
            controls->interact_pressed = true;
        } else if (controls->pause_touch_id == EIK_TOUCH_UNASSIGNED
                && CheckCollisionPointCircle(position, controls->pause_centre,
                    controls->button_radius * 0.72F)) {
            controls->pause_touch_id = touch_id;
            controls->pause_pressed = true;
        }
    }
}

#endif

bool eik_touch_init(EikTouchControls *controls, const char *joystick_path, const char *knob_path,
    const char *button_path, char *error, size_t error_size)
{
    *controls = (EikTouchControls){ 0 };
#if defined(EIK_IOS)
    reset_touch_ids(controls);
    controls->joystick = LoadTexture(joystick_path);
    controls->knob = LoadTexture(knob_path);
    controls->button = LoadTexture(button_path);
    if (controls->joystick.id == 0U || controls->knob.id == 0U || controls->button.id == 0U) {
        eik_touch_unload(controls);
        (void)snprintf(error, error_size, "cannot load touch-control artwork");
        return false;
    }
#else
    (void)joystick_path;
    (void)knob_path;
    (void)button_path;
    (void)error;
    (void)error_size;
#endif
    controls->initialized = true;
    return true;
}

void eik_touch_unload(EikTouchControls *controls)
{
#if defined(EIK_IOS)
    if (controls->joystick.id != 0U) {
        UnloadTexture(controls->joystick);
    }
    if (controls->knob.id != 0U) {
        UnloadTexture(controls->knob);
    }
    if (controls->button.id != 0U) {
        UnloadTexture(controls->button);
    }
#endif
    *controls = (EikTouchControls){ 0 };
}

void eik_touch_update(EikTouchControls *controls, bool playing, bool gamepad_connected,
    EikTouchSafeArea safe_area)
{
    controls->attack_pressed = false;
    controls->interact_pressed = false;
    controls->pause_pressed = false;
#if defined(EIK_IOS)
    controls->visible = controls->initialized && playing && !gamepad_connected;
    if (!controls->visible) {
        reset_touch_ids(controls);
        return;
    }
    layout_controls(controls, safe_area);
    release_finished_touches(controls);
    assign_new_touches(controls);
    {
        const int touch_index = touch_index_for_id(controls->joystick_touch_id);

        if (touch_index >= 0) {
            Vector2 deflection = Vector2Subtract(GetTouchPosition(touch_index),
                controls->joystick_centre);
            const float distance = Vector2Length(deflection);

            if (distance > controls->joystick_radius) {
                deflection = Vector2Scale(deflection, controls->joystick_radius / distance);
            }
            controls->knob_centre = Vector2Add(controls->joystick_centre, deflection);
        }
    }
#else
    (void)playing;
    (void)gamepad_connected;
    (void)safe_area;
    controls->visible = false;
#endif
}

void eik_touch_apply(const EikTouchControls *controls, EikInputFrame *input)
{
#if defined(EIK_IOS)
    const int touch_index = touch_index_for_id(controls->joystick_touch_id);

    if (!controls->visible) {
        return;
    }
    if (touch_index >= 0) {
        const float horizontal = (GetTouchPosition(touch_index).x - controls->joystick_centre.x)
            / controls->joystick_radius;

        if (horizontal < -EIK_TOUCH_DEADZONE) {
            input->horizontal = -1.0F;
        } else if (horizontal > EIK_TOUCH_DEADZONE) {
            input->horizontal = 1.0F;
        }
    }
    input->jump_held = input->jump_held
        || touch_index_for_id(controls->jump_touch_id) >= 0;
    input->attack_pressed = input->attack_pressed || controls->attack_pressed;
    input->interact_pressed = input->interact_pressed || controls->interact_pressed;
    input->pause_pressed = input->pause_pressed || controls->pause_pressed;
#else
    (void)controls;
    (void)input;
#endif
}

#if defined(EIK_IOS)

static void draw_button(const EikTouchControls *controls, Vector2 centre, Color tint)
{
    const float diameter = controls->button_radius * 2.0F;

    DrawTexturePro(controls->button, (Rectangle){ 0.0F, 0.0F, (float)controls->button.width,
            (float)controls->button.height }, (Rectangle){ centre.x - controls->button_radius,
            centre.y - controls->button_radius, diameter, diameter }, (Vector2){ 0.0F, 0.0F },
        0.0F, touch_tint(tint));
}

static void draw_sword(Vector2 centre, float size)
{
    const Color color = touch_tint(RAYWHITE);

    DrawLineEx((Vector2){ centre.x - size * 0.20F, centre.y + size * 0.20F },
        (Vector2){ centre.x + size * 0.23F, centre.y - size * 0.23F }, size * 0.09F, color);
    DrawLineEx((Vector2){ centre.x - size * 0.28F, centre.y - size * 0.03F },
        (Vector2){ centre.x - size * 0.04F, centre.y + size * 0.21F }, size * 0.09F, color);
}

static void draw_hand(Vector2 centre, float size)
{
    const Color color = touch_tint(RAYWHITE);
    const Rectangle palm = { centre.x - size * 0.16F, centre.y - size * 0.05F,
        size * 0.32F, size * 0.28F };

    DrawRectangleRounded(palm, 0.35F, 6, color);
    DrawLineEx((Vector2){ centre.x - size * 0.12F, centre.y - size * 0.06F },
        (Vector2){ centre.x - size * 0.12F, centre.y - size * 0.25F }, size * 0.07F, color);
    DrawLineEx((Vector2){ centre.x, centre.y - size * 0.06F },
        (Vector2){ centre.x, centre.y - size * 0.28F }, size * 0.07F, color);
    DrawLineEx((Vector2){ centre.x + size * 0.12F, centre.y - size * 0.06F },
        (Vector2){ centre.x + size * 0.12F, centre.y - size * 0.23F }, size * 0.07F, color);
}

static void draw_pause(Vector2 centre, float size)
{
    const Color color = touch_tint(RAYWHITE);

    DrawRectangle((int)(centre.x - size * 0.18F), (int)(centre.y - size * 0.25F),
        (int)(size * 0.12F), (int)(size * 0.50F), color);
    DrawRectangle((int)(centre.x + size * 0.06F), (int)(centre.y - size * 0.25F),
        (int)(size * 0.12F), (int)(size * 0.50F), color);
}

#endif

void eik_touch_draw(const EikTouchControls *controls)
{
#if defined(EIK_IOS)
    const float joystick_diameter = controls->joystick_radius * 2.0F;
    const float knob_radius = controls->joystick_radius * 0.43F;
    const float knob_diameter = knob_radius * 2.0F;

    if (!controls->visible) {
        return;
    }
    DrawTexturePro(controls->joystick, (Rectangle){ 0.0F, 0.0F,
            (float)controls->joystick.width, (float)controls->joystick.height },
        (Rectangle){ controls->joystick_centre.x - controls->joystick_radius,
            controls->joystick_centre.y - controls->joystick_radius, joystick_diameter,
            joystick_diameter }, (Vector2){ 0.0F, 0.0F }, 0.0F, touch_tint(WHITE));
    DrawTexturePro(controls->knob, (Rectangle){ 0.0F, 0.0F, (float)controls->knob.width,
            (float)controls->knob.height }, (Rectangle){ controls->knob_centre.x - knob_radius,
            controls->knob_centre.y - knob_radius, knob_diameter, knob_diameter },
        (Vector2){ 0.0F, 0.0F }, 0.0F, touch_tint(WHITE));
    draw_button(controls, controls->jump_centre, WHITE);
    draw_button(controls, controls->attack_centre, ORANGE);
    draw_button(controls, controls->interact_centre, SKYBLUE);
    draw_button(controls, controls->pause_centre, VIOLET);
    draw_sword(controls->attack_centre, controls->button_radius);
    draw_hand(controls->interact_centre, controls->button_radius);
    draw_pause(controls->pause_centre, controls->button_radius);
#else
    (void)controls;
#endif
}
