#include "../include/mz.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static int g_running;
static int g_mouse_x;
static int g_mouse_y;
static int g_mouse_rel_x;
static int g_mouse_rel_y;
static int g_mouse_left;
static int g_mouse_right;
static unsigned char g_keys[SDL_SCANCODE_COUNT];
static Uint8 g_r = 255, g_g = 255, g_b = 255, g_a = 255;

typedef struct
{
    SDL_Texture *texture;
    float w;
    float h;
} MzImage;

static MzImage *g_images;
static int g_image_count;
static int g_image_cap;

static int as_int(MzValue v)
{
    return (int)mz_as_number(v);
}

static char *as_text(MzValue v)
{
    return mz_to_cstring(v);
}

static int need_renderer(MzVM *vm, const char *name)
{
    (void)name;
    (void)vm;
    return g_renderer != NULL;
}

static void free_images(void)
{
    for (int i = 0; i < g_image_count; i++)
    {
        if (g_images[i].texture)
            SDL_DestroyTexture(g_images[i].texture);
    }
    free(g_images);
    g_images = NULL;
    g_image_count = 0;
    g_image_cap = 0;
}

static int image_slot(void)
{
    for (int i = 0; i < g_image_count; i++)
    {
        if (!g_images[i].texture)
            return i;
    }
    if (g_image_count + 1 > g_image_cap)
    {
        int next = g_image_cap < 16 ? 16 : g_image_cap * 2;
        MzImage *items = (MzImage *)realloc(g_images, sizeof(MzImage) * (size_t)next);
        if (!items)
            return -1;
        for (int i = g_image_cap; i < next; i++)
        {
            items[i].texture = NULL;
            items[i].w = 0;
            items[i].h = 0;
        }
        g_images = items;
        g_image_cap = next;
    }
    return g_image_count++;
}

static MzImage *get_image(int id)
{
    int i = id - 1;
    if (i < 0 || i >= g_image_count || !g_images[i].texture)
        return NULL;
    return &g_images[i];
}

static MzValue s_window(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    const char *title = "ManScript Zero";
    int w = 800, h = 600;
    char *owned = NULL;
    if (argc > 0 && argv[0].type == MZ_STRING)
    {
        owned = as_text(argv[0]);
        title = owned;
        if (argc > 1)
            w = as_int(argv[1]);
        if (argc > 2)
            h = as_int(argv[2]);
    }
    else
    {
        if (argc > 0)
            w = as_int(argv[0]);
        if (argc > 1)
            h = as_int(argv[1]);
        if (argc > 2)
        {
            owned = as_text(argv[2]);
            title = owned;
        }
    }
    free_images();
    if (g_renderer)
        SDL_DestroyRenderer(g_renderer);
    if (g_window)
        SDL_DestroyWindow(g_window);
    g_renderer = NULL;
    g_window = NULL;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        free(owned);
        return mz_bool(0);
    }
    g_window = SDL_CreateWindow(title, w, h, 0);
    free(owned);
    if (!g_window)
        return mz_bool(0);
    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (!g_renderer)
    {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return mz_bool(0);
    }
    SDL_SetRenderVSync(g_renderer, 1);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    g_running = 1;
    return mz_bool(1);
}

static MzValue s_logical_size(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (!g_renderer || argc < 2)
        return mz_bool(0);
    int w = as_int(argv[0]);
    int h = as_int(argv[1]);
    int stretch = argc > 2 ? mz_truthy(argv[2]) : 1;
    SDL_RendererLogicalPresentation mode = stretch ? SDL_LOGICAL_PRESENTATION_STRETCH : SDL_LOGICAL_PRESENTATION_LETTERBOX;
    return mz_bool(SDL_SetRenderLogicalPresentation(g_renderer, w, h, mode));
}

static MzValue s_poll(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    g_mouse_rel_x = 0;
    g_mouse_rel_y = 0;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
            g_running = 0;
        else if (e.type == SDL_EVENT_KEY_DOWN)
            g_keys[e.key.scancode] = 1;
        else if (e.type == SDL_EVENT_KEY_UP)
            g_keys[e.key.scancode] = 0;
        else if (e.type == SDL_EVENT_MOUSE_MOTION)
        {
            g_mouse_x = (int)e.motion.x;
            g_mouse_y = (int)e.motion.y;
            g_mouse_rel_x += (int)e.motion.xrel;
            g_mouse_rel_y += (int)e.motion.yrel;
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (e.button.button == SDL_BUTTON_LEFT)
                g_mouse_left = 1;
            if (e.button.button == SDL_BUTTON_RIGHT)
                g_mouse_right = 1;
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            if (e.button.button == SDL_BUTTON_LEFT)
                g_mouse_left = 0;
            if (e.button.button == SDL_BUTTON_RIGHT)
                g_mouse_right = 0;
        }
    }
    return mz_bool(g_running);
}

static MzValue s_running(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_bool(g_running);
}

static MzValue s_quit(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    g_running = 0;
    return mz_null();
}

static MzValue s_key(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_bool(0);
    char *name = as_text(argv[0]);
    SDL_Scancode sc = SDL_GetScancodeFromName(name);
    if (sc == SDL_SCANCODE_UNKNOWN && strlen(name) == 1)
    {
        char up[2] = {name[0], 0};
        if (up[0] >= 'a' && up[0] <= 'z')
            up[0] = (char)(up[0] - 32);
        sc = SDL_GetScancodeFromName(up);
    }
    free(name);
    if (sc == SDL_SCANCODE_UNKNOWN)
        return mz_bool(0);
    return mz_bool(g_keys[sc] != 0);
}

