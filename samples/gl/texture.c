// gl_texture: a fullscreen quad textured with a checkered GL texture.
//
// Demonstrates the texture path end to end: glTexImage2D uploads the pixels
// into guest memory, the driver builds a xe_gpu_texture_fetch_t for the
// sampler, and the fixed-function fragment shader's texture() sample is
// translated into a Xenos tex fetch that xenia's texture cache reads back.

#include <xecore/xboxkrnl.h>

#include <GL/gl.h>

#include "glsample.h"

#define TW 256
#define TH 256
#define CHECK 32

static void make_checker(GLubyte *pixels)
{
    for (int y = 0; y < TH; y++)
    {
        for (int x = 0; x < TW; x++)
        {
            int on = ((x / CHECK) + (y / CHECK)) & 1;
            GLubyte *p = &pixels[(y * TW + x) * 4];
            if (on)
            {
                p[0] = 255; p[1] = 255;   p[2] = 255; p[3] = 255; /* white */
            }
            else
            {
                p[0] = 0;   p[1] = 0; p[2] = 0;   p[3] = 255; /* black */
            }
        }
    }
}

void main(void)
{
    DbgPrint("gl_texture: enter");

    glsample_t s;
    if (!glsample_init(&s))
        return;

    static GLubyte static_pixels[TW * TH * 4];
    make_checker(static_pixels);
    DbgPrint("gl_texture: checker");

    GLuint tex;
    glGenTextures(1, &tex);
    DbgPrint("gl_texture: gen");
    glBindTexture(GL_TEXTURE_2D, tex);
    DbgPrint("gl_texture: bind");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW, TH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, static_pixels);
    DbgPrint("gl_texture: teximage");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    { float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, c); }
    if (1)
        glEnable(GL_TEXTURE_2D);
    DbgPrint("gl_texture: texparams");

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    DbgPrint("gl_texture: setup done");

    for (;;)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
        glEnd();

        glsample_flip(&s);
        glsample_wait(&s);
    }
}