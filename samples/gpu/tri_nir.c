// gpu/tri_nir: dev triangle through the real GPU path
//
// Draws a magenta triangle the way D3D9 would: the vertex/fragment shaders
// are built as NIR and compiled to Xenos microcode by the NIR->microcode
// compiler (xenos_compile_nir in libxbox360.a), uploaded to the GPU with
// PM4_IM_LOAD_IMMEDIATE, and drawn into EDRAM tile 0 (640x480 8_8_8_8).
// The render target is resolved to system memory (RB_MODECONTROL kCopy +
// RB_COPY_*), texture fetch constant 0 is pointed at the resolved surface,
// and VdSwap presents it: xenia samples fetch constant 0 through its texture
// cache, which serves the GPU-written data directly - no CPU readback or
// unswizzle anywhere.
//
// Nothing here touches GL: it exercises the raw PM4/EDRAM path end to end.
// Expected result under Xenia: a magenta triangle on black, presented by the
// host swap path.

#include <xecore/xboxkrnl.h>
#include <xbox360/xenos_gpu.h>

#include "screen.h"

void main(void)
{
    DbgPrint("main: enter");

    screen_t screen;
    if (!screen_init(&screen))
    {
        DbgPrint("main: screen_init failed");
        return;
    }
    DbgPrint("main: screen_init ok xenia=%d %ux%u", screen.xenia,
             screen.width, screen.height);
    if (!screen.xenia)
    {
        DbgPrint("main: GPU dev triangle requires Xenia");
        return;
    }

    screen_clear(&screen, SCREEN_COLOR_BLACK);

    uint32_t wptr = screen.xenia_ring_wptr;
    // Skip verify for now to isolate the NIR triangle's packet stream
    // if (xe_gpu_dev_verify(screen.xenia_ring_address, XENIA_RING_SIZE_LOG2,
    //                       wptr) != 0)
    // {
    //     DbgPrint("main: dev verify failed");
    //     return;
    // }
    // wptr += 64;
    // Render several frames like a real title: with asynchronous host
    // pipeline creation (xenia default), the first draw can execute before
    // its pipeline exists (placeholder = no rasterization); later frames
    // pick up the real pipeline.
    for (uint32_t frame = 0; frame < 16; ++frame)
    {
        if (xe_gpu_dev_triangle_nir(screen.xenia_ring_address,
                                    XENIA_RING_SIZE_LOG2, wptr,
                                    screen.xenia_fb_address, &wptr,
                                    0) != 0)
        {
            /* Non-fatal: keep presenting whatever was rendered so far. */
            DbgPrint("main: dev triangle (NIR) issue failed at frame %u",
                     frame);
            break;
        }
        screen.xenia_ring_wptr = wptr;
    }
    DbgPrint("main: dev triangle (NIR) ok, presenting at wptr=%u", wptr);

    // Present with the GPU: hand VdSwap the tiled resolve destination as the
    // swap source (texture fetch constant 0) instead of a CPU-written front
    // buffer.  Xenia's presenter samples it through its texture cache, which
    // serves GPU-written data directly.
    {
        uint32_t base, w, h;
        xe_gpu_get_resolve_surface(&base, &w, &h);
        screen.present_base = base;
        screen.present_w = w;
        screen.present_h = h;
    }

    // The front buffer holds the unswizzled render; keep handing it to the
    // GPU every frame (Xenia has no scanout).
    for (;;)
    {
        screen_present(&screen);
        int64_t interval = -16 * 1000 * 1000; // -16 ms (100ns units)
        KeDelayExecutionThread(0, 0, &interval);
    }
}