static MzValue s_clear(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "clear"))
        return mz_null();
    int r = argc > 0 ? as_int(argv[0]) : 0;
    int g = argc > 1 ? as_int(argv[1]) : 0;
    int b = argc > 2 ? as_int(argv[2]) : 0;
    SDL_SetRenderDrawColor(g_renderer, (Uint8)r, (Uint8)g, (Uint8)b, 255);
    SDL_RenderClear(g_renderer);
    return mz_null();
}

static MzValue s_color(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    g_r = (Uint8)(argc > 0 ? as_int(argv[0]) : 255);
    g_g = (Uint8)(argc > 1 ? as_int(argv[1]) : 255);
    g_b = (Uint8)(argc > 2 ? as_int(argv[2]) : 255);
    g_a = (Uint8)(argc > 3 ? as_int(argv[3]) : 255);
    return mz_null();
}

static void set_color(void)
{
    SDL_SetRenderDrawColor(g_renderer, g_r, g_g, g_b, g_a);
}

static MzValue s_rect(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "rect") || argc < 4)
        return mz_null();
    SDL_FRect r;
    r.x = (float)mz_as_number(argv[0]);
    r.y = (float)mz_as_number(argv[1]);
    r.w = (float)mz_as_number(argv[2]);
    r.h = (float)mz_as_number(argv[3]);
    set_color();
    SDL_RenderFillRect(g_renderer, &r);
    return mz_null();
}

static MzValue s_rect_line(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "rect_line") || argc < 4)
        return mz_null();
    SDL_FRect r;
    r.x = (float)mz_as_number(argv[0]);
    r.y = (float)mz_as_number(argv[1]);
    r.w = (float)mz_as_number(argv[2]);
    r.h = (float)mz_as_number(argv[3]);
    set_color();
    SDL_RenderRect(g_renderer, &r);
    return mz_null();
}

static MzValue s_line(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "line") || argc < 4)
        return mz_null();
    set_color();
    SDL_RenderLine(g_renderer, (float)mz_as_number(argv[0]), (float)mz_as_number(argv[1]), (float)mz_as_number(argv[2]), (float)mz_as_number(argv[3]));
    return mz_null();
}

static MzValue s_point(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "point") || argc < 2)
        return mz_null();
    set_color();
    SDL_RenderPoint(g_renderer, (float)mz_as_number(argv[0]), (float)mz_as_number(argv[1]));
    return mz_null();
}

static MzValue s_circle(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "circle") || argc < 3)
        return mz_null();
    int cx = as_int(argv[0]), cy = as_int(argv[1]), r = as_int(argv[2]);
    int fill = argc < 4 ? 1 : as_int(argv[3]);
    set_color();
    for (int y = -r; y <= r; y++)
    {
        for (int x = -r; x <= r; x++)
        {
            int d = x * x + y * y;
            if ((fill && d <= r * r) || (!fill && d <= r * r && d >= (r - 2) * (r - 2)))
            {
                SDL_RenderPoint(g_renderer, (float)(cx + x), (float)(cy + y));
            }
        }
    }
    return mz_null();
}

