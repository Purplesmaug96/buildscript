// gl_magenta: minimal GL sample - clear the frame to magenta.
#include <xecore/xboxkrnl.h>
#include <GL/gl.h>
#include "glsample.h"

void main(void)
{
    DbgPrint("gl_magenta: enter");
    glsample_t s;
    if (!glsample_init(&s))
        return;

    for (;;)
    {
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glsample_flip(&s);
        glsample_wait(&s);
    }
}
