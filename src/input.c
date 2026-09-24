#include "input.h"

#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define EIK_STICK_DEADZONE 0.3F
#define EIK_MAX_GAMEPADS 4

static bool any_gamepad_button_down(int button)
{
    int pad = 0;

    for (pad = 0; pad < EIK_MAX_GAMEPADS; ++pad) {
        if (IsGamepadAvailable(pad) && IsGamepadButtonDown(pad, button)) {
            return true;
        }
    }
    return false;
}

static bool any_gamepad_button_pressed(int button)
{
    int pad = 0;

    for (pad = 0; pad < EIK_MAX_GAMEPADS; ++pad) {
        if (IsGamepadAvailable(pad) && IsGamepadButtonPressed(pad, button)) {
            return true;
        }
    }
    return false;
}

static float gamepad_horizontal(void)
{
    bool left = false;
    bool right = false;
    int pad = 0;

    for (pad = 0; pad < EIK_MAX_GAMEPADS; ++pad) {
        const float axis = IsGamepadAvailable(pad)
            ? GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X) : 0.0F;

        left = left || (IsGamepadAvailable(pad)
            && (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT)
                || axis < -EIK_STICK_DEADZONE));
        right = right || (IsGamepadAvailable(pad)
            && (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)
                || axis > EIK_STICK_DEADZONE));
    }
    return (right ? 1.0F : 0.0F) - (left ? 1.0F : 0.0F);
}

static void apply_capture_overlay(EikInputFrame *input)
{
    const char *mode = getenv("EIK_CAPTURE_INPUT");

    if (mode == NULL) {
        return;
    }
    if (strcmp(mode, "run") == 0) {
        input->horizontal = 1.0F;
    } else if (strcmp(mode, "left") == 0) {
        input->horizontal = -1.0F;
    } else if (strcmp(mode, "checkpoint") == 0) {
        input->horizontal = 1.0F;
    } else if (strcmp(mode, "pause") == 0) {
        input->pause_pressed = true;
    }
}

EikInputFrame eik_input_read(void)
{
    const bool left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    const bool right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    EikInputFrame input = {
        .horizontal = (right ? 1.0F : 0.0F) - (left ? 1.0F : 0.0F),
        .jump_held = IsKeyDown(KEY_J) || IsKeyDown(KEY_Z),
        .attack_pressed = IsKeyPressed(KEY_K) || IsKeyPressed(KEY_X),
        .interact_pressed = IsKeyPressed(KEY_L) || IsKeyPressed(KEY_C),
        .pause_pressed = IsKeyPressed(KEY_ESCAPE),
    };
    const float pad_horizontal = gamepad_horizontal();

    if (pad_horizontal != 0.0F) {
        input.horizontal = pad_horizontal;
    }
    input.jump_held = input.jump_held
        || any_gamepad_button_down(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    input.attack_pressed = input.attack_pressed
        || any_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)
        || any_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    input.interact_pressed = input.interact_pressed
        || any_gamepad_button_pressed(GAMEPAD_BUTTON_RIGHT_FACE_UP);
    input.pause_pressed = input.pause_pressed
        || any_gamepad_button_pressed(GAMEPAD_BUTTON_MIDDLE_RIGHT);
    apply_capture_overlay(&input);
    return input;
}
