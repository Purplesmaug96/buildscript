#include <string.h>

#include <xecore/xboxkrnl.h>

#include "screen.h"
#include "font8x8_basic.h"

#define FRONT_BUFFER_FALLBACK 0x1E000000u // physical address used by libxenon/Xell
#define FRONT_BUFFER_SIZE     0x00400000u // 4 MiB is enough for 1080p + pitch slack
#define ATI_INFO_PAGE_PHYS    0xEC806100u // "ATI info" page (libxenon: struct ati_info)
#define ATI_INFO_PAGE_SIZE    0x00001000u

// ---- Xenia guest-GPU presentation path -------------------------------------
//
// Xenia has no scanout memory; the only way to get a frame on screen is to
// drive the GPU "primary ring buffer" the way D3D9 does:
//
//   1. allocate the front buffer and the ring buffer as physical memory
//      (MmAllocatePhysicalMemoryEx, both land in the physical-backed heap);
//   2. hand the ring buffer's *physical* address to VdInitializeRingBuffer;
//   3. every frame, call VdSwap() at the current ring write position with a
//      D3D9-style texture header fetch describing the front buffer, then
//      write the new write pointer into CP_RB_WPTR (GPU register 0x01C5,
//      guest MMIO address 0x7FC80714).
//
// Xenia's GPU command processor picks the packet up and presents the buffer.
//
// Detection: Xenia's MmAllocatePhysicalMemoryEx hands out addresses from the
// vE0000000 physical heap (0xE0000000-0xFFD00000); real hardware RAM is
// below 0x40000000. A secondary check (VdQueryRealVideoMode leaving the
// mode zeroed) covers unusual builds.

#define XENIA_PHYS_HEAP_BASE  0x80000000u // anything at/above this is an emulator heap
#define XENIA_PAGE_READWRITE  0x00000004u // X_PAGE_READWRITE
#define XENIA_RING_BYTES      (1u << (XENIA_RING_SIZE_LOG2 + 3)) // 64 KiB
#define XENIA_RING_DWORDS     (XENIA_RING_BYTES / 4)             // 16384
#define XENIA_RING_BLOCK      64u         // dwords VdSwap consumes per call
#define XENIA_CP_RB_WPTR      (*(volatile uint32_t *)0x7FC80714u)
#define XENIA_TEXEL_ENDIANNESS 0          // fetch endianness field (0 = none, 2 = k8in32)

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

// Xenia hands out physical-backed memory from a high virtual heap; real
// kernels return RAM below 0x40000000.
static bool running_under_xenia(uint32_t fb_address)
{
    if (fb_address >= XENIA_PHYS_HEAP_BASE)
        return true;

    // Fallback: some builds leave the mode zeroed.
    VIDEO_MODE mode;
    memset(&mode, 0, sizeof(mode));
    VdQueryRealVideoMode(&mode);
    return mode.display_width == 0 && mode.display_height == 0;
}

// D3D9-style texture header fetch, laid out the way Xenia's
// xe_gpu_texture_fetch_t expects. The field bit positions below are the host
// (little-endian) ones; the guest is big-endian, so it stores each dword in
// native (big-endian) byte order. Xenia's VdSwap copies the fetch with
// copy_and_swap_32_unaligned, which byte-swaps each dword BE->LE, so storing
// anything else (e.g. a pre-swapped dword) would double-swap the fields.
typedef struct xenia_fetch
{
    uint32_t dword[6];
} xenia_fetch_t;

