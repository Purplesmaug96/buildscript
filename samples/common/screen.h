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
// Xenia
// -----
// Xenia has no scanout memory: the only way to get a frame on screen is to
// drive the GPU primary ring buffer (VdInitializeRingBuffer + VdSwap +
// writing CP_RB_WPTR), like D3D9 does. When running under Xenia (detected by
// MmAllocatePhysicalMemoryEx returning a high address from the emulated
// physical heap), screen_init() instead allocates a linear front buffer as
// physical memory and screen_present() hands it to the GPU. See
// samples/common/screen.c for the details.
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

#define XENIA_RING_SIZE_LOG2  13u         // ring size = 1 << (size_log2 + 3) bytes

typedef struct screen
{
    volatile uint32_t *front_buffer; // virtual address of the mapped front buffer
    uint32_t width;                  // visible width in pixels
    uint32_t height;                 // visible height in pixels
    uint32_t pitch_pixels;           // stride between rows in pixels
    uint32_t tiles_per_row;          // ceil(pitch_pixels / 32), for tiled writes
    bool tiled;                      // whether text/shapes must use the swizzle
    bool active;                     // true once init succeeded
    bool xenia;                      // true: running under Xenia (VdSwap path)
    uint32_t xenia_fb_address;       // guest address of the front buffer (Xenia)
    uint32_t xenia_ring_address;     // guest address of the primary ring buffer (Xenia)
    uint32_t xenia_ring_wptr;        // ring buffer write index in dwords (Xenia)
    uint32_t frame_count;            // frames presented (Xenia)
    // Optional GPU-present source override: when present_base is nonzero,
    // VdSwap presents this physical surface instead of the front buffer
    // (e.g. a tiled resolve destination written by the GPU).
    uint32_t present_base;           // physical address, 0 = front buffer
    uint32_t present_w;
    uint32_t present_h;
    volatile uint32_t *xenia_rptr_page; // CP read-pointer writeback (Xenia)
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

// Presents the front buffer. Under Xenia this enqueues a VdSwap packet in
// the primary ring buffer and kicks CP_RB_WPTR; elsewhere it is a no-op
// (the real GPU scans the buffer out on its own).
void screen_present(screen_t *screen);

// Copies an off-screen render into the front buffer. `src` points at a
// 32bpp BGRA8 (byte order B,G,R,A / softpipe PIPE_FORMAT_B8G8R8A8_UNORM)
// image with `src_stride_bytes` per row, of size src_w × src_h, copied into
// the front buffer. If src_w/src_h differ from the screen size, the copy is
// an integer nearest-neighbour upscale. Byte-swapping for the scanout
// format and the Xenia RGBA8 fetch is handled here, so a GL frame can be
// moved straight in.
void screen_blit_bgra(const screen_t *screen, const void *src,
                      uint32_t src_stride_bytes, uint32_t src_w,
                      uint32_t src_h);
