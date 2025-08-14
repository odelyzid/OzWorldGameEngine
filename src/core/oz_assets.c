#include "oz/oz_assets.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static int oz_strcasecmp(const char* a, const char* b) {
    if (!a || !b) return (a==b)?0:(a?1:-1);
    while (*a && *b) {
        int da = tolower((unsigned char)*a++);
        int db = tolower((unsigned char)*b++);
        if (da != db) return da - db;
    }
    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

static const char* oz_find_ext(const char* path) {
    if (!path) return NULL;
    const char* dot = strrchr(path, '.');
    if (!dot) return NULL;
    const char* slash1 = strrchr(path, '/');
    const char* slash2 = strrchr(path, '\\');
    const char* slash = slash1 > slash2 ? slash1 : slash2;
    if (slash && dot < slash) return NULL; // dot is in directory part
    return dot;
}

bool oz_path_has_extension(const char* path, const char* ext) {
    const char* e = oz_find_ext(path);
    if (!e || !ext) return false;
    return oz_strcasecmp(e, ext) == 0;
}

char* oz_path_with_default_extension(const char* path, const char* ext) {
    if (!path) return NULL;
    const char* e = oz_find_ext(path);
    if (e) {
        size_t n = strlen(path);
        char* out = (char*)malloc(n + 1);
        if (!out) return NULL;
        memcpy(out, path, n + 1);
        return out;
    }
    const char* use = (ext && *ext) ? ext : OZ_EXT_MAP;
    size_t n = strlen(path), m = strlen(use);
    char* out = (char*)malloc(n + m + 1);
    if (!out) return NULL;
    memcpy(out, path, n);
    memcpy(out + n, use, m + 1);
    return out;
}

const char* oz_asset_detect_type_from_path(const char* path) {
    if (!path) return NULL;
    if (oz_path_has_extension(path, OZ_EXT_MAP) || oz_path_has_extension(path, OZ_EXT_MAP_LEGACY)) return "map";
    if (oz_path_has_extension(path, OZ_EXT_TEXTURE)) return "texture";
    if (oz_path_has_extension(path, OZ_EXT_MESH_BUNDLE)) return "bundle";
    if (oz_path_has_extension(path, OZ_EXT_MUSIC)) return "music";
    return NULL;
}