static unsigned char text_glyph(char ch, int row)
{
    if (ch >= 'a' && ch <= 'z')
        ch = (char)(ch - 32);
    switch (ch)
    {
    case 'A':
    {
        static unsigned char p[7] = {14, 17, 17, 31, 17, 17, 17};
        return p[row];
    }
    case 'B':
    {
        static unsigned char p[7] = {30, 17, 17, 30, 17, 17, 30};
        return p[row];
    }
    case 'C':
    {
        static unsigned char p[7] = {14, 17, 16, 16, 16, 17, 14};
        return p[row];
    }
    case 'D':
    {
        static unsigned char p[7] = {30, 17, 17, 17, 17, 17, 30};
        return p[row];
    }
    case 'E':
    {
        static unsigned char p[7] = {31, 16, 16, 30, 16, 16, 31};
        return p[row];
    }
    case 'F':
    {
        static unsigned char p[7] = {31, 16, 16, 30, 16, 16, 16};
        return p[row];
    }
    case 'G':
    {
        static unsigned char p[7] = {14, 17, 16, 23, 17, 17, 14};
        return p[row];
    }
    case 'H':
    {
        static unsigned char p[7] = {17, 17, 17, 31, 17, 17, 17};
        return p[row];
    }
    case 'I':
    {
        static unsigned char p[7] = {14, 4, 4, 4, 4, 4, 14};
        return p[row];
    }
    case 'J':
    {
        static unsigned char p[7] = {7, 2, 2, 2, 18, 18, 12};
        return p[row];
    }
    case 'K':
    {
        static unsigned char p[7] = {17, 18, 20, 24, 20, 18, 17};
        return p[row];
    }
    case 'L':
    {
        static unsigned char p[7] = {16, 16, 16, 16, 16, 16, 31};
        return p[row];
    }
    case 'M':
    {
        static unsigned char p[7] = {17, 27, 21, 21, 17, 17, 17};
        return p[row];
    }
    case 'N':
    {
        static unsigned char p[7] = {17, 25, 21, 19, 17, 17, 17};
        return p[row];
    }
    case 'O':
    {
        static unsigned char p[7] = {14, 17, 17, 17, 17, 17, 14};
        return p[row];
    }
    case 'P':
    {
        static unsigned char p[7] = {30, 17, 17, 30, 16, 16, 16};
        return p[row];
    }
    case 'Q':
    {
        static unsigned char p[7] = {14, 17, 17, 17, 21, 18, 13};
        return p[row];
    }
    case 'R':
    {
        static unsigned char p[7] = {30, 17, 17, 30, 20, 18, 17};
        return p[row];
    }
    case 'S':
    {
        static unsigned char p[7] = {15, 16, 16, 14, 1, 1, 30};
        return p[row];
    }
    case 'T':
    {
        static unsigned char p[7] = {31, 4, 4, 4, 4, 4, 4};
        return p[row];
    }
    case 'U':
    {
        static unsigned char p[7] = {17, 17, 17, 17, 17, 17, 14};
        return p[row];
    }
    case 'V':
    {
        static unsigned char p[7] = {17, 17, 17, 17, 17, 10, 4};
        return p[row];
    }
    case 'W':
    {
        static unsigned char p[7] = {17, 17, 17, 21, 21, 21, 10};
        return p[row];
    }
    case 'X':
    {
        static unsigned char p[7] = {17, 17, 10, 4, 10, 17, 17};
        return p[row];
    }
    case 'Y':
    {
        static unsigned char p[7] = {17, 17, 10, 4, 4, 4, 4};
        return p[row];
    }
    case 'Z':
    {
        static unsigned char p[7] = {31, 1, 2, 4, 8, 16, 31};
        return p[row];
    }
    case '0':
    {
        static unsigned char p[7] = {14, 17, 19, 21, 25, 17, 14};
        return p[row];
    }
    case '1':
    {
        static unsigned char p[7] = {4, 12, 4, 4, 4, 4, 14};
        return p[row];
    }
    case '2':
    {
        static unsigned char p[7] = {14, 17, 1, 2, 4, 8, 31};
        return p[row];
    }
    case '3':
    {
        static unsigned char p[7] = {30, 1, 1, 14, 1, 1, 30};
        return p[row];
    }
    case '4':
    {
        static unsigned char p[7] = {2, 6, 10, 18, 31, 2, 2};
        return p[row];
    }
    case '5':
    {
        static unsigned char p[7] = {31, 16, 16, 30, 1, 1, 30};
        return p[row];
    }
    case '6':
    {
        static unsigned char p[7] = {14, 16, 16, 30, 17, 17, 14};
        return p[row];
    }
    case '7':
    {
        static unsigned char p[7] = {31, 1, 2, 4, 8, 16, 16};
        return p[row];
    }
    case '8':
    {
        static unsigned char p[7] = {14, 17, 17, 14, 17, 17, 14};
        return p[row];
    }
    case '9':
    {
        static unsigned char p[7] = {14, 17, 17, 15, 1, 1, 14};
        return p[row];
    }
    case '.':
    {
        static unsigned char p[7] = {0, 0, 0, 0, 0, 12, 12};
        return p[row];
    }
    case ',':
    {
        static unsigned char p[7] = {0, 0, 0, 0, 0, 12, 8};
        return p[row];
    }
    case ':':
    {
        static unsigned char p[7] = {0, 12, 12, 0, 12, 12, 0};
        return p[row];
    }
    case '!':
    {
        static unsigned char p[7] = {4, 4, 4, 4, 4, 0, 4};
        return p[row];
    }
    case '?':
    {
        static unsigned char p[7] = {14, 17, 1, 2, 4, 0, 4};
        return p[row];
    }
    case '-':
    {
        static unsigned char p[7] = {0, 0, 0, 31, 0, 0, 0};
        return p[row];
    }
    case '+':
    {
        static unsigned char p[7] = {0, 4, 4, 31, 4, 4, 0};
        return p[row];
    }
    case '/':
    {
        static unsigned char p[7] = {1, 1, 2, 4, 8, 16, 16};
        return p[row];
    }
    case '(':
    {
        static unsigned char p[7] = {2, 4, 8, 8, 8, 4, 2};
        return p[row];
    }
    case ')':
    {
        static unsigned char p[7] = {8, 4, 2, 2, 2, 4, 8};
        return p[row];
    }
    default:
        return 0;
    }
}

static MzValue s_text(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "text") || argc < 3)
        return mz_null();
    char *msg = as_text(argv[0]);
    int start_x = as_int(argv[1]);
    int x = start_x;
    int y = as_int(argv[2]);
    int scale = argc > 3 ? as_int(argv[3]) : 2;
    if (scale < 1)
        scale = 1;
    set_color();
    for (int i = 0; msg[i]; i++)
    {
        char ch = msg[i];
        if (ch == '\n')
        {
            x = start_x;
            y += 8 * scale;
            continue;
        }
        if (ch == ' ')
        {
            x += 4 * scale;
            continue;
        }
        for (int row = 0; row < 7; row++)
        {
            unsigned char bits = text_glyph(ch, row);
            for (int col = 0; col < 5; col++)
            {
                if (bits & (1u << (4 - col)))
                {
                    SDL_FRect r = {(float)(x + col * scale), (float)(y + row * scale), (float)scale, (float)scale};
                    SDL_RenderFillRect(g_renderer, &r);
                }
            }
        }
        x += 6 * scale;
    }
    free(msg);
    return mz_null();
}

