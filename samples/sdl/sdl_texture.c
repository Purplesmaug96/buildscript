// sdl_texture: fullscreen quad with a checkered GL texture via SDL3.
//
// Port of samples/gl/texture.c onto the SDL3 API.  glTexImage2D uploads the
// checker into guest memory and the texture is sampled each frame; only the
// window/context/swap come through SDL.

#include <xecore/xboxkrnl.h>

#include <SDL3/SDL.h>
#include <GL/gl.h>

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
    DbgPrint("sdl_texture: enter");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DbgPrint("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_Window *window = SDL_CreateWindow("sdl_texture", 1280, 720,
                                          SDL_WINDOW_OPENGL);
    if (!window) {
        DbgPrint("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        DbgPrint("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    SDL_GL_MakeCurrent(window, context);

    glViewport(0, 0, 1280, 720);

    static GLubyte static_pixels[TW * TH * 4];
    make_checker(static_pixels);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW, TH, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, static_pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);

    uint32_t frame = 0;
    for (;;)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
        glEnd();

        if ((frame++ % 60) == 0)
            DbgPrint("sdl_texture: frame %u", frame);

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }
}