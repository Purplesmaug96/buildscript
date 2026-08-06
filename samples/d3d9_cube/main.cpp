// d3d9_cube: a rotating 3D cube using Direct3D 9 types
//
// Uses the vendored XDK Direct3D 9 headers (xecore/d3d9.h) for colour
// handling (D3DCOLOR, D3DCOLOR_ARGB) and the D3D9 4-component vector
// (D3DXVECTOR4 from d3d9types.h) for the 3D maths. The cube's 12 edges are
// transformed, perspective-projected and rasterized as lines, then presented
// through the shared screen library.

#include <xecore/d3d9.h>
#include <xecore/xboxkrnl.h>
#include <math.h>

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

struct vec3
{
    float x, y, z;
};

// Draws a line between two projected points using a DDA.
static void draw_line(screen_t *screen, float x0, float y0,
                      float x1, float y1, uint32_t color)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    int steps = (int)((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy)
                          ? (dx < 0 ? -dx : dx)
                          : (dy < 0 ? -dy : dy));
    if (steps < 1)
        steps = 1;
    float xinc = dx / (float)steps;
    float yinc = dy / (float)steps;
    float x = x0;
    float y = y0;
    for (int i = 0; i <= steps; i++)
    {
        screen_put_pixel(screen, (int)(x + 0.5f), (int)(y + 0.5f), color);
        x += xinc;
        y += yinc;
    }
}

extern "C" void main(void)
{
    DbgPrint("d3d9_cube: main enter");

    screen_t screen;
    if (!screen_init(&screen))
    {
        DbgPrint("d3d9_cube: screen_init failed");
        return;
    }
    DbgPrint("d3d9_cube: screen_init ok %ux%u", screen.width, screen.height);

    const float cx = (float)screen.width / 2.0f;
    const float cy = (float)screen.height / 2.0f;
    const float focal = (float)screen.height; // simple perspective focal length

    // A unit cube centred on the origin.
    const float h = 1.5f;
    vec3 corners[8] = {
        { -h, -h, -h }, { h, -h, -h }, { h, h, -h }, { -h, h, -h },
        { -h, -h,  h }, { h, -h,  h }, { h, h,  h }, { -h, h,  h },
    };
    // The 12 edges of a cube, as index pairs.
    const int edges[12][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };

    D3DCOLOR edge_color = D3DCOLOR_ARGB(255, 255, 255, 0); // yellow

    uint32_t frame = 0;
    for (;;)
    {
        screen_clear(&screen, SCREEN_COLOR_BLACK);

        float angle = (float)(frame % 3600) * 0.001f; // ~0.1 deg/frame
        float ca = cosf(angle);
        float sa = sinf(angle);

        // Rotate around Y then X, then push the cube down the +Z axis.
        vec3 world[8];
        for (int i = 0; i < 8; i++)
        {
            float x = corners[i].x;
            float y = corners[i].y;
            float z = corners[i].z;
            float xr = x * ca - z * sa;
            float zr = x * sa + z * ca;
            float yr = y * ca - zr * sa;
            zr = y * sa + zr * ca;
            world[i] = { xr, yr, zr + 5.0f };
        }

        // Project and draw the edges.
        for (int e = 0; e < 12; e++)
        {
            vec3 a = world[edges[e][0]];
            vec3 b = world[edges[e][1]];
            float ax = cx + a.x * focal / a.z;
            float ay = cy - a.y * focal / a.z;
            float bx = cx + b.x * focal / b.z;
            float by = cy - b.y * focal / b.z;
            draw_line(&screen, ax, ay, bx, by, d3dcolor_to_xrgb(edge_color));
        }

        screen_draw_string(&screen, 8, 8, SCREEN_COLOR_WHITE, SCREEN_COLOR_BLACK,
                           "d3d9 cube");

        // Present the rendered frame.
        screen_present(&screen);
        int64_t interval = -16 * 1000 * 1000;
        KeDelayExecutionThread(0, 0, &interval);

        frame++;
    }
}
