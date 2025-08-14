#include "oz/oz_tex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool oz_tex_load_oztex(const char* path, int* out_w, int* out_h, int* out_channels, unsigned char** out_pixels) {
    if (!path || !out_w || !out_h || !out_channels || !out_pixels) return false;
    *out_pixels = NULL; *out_w = *out_h = *out_channels = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char header[32];
    if (!fgets(header, sizeof(header), f)) { fclose(f); return false; }
    if (strncmp(header, "OZTEX 1", 7) != 0) { fclose(f); return false; }
    int w=0,h=0,c=0;
    if (fscanf(f, "%d %d %d\n", &w, &h, &c) != 3) { fclose(f); return false; }
    if (w <= 0 || h <= 0 || (c != 3 && c != 4)) { fclose(f); return false; }
    size_t total = (size_t)w * (size_t)h * (size_t)c;
    unsigned char* p = (unsigned char*)malloc(total);
    if (!p) { fclose(f); return false; }
    size_t n = fread(p, 1, total, f);
    fclose(f);
    if (n != total) { free(p); return false; }
    *out_w = w; *out_h = h; *out_channels = c; *out_pixels = p;
    return true;
}
