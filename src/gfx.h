#ifndef _AMINOGFX_H
#define _AMINOGFX_H

#define INVALID_TEXTURE 0
#define INVALID_PROGRAM 0
#define INVALID_BUFFER 0

/*
 * Platform specific headers.
 *
 * Current time in milliseconds.
 */

#ifdef MAC

//macOS
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <sys/time.h>

/**
 * Get monotonic time for timer.
 */
static double __attribute__((unused)) getTime(void) {
    struct timespec res;

    clock_gettime(CLOCK_MONOTONIC, &res);

    return 1000.0 * res.tv_sec + ((double) res.tv_nsec / 1e6);
}

#endif

#ifdef RPI

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "GLES2/gl2.h"
#include "GLES2/gl2ext.h"

#include <time.h>

/**
 * Get monotonic time for timer.
 */
static double __attribute__((unused)) getTime(void) {
    struct timespec res;

    clock_gettime(CLOCK_MONOTONIC, &res);

    return 1000.0 * res.tv_sec + ((double) res.tv_nsec / 1e6);
}

#endif

#endif
