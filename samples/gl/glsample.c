// Shared glue for the OpenGL samples. See glsample.h.
//
// SPDX-License-Identifier: MIT

#include <xecore/xboxkrnl.h>
#include <xbox360/xbox360_api.h>

#include "glsample.h"

/* gpu/xenos_gpu.c — PM4 ring verification hook (installed header pending). */

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

    if (!xbox360_create(&s->gl, s->screen.width / 3, s->screen.height / 3))
    {
        DbgPrint("glsample: xbox360_create failed");
        return false;
    }
    xbox360_make_current(s->gl);
    DbgPrint("glsample: GL context ready (%ux%u)", s->screen.width,
             s->screen.height);

    if (s->screen.xenia && s->screen.xenia_ring_address)
    {
        int r = xe_gpu_dev_verify(s->screen.xenia_ring_address,
                                  XENIA_RING_SIZE_LOG2,
                                  s->screen.xenia_ring_wptr);
        DbgPrint("gpu: PM4 dev verify %s", r == 0 ? "OK" : "FAIL");
        if (r == 0)
        {
            r = xe_gpu_dev_triangle(s->screen.xenia_ring_address,
                                    XENIA_RING_SIZE_LOG2,
                                    s->screen.xenia_ring_wptr);
            DbgPrint("gpu: PM4 first draw submitted %s", r == 0 ? "OK" : "FAIL");
        }
    }

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

    screen_blit_bgra(&s->screen, frame.ptr, frame.stride, frame.width,
                     frame.height);
    screen_present(&s->screen);
}

void glsample_wait(glsample_t *s)
{
    int64_t interval = s->screen.xenia ? -16 * 10000 : -10 * 10000;
    KeDelayExecutionThread(0, 0, &interval);
}
