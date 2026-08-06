// d3d9_triangle: a colour-interpolated triangle using Direct3D 9 types
//
// Uses the vendored XDK Direct3D 9 headers (xecore/d3d9.h) for the vertex
// format (a D3D9-style position + D3DCOLOR layout), colours built with
// D3DCOLOR_ARGB and a viewport described by D3DVIEWPORT9. The triangle is
// rasterized with barycentric interpolation - the same math the fixed-function
// D3D9 pipeline applies - and presented through the shared screen library.

#include <xecore/d3d9.h>
#include <xecore/xboxkrnl.h>

#include "screen.h"

// Converts a D3DCOLOR (0xAARRGGBB) to the screen library's k_8_8_8_8 byte
// order: R, G, B, A with R at the lowest address, so byte lanes are swapped.
static inline uint32_t d3dcolor_to_xrgb(D3DCOLOR color)
{
    uint32_t a = (color >> 24) & 0xFF;
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

// A D3D9-style vertex: position plus a per-vertex colour.
struct vertex
{
    float x, y;
    D3DCOLOR color;
};

// Signed area / edge function for the half-plane test.
static inline float edge(const vertex &a, const vertex &b, float px, float py)
{
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

static void fill_triangle(screen_t *screen, const vertex &v0,
                          const vertex &v1, const vertex &v2)
{
    float min_x = v0.x, max_x = v0.x;
    float min_y = v0.y, max_y = v0.y;
    const vertex *verts[3] = { &v0, &v1, &v2 };
    for (int i = 1; i < 3; i++)
    {
        if (verts[i]->x < min_x) min_x = verts[i]->x;
        if (verts[i]->x > max_x) max_x = verts[i]->x;
        if (verts[i]->y < min_y) min_y = verts[i]->y;
        if (verts[i]->y > max_y) max_y = verts[i]->y;
    }

    float area = edge(v0, v1, v2.x, v2.y); // 2x area; sign encodes winding
    if (area == 0.0f)
        return;

    int ix0 = (int)min_x, ix1 = (int)max_x;
    int iy0 = (int)min_y, iy1 = (int)max_y;
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 >= (int)screen->width)  ix1 = screen->width - 1;
    if (iy1 >= (int)screen->height) iy1 = screen->height - 1;

    for (int y = iy0; y <= iy1; y++)
    {
        for (int x = ix0; x <= ix1; x++)
        {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float w0 = edge(v1, v2, px, py);
            float w1 = edge(v2, v0, px, py);
            float w2 = edge(v0, v1, px, py);
            if (area < 0.0f) { w0 = -w0; w1 = -w1; w2 = -w2; }
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                continue;

            float rcp = 1.0f / area;
            if (area < 0.0f) rcp = -rcp;
            float b0 = w0 * rcp;
            float b1 = w1 * rcp;
            float b2 = w2 * rcp;

            // Interpolate the per-vertex colours (ARGB channels independently).
            uint32_t ar = (v0.color >> 16) & 0xFF, ag = (v0.color >> 8) & 0xFF,
                     ab = v0.color & 0xFF;
            uint32_t br = (v1.color >> 16) & 0xFF, bg = (v1.color >> 8) & 0xFF,
                     bb = v1.color & 0xFF;
            uint32_t cr = (v2.color >> 16) & 0xFF, cg = (v2.color >> 8) & 0xFF,
                     cb = v2.color & 0xFF;

            uint32_t r = (uint32_t)(b0 * (float)ar + b1 * (float)br + b2 * (float)cr);
            uint32_t g = (uint32_t)(b0 * (float)ag + b1 * (float)bg + b2 * (float)cg);
            uint32_t b = (uint32_t)(b0 * (float)ab + b1 * (float)bb + b2 * (float)cb);
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            D3DCOLOR color = D3DCOLOR_ARGB(255, r, g, b);
            screen_put_pixel(screen, x, y, d3dcolor_to_xrgb(color));
        }
    }
}

extern "C" void main(void)
{
    DbgPrint("d3d9_triangle: main enter");

    screen_t screen;
    if (!screen_init(&screen))
    {
        DbgPrint("d3d9_triangle: screen_init failed");
        return;
    }
    DbgPrint("d3d9_triangle: screen_init ok %ux%u", screen.width, screen.height);

    float cx = (float)screen.width / 2.0f;
    float cy = (float)screen.height / 2.0f;

    // A triangle with three corners in different colours.
    vertex v0 = { cx, cy - 200.0f, D3DCOLOR_ARGB(255, 255, 0, 0) };
    vertex v1 = { cx - 220.0f, cy + 180.0f, D3DCOLOR_ARGB(255, 0, 255, 0) };
    vertex v2 = { cx + 220.0f, cy + 180.0f, D3DCOLOR_ARGB(255, 0, 0, 255) };

    for (;;)
    {
        screen_clear(&screen, SCREEN_COLOR_BLACK);
        fill_triangle(&screen, v0, v1, v2);

        screen_draw_string(&screen, 8, 8, SCREEN_COLOR_WHITE, SCREEN_COLOR_BLACK,
                           "d3d9 triangle");

        // Present the rendered frame.
        screen_present(&screen);
        int64_t interval = -16 * 1000 * 1000;
        KeDelayExecutionThread(0, 0, &interval);
    }
}
