// sdl_cube: spinning cube via SDL3 + classic GL fixed function pipeline.
//
// Port of samples/gl/cube.c onto the SDL3 API: the window/context come from
// SDL_* calls and the rotation clock from the SDL performance counter, which
// on this target is the kernel system timer (100 ns ticks).

#include <xecore/xboxkrnl.h>

#include <SDL3/SDL.h>
#include <GL/gl.h>

// Unit cube corners (x, y, z).
static const GLfloat g_cube_verts[8][3] = {
    { -1.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, -1.0f },
    { -1.0f, 1.0f, -1.0f },  { -1.0f, -1.0f, 1.0f },  { 1.0f, -1.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f },    { -1.0f, 1.0f, 1.0f },
};

// 12 triangles, each a face quad (CCW).
static const unsigned char cube_tris[12][3] = {
    { 0, 1, 2 }, { 0, 2, 3 }, /* -Z face */
    { 4, 6, 5 }, { 4, 7, 6 }, /* +Z face */
    { 0, 4, 5 }, { 0, 5, 1 }, /* -Y face */
    { 1, 5, 6 }, { 1, 6, 2 }, /* +X face */
    { 3, 2, 6 }, { 3, 6, 7 }, /* +Y face */
    { 3, 7, 4 }, { 3, 4, 0 }, /* -X face */
};

static void draw_cube(void)
{
    static const GLfloat face_colors[6][3] = {
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f },
    };

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 12; i++)
    {
        const GLfloat *color = face_colors[i / 2];
        glColor3f(color[0], color[1], color[2]);
        for (int v = 0; v < 3; v++)
        {
            const GLfloat *p = g_cube_verts[cube_tris[i][v]];
            glVertex3f(p[0], p[1], p[2]);
        }
    }
    glEnd();
}

void main(void)
{
    DbgPrint("sdl_cube: enter");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DbgPrint("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_Window *window = SDL_CreateWindow("sdl_cube", 1280, 720,
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
    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.5f, 20.0f);

    // Start time: rotate ~45 degrees per second, glued to the wall clock
    // (SDL performance counter = KeQuerySystemTime, 100 ns ticks).
    Uint64 start = SDL_GetPerformanceCounter();

    uint32_t frame = 0;
    for (;;)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        float t = (float)((double)(now - start) / (double)SDL_GetPerformanceFrequency());

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -6.0f);
        glRotatef(t * 45.0f, 1.0f, 0.0f, 0.0f);
        glRotatef(t * 65.0f, 0.0f, 1.0f, 0.0f);

        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        draw_cube();

        if ((frame++ % 60) == 0)
            DbgPrint("sdl_cube: frame %u", frame);

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }
}