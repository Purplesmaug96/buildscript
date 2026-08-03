// Minimal framebuffer support for the Xbox 360, for OpenXeChain samples.
//
// This draws directly into the front buffer that the Xenos GPU is scanning
// out, without going through the D3D/XAPI stack. Changes appear on screen
// immediately; there is no VdSwap-style handshake involved.
//
// Front buffer location
// ---------------------
// The scanout surface traditionally lives at physical 0x1E000000 (the same
// base used by libxenon/Xell). We first try to read the real values from the
// hardware's ATI info page at physical 0xEC806100 (mapped through
// MmMapIoSpace), falling back to 0x1E000000 if the info page is not
// maintained by the running kernel.
//
// Pixel format
// ------------
// Pixels are 32bpp XRGB. The GPU reads each pixel as a little-endian dword,
// so on the big-endian PowerPC CPU the colour constants are byte-swapped:
//   color = (B << 24) | (G << 16) | (R << 8)
// (this matches libxenon's video code). E.g. magenta is 0xFF00FF00.
//
// Tiling
// ------
// Scanout surfaces are swizzled in 32x32-pixel tiles. A whole-screen fill
// works with a plain linear write either way, but drawing shapes/text needs
// the swizzle formula, which is what put_pixel() implements. Set
// screen.tiled = false before drawing if the surface turns out to be linear.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_COLOR_RGB(r, g, b) \
    (((uint32_t)(b) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8))

#define SCREEN_COLOR_BLACK   SCREEN_COLOR_RGB(0x00, 0x00, 0x00)
#define SCREEN_COLOR_WHITE   SCREEN_COLOR_RGB(0xFF, 0xFF, 0xFF)
#define SCREEN_COLOR_RED     SCREEN_COLOR_RGB(0xFF, 0x00, 0x00)
#define SCREEN_COLOR_GREEN   SCREEN_COLOR_RGB(0x00, 0xFF, 0x00)
#define SCREEN_COLOR_BLUE    SCREEN_COLOR_RGB(0x00, 0x00, 0xFF)
#define SCREEN_COLOR_YELLOW  SCREEN_COLOR_RGB(0xFF, 0xFF, 0x00)
#define SCREEN_COLOR_CYAN    SCREEN_COLOR_RGB(0x00, 0xFF, 0xFF)
#define SCREEN_COLOR_MAGENTA SCREEN_COLOR_RGB(0xFF, 0x00, 0xFF)

typedef struct screen
{
    volatile uint32_t *front_buffer; // virtual address of the mapped front buffer
    uint32_t width;                  // visible width in pixels
    uint32_t height;                 // visible height in pixels
    uint32_t pitch_pixels;           // stride between rows in pixels
    uint32_t tiles_per_row;          // ceil(pitch_pixels / 32), for tiled writes
    bool tiled;                      // whether text/shapes must use the swizzle
    bool active;                     // true once init succeeded
} screen_t;

// Maps the front buffer and fills in the video mode. Returns false (and
// leaves screen->active = false) if the buffer could not be mapped.
bool screen_init(screen_t *screen);

// Fills the whole screen with a colour. Works for tiled and linear surfaces.
void screen_clear(const screen_t *screen, uint32_t color);

// Plots a single pixel at (x, y).
void screen_put_pixel(const screen_t *screen, int x, int y, uint32_t color);

// Draws one 8x8 glyph; the background colour is only written for pixels
// inside the glyph's bounding box (set bg == SCREEN_COLOR_BLACK for a
// transparent look on a black screen).
void screen_draw_char(const screen_t *screen, int x, int y,
                      uint32_t fg, uint32_t bg, char c);

// Draws a NUL-terminated string of 8x8 glyphs, starting at (x, y).
void screen_draw_string(const screen_t *screen, int x, int y,
                        uint32_t fg, uint32_t bg, const char *str);
