#include <string.h>

#include <xecore/xboxkrnl.h>

#include "screen.h"
#include "font8x8_basic.h"

// GPU command ring. Size is chosen to comfortably hold a frame's worth of
// present packets; VdSwap() consumes exactly 64 dwords per call.
#define RING_SIZE_LOG2 13u          // xenia: ring bytes = 1 << (size_log2 + 3)
#define RING_BYTES     (1u << (RING_SIZE_LOG2 + 3)) // 64 KiB
#define RING_DWORDS    (RING_BYTES / 4)             // 16384

// Number of dwords VdSwap() writes into the ring per call (the packet block).
#define VDSWAP_BLOCK_DWORDS 64u

// GPU register 0x01C5 (CP_RB_WPTR) is mapped in the MMIO register space here.
// Writing it tells the command processor how much of the ring is fresh.
#define CP_RB_WPTR (*(volatile uint32_t *)0x7FC80714u)

// An Xbox 360 texture's linear rows must be 256-byte aligned.
#define ROW_ALIGN_BYTES 256u

// D3D9-style 6-dword texture header fetch, as consumed by the GPU command
// processor (k_8_8_8_8 linear 2D texture). The Xenon CPU is big-endian, so
// plain stores already lay each dword out in the big-endian byte order the
// GPU parses fetch constants in; build_fetch() must NOT byte-swap.
typedef struct d3d_fetch
{
    uint32_t dword[6];
} d3d_fetch_t;

static void build_fetch(d3d_fetch_t *fetch, uint32_t frame_address,
                        uint32_t width, uint32_t height, uint32_t pitch_pixels)
{
    uint32_t host[6] = { 0, 0, 0, 0, 0, 0 };

    // dword 0: type 2 (kTexture) at bits 0-1, row pitch (texels >> 5) at 22-30.
    host[0] = 2u | ((pitch_pixels >> 5) << 22);
    // dword 1: format 6 (k_8_8_8_8) at bits 0-5, endianness 0 (kNone) at 6-7,
    //           base address (frame guest virtual address >> 12) at 12-31.
    host[1] = 6u | ((frame_address >> 12) << 12);
    // dword 2: size_2d: width-1 at bits 0-12, height-1 at bits 13-25.
    host[2] = (width - 1) | ((height - 1) << 13);
    // dword 3: identity swizzle R,G,B,A (0x688) at bits 1-12.
    host[3] = 0x688u << 1;
    // dword 5: dimension 1 (k2DOrStacked) at bits 9-10.
    host[5] = 1u << 9;

    for (int i = 0; i < 6; i++)
        fetch->dword[i] = host[i];
}

bool screen_init(screen_t *screen)
{
    memset(screen, 0, sizeof(*screen));

    VIDEO_MODE mode;
    memset(&mode, 0, sizeof(mode));
    VdQueryVideoMode(&mode);
    screen->width = mode.display_width;
    screen->height = mode.display_height;
    DbgPrint("screen_init: mode=%ux%u", screen->width, screen->height);
    if (screen->width < 320 || screen->width > 2560 ||
        screen->height < 200 || screen->height > 1600)
    {
        screen->width = 1280;
        screen->height = 720;
    }

    // 32bpp rows, padded up to the GPU's 256-byte linear row alignment.
    uint32_t row_bytes = (screen->width * 4 + (ROW_ALIGN_BYTES - 1)) &
                         ~(ROW_ALIGN_BYTES - 1u);
    screen->pitch_pixels = row_bytes / 4;

    uint32_t fb_size = screen->pitch_pixels * screen->height * 4;
    screen->front_buffer = (volatile uint32_t *)MmAllocatePhysicalMemoryEx(
        REGION_AUTO, fb_size, PAGE_READWRITE,
        0, 0xFFFFFFFFu, 0x1000);
    void *ring = MmAllocatePhysicalMemoryEx(
        REGION_AUTO, RING_BYTES, PAGE_READWRITE,
        0, 0xFFFFFFFFu, 0x1000);
    if (!screen->front_buffer || !ring)
    {
        DbgPrint("screen_init: allocation failed fb=%p ring=%p",
                 screen->front_buffer, ring);
        return false;
    }

    screen->frame_address = (uint32_t)(uintptr_t)screen->front_buffer;
    screen->ring_address = (uint32_t)(uintptr_t)ring;
    screen->ring_wptr = 0;

    // Make the first present a clean black frame.
    memset((void *)screen->front_buffer, 0, fb_size);

    // Tell the GPU where the command ring lives (using its physical address).
    uint32_t ring_phys = MmGetPhysicalAddress(ring);
    VdInitializeRingBuffer((void *)(uintptr_t)ring_phys, RING_SIZE_LOG2);

    screen->active = true;
    DbgPrint("screen_init: ok fb=0x%08x ring=0x%08x phys=0x%08x %ux%u p=%u",
             screen->frame_address, screen->ring_address, ring_phys,
             screen->width, screen->height, screen->pitch_pixels);
    return true;
}

void screen_present(screen_t *screen)
{
    if (!screen->active)
        return;

    uint32_t w = screen->width;
    uint32_t h = screen->height;
    uint32_t fb = screen->frame_address;
    uint32_t format = 6;       // XG_TEXTURE_FORMAT: k_8_8_8_8
    uint32_t color_space = 0;  // RGB
    uint32_t dims[2] = { w, h };

    d3d_fetch_t fetch;
    build_fetch(&fetch, fb, w, h, screen->pitch_pixels);

    // VdSwap writes a register write + XE_SWAP packet into the ring at the
    // current write position. The write pointer is free-running; the GPU
    // wraps it into the ring internally, so never mask it to ring size.
    uint32_t slot_dword = screen->ring_wptr & (RING_DWORDS - 1);
    void *ring_slot = (void *)(uintptr_t)(screen->ring_address + slot_dword * 4);

    bool verbose = screen->frame_count < 5;
    if (verbose)
        DbgPrint("present[%u]: pre-vdswap slot=%u", screen->frame_count,
                 slot_dword);

    VdSwap(ring_slot, &fetch, 0, 0, 0,
           &fb, &format, &color_space, &dims[0], &dims[1]);

    if (verbose)
        DbgPrint("present[%u]: post-vdswap", screen->frame_count);

    screen->ring_wptr += VDSWAP_BLOCK_DWORDS;
    CP_RB_WPTR = screen->ring_wptr;

    screen->frame_count++;
    if (verbose)
        DbgPrint("present[%u]: done wptr=%u", screen->frame_count,
                 screen->ring_wptr);
    else if ((screen->frame_count % 120) == 0)
        DbgPrint("present #%u wptr=%u", screen->frame_count, screen->ring_wptr);
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

void screen_put_pixel(const screen_t *screen, int x, int y, uint32_t color)
{
    if (!screen->active || x < 0 || y < 0 ||
        (uint32_t)x >= screen->width || (uint32_t)y >= screen->height)
        return;

    screen->front_buffer[(uint32_t)y * screen->pitch_pixels + (uint32_t)x] =
        color;
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