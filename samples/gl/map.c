// map5-probe: full quad, 6 unique colors.

#include <xecore/xboxkrnl.h>
#include <GL/gl.h>
#include "glsample.h"

void main(void)
{
    DbgPrint("gl_map: enter (single-draw quad)");

    glsample_t s;
    if (!glsample_init(&s))
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (;;)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        /* interior triangle probe (map-probe33): colors M/G/Y.
         * v0 MAGENTA (0.0,0.2)  | v1 GREEN (-0.9,-0.8) | v2 YELLOW (0.9,0.8)
         * pos.x = {0.0,-0.9,0.9} -> floor = {0,-1,0}
         * vertex-index model predicts colors {M,G,Y};
         * floor(pos.x) model predicts {mem@base-24, M, M} */
        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 1.0f);
        glVertex3f(0.0f, 0.2f, 0.0f);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-0.9f, -0.8f, 0.0f);
        glColor3f(1.0f, 1.0f, 0.0f);
        glVertex3f(0.9f, 0.8f, 0.0f);
        glEnd();

        glsample_flip(&s);
        glsample_wait(&s);
    }
}