#ifndef OZ_ASSETS_H
#define OZ_ASSETS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Canonical file extensions for OzWorld assets
#define OZ_EXT_MAP           ".ozone"
#define OZ_EXT_MAP_LEGACY    ".ozmap"
#define OZ_EXT_TEXTURE       ".oztex"
#define OZ_EXT_MESH_BUNDLE   ".ozbag"
#define OZ_EXT_MUSIC         ".ozmux"

// Returns true if path ends with the given extension (case-insensitive)
bool oz_path_has_extension(const char* path, const char* ext);

// Returns a freshly-allocated string. If path has no extension from the set
// { .ozone, .ozmap, .oztex, .ozbag, .ozmux }, or from the passed ext (if provided),
// it appends ext. Caller must free(). If path already has an extension, returns
// a duplicate of path.
char* oz_path_with_default_extension(const char* path, const char* ext);

// Lightweight asset type detection by extension. Returns one of:
//   "map", "texture", "bundle", "music", or NULL when unknown.
const char* oz_asset_detect_type_from_path(const char* path);

#ifdef __cplusplus
}
#endif

#endif // OZ_ASSETS_H