// Fills the fetch describing a linear 32bpp XRGB surface at fb_address.
static void xenia_build_fetch(xenia_fetch_t *fetch, uint32_t fb_address,
                              uint32_t width, uint32_t height)
{
    uint32_t pitch_bytes = width * 4; // 1280x720 32bpp: 5120 bytes/row, 256-aligned

    uint32_t host[6] = { 0, 0, 0, 0, 0, 0 };

    // dword 0: type (2 = kTexture) at bits 0-1, pitch at 22-30. Xenia reads
    // the pitch field as texels >> 5 (pixels per 32-texel row), so 1280 wide
    // -> 40, and derives the byte pitch from it (like real games' D3D9
    // headers). Bytes>>5 (160) would make Xenia think rows are 4x too wide.
    host[0] = 2u | ((width >> 5) << 22);
    // dword 1: format (6 = k_8_8_8_8, xenos TextureFormat) at bits 0-5,
    //           endianness at 6-7, base address >> 12 at bits 12-31
    host[1] = (6 /* k_8_8_8_8 */ << 0) | (XENIA_TEXEL_ENDIANNESS << 6) | ((fb_address >> 12) << 12);
    // dword 2: size_2d: width-1 at bits 0-12, height-1 at bits 13-25
    host[2] = (width - 1) | ((height - 1) << 13);
    // dword 3: identity swizzle (R, G, B, A) at bits 1-12
    host[3] = 0x688u << 1;
    // dword 5: dimension (1 = k2DOrStacked) at bits 9-10
    host[5] = 1u << 9;

    for (int i = 0; i < 6; i++)
        fetch->dword[i] = host[i];
}

static bool xenia_init(screen_t *screen, const VIDEO_MODE *mode,
                       void *fb, void *ring)
{
    screen->width = mode->display_width;
    screen->height = mode->display_height;
    screen->pitch_pixels = mode->display_width;
    screen->tiles_per_row = (mode->display_width + 31) / 32;
    screen->tiled = false; // xenia presents linear surfaces
    screen->front_buffer = (volatile uint32_t *)fb;
    screen->active = true;
    screen->xenia = true;
    screen->xenia_fb_address = (uint32_t)(uintptr_t)fb;
    screen->xenia_ring_address = (uint32_t)(uintptr_t)ring;
    screen->xenia_ring_wptr = 0;

    // The ring buffer lives in physical memory; tell the GPU where it is.
    uint32_t ring_phys = MmGetPhysicalAddress(ring);
    DbgPrint("xenia_init: fb=0x%08x ring=0x%08x ring_phys=0x%08x w=%u h=%u",
             screen->xenia_fb_address, screen->xenia_ring_address, ring_phys,
             screen->width, screen->height);
    VdInitializeRingBuffer((void *)(uintptr_t)ring_phys, XENIA_RING_SIZE_LOG2);

    return true;
}

