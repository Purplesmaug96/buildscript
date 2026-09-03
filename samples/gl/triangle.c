// gl_triangle: hardware-accelerated magenta triangle via real OpenGL.
//
// Classic fixed-function pipeline (glClear + glBegin/glVertex/glEnd) on a
// Mesa compatibility context.  With the xenos backend the whole frame -
// draw, resolve and present - runs on the GPU through PM4; with softpipe
// it falls back to the CPU blit path.

#include <xecore/xboxkrnl.h>

#include <GL/gl.h>

#include "glsample.h"

void main(void)
{
    DbgPrint("gl_triangle: enter");

    glsample_t s;
    if (!glsample_init(&s))
        return;

    /* The default GL viewport is (0,0,0,0); without a real viewport the
     * NDC->pixel transform has zero scale, so no fragments are rasterized. */
    glViewport(0, 0, s.screen.width, s.screen.height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    uint32_t frame = 0;
    for (;;)
    {
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);   // red clear (diagnostic)
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 1.0f);   // magenta
        glVertex2f(-0.6f, -0.6f);
        glVertex2f( 0.6f, -0.6f);
        glVertex2f( 0.0f,  0.6f);
        glEnd();

        if ((frame++ % 30) == 0)
            DbgPrint("gl_triangle: frame %u", frame);

        glsample_flip(&s);
        glsample_wait(&s);
    }
}
