#include <string.h>

#include <xecore/xboxkrnl.h>

#include "screen.h"
#include "font8x8_basic.h"

#define FRONT_BUFFER_FALLBACK 0x1E000000u // physical address used by libxenon/Xell
#define FRONT_BUFFER_SIZE     0x00400000u // 4 MiB is enough for 1080p + pitch slack
#define ATI_INFO_PAGE_PHYS    0xEC806100u // "ATI info" page (libxenon: struct ati_info)
#define ATI_INFO_PAGE_SIZE    0x00001000u

// The info page layout, from libxenon's console driver:
//   offset 0x00: reserved[4]
//   offset 0x10: base   (front buffer physical address)
//   offset 0x14: reserved[8]
//   offset 0x34: width
//   offset 0x38: height
typedef struct ati_info
{
    uint32_t reserved1[4];
    uint32_t base;
    uint32_t reserved2[8];
    uint32_t width;
    uint32_t height;
} ati_info_t;

static bool valid_dimensions(uint32_t w, uint32_t h)
{
    return w >= 320 && w <= 2560 && h >= 200 && h <= 1600;
}

bool screen_init(screen_t *screen)
{
    memset(screen, 0, sizeof(*screen));

    uint32_t fb_phys = FRONT_BUFFER_FALLBACK;

    // Try the ATI info page first: it holds the kernel's actual front buffer.
    const ati_info_t *info =
        (const ati_info_t *)MmMapIoSpace(0, ATI_INFO_PAGE_PHYS, ATI_INFO_PAGE_SIZE, 0);
    if (info && info->base != 0 && valid_dimensions(info->width, info->height))
    {
        fb_phys = info->base;
        screen->width = info->width;
        screen->height = info->height;
    }

    // Fill in the mode from the kernel if we have not already.
    if (screen->width == 0 || screen->height == 0)
    {
        VIDEO_MODE mode;
        memset(&mode, 0, sizeof(mode));
        VdQueryRealVideoMode(&mode);
        if (valid_dimensions(mode.display_width, mode.display_height))
        {
            screen->width = mode.display_width;
            screen->height = mode.display_height;
        }
        else
        {
            screen->width = 1280;
            screen->height = 720;
        }
    }

    screen->pitch_pixels = screen->width; // 32bpp scanout: pitch == width
    screen->tiles_per_row = (screen->pitch_pixels + 31) / 32;
    screen->tiled = true;

    screen->front_buffer = (volatile uint32_t *)MmMapIoSpace(0, fb_phys, FRONT_BUFFER_SIZE, 0);
    if (!screen->front_buffer)
        return false;

    screen->active = true;
    return true;
}

void screen_clear(const screen_t *screen, uint32_t color)
{
    if (!screen->active)
        return;

    volatile uint32_t *p = screen->front_buffer;
    const uint32_t total = screen->pitch_pixels * screen->height;
    for (uint32_t i = 0; i < total; i++)
        p[i] = color;
}

// Xbox 360 swizzle: 32x32-pixel tiles, each tile laid out in 4x2-pixel
// blocks, with the classic "bit 6 swap" between 8-pixel bands. This is the
// same formula libxenon uses to write to the scanout surface.
static uint32_t tiled_index(const screen_t *screen, int x, int y)
{
    const uint32_t tx = (uint32_t)x >> 5;
    const uint32_t ty = (uint32_t)y >> 5;
    const uint32_t ix = (uint32_t)x & 31;
    const uint32_t iy = (uint32_t)y & 31;

    // One tile is 32x32 pixels = 1024 pixels; tiles are laid out row-major.
    uint32_t index = (ty * screen->tiles_per_row + tx) * 1024;
    index += ((ix & 3) | ((iy & 1) << 2) | (((ix & 31) >> 2) << 3) | (((iy & 31) >> 1) << 6));
    index ^= (iy & 8) << 2;
    return index;
}

void screen_put_pixel(const screen_t *screen, int x, int y, uint32_t color)
{
    if (!screen->active || x < 0 || y < 0 ||
        (uint32_t)x >= screen->width || (uint32_t)y >= screen->height)
        return;

    if (screen->tiled)
        screen->front_buffer[tiled_index(screen, x, y)] = color;
    else
        screen->front_buffer[(uint32_t)y * screen->pitch_pixels + (uint32_t)x] = color;
}

void screen_draw_char(const screen_t *screen, int x, int y,
                      uint32_t fg, uint32_t bg, char c)
{
    const unsigned char *glyph = (const unsigned char *)font8x8_basic[(unsigned char)c];

    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            uint32_t color = (glyph[row] & (1 << (7 - col))) ? fg : bg;
            screen_put_pixel(screen, x + col, y + row, color);
        }
    }
}

void screen_draw_string(const screen_t *screen, int x, int y,
                        uint32_t fg, uint32_t bg, const char *str)
{
    while (*str)
    {
        screen_draw_char(screen, x, y, fg, bg, *str);
        x += 8;
        str++;
    }
}
