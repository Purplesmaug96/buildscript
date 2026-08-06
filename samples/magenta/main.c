// magenta: magenta screen clear
//
// Maps the front buffer and fills the whole screen with magenta - the
// classic "does the framebuffer work at all?" test for Xbox 360 homebrew -
// then draws a bit of text on top and stays there until rebooted.
//
// If nothing appears on screen, check samples/common/screen.h: the buffer
// address may need adjusting for your kernel/firmware.

#include <xecore/xboxkrnl.h>

#include "screen.h"

void main(void)
{
    DbgPrint("main: enter");

    screen_t screen;
    if (!screen_init(&screen))
    {
        DbgPrint("main: screen_init failed");
        return; // could not map the front buffer; nothing else to do
    }
    DbgPrint("main: screen_init ok %ux%u", screen.width, screen.height);

    // Magenta everywhere.
    screen_clear(&screen, SCREEN_COLOR_MAGENTA);

    // A little text on top: white on magenta.
    int x = (int)(screen.width - 14 * 8) / 2;
    int y = (int)(screen.height - 8) / 2;
    screen_draw_string(&screen, x, y, SCREEN_COLOR_WHITE, SCREEN_COLOR_MAGENTA,
                       "Hello,");
    screen_draw_string(&screen, x, y + 10, SCREEN_COLOR_WHITE, SCREEN_COLOR_MAGENTA,
                       "World!");

    // Stay here. Returning would exit back to the dashboard.
    for (;;)
    {
        // Re-present the (unchanging) frame so the GPU scans it out.
        screen_present(&screen);
        int64_t interval = -16 * 1000 * 1000; // ~60 Hz (100ns units)
        KeDelayExecutionThread(0, 0, &interval);
    }
}
