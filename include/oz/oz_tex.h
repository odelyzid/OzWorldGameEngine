#ifndef OZ_TEX_H
#define OZ_TEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Very simple Oz texture file format (OZTEX 1):
// Header (ASCII):
//   OZTEX 1\n
//   <width> <height> <channels>\n   // channels currently 3 (RGB) or 4 (RGBA)
// Payload: width*height*channels bytes, row-major, top-to-bottom
// Returns true on success. Caller owns *out_pixels (malloc), free() when done.

typedef struct OzColor3 {
  float r;
  float g;
  float b;
} OzColor;

bool oz_tex_load_oztex(const char *path, int *out_w, int *out_h,
                       int *out_channels, unsigned char **out_pixels);

// Convenience: free pixels allocated by loader
static inline void oz_tex_free(unsigned char *pixels) {
  if (pixels)
    free(pixels);
}

#ifdef __cplusplus
}
#endif

#endif // OZ_TEX_H
