#ifndef EIK_INPUT_H
#define EIK_INPUT_H

#include <stdbool.h>

typedef struct EikInputFrame {
    float horizontal;
    bool jump_held;
    bool attack_pressed;
    bool interact_pressed;
    bool pause_pressed;
} EikInputFrame;

/* Reads every supported physical control, then applies EIK_CAPTURE_INPUT. */
EikInputFrame eik_input_read(void);

#endif
