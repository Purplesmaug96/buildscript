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
    screen_t screen;
    if (!screen_init(&screen))
        return; // could not map the front buffer; nothing else to do

    // Magenta everywhere.
    screen_clear(&screen, SCREEN_COLOR_MAGENTA);

    // A little text on top: white on magenta.
    int x = (int)(screen.width - 14 * 8) / 2;
    int y = (int)(screen.height - 8) / 2;
    screen_draw_string(&screen, x, y, SCREEN_COLOR_WHITE, SCREEN_COLOR_MAGENTA,
                       "OpenXeChain");
    screen_draw_string(&screen, x, y + 10, SCREEN_COLOR_WHITE, SCREEN_COLOR_MAGENTA,
                       "magenta!");

    // Stay here. Returning would exit back to the dashboard.
    for (;;)
    {
        int64_t interval = -10 * 1000 * 1000; // -10 ms (100ns units)
        KeDelayExecutionThread(0, 0, &interval);
    }
}
