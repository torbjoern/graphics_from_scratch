#include "display.h"

#include <SDL2/SDL.h>

enum eCull_method cull_method;
enum eRender_method render_method;
int window_width = 800;
int window_height = 800;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture* color_buffer_texture = NULL;
uint32_t *color_buffer = NULL;
float *z_buffer = NULL;

bool init_window(void) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_Log("Error initing SDL.\n");
        return false;
    }

    // Set size of SDL window to screen size
    SDL_DisplayMode display_mode;
    SDL_GetDisplayMode(0, 0, &display_mode);
    //window_width = display_mode.w;
    //window_height = display_mode.h;
    SDL_Log("set window %dx%d\n", window_width, window_height);


    int posX=SDL_WINDOWPOS_CENTERED;
    int posY=SDL_WINDOWPOS_CENTERED;
    window = SDL_CreateWindow(
                 NULL /*border*/,
                 posX,
                 posY,
                 window_width,
                 window_height,
                 SDL_WINDOW_BORDERLESS /*flags*/
             );
    if (!window) {
        SDL_Log("Error creating SDL window.\n");
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, 0 /*flags*/);
    if (!renderer) {
        SDL_Log("Error creating SDL renderer.\n");
        return false;
    }

    int c_bytes = sizeof(uint32_t) * window_width * window_height;
    int z_bytes = sizeof(float) * window_width * window_height;
    color_buffer = (uint32_t*)malloc(c_bytes);
    z_buffer = (float*)malloc(z_bytes);

    color_buffer_texture = SDL_CreateTexture(
                               renderer,
                               SDL_PIXELFORMAT_RGBA32,
                               SDL_TEXTUREACCESS_STREAMING,
                               window_width,
                               window_height
                           );

    return true;
}

void clear_color_buffer(uint32_t color) {
    size_t count = window_width * window_height;
    for (size_t i = 0; i < count; i++) {
        color_buffer[i] = color;
    }
}
void clear_z_buffer(float depth) {
    size_t count = window_width * window_height;
    for (size_t i = 0; i < count; i++) {
        z_buffer[i] = depth;
    }
}

void render_color_buffer(void) {
    SDL_UpdateTexture(
        color_buffer_texture,
        NULL, // rect
        color_buffer, // pixels
        (int)(window_width * sizeof(uint32_t) ) // bytes per row, pitch
    );
    SDL_RenderCopy(renderer, color_buffer_texture, NULL, NULL);

    SDL_RenderPresent(renderer);
}

void destroy_window(void)
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
