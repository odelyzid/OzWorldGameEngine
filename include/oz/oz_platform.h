#ifndef OZ_PLATFORM_H
#define OZ_PLATFORM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct OzWindowConfig {
    int width;
    int height;
    const char* title;
    bool vsync;
};

bool oz_platform_init(const struct OzWindowConfig* config);
void oz_platform_shutdown(void);

// Returns false on fatal error; sets *out_should_quit if user requested to close
bool oz_platform_pump_events(bool* out_should_quit);

void oz_platform_clear(float r, float g, float b, float a);
void oz_platform_swap_buffers(void);

void oz_platform_sleep_ms(unsigned int ms);

double oz_platform_time_now_seconds(void);

// Returns the current window drawable size in pixels
void oz_platform_get_window_size(int* out_width, int* out_height);

// Returns true when the platform fell back to a non-OpenGL software renderer.
// Useful for apps to avoid issuing GL calls when running over remote X servers.
bool oz_platform_is_software_renderer(void);

// When in software mode, present an ARGB32 image buffer to the window.
// - pixels: pointer to top-left of image
// - width/height: in pixels
// - stride: bytes per row
void oz_platform_present_software_rgba32(const unsigned char* pixels, int width, int height, int stride);

// Minimal key input API for demo/editor controls
typedef enum OzKey {
    OZ_KEY_W,
    OZ_KEY_A,
    OZ_KEY_S,
    OZ_KEY_D,
    OZ_KEY_Q,
    OZ_KEY_E,
    OZ_KEY_LEFT,
    OZ_KEY_RIGHT,
    OZ_KEY_UP,
    OZ_KEY_DOWN,
    OZ_KEY_ESCAPE,
} OzKey;

bool oz_platform_key_down(enum OzKey key);

#ifdef __cplusplus
}
#endif

#endif // OZ_PLATFORM_H