static MzValue s_image_load(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "image_load") || argc < 1)
        return mz_number(0);
    char *path = as_text(argv[0]);
    SDL_Surface *surface = SDL_LoadSurface(path);
    free(path);
    if (!surface)
        return mz_number(0);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture)
        return mz_number(0);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    int slot = image_slot();
    if (slot < 0)
    {
        SDL_DestroyTexture(texture);
        return mz_number(0);
    }

    g_images[slot].texture = texture;
    if (!SDL_GetTextureSize(texture, &g_images[slot].w, &g_images[slot].h))
    {
        g_images[slot].w = 0;
        g_images[slot].h = 0;
    }
    return mz_number(slot + 1);
}

static MzValue store_surface_as_image(SDL_Surface *surface)
{
    SDL_Texture *texture = SDL_CreateTextureFromSurface(g_renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture)
        return mz_number(0);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    int slot = image_slot();
    if (slot < 0)
    {
        SDL_DestroyTexture(texture);
        return mz_number(0);
    }

    g_images[slot].texture = texture;
    if (!SDL_GetTextureSize(texture, &g_images[slot].w, &g_images[slot].h))
    {
        g_images[slot].w = 0;
        g_images[slot].h = 0;
    }
    return mz_number(slot + 1);
}

static MzValue s_image_load_key(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "image_load_key") || argc < 4)
        return mz_number(0);
    char *path = as_text(argv[0]);
    SDL_Surface *loaded = SDL_LoadSurface(path);
    free(path);
    if (!loaded)
        return mz_number(0);

    SDL_Surface *surface = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!surface)
        return mz_number(0);

    Uint8 key_r = (Uint8)as_int(argv[1]);
    Uint8 key_g = (Uint8)as_int(argv[2]);
    Uint8 key_b = (Uint8)as_int(argv[3]);
    int tolerance = argc > 4 ? as_int(argv[4]) : 0;
    const SDL_PixelFormatDetails *fmt = SDL_GetPixelFormatDetails(surface->format);

    if (fmt && SDL_LockSurface(surface))
    {
        for (int y = 0; y < surface->h; y++)
        {
            Uint32 *row = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
            for (int x = 0; x < surface->w; x++)
            {
                Uint8 r, g, b, a;
                SDL_GetRGBA(row[x], fmt, NULL, &r, &g, &b, &a);
                if (abs((int)r - (int)key_r) <= tolerance &&
                    abs((int)g - (int)key_g) <= tolerance &&
                    abs((int)b - (int)key_b) <= tolerance)
                {
                    row[x] = SDL_MapRGBA(fmt, NULL, r, g, b, 0);
                }
            }
        }
        SDL_UnlockSurface(surface);
    }

    return store_surface_as_image(surface);
}

static MzValue s_image_free(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_null();
    MzImage *img = get_image(as_int(argv[0]));
    if (img)
    {
        SDL_DestroyTexture(img->texture);
        img->texture = NULL;
        img->w = 0;
        img->h = 0;
    }
    return mz_null();
}

static MzValue s_image_w(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_number(0);
    MzImage *img = get_image(as_int(argv[0]));
    return mz_number(img ? img->w : 0);
}

static MzValue s_image_h(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 1)
        return mz_number(0);
    MzImage *img = get_image(as_int(argv[0]));
    return mz_number(img ? img->h : 0);
}

static MzValue s_image(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "image") || argc < 3)
        return mz_null();
    MzImage *img = get_image(as_int(argv[0]));
    if (!img)
        return mz_null();

    SDL_FRect dst;
    dst.x = (float)mz_as_number(argv[1]);
    dst.y = (float)mz_as_number(argv[2]);
    dst.w = argc > 3 ? (float)mz_as_number(argv[3]) : img->w;
    dst.h = argc > 4 ? (float)mz_as_number(argv[4]) : img->h;
    SDL_RenderTexture(g_renderer, img->texture, NULL, &dst);
    return mz_null();
}

static MzValue s_image_part(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "image_part") || argc < 7)
        return mz_null();
    MzImage *img = get_image(as_int(argv[0]));
    if (!img)
        return mz_null();

    SDL_FRect src;
    src.x = (float)mz_as_number(argv[1]);
    src.y = (float)mz_as_number(argv[2]);
    src.w = (float)mz_as_number(argv[3]);
    src.h = (float)mz_as_number(argv[4]);

    SDL_FRect dst;
    dst.x = (float)mz_as_number(argv[5]);
    dst.y = (float)mz_as_number(argv[6]);
    dst.w = argc > 7 ? (float)mz_as_number(argv[7]) : src.w;
    dst.h = argc > 8 ? (float)mz_as_number(argv[8]) : src.h;
    SDL_RenderTexture(g_renderer, img->texture, &src, &dst);
    return mz_null();
}

