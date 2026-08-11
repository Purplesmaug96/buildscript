// gl_magenta: magenta clear, via the Mesa softpipe GL backend.
//
// The classic "is the framebuffer alive?" test, but rendered with real GL
// entry points: it creates an off-screen compatibility-profile GL context
// (xbox360_create), clears it to magenta with glClearColor/glClear, and
// blits the RGBA8 result into the scanout front buffer.

#include <xecore/xboxkrnl.h>

#include <GL/gl.h>

#include "glsample.h"

void main(void)
{
    DbgPrint("gl_magenta: enter");

    glsample_t s;
    if (!glsample_init(&s))
        return;

    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    DbgPrint("gl_magenta: post-glClear");

    glsample_flip(&s);
    DbgPrint("gl_magenta: post-flip");

    for (;;)
    {
        glsample_wait(&s);
    }
}