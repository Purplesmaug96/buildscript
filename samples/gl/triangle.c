// gl_triangle: a white triangle on black, via the Mesa softpipe GL backend.
//
// Demonstrates the classic immediate-mode fixed-function pipeline
// (glBegin/glVertex2f/glEnd) working on the compatibility-profile context.

#include <xecore/xboxkrnl.h>

#include <GL/gl.h>

#include "glsample.h"

void main(void)
{
    DbgPrint("gl_triangle: enter");

    glsample_t s;
    if (!glsample_init(&s))
        return;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();

    glsample_flip(&s);

    for (;;)
    {
        glsample_wait(&s);
    }
}