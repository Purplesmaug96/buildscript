// Minimal Direct3D-style presentation for the Xbox 360, for OpenXeChain
// samples.
//
// The frame is a normal D3D9 k_8_8_8_8 (R8G8B8A8) 32bpp surface allocated in
// physical memory and presented with the kernel's real VdSwap() call, exactly
// the way a Direct3D 9 title presents. This works identically on real
// hardware and under an emulator:
//
//   * screen_init() allocates a frame buffer and a GPU command ring,
//     then hands the ring's physical address to VdInitializeRingBuffer().
//   * screen_present() builds a D3D9-style texture-header fetch describing
//     the frame, runs it through VdSwap() at the current ring position and
//     finally advances the GPU's command-ring write pointer (CP_RB_WPTR,
//     register 0x01C5 at guest address 0x7FC80714).
//
// There is deliberately no emulator-specific code: it is the standard Xbox
// 360 presentation path.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A presented k_8_8_8_8 texel is stored as four bytes R, G, B, A with R at the
// lowest address. On this big-endian CPU a 32-bit word is therefore laid out
// as (R << 24) | (G << 16) | (B << 8) | A (so D3DCOLOR_ARGB values need their
// byte lanes swapped to match).
#define SCREEN_COLOR_RGB(r, g, b) \
    (((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) | 0xFFu)

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
    volatile uint32_t *front_buffer; // guest virtual address of the frame
    uint32_t width;                  // visible width in pixels
    uint32_t height;                 // visible height in pixels
    uint32_t pitch_pixels;           // stride between rows in pixels
    uint32_t frame_address;          // front_buffer as a guest virtual address
    uint32_t ring_address;           // guest virtual address of the command ring
    uint32_t ring_wptr;              // free-running ring write pointer (dwords)
    uint32_t frame_count;            // number of frames presented
    bool active;                     // true once init succeeded
} screen_t;

// Allocates and presents the initial (black) frame. Returns false (and leaves
// screen->active = false) if the buffers could not be set up.
bool screen_init(screen_t *screen);

// Presents the current frame contents. Call once per frame after drawing,
// preferably at (or slower than) display refresh. No-op if init failed.
void screen_present(screen_t *screen);

// Fills the whole frame with a colour.
void screen_clear(const screen_t *screen, uint32_t color);

// Plots a single pixel at (x, y).
void screen_put_pixel(const screen_t *screen, int x, int y, uint32_t color);

// Draws one 8x8 glyph; pixels outside the glyph become bg.
void screen_draw_char(const screen_t *screen, int x, int y,
                      uint32_t fg, uint32_t bg, char c);

// Draws a NUL-terminated string of 8x8 glyphs, starting at (x, y).
void screen_draw_string(const screen_t *screen, int x, int y,
                        uint32_t fg, uint32_t bg, const char *str);

#ifdef __cplusplus
} // extern "C"
#endif