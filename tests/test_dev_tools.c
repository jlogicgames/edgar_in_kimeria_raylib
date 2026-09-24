#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdlib.h>

#include "dev_tools.h"

int main(void)
{
    EikCapture capture;
    char error[128];
    char path[EIK_CAPTURE_PATH_MAX];

    (void)unsetenv("EIK_CAPTURE");
    assert(eik_capture_configure(&capture, error, sizeof(error)));
    assert(!capture.enabled);

    assert(setenv("EIK_CAPTURE", "/tmp", 1) == 0);
    assert(setenv("EIK_CAPTURE_INTERVAL", "0.5", 1) == 0);
    assert(setenv("EIK_CAPTURE_SHOTS", "2", 1) == 0);
    assert(setenv("EIK_CAPTURE_LEVEL", "1", 1) == 0);
    assert(setenv("EIK_CAPTURE_DELAY", "1.0", 1) == 0);
    assert(eik_capture_configure(&capture, error, sizeof(error)));
    assert(capture.enabled);
    assert(capture.level == 1U);
    assert(!eik_capture_should_take(&capture, 0.49F));
    assert(eik_capture_should_take(&capture, 0.01F));
    assert(eik_capture_path(&capture, path, sizeof(path)));
    assert(eik_capture_should_start(&capture) == false);
    assert(eik_capture_should_take(&capture, 0.5F));
    assert(eik_capture_should_start(&capture));
    eik_capture_mark_started(&capture);
    assert(capture.started);

    assert(setenv("EIK_CAPTURE_LEVEL", "2", 1) == 0);
    assert(!eik_capture_configure(&capture, error, sizeof(error)));
    return EXIT_SUCCESS;
}
