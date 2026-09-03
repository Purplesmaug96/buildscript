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

    /* Colour surface at full display resolution so the resolve matches the
     * swap texture VdSwap presents (GPU present path reads frame.width/h =
     * the EDRAM colour size; a 1/3-scale target broke the resolve pitch). */
    if (!xbox360_create(&s->gl, s->screen.width, s->screen.height))
    {
        DbgPrint("glsample: xbox360_create failed");
        return false;
    }
    xbox360_make_current(s->gl);
    DbgPrint("glsample: GL context ready (%ux%u)", s->screen.width,
             s->screen.height);

    // Attach the shared primary ring to the hardware driver so GL draws go
    // over PM4 (no-op with the softpipe backend).
    if (s->screen.xenia && s->screen.xenia_ring_address)
    {
        xbox360_attach_ring(s->gl,
                            (volatile uint32_t *)(uintptr_t)
                                s->screen.xenia_ring_address,
                            XENIA_RING_SIZE_LOG2,
                            &s->screen.xenia_ring_wptr,
                            s->screen.xenia_rptr_page);
        DbgPrint("glsample: ring attached to hw driver");
    }

    if (s->screen.xenia && s->screen.xenia_ring_address)
    {
        /* Skip the standalone PM4 smoke test when the hw driver ring is
         * attached — it races with VdSwap (both read MMIO wptr and write
         * at the same ring offset).  The GL path exercises the full pipeline. */
        DbgPrint("glsample: ring attached, skipping PM4 smoke test");
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

    if (frame.gpu_tiled)
    {
        // Hardware path: the GPU resolved into a tiled surface - present it
        // directly via VdSwap's swap texture (fetch constant 0).
        s->screen.present_base = frame.gpu_phys;
        s->screen.present_w = frame.width;
        s->screen.present_h = frame.height;
        screen_present(&s->screen);
        return;
    }

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
