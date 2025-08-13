#include "oz/oz_platform.h"
#include "oz/oz_log.h"

#ifdef OZ_HAVE_SDL2
#include <SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static SDL_Window* s_window = NULL;
static Uint8 s_keys[SDL_NUM_SCANCODES];

bool oz_platform_init(const struct OzWindowConfig* config) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    const char* disp = getenv("DISPLAY");
    const char* libgl_ind = getenv("LIBGL_ALWAYS_INDIRECT");
    const char* libgl_dbg = getenv("LIBGL_DEBUG");
    const char* mesa_dbg = getenv("MESA_DEBUG");
    const char* sdl_videodrv = getenv("SDL_VIDEODRIVER");
    const char* gdk_gl = getenv("GDK_GL");
    const char* force_sw_env = getenv("OZ_FORCE_SOFTWARE");
    const bool is_remote = (disp && disp[0] != ':');
    OZ_INFO("Platform env (SW): DISPLAY=%s remote=%s LIBGL_ALWAYS_INDIRECT=%s LIBGL_DEBUG=%s MESA_DEBUG=%s SDL_VIDEODRIVER=%s GDK_GL=%s OZ_FORCE_SOFTWARE=%s",
            disp ? disp : "(null)", is_remote ? "yes" : "no",
            libgl_ind ? libgl_ind : "(null)",
            libgl_dbg ? libgl_dbg : "(null)",
            mesa_dbg ? mesa_dbg : "(null)",
            sdl_videodrv ? sdl_videodrv : "(null)",
            gdk_gl ? gdk_gl : "(null)",
            force_sw_env ? force_sw_env : "(null)");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        OZ_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    // Force software renderer backend to avoid GLX usage on X11
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    s_window = SDL_CreateWindow(
        config && config->title ? config->title : "OzWorld (SW)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config ? config->width : 640,
        config ? config->height : 480,
        SDL_WINDOW_SHOWN
    );
    if (!s_window) {
        OZ_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    OZ_INFO("Platform ready (SW): window=%p", (void*)s_window);
    return true;
}

void oz_platform_shutdown(void) {
    if (s_window) { SDL_DestroyWindow(s_window); s_window = NULL; }
    SDL_Quit();
}

bool oz_platform_pump_events(bool* out_should_quit) {
    if (out_should_quit) *out_should_quit = false;
    SDL_Event e; while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) { if (out_should_quit) *out_should_quit = true; } }
    const Uint8* state = SDL_GetKeyboardState(NULL); memcpy(s_keys, state, sizeof(s_keys));
    return true;
}

void oz_platform_clear(float r, float g, float b, float a) { (void)r; (void)g; (void)b; (void)a; }
void oz_platform_swap_buffers(void) {}
void oz_platform_sleep_ms(unsigned int ms) { SDL_Delay(ms); }

double oz_platform_time_now_seconds(void) { return (double)SDL_GetTicks() / 1000.0; }

void oz_platform_get_window_size(int* out_width, int* out_height) {
    int w=0,h=0; if (s_window) SDL_GetWindowSize(s_window, &w, &h); if (out_width) *out_width = w; if (out_height) *out_height = h;
}

bool oz_platform_is_software_renderer(void) { return true; }

static SDL_Scancode key_to_scancode(enum OzKey key) {
    switch (key) {
        case OZ_KEY_W: return SDL_SCANCODE_W;
        case OZ_KEY_A: return SDL_SCANCODE_A;
        case OZ_KEY_S: return SDL_SCANCODE_S;
        case OZ_KEY_D: return SDL_SCANCODE_D;
        case OZ_KEY_Q: return SDL_SCANCODE_Q;
        case OZ_KEY_E: return SDL_SCANCODE_E;
        case OZ_KEY_LEFT: return SDL_SCANCODE_LEFT;
        case OZ_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
        case OZ_KEY_UP: return SDL_SCANCODE_UP;
        case OZ_KEY_DOWN: return SDL_SCANCODE_DOWN;
        case OZ_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

bool oz_platform_key_down(enum OzKey key) { SDL_Scancode sc = key_to_scancode(key); if (sc==SDL_SCANCODE_UNKNOWN) return false; return s_keys[sc] != 0; }

void oz_platform_present_software_rgba32(const unsigned char* pixels, int width, int height, int stride) {
    if (!s_window || !pixels || width<=0 || height<=0 || stride<=0) return;
    static SDL_Renderer* renderer = NULL; static SDL_Texture* texture = NULL; static int tex_w=0, tex_h=0;
    int win_w=0, win_h=0; SDL_GetWindowSize(s_window, &win_w, &win_h);
    if (!renderer) { renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE); if (!renderer) return; }
    if (!texture || tex_w != width || tex_h != height) { if (texture) SDL_DestroyTexture(texture); texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height); if (!texture) return; tex_w=width; tex_h=height; }
    void* dst=NULL; int dst_pitch=0; if (SDL_LockTexture(texture, NULL, &dst, &dst_pitch)==0 && dst) { const unsigned char* src=pixels; unsigned char* dr=(unsigned char*)dst; int row_bytes = stride < dst_pitch ? stride : dst_pitch; for (int y=0;y<height;++y){ memcpy(dr, src, (size_t)row_bytes); src += stride; dr += dst_pitch; } SDL_UnlockTexture(texture); }
    SDL_SetRenderDrawColor(renderer, 26,31,38,255); SDL_RenderClear(renderer);
    float scale = (float)win_w / (float)width; float sy = (float)win_h / (float)height; if (sy < scale) scale = sy; int rw=(int)(width*scale), rh=(int)(height*scale); SDL_Rect dstrect={(win_w-rw)/2,(win_h-rh)/2,rw,rh};
    SDL_RenderCopy(renderer, texture, NULL, &dstrect); SDL_RenderPresent(renderer);
}

#else
#warning "oz_platform_sdl_sw.c compiled without OZ_HAVE_SDL2"
#endif
