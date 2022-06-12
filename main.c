#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <SDL2/SDL.h>

#include "image.h"

void scc(int code)
{
    if (code < 0)
    {
        fprintf(stderr, "ERROR: SDL pooped itself: %s\n",
                SDL_GetError());
        exit(1);
    }
}

void *sccp(void *ptr)
{
    if (ptr == NULL) {
        fprintf(stderr, "SDL pooped itself: %s\n", SDL_GetError());
        abort();
    }

    return ptr;
}

typedef enum {
    MODE_ASCENDING,
    MODE_COUNTDOWN,
    MODE_CLOCK
} Mode;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* digits;
    int char_x;
    int char_y;
    int sprite;
    bool quit;
    bool paused;
    Mode mode;
    float displayed_time;
    float sprite_cooldown;
    float fit_scale;
    float user_scale;
} Context;

#define COLON_INDEX         10
#define FPS                 60
#define DELTA_TIME         (1.0f / FPS)
#define MAIN_COLOR_R        220
#define MAIN_COLOR_G        220
#define MAIN_COLOR_B        220
#define BACKGROUND_COLOR_R  24
#define BACKGROUND_COLOR_G  24
#define BACKGROUND_COLOR_B  24
#define PBACKGROUND_COLOR_R 40
#define PBACKGROUND_COLOR_G 24
#define PBACKGROUND_COLOR_B 24
#define WIDTH               800
#define HEIGHT              600
#define SPRITE_CHAR_WIDTH  (300 / 2)
#define SPRITE_CHAR_HEIGHT (380 / 2)
#define CHAR_WIDTH         (300 / 2)
#define CHAR_HEIGHT        (380 / 2)
#define CHARS_COUNT         8
#define TEXT_WIDTH         (CHAR_WIDTH * CHARS_COUNT)
#define TEXT_HEIGHT        (CHAR_HEIGHT)
#define SPRITE_DURATION    (0.40f / SPRITE_COUNT)
#define SPRITE_COUNT        3

SDL_Surface* load_png_file_as_surface()
{
    SDL_Surface* image_surface =
        sccp(SDL_CreateRGBSurfaceFrom(
                 png,
                 (int) png_width,
                 (int) png_height,
                 32,
                 (int) png_width * 4,
                 0x000000FF,
                 0x0000FF00,
                 0x00FF0000,
                 0xFF000000));
    return image_surface;
}

SDL_Texture* load_png_file_as_texture(Context* context)
{
    SDL_Surface* image_surface = load_png_file_as_surface();
    return sccp(SDL_CreateTextureFromSurface(context->renderer, image_surface));
}

