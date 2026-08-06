// d3d9_magenta: magenta screen clear using Direct3D 9 types
//
// The Direct3D 9 headers from the Microsoft Xbox 360 SDK are vendored into
// xecorelib (include <xecore/d3d9.h>). This sample uses the D3D9 colour
// types (D3DCOLOR, D3DCOLOR_ARGB) and viewport definition (D3DVIEWPORT9) to
// describe the frame, then fills and presents it through the shared screen
// library (which uses the kernel's VdSwap presentation path).
//
// Note: xecorelib provides the XAM import library, not the full D3D9 runtime
// (that ships as d3d9.lib in the XDK and cannot be redistributed). GPU-side
// D3DDevice_* calls therefore link but are only meaningful with a D3D runtime
// present; rendering here goes through the framebuffer.

#include <xecore/d3d9.h>
#include <xecore/xboxkrnl.h>

#include "screen.h"

// Converts a D3DCOLOR (0xAARRGGBB) to the screen library's k_8_8_8_8 byte
// order: the surface stores R, G, B, A with R at the lowest address, so the
// byte lanes are swapped within the word.
static inline uint32_t d3dcolor_to_xrgb(D3DCOLOR color)
{
    uint32_t a = (color >> 24) & 0xFF;
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

extern "C" void main(void)
{
    DbgPrint("d3d9_magenta: main enter");

    screen_t screen;
    if (!screen_init(&screen))
    {
        DbgPrint("d3d9_magenta: screen_init failed");
        return;
    }
    DbgPrint("d3d9_magenta: screen_init ok %ux%u", screen.width, screen.height);

    // Magenta everywhere, expressed as a D3D9 colour.
    D3DCOLOR magenta = D3DCOLOR_ARGB(255, 255, 0, 255);
    screen_clear(&screen, d3dcolor_to_xrgb(magenta));

    // A little text on top: white on magenta.
    int x = (int)(screen.width - 14 * 8) / 2;
    int y = (int)(screen.height - 8) / 2;
    screen_draw_string(&screen, x, y, SCREEN_COLOR_WHITE, SCREEN_COLOR_MAGENTA,
                       "OpenXeChain");
    screen_draw_string(&screen, x, y + 10, SCREEN_COLOR_WHITE, SCREEN_COLOR_MAGENTA,
                       "d3d9 magenta!");

    for (;;)
    {
        // Present the (unchanging) frame so the GPU scans it out.
        screen_present(&screen);
        int64_t interval = -16 * 1000 * 1000; // ~60 Hz (100ns units)
        KeDelayExecutionThread(0, 0, &interval);
    }
}
