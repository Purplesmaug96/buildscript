// sdl_magenta: the simplest possible SDL3 "hello screen".
//
// Creates the window and a GL context, fills the whole framebuffer magenta
// and swaps it once per frame.  Exercises the SDL3 driver bootstrap, window
// creation, GL context load (xbox360_create/xbox360_present) and the swap
// + scanout present path.

#include <xecore/xboxkrnl.h>

#include <SDL3/SDL.h>
#include <GL/gl.h>

void main(void)
{
    DbgPrint("sdl_magenta: enter");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DbgPrint("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_Window *window = SDL_CreateWindow("sdl_magenta", 1280, 720,
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

    uint32_t frame = 0;
    for (;;)
    {
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);   // magenta
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();

        if ((frame++ % 60) == 0)
            DbgPrint("sdl_magenta: frame %u", frame);

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }
}