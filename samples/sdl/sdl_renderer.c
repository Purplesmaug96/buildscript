// sdl_renderer: software (SDL_Renderer) API demo.
//
// No OpenGL at all: SDL_CreateRenderer picks the built-in software renderer,
// which draws into the window surface (XRGB8888) that the xbox360 driver
// blits to the scanout on SDL_RenderPresent.  Exercises framebuffer path
// (SDL_CreateWindowFramebuffer/UpdateWindowFramebuffer) and the SDL renderer.

#include <xecore/xboxkrnl.h>

#include <SDL3/SDL.h>

void main(void)
{
    DbgPrint("sdl_renderer: enter");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DbgPrint("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_Window *window = SDL_CreateWindow("sdl_renderer", 1280, 720, 0);
    if (!window) {
        DbgPrint("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    // The xbox360 driver only ships the software renderer; asking for it by
    // name also sidesteps any hint-based driver selection.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "software");
    if (!renderer) {
        DbgPrint("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    Uint64 start = SDL_GetPerformanceCounter();

    uint32_t frame = 0;
    for (;;)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        float t = (float)((double)(now - start) / (double)SDL_GetPerformanceFrequency());

        SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
        SDL_RenderClear(renderer);

        // Bouncing magenta bar, like the classic "software renderer" demo.
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_Rect bar;
        bar.h = 120;
        bar.w = 200;
        bar.x = (int)(1280 * (0.5f + 0.5f * (float)SDL_sin((double)t * 2.0f))) - 100;
        bar.y = (int)(720 * (0.5f + 0.5f * (float)SDL_cos((double)t * 1.7f))) - 60;
        SDL_RenderFillRect(renderer, &bar);

        // A fixed green rectangle, drawn with the classic pipeline.
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_Rect rect = { 40, 40, 200, 120 };
        SDL_RenderFillRect(renderer, &rect);

        if ((frame++ % 60) == 0)
            DbgPrint("sdl_renderer: frame %u", frame);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}