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

bool eik_input_gamepad_connected(void)
{
    int pad = 0;

    for (pad = 0; pad < EIK_MAX_GAMEPADS; ++pad) {
        if (IsGamepadAvailable(pad)) {
            return true;
        }
    }
    return false;
}

static void apply_capture_overlay(EikInputFrame *input)
{
    const char *mode = getenv("EIK_CAPTURE_INPUT");
    static int last_second = -1;
    static int last_fx_period = -1;
    static int last_cycle_period = -1;
    static bool checkpoint_sent = false;
    const double time = GetTime();
    const int second = (int)time;

    if (getenv("EIK_CAPTURE") == NULL || mode == NULL) {
        return;
    }
    if (strcmp(mode, "left") == 0) {
        input->horizontal = -1.0F;
    } else if (strcmp(mode, "run") == 0 || strcmp(mode, "fx") == 0
            || strcmp(mode, "cycle") == 0 || strcmp(mode, "checkpoint") == 0) {
        input->horizontal = 1.0F;
    } else if (strcmp(mode, "pause") == 0) {
        if (second >= 1 && last_second != second) {
            input->pause_pressed = true;
        }
    } else {
        return;
    }
    if (second != last_second) {
        input->jump_held = true;
        if (second > 0 && second % 3 == 0) {
            input->attack_pressed = true;
        }
        last_second = second;
    }
    if (strcmp(mode, "fx") == 0 && (int)(time / 2.0) != last_fx_period) {
        input->debug_fx_pressed = true;
        last_fx_period = (int)(time / 2.0);
    }
    if (strcmp(mode, "cycle") == 0 && (int)(time / 2.5) != last_cycle_period) {
        input->debug_advance_level_pressed = true;
        last_cycle_period = (int)(time / 2.5);
    }
    if (strcmp(mode, "checkpoint") == 0 && !checkpoint_sent && time >= 5.0) {
        input->debug_checkpoint_pressed = true;
        checkpoint_sent = true;
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