bool screen_init(screen_t *screen)
{
    memset(screen, 0, sizeof(*screen));
    DbgPrint("screen_init: enter");

    VIDEO_MODE mode;
    memset(&mode, 0, sizeof(mode));
    VdQueryVideoMode(&mode);
    DbgPrint("screen_init: mode=%ux%u", mode.display_width, mode.display_height);
    if (!valid_dimensions(mode.display_width, mode.display_height))
    {
        mode.display_width = 1280;
        mode.display_height = 720;
    }

    // Allocate the front buffer and ring on both targets first: the returned
    // address doubles as the Xenia detector.
    uint32_t fb_size = mode.display_width * mode.display_height * 4;
    void *fb = MmAllocatePhysicalMemoryEx(REGION_AUTO, fb_size,
                                          XENIA_PAGE_READWRITE, 0, 0xFFFFFFFFu, 0x1000);
    void *ring = MmAllocatePhysicalMemoryEx(REGION_AUTO, XENIA_RING_BYTES,
                                            XENIA_PAGE_READWRITE, 0, 0xFFFFFFFFu, 0x1000);
    if (!fb || !ring)
        return false;

    if (running_under_xenia((uint32_t)(uintptr_t)fb))
        return xenia_init(screen, &mode, fb, ring);

    // ---- real hardware path ----------------------------------------------
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

// Under Xenia the GPU interprets the surface as 32bpp RGBA; make the pixel
// fully opaque so the presenter never blends it away.
static uint32_t xenia_color(const screen_t *screen, uint32_t color)
{
    if (!screen->xenia)
        return color;
    color |= 0xFFu;
#if XENIA_TEXEL_ENDIANNESS == 2
    color = __builtin_bswap32(color);
#endif
    return color;
}

void screen_clear(const screen_t *screen, uint32_t color)
{
    if (!screen->active)
        return;

    volatile uint32_t *p = screen->front_buffer;
    const uint32_t total = screen->pitch_pixels * screen->height;
    color = xenia_color(screen, color);
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
        screen->front_buffer[(uint32_t)y * screen->pitch_pixels + (uint32_t)x] =
            xenia_color(screen, color);
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

void screen_present(screen_t *screen)
{
    if (!screen->active || !screen->xenia)
        return;

    uint32_t w = screen->width;
    uint32_t h = screen->height;
    uint32_t fb = screen->xenia_fb_address;
    uint32_t format = 6; // xenos TextureFormat: k_8_8_8_8
    uint32_t color_space = 0; // RGB
    uint32_t dims[2] = { w, h };

    xenia_fetch_t fetch;
    xenia_build_fetch(&fetch, fb, w, h);

    // The write pointer is a free-running dword counter; the GPU wraps it
    // into the ring internally, so never mask it down to the ring size.
    uint32_t block = screen->xenia_ring_wptr & (XENIA_RING_DWORDS - 1);
    void *ring_slot = (void *)(uintptr_t)(screen->xenia_ring_address + block * 4);

    VdSwap(ring_slot, &fetch, 0, 0, 0, &fb, &format, &color_space, &dims[0], &dims[1]);

    screen->xenia_ring_wptr += XENIA_RING_BLOCK;
    XENIA_CP_RB_WPTR = screen->xenia_ring_wptr;

    screen->frame_count++;
    if ((screen->frame_count % 120) == 0)
        DbgPrint("present #%u wptr=%u", screen->frame_count,
                 screen->xenia_ring_wptr);
}

// Copies a 32bpp BGRA8 render target (byte order B,G,R,A, as produced by
// Mesa softpipe's B8G8R8A8 surface) into the front buffer. The scanout
// expects little-endian XRGB dwords, i.e. on the big-endian CPU the colour
// constant is (B<<24)|(G<<16)|(R<<8); Xenia's presenter reads the surface as
// RGBA8 instead, so the R and B bytes are swapped there.
// If src_w/src_h differ from the front buffer, the copy is an integer
// nearest-neighbour upscale.
void screen_blit_bgra(const screen_t *screen, const void *src,
                      uint32_t src_stride_bytes, uint32_t src_w,
                      uint32_t src_h)
{
    if (!screen->active || !src)
        return;

    const uint32_t *src32 = (const uint32_t *)src;
    uint32_t *dst = screen->front_buffer;
    uint32_t dst_pitch = screen->pitch_pixels;

    for (uint32_t y = 0; y < screen->height; y++)
    {
        uint32_t sy = (src_h == screen->height) ? y
                                                : (y * src_h) / screen->height;
        const uint32_t *srow = src32 + (sy * (src_stride_bytes / 4));
        uint32_t *drow = dst + y * dst_pitch;

        if (src_w == screen->width)
        {
            for (uint32_t x = 0; x < screen->width; x++)
            {
                uint32_t c = srow[x];
                if (screen->xenia)
                    drow[x] = (c << 8) | 0xFFu;
                else
                    drow[x] = ((c & 0xFFu) << 24) | ((c & 0xFF00u) << 8) |
                              ((c & 0xFF0000u) >> 8) | (c >> 24);
            }
        }
        else
        {
            for (uint32_t x = 0; x < screen->width; x++)
            {
                uint32_t c = srow[(x * src_w) / screen->width];
                if (screen->xenia)
                    drow[x] = (c << 8) | 0xFFu;
                else
                    drow[x] = ((c & 0xFFu) << 24) | ((c & 0xFF00u) << 8) |
                              ((c & 0xFF0000u) >> 8) | (c >> 24);
            }
        }
    }
}
