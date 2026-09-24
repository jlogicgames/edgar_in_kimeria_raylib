#ifndef EIK_DEV_TOOLS_H
#define EIK_DEV_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#define EIK_CAPTURE_DIRECTORY_MAX 1024U
#define EIK_CAPTURE_PATH_MAX 1200U

typedef struct EikCapture {
    char directory[EIK_CAPTURE_DIRECTORY_MAX];
    float interval;
    float timer;
    float delay;
    float elapsed;
    unsigned int remaining;
    unsigned int index;
    size_t level;
    bool enabled;
    bool started;
    bool about;
} EikCapture;

/* Reads the EIK_CAPTURE configuration. A disabled capture is always valid. */
bool eik_capture_configure(EikCapture *capture, char *error, size_t error_size);

/* Advances the capture clock. Screenshot requests remain valid until consumed. */
bool eik_capture_should_take(EikCapture *capture, float real_dt);
bool eik_capture_should_start(const EikCapture *capture);
void eik_capture_mark_started(EikCapture *capture);
bool eik_capture_path(EikCapture *capture, char *path, size_t path_size);

#endif
