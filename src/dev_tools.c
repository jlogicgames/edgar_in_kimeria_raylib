#include "dev_tools.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define EIK_CAPTURE_DEFAULT_INTERVAL 1.5F
#define EIK_CAPTURE_DEFAULT_SHOTS 6U

static bool parse_float(const char *name, const char *value, float minimum, float *result,
    char *error, size_t error_size)
{
    char *end = NULL;
    float parsed = 0.0F;

    if (value == NULL || value[0] == '\0') {
        return true;
    }
    errno = 0;
    parsed = strtof(value, &end);
    if (errno != 0 || end == value || *end != '\0' || parsed < minimum) {
        (void)snprintf(error, error_size, "%s must be a number no less than %.1f", name,
            (double)minimum);
        return false;
    }
    *result = parsed;
    return true;
}

static bool parse_unsigned(const char *name, const char *value, unsigned int maximum,
    unsigned int *result, char *error, size_t error_size)
{
    char *end = NULL;
    unsigned long parsed = 0UL;

    if (value == NULL || value[0] == '\0') {
        return true;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > maximum) {
        (void)snprintf(error, error_size, "%s must be an integer from 0 to %u", name, maximum);
        return false;
    }
    *result = (unsigned int)parsed;
    return true;
}

bool eik_capture_configure(EikCapture *capture, char *error, size_t error_size)
{
    const char *directory = getenv("EIK_CAPTURE");
    unsigned int level = 0U;

    *capture = (EikCapture){
        .interval = EIK_CAPTURE_DEFAULT_INTERVAL,
        .remaining = EIK_CAPTURE_DEFAULT_SHOTS,
    };
    if (directory == NULL || directory[0] == '\0') {
        return true;
    }
    if (strlen(directory) >= sizeof(capture->directory)) {
        (void)snprintf(error, error_size, "EIK_CAPTURE directory path is too long");
        return false;
    }
    if (!DirectoryExists(directory)) {
        (void)snprintf(error, error_size, "EIK_CAPTURE directory does not exist: %s", directory);
        return false;
    }
    if (!parse_float("EIK_CAPTURE_INTERVAL", getenv("EIK_CAPTURE_INTERVAL"), 0.001F,
            &capture->interval, error, error_size)
            || !parse_float("EIK_CAPTURE_DELAY", getenv("EIK_CAPTURE_DELAY"), 0.0F,
                &capture->delay, error, error_size)
            || !parse_unsigned("EIK_CAPTURE_SHOTS", getenv("EIK_CAPTURE_SHOTS"), 9999U,
                &capture->remaining, error, error_size)
            || !parse_unsigned("EIK_CAPTURE_LEVEL", getenv("EIK_CAPTURE_LEVEL"), 1U, &level,
                error, error_size)) {
        return false;
    }
    (void)snprintf(capture->directory, sizeof(capture->directory), "%s", directory);
    capture->level = (size_t)level;
    capture->enabled = true;
#if defined(EIK_DEV)
    capture->about = getenv("EIK_CAPTURE_ABOUT") != NULL;
#endif
    return true;
}

bool eik_capture_should_take(EikCapture *capture, float real_dt)
{
    if (!capture->enabled) {
        return false;
    }
    capture->elapsed += real_dt;
    if (capture->remaining == 0U) {
        return false;
    }
    capture->timer += real_dt;
    if (capture->timer < capture->interval) {
        return false;
    }
    capture->timer = 0.0F;
    --capture->remaining;
    return true;
}

bool eik_capture_should_start(const EikCapture *capture)
{
    return capture->enabled && !capture->started && capture->elapsed >= capture->delay;
}

void eik_capture_mark_started(EikCapture *capture)
{
    capture->started = true;
}

bool eik_capture_path(EikCapture *capture, char *path, size_t path_size)
{
    const int length = snprintf(path, path_size, "%s/frame-%02u.png", capture->directory,
        capture->index);

    if (length < 0 || (size_t)length >= path_size) {
        return false;
    }
    ++capture->index;
    return true;
}
