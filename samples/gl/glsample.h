// Shared glue for the OpenGL samples: initialises the screen and the
// xbox360 (Mesa softpipe) GL backend, and hands rendered frames to the
// Xenos scanout path.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>

#include <xbox360/xbox360_api.h>

#include "screen.h"

typedef struct glsample
{
    screen_t screen;               // scanout front buffer
    struct xbox360_display *gl;    // Mesa softpipe display + GL context
    bool ok;                       // false if init failed
} glsample_t;

// Maps the front buffer and creates the GL context. Renders off-screen at
// the scanout resolution; use glsample_flip() to move each frame to screen.
// Returns false on failure (prints the reason via DbgPrint).
bool glsample_init(glsample_t *s);

// Presents the current GL frame: flushes, reads back the colour buffer and
// blits it into the front buffer. Call after issuing GL drawing commands.
void glsample_flip(glsample_t *s);

// Pauses for one frame period (16 ms under Xenia, 10 ms on hardware).
void glsample_wait(glsample_t *s);

// PM4 ring verification / first-draw smoke test hooks (xenos_gpu.c).
int xe_gpu_dev_verify(uint32_t ring_va, uint32_t size_log2,
                      uint32_t start_wptr);
int xe_gpu_dev_triangle(uint32_t ring_va, uint32_t size_log2,
                        uint32_t start_wptr);
