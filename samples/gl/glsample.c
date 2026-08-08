// Shared glue for the OpenGL samples. See glsample.h.
//
// SPDX-License-Identifier: MIT

#include <xecore/xboxkrnl.h>
#include <xbox360/xbox360_api.h>

#include "glsample.h"

bool glsample_init(glsample_t *s)
{
    s->ok = false;
    s->gl = NULL;

    if (!screen_init(&s->screen))
    {
        DbgPrint("glsample: screen_init failed");
        return false;
    }
    DbgPrint("glsample: screen %ux%u (xenia=%d)", s->screen.width,
             s->screen.height, s->screen.xenia);

    if (!xbox360_create(&s->gl, s->screen.width, s->screen.height))
    {
        DbgPrint("glsample: xbox360_create failed");
        return false;
    }
    xbox360_make_current(s->gl);
    DbgPrint("glsample: GL context ready (%ux%u)", s->screen.width,
             s->screen.height);

    s->ok = true;
    return true;
}

void glsample_flip(glsample_t *s)
{
    struct xbox360_frame frame;

    if (!s->ok)
        return;

    xbox360_present(s->gl, &frame);
    if (!frame.ptr)
        return;

    screen_blit_bgra(&s->screen, frame.ptr, frame.stride);
    screen_present(&s->screen);
}

void glsample_wait(glsample_t *s)
{
    int64_t interval = s->screen.xenia ? -16 * 1000000 : -10 * 1000000;
    KeDelayExecutionThread(0, 0, &interval);
}