void context_init_sdl(Context* context)
{
    scc(SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"));
    scc(SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0"));

    context->window = SDL_CreateWindow("Counter", 0, 0,
                                       WIDTH,
                                       HEIGHT,
                                       SDL_WINDOW_RESIZABLE);
    context->renderer = SDL_CreateRenderer(
        context->window, -1,
        SDL_RENDERER_PRESENTVSYNC
        | SDL_RENDERER_ACCELERATED);

    context->digits = load_png_file_as_texture(context);
    scc(SDL_SetTextureColorMod(context->digits, MAIN_COLOR_R, MAIN_COLOR_G, MAIN_COLOR_B));

    context->sprite_cooldown = SPRITE_DURATION;
    context->user_scale = 1;
}

void context_init_coordinates(Context* context)
{
    int w, h;
    SDL_GetWindowSize(context->window, &w, &h);

    float text_aspect_ratio = (float) TEXT_WIDTH / (float) TEXT_HEIGHT;
    float window_aspect_ratio = (float) w / (float) h;
    if(text_aspect_ratio > window_aspect_ratio) {
        context->fit_scale = (float) w / (float) TEXT_WIDTH;
    } else {
        context->fit_scale = (float) h / (float) TEXT_HEIGHT;
    }

    const int effective_digit_width = (int) floorf((float) CHAR_WIDTH * context->user_scale * context->fit_scale);
    const int effective_digit_height = (int) floorf((float) CHAR_HEIGHT * context->user_scale * context->fit_scale);
    context->char_x = w / 2 - effective_digit_width * CHARS_COUNT / 2;
    context->char_y = h / 2 - effective_digit_height / 2;
}

void context_render_char(Context* context, int number)
{
    if (context->sprite >= 3) context->sprite = 0;

    const int effective_digit_width = (int) floorf((float) CHAR_WIDTH * context->user_scale * context->fit_scale);
    const int effective_digit_height = (int) floorf((float) CHAR_HEIGHT * context->user_scale * context->fit_scale);
    const SDL_Rect src = {
        (int) (number * SPRITE_CHAR_WIDTH),
        (int) (context->sprite * SPRITE_CHAR_HEIGHT),
        SPRITE_CHAR_WIDTH,
        SPRITE_CHAR_HEIGHT
    };
    const SDL_Rect dst = {
        context->char_x,
        context->char_y,
        effective_digit_width,
        effective_digit_height
    };
    SDL_RenderCopy(context->renderer, context->digits, &src, &dst);
    context->char_x += effective_digit_width;
}

float parse_time(const char* time)
{
    float result = 0.0f;

    while (*time) {
        char* endptr = NULL;
        float x = strtof(time, &endptr);

        if (time == endptr) {
            fprintf(stderr, "ERROR: `%s` is not a number\n", time);
            exit(1);
        }

        switch (*endptr) {
        case '\0':
        case 's': result += x;                 break;
        case 'm': result += x * 60.0f;         break;
        case 'h': result += x * 60.0f * 60.0f; break;
        default:
            fprintf(stderr, "ERROR: `%c` is an unknown time unit\n", *endptr);
            exit(1);
        }

        time = endptr;
        if (*time) time += 1;
    }

    return result;
}

void usage(FILE* stream)
{
    fprintf(stream, "Usage: count [SUBCOMMAND] [TIME]\n");
    fprintf(stream, "    Subcommands:\n");
    fprintf(stream, "        clock    Change to clock mode\n");
    fprintf(stream, "        help     Print this help and exit with exit code 0\n");
    fprintf(stream, "        pause    Start the counter paused\n");
    fprintf(stream, "\n");
    fprintf(stream, "    Any other kind of subcommand will be interpreted as TIME\n");
}

int main(int argc, const char** argv)
{
    float displayed_time = 0.0f;
    bool paused = false;
    Mode mode = MODE_ASCENDING;

    for (size_t i = 1; i < (size_t) argc; ++i)
    {
        if (strcmp(argv[i], "clock") == 0) {
            mode = MODE_CLOCK;
        } else if (strcmp(argv[i], "help") == 0) {
            usage(stdout);
            exit(0);
        } else if (strcmp(argv[i], "pause") == 0) {
            paused = true;
        } else {
            mode = MODE_COUNTDOWN;
            displayed_time = parse_time(argv[i]);
        }
    }

    scc(SDL_Init(SDL_INIT_EVERYTHING));

    Context context = {0};
    context_init_sdl(&context);

    context.displayed_time = displayed_time;
    context.mode = mode;
    context.paused = paused;

    while (!context.quit)
    {
        SDL_Event event = {0};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                context.quit = true;
                break;
            case SDL_KEYDOWN: {
                switch (event.key.keysym.sym) {
                case SDLK_SPACE:
                    context.paused = !context.paused;
                    break;
                case SDLK_F11: {
                    Uint32 window_flags;
                    scc(window_flags = SDL_GetWindowFlags(context.window));
                    if (window_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        scc(SDL_SetWindowFullscreen(context.window, 0));
                    } else {
                        scc(SDL_SetWindowFullscreen(context.window, SDL_WINDOW_FULLSCREEN_DESKTOP));
                    }
                } break;
                }
            } break;
            }
        }

        SDL_RenderClear(context.renderer);
        {
            context.fit_scale = 1.0f;
            context_init_coordinates(&context);

            size_t t = (size_t) ceilf(fmaxf(context.displayed_time, 0.0f));

            const size_t hours = t / 60 / 60;
            context_render_char(&context, hours / 10);
            context_render_char(&context, hours % 10);
            context_render_char(&context, COLON_INDEX);

            const size_t minutes = t / 60 % 60;
            context_render_char(&context, minutes / 10);
            context_render_char(&context, minutes % 10);
            context_render_char(&context, COLON_INDEX);

            const size_t seconds = t % 60;
            context_render_char(&context, seconds / 10);
            context_render_char(&context, seconds % 10);
        }
        SDL_RenderPresent(context.renderer);

        if (context.sprite_cooldown <= 0.0f) {
            context.sprite++;
            context.sprite_cooldown = SPRITE_DURATION;
        }
        context.sprite_cooldown -= DELTA_TIME;

        if (!context.paused) {
            SDL_SetRenderDrawColor(context.renderer,
                                   BACKGROUND_COLOR_R,
                                   BACKGROUND_COLOR_G,
                                   BACKGROUND_COLOR_B,
                                   255);

            switch (context.mode) {
            case MODE_ASCENDING: {
                context.displayed_time += DELTA_TIME;
            } break;
            case MODE_COUNTDOWN: {
                if (context.displayed_time > 1e-6) {
                    context.displayed_time -= DELTA_TIME;
                } else {
                    context.displayed_time = 0.0f;
                }
            } break;
            case MODE_CLOCK: {
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                context.displayed_time = tm->tm_sec
                               + tm->tm_min  * 60.0f
                               + tm->tm_hour * 60.0f * 60.0f;
                break;
            }
            }
        } else {
            SDL_SetRenderDrawColor(context.renderer,
                                   PBACKGROUND_COLOR_R,
                                   PBACKGROUND_COLOR_G,
                                   PBACKGROUND_COLOR_B,
                                   255);
        }

        SDL_Delay((int) floorf(DELTA_TIME * 1000.0f));
    }

    SDL_Quit();
    return 0;
}