static MzValue s_image_part_flip(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "image_part_flip") || argc < 11)
        return mz_null();
    MzImage *img = get_image(as_int(argv[0]));
    if (!img)
        return mz_null();

    SDL_FRect src;
    src.x = (float)mz_as_number(argv[1]);
    src.y = (float)mz_as_number(argv[2]);
    src.w = (float)mz_as_number(argv[3]);
    src.h = (float)mz_as_number(argv[4]);

    SDL_FRect dst;
    dst.x = (float)mz_as_number(argv[5]);
    dst.y = (float)mz_as_number(argv[6]);
    dst.w = (float)mz_as_number(argv[7]);
    dst.h = (float)mz_as_number(argv[8]);

    SDL_FlipMode flip = SDL_FLIP_NONE;
    if (mz_truthy(argv[9]))
        flip = (SDL_FlipMode)(flip | SDL_FLIP_HORIZONTAL);
    if (mz_truthy(argv[10]))
        flip = (SDL_FlipMode)(flip | SDL_FLIP_VERTICAL);
    SDL_RenderTextureRotated(g_renderer, img->texture, &src, &dst, 0.0, NULL, flip);
    return mz_null();
}

static int ray_map_at(MzValue map, int map_w, int map_h, int x, int y)
{
    if (x < 0 || y < 0 || x >= map_w || y >= map_h)
        return 1;
    return (int)mz_as_number(mz_array_get(map, y * map_w + x));
}

static MzValue s_ray_wall_distance(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 7)
        return mz_number(0);
    MzValue map = argv[0];
    int map_w = as_int(argv[1]);
    int map_h = as_int(argv[2]);
    double px = mz_as_number(argv[3]);
    double py = mz_as_number(argv[4]);
    double ex = mz_as_number(argv[5]);
    double ey = mz_as_number(argv[6]);
    double dx = ex - px;
    double dy = ey - py;
    double dist = sqrt(dx * dx + dy * dy);
    int steps = (int)floor(dist * 12.0);
    if (steps < 1)
        return mz_number(dist);
    for (int i = 1; i < steps; i++)
    {
        double sx = px + dx * (double)i / (double)steps;
        double sy = py + dy * (double)i / (double)steps;
        if (ray_map_at(map, map_w, map_h, (int)floor(sx), (int)floor(sy)) != 0)
        {
            double wx = sx - px;
            double wy = sy - py;
            return mz_number(sqrt(wx * wx + wy * wy));
        }
    }
    return mz_number(dist + 1.0);
}

static MzValue s_next_path_cell(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (argc < 7)
        return mz_number(-1);
    MzValue map = argv[0];
    int map_w = as_int(argv[1]);
    int map_h = as_int(argv[2]);
    int sx = as_int(argv[3]);
    int sy = as_int(argv[4]);
    int gx = as_int(argv[5]);
    int gy = as_int(argv[6]);
    if (sx < 0 || sy < 0 || sx >= map_w || sy >= map_h)
        return mz_number(-1);
    if (gx < 0 || gy < 0 || gx >= map_w || gy >= map_h)
        return mz_number(sy * map_w + sx);
    int total = map_w * map_h;
    if (total <= 0 || total > 4096)
        return mz_number(sy * map_w + sx);
    int start = sy * map_w + sx;
    int goal = gy * map_w + gx;
    if (start == goal)
        return mz_number(start);

    unsigned char visited[4096];
    int prev[4096];
    int q[4096];
    for (int i = 0; i < total; i++)
    {
        visited[i] = 0;
        prev[i] = -1;
    }
    int head = 0, tail = 0;
    q[tail++] = start;
    visited[start] = 1;
    static const int dirs[16] = {-1, 0, 0, -1, 1, 0, 0, 1, -1, -1, 1, -1, 1, 1, -1, 1};
    while (head < tail)
    {
        int cur = q[head++];
        if (cur == goal)
            break;
        int cx = cur % map_w;
        int cy = cur / map_w;
        for (int d = 0; d < 8; d++)
        {
            int nx = cx + dirs[d * 2];
            int ny = cy + dirs[d * 2 + 1];
            if (nx < 0 || ny < 0 || nx >= map_w || ny >= map_h)
                continue;
            int ni = ny * map_w + nx;
            if (visited[ni] || ray_map_at(map, map_w, map_h, nx, ny) != 0)
                continue;
            visited[ni] = 1;
            prev[ni] = cur;
            q[tail++] = ni;
        }
    }
    if (!visited[goal])
        return mz_number(start);
    int step = goal;
    while (prev[step] != -1 && prev[step] != start)
        step = prev[step];
    return mz_number(step);
}

