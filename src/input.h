#ifndef EIK_INPUT_H
#define EIK_INPUT_H

#include <stdbool.h>

typedef struct EikInputFrame {
    float horizontal;
    bool jump_held;
    bool attack_pressed;
    bool interact_pressed;
    bool pause_pressed;
    bool debug_fx_pressed;
    bool debug_advance_level_pressed;
    bool debug_checkpoint_pressed;
} EikInputFrame;

/* Reads every supported physical control, then applies EIK_CAPTURE_INPUT. */
EikInputFrame eik_input_read(void);

/* True when any of the supported physical gamepads is connected. */
bool eik_input_gamepad_connected(void);

#endif
