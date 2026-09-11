// sdl_triangle: magenta triangle rendered through SDL3.
//
// The xbox360 SDL3 video driver owns the scanout; the window is created with
// SDL_CreateWindow and the GL context with SDL_GL_CreateContext, then the
// classic fixed-function pipeline runs exactly like samples/gl/triangle.c and
// every frame is swapped with SDL_GL_SwapWindow.

#include <xecore/xboxkrnl.h>

#include <SDL3/SDL.h>
#include <GL/gl.h>

void main(void)
{
    DbgPrint("sdl_triangle: enter");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DbgPrint("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_Window *window = SDL_CreateWindow("sdl_triangle", 1280, 720,
                                          SDL_WINDOW_OPENGL);
    if (!window) {
        DbgPrint("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    // The xbox360 backend exposes a single Mesa compatibility context
    // (2.1); these attributes describe it so apps that check them work.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        DbgPrint("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    SDL_GL_MakeCurrent(window, context);

    glViewport(0, 0, 1280, 720);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
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
            DbgPrint("sdl_triangle: frame %u", frame);

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }
}