static MzValue s_raycast_walls(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "raycast_walls") || argc < 13)
        return mz_null();
    MzValue map = argv[0];
    int map_w = as_int(argv[1]);
    int map_h = as_int(argv[2]);
    double px = mz_as_number(argv[3]);
    double py = mz_as_number(argv[4]);
    double ang = mz_as_number(argv[5]);
    double fov = mz_as_number(argv[6]);
    double screen_dist = mz_as_number(argv[7]);
    int scale = as_int(argv[8]);
    MzValue textures = argv[9];
    int tex_size = as_int(argv[10]);
    int width = as_int(argv[11]);
    int height = as_int(argv[12]);
    int half_h = height / 2;
    int rays = scale > 0 ? width / scale : width;
    if (scale <= 0)
        scale = 1;
    if (tex_size <= 0)
        tex_size = 256;

    for (int ray = 0; ray < rays; ray++)
    {
        double ray_angle = ang - fov * 0.5 + ((double)ray / (double)rays) * fov + 0.0001;
        double sin_a = sin(ray_angle);
        double cos_a = cos(ray_angle);
        int x_map = (int)floor(px);
        int y_map = (int)floor(py);

        double depth_hor = 99999.0, x_hor = px;
        int texture_hor = 1;
        if (fabs(sin_a) > 0.00001)
        {
            int y_step = 1;
            double y_first = y_map + 1.0;
            if (sin_a < 0)
            {
                y_step = -1;
                y_first = y_map - 0.000001;
            }
            depth_hor = (y_first - py) / sin_a;
            x_hor = px + depth_hor * cos_a;
            double delta_depth = y_step / sin_a;
            double dx = delta_depth * cos_a;
            for (int i = 0; i < 32; i++)
            {
                int t = ray_map_at(map, map_w, map_h, (int)floor(x_hor), (int)floor(y_first));
                if (t > 0)
                {
                    texture_hor = t;
                    break;
                }
                x_hor += dx;
                y_first += y_step;
                depth_hor += delta_depth;
            }
        }

        double depth_vert = 99999.0, y_vert = py;
        int texture_vert = 1;
        if (fabs(cos_a) > 0.00001)
        {
            int x_step = 1;
            double x_first = x_map + 1.0;
            if (cos_a < 0)
            {
                x_step = -1;
                x_first = x_map - 0.000001;
            }
            depth_vert = (x_first - px) / cos_a;
            y_vert = py + depth_vert * sin_a;
            double delta_depth = x_step / cos_a;
            double dy = delta_depth * sin_a;
            for (int i = 0; i < 32; i++)
            {
                int t = ray_map_at(map, map_w, map_h, (int)floor(x_first), (int)floor(y_vert));
                if (t > 0)
                {
                    texture_vert = t;
                    break;
                }
                x_first += x_step;
                y_vert += dy;
                depth_vert += delta_depth;
            }
        }

        double depth = depth_hor;
        int texture = texture_hor;
        double offset = x_hor - floor(x_hor);
        int side = 1;
        if (sin_a > 0)
            offset = 1.0 - offset;
        if (depth_vert < depth_hor)
        {
            depth = depth_vert;
            texture = texture_vert;
            offset = y_vert - floor(y_vert);
            if (cos_a < 0)
                offset = 1.0 - offset;
            side = 0;
        }

        depth *= cos(ang - ray_angle);
        if (depth < 0.0001)
            depth = 0.0001;
        double proj_h = screen_dist / depth;
        int tex_id = as_int(mz_array_get(textures, texture));
        MzImage *img = get_image(tex_id);
        if (!img)
            continue;

        SDL_FRect src;
        src.x = (float)floor(offset * (tex_size - scale));
        src.w = (float)scale;
        SDL_FRect dst;
        dst.x = (float)(ray * scale);
        dst.w = (float)scale;
        if (proj_h < height)
        {
            src.y = 0;
            src.h = (float)tex_size;
            dst.y = (float)(half_h - proj_h / 2.0);
            dst.h = (float)proj_h;
        }
        else
        {
            double texture_h = tex_size * height / proj_h;
            src.y = (float)(tex_size / 2.0 - texture_h / 2.0);
            src.h = (float)texture_h;
            dst.y = 0;
            dst.h = (float)height;
        }
        SDL_RenderTexture(g_renderer, img->texture, &src, &dst);
        if (side == 1)
        {
            SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 72);
            SDL_RenderFillRect(g_renderer, &dst);
        }
    }

    return mz_null();
}

typedef struct
{
    SDL_Vertex v[4];
    int indices[6];
    double depth;
    int tile;
    float shade;
} VoxelFace;

static VoxelFace *g_voxel_faces;
static int g_voxel_face_cap;

static int voxel_index(int x, int y, int z, int w, int h, int d)
{
    (void)h;
    return (y * d + z) * w + x;
}

static int voxel_map_at(MzValue map, int w, int h, int d, int x, int y, int z)
{
    if (x < 0 || y < 0 || z < 0 || x >= w || y >= h || z >= d)
        return 0;
    return (int)mz_as_number(mz_array_get(map, voxel_index(x, y, z, w, h, d)));
}

static int voxel_face_cmp(const void *a, const void *b)
{
    const VoxelFace *fa = (const VoxelFace *)a;
    const VoxelFace *fb = (const VoxelFace *)b;
    if (fa->depth < fb->depth)
        return 1;
    if (fa->depth > fb->depth)
        return -1;
    return 0;
}

static int ensure_voxel_faces(int cap)
{
    if (cap <= g_voxel_face_cap)
        return 1;
    int next = g_voxel_face_cap < 4096 ? 4096 : g_voxel_face_cap;
    while (next < cap)
        next *= 2;
    VoxelFace *faces = (VoxelFace *)realloc(g_voxel_faces, sizeof(VoxelFace) * (size_t)next);
    if (!faces)
        return 0;
    g_voxel_faces = faces;
    g_voxel_face_cap = next;
    return 1;
}

static int project_voxel_point(double wx, double wy, double wz,
                               double cam_x, double cam_y, double cam_z,
                               double sin_yaw, double cos_yaw,
                               double sin_pitch, double cos_pitch,
                               double focal, int screen_w, int screen_h,
                               float *sx, float *sy, double *depth)
{
    double dx = wx - cam_x;
    double dy = wy - cam_y;
    double dz = wz - cam_z;
    double cx = cos_yaw * dx - sin_yaw * dz;
    double cz = sin_yaw * dx + cos_yaw * dz;
    double cy = cos_pitch * dy - sin_pitch * cz;
    double fz = sin_pitch * dy + cos_pitch * cz;
    if (fz <= 0.06)
        return 0;
    *sx = (float)(screen_w * 0.5 + cx * focal / fz);
    *sy = (float)(screen_h * 0.5 - cy * focal / fz);
    *depth = fz;
    return 1;
}

