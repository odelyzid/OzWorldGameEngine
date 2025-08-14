#ifndef OZ_BSP_H
#define OZ_BSP_H

#include <stdbool.h>
#include <stddef.h>
#include "oz/oz_math.h"

#ifdef __cplusplus
extern "C" {
#endif

// Very basic brush types for prototyping
typedef enum OzBrushType {
    OZ_BRUSH_BOX = 1,
    OZ_BRUSH_CYLINDER = 2,
    OZ_BRUSH_SPHERE = 3,
    OZ_BRUSH_PYRAMID = 4,
    OZ_BRUSH_PLANE = 5,
} OzBrushType;

typedef struct OzBrushBox {
    OzVec3 center;
    OzVec3 half; // half-extents along each axis
    float rotation_z; // radians, rotation around Z axis (editor only for now)
} OzBrushBox;

typedef struct OzBrushCylinder {
    OzVec3 center;
    float radius_x; // radius along X (for ellipse base)
    float radius_y; // radius along Y
    float height;   // height along Z
    int segments;   // tesselation for rendering
    float rotation_z; // radians around Z axis
} OzBrushCylinder;

typedef struct OzBrushSphere {
    OzVec3 center;
    float radius;
    int segments; // tesselation for rendering
} OzBrushSphere;

typedef struct OzBrushPyramid {
    OzVec3 center;
    float half_x;
    float half_y;
    float height; // along Z
} OzBrushPyramid;

typedef struct OzBrushPlane {
    OzVec3 center;
    OzVec3 normal; // unit normal
    float half_extent; // visual extent for editor
} OzBrushPlane;

typedef struct OzBrush {
    OzBrushType type;
    union {
        OzBrushBox box;
        OzBrushCylinder cyl;
        OzBrushSphere sph;
        OzBrushPyramid pyr;
        OzBrushPlane plane;
    } as;
} OzBrush;

typedef struct OzMap {
    OzBrush* brushes;
    size_t count;
    size_t capacity;
} OzMap;

// Simple editor/world properties (kept minimal and optional)
typedef struct OzWorldProps {
    float ambient_rgb[3];
    float gravity_z; // negative downwards
} OzWorldProps;

void oz_world_props_default(OzWorldProps* p);

// Append a brush to the map (copies contents). Returns true on success.
bool oz_map_append_brush(OzMap* map, const OzBrush* brush);

void oz_map_init(OzMap* map);
void oz_map_free(OzMap* map);

// Adds a box brush with given center and full size (width/height/depth).
// Returns pointer to the stored brush or NULL on failure.
OzBrush* oz_map_add_box(OzMap* map, OzVec3 center, OzVec3 size);
OzBrush* oz_map_add_cylinder(OzMap* map, OzVec3 center, float radius_x, float radius_y, float height, int segments);
OzBrush* oz_map_add_sphere(OzMap* map, OzVec3 center, float radius, int segments);
OzBrush* oz_map_add_pyramid(OzMap* map, OzVec3 center, float half_x, float half_y, float height);
OzBrush* oz_map_add_plane(OzMap* map, OzVec3 center, OzVec3 normal, float half_extent);

// Save/load as a very simple text format. Returns true on success.
bool oz_map_save_text(const char* path, const OzMap* map);
bool oz_map_load_text(const char* path, OzMap* out_map);

// --- CSG (approximate; axis-aligned and Z-rotation only) ---
// These operations return a brush approximating the boolean of A and B.
// For now, only OZ_BRUSH_BOX is handled precisely; cylinders use AABB approximation.
bool oz_csg_union(const OzBrush* a, const OzBrush* b, OzBrush* out);
bool oz_csg_intersect(const OzBrush* a, const OzBrush* b, OzBrush* out);
bool oz_csg_subtract(const OzBrush* a, const OzBrush* b, OzBrush* out);

// --- BSP scaffolding ---
typedef struct OzBspNode {
    // Plane: ax + by + cz + d = 0
    float a, b, c, d;
    int left;   // index into nodes array (-1 if none)
    int right;  // index into nodes array (-1 if none)
    int* brush_indices; // indices into OzMap.brushes that lie on this leaf (NULL if not a leaf)
    size_t brush_count;
} OzBspNode;

typedef struct OzBspTree {
    OzBspNode* nodes;
    size_t count;
    size_t capacity;
    int root; // index of root node
} OzBspTree;

void oz_bsp_free(OzBspTree* t);
bool oz_bsp_build_from_map(const OzMap* map, OzBspTree* out_tree);

#ifdef __cplusplus
}
#endif

#endif // OZ_BSP_H
