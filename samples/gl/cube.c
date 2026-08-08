// gl_cube: a spinning cube, via the classic GL fixed-function pipeline.
//
// Demonstrates a full compatibility-profile scene: matrix push/pop,
// glFrustum projection, per-frame glRotatef animation driven by the kernel
// system timer, a depth buffer and immediate-mode triangle vertices.

#include <xecore/xboxkrnl.h>

#include <GL/gl.h>

#include "glsample.h"

// Unit cube corners (x, y, z).
static const GLfloat g_cube_verts[8][3] = {
    { -1.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, -1.0f },
    { -1.0f, 1.0f, -1.0f },  { -1.0f, -1.0f, 1.0f },  { 1.0f, -1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },    { -1.0f, 1.0f, 1.0f },
};

// 12 triangles, each a face quad (CCW).
static const unsigned char cube_tris[12][3] = {
    { 0, 1, 2 }, { 0, 2, 3 }, /* -Z face */
    { 4, 6, 5 }, { 4, 7, 6 }, /* +Z face */
    { 0, 4, 5 }, { 0, 5, 1 }, /* -Y face */
    { 1, 5, 6 }, { 1, 6, 2 }, /* +X face */
    { 3, 2, 6 }, { 3, 6, 7 }, /* +Y face */
    { 3, 7, 4 }, { 3, 4, 0 }, /* -X face */
};

static void draw_cube(void)
{
    static const GLfloat face_colors[6][3] = {
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f },
    };

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 12; i++)
    {
        const GLfloat *color = face_colors[i / 2];
        glColor3f(color[0], color[1], color[2]);
        for (int v = 0; v < 3; v++)
        {
            const GLfloat *p = g_cube_verts[cube_tris[i][v]];
            glVertex3f(p[0], p[1], p[2]);
        }
    }
    glEnd();
}

void main(void)
{
    DbgPrint("gl_cube: enter");

    glsample_t s;
    if (!glsample_init(&s))
        return;

    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.5f, 20.0f);

    // Start time: rotate ~45 degrees per second, glued to the wall clock.
    int64_t start;
    KeQuerySystemTime(&start);

    for (;;)
    {
        int64_t now;
        KeQuerySystemTime(&now);
        float t = (float)((now - start) * 1e-7);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -6.0f);
        glRotatef(t * 45.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(t * 65.0f, 0.0f, 1.0f, 0.0f);

        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        draw_cube();
        glsample_flip(&s);

        glsample_wait(&s);
    }
}