static void voxel_uv(int tile, int tile_size, float tex_w, float tex_h,
                     float *u0, float *v0, float *u1, float *v1)
{
    int t = tile - 1;
    if (t < 0)
        t = 0;
    int cols = tile_size > 0 ? (int)(tex_w / (float)tile_size) : 1;
    if (cols < 1)
        cols = 1;
    int tx = t % cols;
    int ty = t / cols;
    float inset = 0.5f;
    *u0 = (tx * tile_size + inset) / tex_w;
    *v0 = (ty * tile_size + inset) / tex_h;
    *u1 = ((tx + 1) * tile_size - inset) / tex_w;
    *v1 = ((ty + 1) * tile_size - inset) / tex_h;
}

static MzValue s_voxel3d_render(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "voxel3d_render") || argc < 15)
        return mz_number(0);
    MzValue map = argv[0];
    int w = as_int(argv[1]);
    int h = as_int(argv[2]);
    int d = as_int(argv[3]);
    double cam_x = mz_as_number(argv[4]);
    double cam_y = mz_as_number(argv[5]);
    double cam_z = mz_as_number(argv[6]);
    double yaw = mz_as_number(argv[7]);
    double pitch = mz_as_number(argv[8]);
    double fov = mz_as_number(argv[9]);
    int atlas_id = as_int(argv[10]);
    int tile_size = as_int(argv[11]);
    int screen_w = as_int(argv[12]);
    int screen_h = as_int(argv[13]);
    double max_dist = mz_as_number(argv[14]);
    int max_faces = argc > 15 ? as_int(argv[15]) : 12000;
    if (w <= 0 || h <= 0 || d <= 0 || max_faces <= 0)
        return mz_number(0);
    if (fov <= 0.1)
        fov = 1.04719755;
    if (tile_size <= 0)
        tile_size = 128;
    if (screen_w <= 0)
        screen_w = 640;
    if (screen_h <= 0)
        screen_h = 360;
    if (max_dist <= 1.0)
        max_dist = 32.0;
    MzImage *atlas = get_image(atlas_id);
    if (!atlas || atlas->w <= 0 || atlas->h <= 0)
        return mz_number(0);
    if (!ensure_voxel_faces(max_faces))
        return mz_number(0);

    double sin_yaw = sin(yaw);
    double cos_yaw = cos(yaw);
    double sin_pitch = sin(pitch);
    double cos_pitch = cos(pitch);
    double focal = (screen_w * 0.5) / tan(fov * 0.5);
    double max_dist2 = max_dist * max_dist;
    int face_count = 0;

    static const int dirs[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const double verts[6][4][3] = {
        {{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}},
        {{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}},
        {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
        {{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}},
        {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}};
    static const float shades[6] = {0.82f, 0.70f, 1.00f, 0.52f, 0.90f, 0.76f};

    for (int y = 0; y < h; y++)
    {
        for (int z = 0; z < d; z++)
        {
            for (int x = 0; x < w; x++)
            {
                int tile = voxel_map_at(map, w, h, d, x, y, z);
                if (tile <= 0)
                    continue;
                double cx = x + 0.5 - cam_x;
                double cy = y + 0.5 - cam_y;
                double cz = z + 0.5 - cam_z;
                if (cx * cx + cy * cy + cz * cz > max_dist2)
                    continue;

                for (int f = 0; f < 6; f++)
                {
                    if (voxel_map_at(map, w, h, d, x + dirs[f][0], y + dirs[f][1], z + dirs[f][2]) > 0)
                        continue;
                    double face_cx = x + 0.5 + dirs[f][0] * 0.5;
                    double face_cy = y + 0.5 + dirs[f][1] * 0.5;
                    double face_cz = z + 0.5 + dirs[f][2] * 0.5;
                    double vx = cam_x - face_cx;
                    double vy = cam_y - face_cy;
                    double vz = cam_z - face_cz;
                    if (vx * dirs[f][0] + vy * dirs[f][1] + vz * dirs[f][2] <= 0.0)
                        continue;
                    if (face_count >= max_faces)
                        break;

                    VoxelFace *face = &g_voxel_faces[face_count];
                    double depth_sum = 0.0;
                    int ok = 1;
                    for (int i = 0; i < 4; i++)
                    {
                        float sx, sy;
                        double depth;
                        if (!project_voxel_point(x + verts[f][i][0], y + verts[f][i][1], z + verts[f][i][2],
                                                 cam_x, cam_y, cam_z,
                                                 sin_yaw, cos_yaw, sin_pitch, cos_pitch,
                                                 focal, screen_w, screen_h, &sx, &sy, &depth))
                        {
                            ok = 0;
                            break;
                        }
                        face->v[i].position.x = sx;
                        face->v[i].position.y = sy;
                        depth_sum += depth;
                    }
                    if (!ok)
                        continue;

                    float u0, v0, u1, v1;
                    voxel_uv(tile, tile_size, atlas->w, atlas->h, &u0, &v0, &u1, &v1);
                    face->v[0].tex_coord.x = u0;
                    face->v[0].tex_coord.y = v1;
                    face->v[1].tex_coord.x = u0;
                    face->v[1].tex_coord.y = v0;
                    face->v[2].tex_coord.x = u1;
                    face->v[2].tex_coord.y = v0;
                    face->v[3].tex_coord.x = u1;
                    face->v[3].tex_coord.y = v1;
                    float fog = (float)(1.0 - (depth_sum * 0.25) / max_dist);
                    if (fog < 0.25f)
                        fog = 0.25f;
                    if (fog > 1.0f)
                        fog = 1.0f;
                    float shade = shades[f] * fog;
                    for (int i = 0; i < 4; i++)
                    {
                        face->v[i].color.r = shade;
                        face->v[i].color.g = shade;
                        face->v[i].color.b = shade;
                        face->v[i].color.a = 1.0f;
                    }
                    face->indices[0] = 0;
                    face->indices[1] = 1;
                    face->indices[2] = 2;
                    face->indices[3] = 0;
                    face->indices[4] = 2;
                    face->indices[5] = 3;
                    face->depth = depth_sum * 0.25;
                    face->tile = tile;
                    face->shade = shade;
                    face_count++;
                }
            }
        }
    }

    qsort(g_voxel_faces, (size_t)face_count, sizeof(VoxelFace), voxel_face_cmp);
    for (int i = 0; i < face_count; i++)
    {
        SDL_RenderGeometry(g_renderer, atlas->texture, g_voxel_faces[i].v, 4, g_voxel_faces[i].indices, 6);
    }
    return mz_number(face_count);
}

static MzValue s_present(MzVM *vm, int argc, MzValue *argv)
{
    if (!need_renderer(vm, "present"))
        return mz_null();
    (void)argc;
    (void)argv;
    SDL_RenderPresent(g_renderer);
    return mz_null();
}

static MzValue s_delay(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    SDL_Delay((Uint32)(argc > 0 ? as_int(argv[0]) : 16));
    return mz_null();
}

static MzValue s_ticks(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_number((double)SDL_GetTicks());
}

static MzValue s_mouse_x(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_number(g_mouse_x);
}
static MzValue s_mouse_y(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_number(g_mouse_y);
}
static MzValue s_mouse_rel_x(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_number(g_mouse_rel_x);
}
static MzValue s_mouse_rel_y(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_number(g_mouse_rel_y);
}
static MzValue s_mouse_left(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_bool(g_mouse_left);
}
static MzValue s_mouse_right(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    (void)argc;
    (void)argv;
    return mz_bool(g_mouse_right);
}

static MzValue s_mouse_relative(MzVM *vm, int argc, MzValue *argv)
{
    (void)vm;
    if (!g_window)
        return mz_bool(0);
    int enabled = argc > 0 ? mz_truthy(argv[0]) : 1;
    return mz_bool(SDL_SetWindowRelativeMouseMode(g_window, enabled != 0));
}

void mz_sdl_register(MzVM *vm)
{
    mz_define_native(vm, "window", s_window);
    mz_define_native(vm, "logical_size", s_logical_size);
    mz_define_native(vm, "poll", s_poll);
    mz_define_native(vm, "running", s_running);
    mz_define_native(vm, "quit", s_quit);
    mz_define_native(vm, "key", s_key);
    mz_define_native(vm, "clear", s_clear);
    mz_define_native(vm, "color", s_color);
    mz_define_native(vm, "rect", s_rect);
    mz_define_native(vm, "rect_line", s_rect_line);
    mz_define_native(vm, "line", s_line);
    mz_define_native(vm, "point", s_point);
    mz_define_native(vm, "circle", s_circle);
    mz_define_native(vm, "text", s_text);
    mz_define_native(vm, "image_load", s_image_load);
    mz_define_native(vm, "image_load_key", s_image_load_key);
    mz_define_native(vm, "image_free", s_image_free);
    mz_define_native(vm, "image_w", s_image_w);
    mz_define_native(vm, "image_h", s_image_h);
    mz_define_native(vm, "image", s_image);
    mz_define_native(vm, "image_part", s_image_part);
    mz_define_native(vm, "image_part_flip", s_image_part_flip);
    mz_define_native(vm, "ray_wall_distance_fast", s_ray_wall_distance);
    mz_define_native(vm, "next_path_cell_fast", s_next_path_cell);
    mz_define_native(vm, "raycast_walls", s_raycast_walls);
    mz_define_native(vm, "voxel3d_render", s_voxel3d_render);
    mz_define_native(vm, "present", s_present);
    mz_define_native(vm, "delay", s_delay);
    mz_define_native(vm, "ticks", s_ticks);
    mz_define_native(vm, "mouse_x", s_mouse_x);
    mz_define_native(vm, "mouse_y", s_mouse_y);
    mz_define_native(vm, "mouse_rel_x", s_mouse_rel_x);
    mz_define_native(vm, "mouse_rel_y", s_mouse_rel_y);
    mz_define_native(vm, "mouse_left", s_mouse_left);
    mz_define_native(vm, "mouse_right", s_mouse_right);
    mz_define_native(vm, "mouse_relative", s_mouse_relative);
}

void mz_sdl_shutdown(void)
{
    free_images();
    if (g_renderer)
        SDL_DestroyRenderer(g_renderer);
    if (g_window)
        SDL_DestroyWindow(g_window);
    free(g_voxel_faces);
    g_voxel_faces = NULL;
    g_voxel_face_cap = 0;
    g_renderer = NULL;
    g_window = NULL;
    SDL_Quit();
}
