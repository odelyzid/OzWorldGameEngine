#include "oz/oz_bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool oz_map_reserve(OzMap* map, size_t new_capacity) {
    if (new_capacity <= map->capacity) return true;
    size_t capacity = map->capacity ? map->capacity : 4;
    while (capacity < new_capacity) capacity *= 2;
    void* mem = realloc(map->brushes, capacity * sizeof(OzBrush));
    if (!mem) return false;
    map->brushes = (OzBrush*)mem;
    map->capacity = capacity;
    return true;
}

void oz_map_init(OzMap* map) {
    if (!map) return;
    map->brushes = NULL;
    map->count = 0;
    map->capacity = 0;
}

void oz_map_free(OzMap* map) {
    if (!map) return;
    free(map->brushes);
    map->brushes = NULL;
    map->count = 0;
    map->capacity = 0;
}

OzBrush* oz_map_add_box(OzMap* map, OzVec3 center, OzVec3 size) {
    if (!map) return NULL;
    if (!oz_map_reserve(map, map->count + 1)) return NULL;
    OzBrush* b = &map->brushes[map->count++];
    b->type = OZ_BRUSH_BOX;
    b->as.box.center = center;
    b->as.box.half.x = size.x * 0.5f;
    b->as.box.half.y = size.y * 0.5f;
    b->as.box.half.z = size.z * 0.5f;
    b->as.box.rotation_z = 0.0f;
    return b;
}

OzBrush* oz_map_add_cylinder(OzMap* map, OzVec3 center, float radius_x, float radius_y, float height, int segments) {
    if (!map) return NULL;
    if (segments < 3) segments = 3;
    if (!oz_map_reserve(map, map->count + 1)) return NULL;
    OzBrush* b = &map->brushes[map->count++];
    b->type = OZ_BRUSH_CYLINDER;
    b->as.cyl.center = center;
    b->as.cyl.radius_x = radius_x;
    b->as.cyl.radius_y = radius_y;
    b->as.cyl.height = height;
    b->as.cyl.segments = segments;
    b->as.cyl.rotation_z = 0.0f;
    return b;
}

OzBrush* oz_map_add_sphere(OzMap* map, OzVec3 center, float radius, int segments) {
    if (!map) return NULL;
    if (segments < 6) segments = 6;
    if (!oz_map_reserve(map, map->count + 1)) return NULL;
    OzBrush* b = &map->brushes[map->count++];
    b->type = OZ_BRUSH_SPHERE;
    b->as.sph.center = center;
    b->as.sph.radius = radius;
    b->as.sph.segments = segments;
    return b;
}

OzBrush* oz_map_add_pyramid(OzMap* map, OzVec3 center, float half_x, float half_y, float height) {
    if (!map) return NULL;
    if (!oz_map_reserve(map, map->count + 1)) return NULL;
    OzBrush* b = &map->brushes[map->count++];
    b->type = OZ_BRUSH_PYRAMID;
    b->as.pyr.center = center; b->as.pyr.half_x = half_x; b->as.pyr.half_y = half_y; b->as.pyr.height = height;
    return b;
}

OzBrush* oz_map_add_plane(OzMap* map, OzVec3 center, OzVec3 normal, float half_extent) {
    if (!map) return NULL;
    if (!oz_map_reserve(map, map->count + 1)) return NULL;
    OzBrush* b = &map->brushes[map->count++];
    b->type = OZ_BRUSH_PLANE;
    b->as.plane.center = center; b->as.plane.normal = normal; b->as.plane.half_extent = half_extent;
    return b;
}

bool oz_map_save_text(const char* path, const OzMap* map) {
    if (!path || !map) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int ok = 1;
    if (fprintf(f, "OZONE 1\n") < 0) ok = 0;
    if (ok && fprintf(f, "brushes %zu\n", map->count) < 0) ok = 0;
    for (size_t i = 0; ok && i < map->count; ++i) {
        const OzBrush* b = &map->brushes[i];
        if (b->type == OZ_BRUSH_BOX) {
            const OzBrushBox* bx = &b->as.box;
            if (fprintf(f, "box %g %g %g %g %g %g %g\n",
                        (double)bx->center.x, (double)bx->center.y, (double)bx->center.z,
                        (double)(bx->half.x * 2.0f), (double)(bx->half.y * 2.0f), (double)(bx->half.z * 2.0f), (double)bx->rotation_z) < 0) ok = 0;
        } else if (b->type == OZ_BRUSH_CYLINDER) {
            const OzBrushCylinder* cy = &b->as.cyl;
            if (fprintf(f, "cyl %g %g %g %g %g %g %d %g\n",
                        (double)cy->center.x, (double)cy->center.y, (double)cy->center.z,
                        (double)cy->radius_x, (double)cy->radius_y, (double)cy->height, cy->segments, (double)cy->rotation_z) < 0) ok = 0;
        } else if (b->type == OZ_BRUSH_SPHERE) {
            const OzBrushSphere* sp = &b->as.sph;
            if (fprintf(f, "sph %g %g %g %g %d\n",
                        (double)sp->center.x, (double)sp->center.y, (double)sp->center.z,
                        (double)sp->radius, sp->segments) < 0) ok = 0;
        } else if (b->type == OZ_BRUSH_PYRAMID) {
            const OzBrushPyramid* py = &b->as.pyr;
            if (fprintf(f, "pyr %g %g %g %g %g %g\n",
                        (double)py->center.x, (double)py->center.y, (double)py->center.z,
                        (double)py->half_x*2.0, (double)py->half_y*2.0, (double)py->height) < 0) ok = 0;
        } else if (b->type == OZ_BRUSH_PLANE) {
            const OzBrushPlane* pl = &b->as.plane;
            if (fprintf(f, "pln %g %g %g %g %g %g %g\n",
                        (double)pl->center.x, (double)pl->center.y, (double)pl->center.z,
                        (double)pl->normal.x, (double)pl->normal.y, (double)pl->normal.z,
                        (double)pl->half_extent) < 0) ok = 0;
        }
    }
    if (fclose(f) != 0) ok = 0;
    return ok != 0;
}

bool oz_map_load_text(const char* path, OzMap* out_map) {
    if (!path || !out_map) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    // Initialize out_map cleanly
    oz_map_free(out_map);
    oz_map_init(out_map);

    char header[16];
    if (!fgets(header, sizeof(header), f)) { fclose(f); return false; }
    if (strncmp(header, "OZONE 1", 7) != 0 && strncmp(header, "OZMAP 1", 7) != 0) { fclose(f); return false; }

    size_t expected = 0;
    if (fscanf(f, "brushes %zu\n", &expected) != 1) { fclose(f); return false; }
    if (!oz_map_reserve(out_map, expected)) { fclose(f); return false; }

    for (size_t i = 0; i < expected; ++i) {
        char kind[8];
        if (fscanf(f, "%7s", kind) != 1) { fclose(f); oz_map_free(out_map); return false; }
        if (strcmp(kind, "box") == 0) {
            float cx, cy, cz, sx, sy, sz, rz = 0.0f;
            int n = fscanf(f, " %f %f %f %f %f %f %f\n", &cx, &cy, &cz, &sx, &sy, &sz, &rz);
            if (n != 6 && n != 7) {
                fclose(f); oz_map_free(out_map); return false; }
            OzVec3 center = { cx, cy, cz };
            OzVec3 size = { sx, sy, sz };
            OzBrush* nb = oz_map_add_box(out_map, center, size);
            if (!nb) { fclose(f); oz_map_free(out_map); return false; }
            if (n == 7) nb->as.box.rotation_z = rz;
        } else if (strcmp(kind, "cyl") == 0) {
            float cx, cy, cz, rx, ry, h, rz = 0.0f; int seg; int n;
            n = fscanf(f, " %f %f %f %f %f %f %d %f\n", &cx, &cy, &cz, &rx, &ry, &h, &seg, &rz);
            if (n != 7 && n != 8) {
                fclose(f); oz_map_free(out_map); return false; }
            OzBrush* nb = oz_map_add_cylinder(out_map, (OzVec3){cx, cy, cz}, rx, ry, h, seg);
            if (!nb) {
                fclose(f); oz_map_free(out_map); return false; }
            if (n == 8) nb->as.cyl.rotation_z = rz;
        } else if (strcmp(kind, "sph") == 0) {
            float cx, cy, cz, r; int seg;
            if (fscanf(f, " %f %f %f %f %d\n", &cx, &cy, &cz, &r, &seg) != 5) { fclose(f); oz_map_free(out_map); return false; }
            if (!oz_map_add_sphere(out_map, (OzVec3){cx,cy,cz}, r, seg)) { fclose(f); oz_map_free(out_map); return false; }
        } else if (strcmp(kind, "pyr") == 0) {
            float cx, cy, cz, sx, sy, h;
            if (fscanf(f, " %f %f %f %f %f %f\n", &cx,&cy,&cz,&sx,&sy,&h) != 6) { fclose(f); oz_map_free(out_map); return false; }
            if (!oz_map_add_pyramid(out_map, (OzVec3){cx,cy,cz}, sx*0.5f, sy*0.5f, h)) { fclose(f); oz_map_free(out_map); return false; }
        } else if (strcmp(kind, "pln") == 0) {
            float cx,cy,cz,nx,ny,nz,he;
            if (fscanf(f, " %f %f %f %f %f %f %f\n", &cx,&cy,&cz,&nx,&ny,&nz,&he) != 7) { fclose(f); oz_map_free(out_map); return false; }
            if (!oz_map_add_plane(out_map, (OzVec3){cx,cy,cz}, (OzVec3){nx,ny,nz}, he)) { fclose(f); oz_map_free(out_map); return false; }
        } else {
            fclose(f); oz_map_free(out_map); return false;
        }
    }
    fclose(f);
    return true;
}

// --- Simple CSG using AABB approximation for non-boxes ---
static void brush_get_aabb(const OzBrush* b, OzVec3* out_min, OzVec3* out_max) {
    if (b->type == OZ_BRUSH_BOX) {
        const OzBrushBox* bx = &b->as.box;
        // For now, ignore rotation_z in AABB; expand conservatively if rotated
        float cosr = 1.0f, sinr = 0.0f; (void)cosr; (void)sinr;
        if (bx->rotation_z != 0.0f) {
            // approximate by taking max radius in XY
            float r = (float)hypot(bx->half.x, bx->half.y);
            out_min->x = bx->center.x - r;
            out_max->x = bx->center.x + r;
            out_min->y = bx->center.y - r;
            out_max->y = bx->center.y + r;
            out_min->z = bx->center.z - bx->half.z;
            out_max->z = bx->center.z + bx->half.z;
            return;
        }
        out_min->x = bx->center.x - bx->half.x;
        out_max->x = bx->center.x + bx->half.x;
        out_min->y = bx->center.y - bx->half.y;
        out_max->y = bx->center.y + bx->half.y;
        out_min->z = bx->center.z - bx->half.z;
        out_max->z = bx->center.z + bx->half.z;
    } else {
        const OzBrushCylinder* cy = &b->as.cyl;
        float r = (float)hypot(cy->radius_x, cy->radius_y);
        out_min->x = cy->center.x - r;
        out_max->x = cy->center.x + r;
        out_min->y = cy->center.y - r;
        out_max->y = cy->center.y + r;
        out_min->z = cy->center.z - cy->height * 0.5f;
        out_max->z = cy->center.z + cy->height * 0.5f;
    }
}

static void aabb_union(const OzVec3 a_min, const OzVec3 a_max, const OzVec3 b_min, const OzVec3 b_max, OzVec3* out_min, OzVec3* out_max) {
    out_min->x = (a_min.x < b_min.x) ? a_min.x : b_min.x;
    out_min->y = (a_min.y < b_min.y) ? a_min.y : b_min.y;
    out_min->z = (a_min.z < b_min.z) ? a_min.z : b_min.z;
    out_max->x = (a_max.x > b_max.x) ? a_max.x : b_max.x;
    out_max->y = (a_max.y > b_max.y) ? a_max.y : b_max.y;
    out_max->z = (a_max.z > b_max.z) ? a_max.z : b_max.z;
}

static bool aabb_intersect(const OzVec3 a_min, const OzVec3 a_max, const OzVec3 b_min, const OzVec3 b_max, OzVec3* out_min, OzVec3* out_max) {
    out_min->x = (a_min.x > b_min.x) ? a_min.x : b_min.x;
    out_min->y = (a_min.y > b_min.y) ? a_min.y : b_min.y;
    out_min->z = (a_min.z > b_min.z) ? a_min.z : b_min.z;
    out_max->x = (a_max.x < b_max.x) ? a_max.x : b_max.x;
    out_max->y = (a_max.y < b_max.y) ? a_max.y : b_max.y;
    out_max->z = (a_max.z < b_max.z) ? a_max.z : b_max.z;
    return (out_min->x <= out_max->x) && (out_min->y <= out_max->y) && (out_min->z <= out_max->z);
}

bool oz_csg_union(const OzBrush* a, const OzBrush* b, OzBrush* out) {
    if (!a || !b || !out) return false;
    OzVec3 amin, amax, bmin, bmax, umin, umax;
    brush_get_aabb(a, &amin, &amax);
    brush_get_aabb(b, &bmin, &bmax);
    aabb_union(amin, amax, bmin, bmax, &umin, &umax);
    out->type = OZ_BRUSH_BOX;
    out->as.box.center.x = (umin.x + umax.x) * 0.5f;
    out->as.box.center.y = (umin.y + umax.y) * 0.5f;
    out->as.box.center.z = (umin.z + umax.z) * 0.5f;
    out->as.box.half.x = (umax.x - umin.x) * 0.5f;
    out->as.box.half.y = (umax.y - umin.y) * 0.5f;
    out->as.box.half.z = (umax.z - umin.z) * 0.5f;
    out->as.box.rotation_z = 0.0f;
    return true;
}

bool oz_csg_intersect(const OzBrush* a, const OzBrush* b, OzBrush* out) {
    if (!a || !b || !out) return false;
    OzVec3 amin, amax, bmin, bmax, imin, imax;
    brush_get_aabb(a, &amin, &amax);
    brush_get_aabb(b, &bmin, &bmax);
    if (!aabb_intersect(amin, amax, bmin, bmax, &imin, &imax)) return false;
    out->type = OZ_BRUSH_BOX;
    out->as.box.center.x = (imin.x + imax.x) * 0.5f;
    out->as.box.center.y = (imin.y + imax.y) * 0.5f;
    out->as.box.center.z = (imin.z + imax.z) * 0.5f;
    out->as.box.half.x = (imax.x - imin.x) * 0.5f;
    out->as.box.half.y = (imax.y - imin.y) * 0.5f;
    out->as.box.half.z = (imax.z - imin.z) * 0.5f;
    out->as.box.rotation_z = 0.0f;
    return true;
}

bool oz_csg_subtract(const OzBrush* a, const OzBrush* b, OzBrush* out) {
    if (!a || !b || !out) return false;
    // Approximate: subtracting B from A yields intersection of A with complement of B.
    // Using AABB, this devolves to (AABB(A) minus AABB(B)) which is not a single box in general.
    // We return the intersection if it is non-empty; otherwise false.
    // Better: if B's AABB is fully inside A's AABB, shrink along the largest axis.
    OzVec3 amin, amax, bmin, bmax;
    brush_get_aabb(a, &amin, &amax);
    brush_get_aabb(b, &bmin, &bmax);
    // If disjoint, return A
    OzVec3 tmin, tmax;
    if (!aabb_intersect(amin, amax, bmin, bmax, &tmin, &tmax)) {
        *out = *a;
        return true;
    }
    // If B fully contains A, nothing remains
    if (bmin.x <= amin.x && bmin.y <= amin.y && bmin.z <= amin.z && bmax.x >= amax.x && bmax.y >= amax.y && bmax.z >= amax.z) {
        return false;
    }
    // Choose axis with largest extent and clip that side
    float ax = amax.x - amin.x;
    float ay = amax.y - amin.y;
    float az = amax.z - amin.z;
    int axis = (ax >= ay && ax >= az) ? 0 : (ay >= az ? 1 : 2);
    OzVec3 nmin = amin, nmax = amax;
    if (axis == 0) {
        if (bmin.x - amin.x > amax.x - bmax.x) nmin.x = bmax.x; else nmax.x = bmin.x;
    } else if (axis == 1) {
        if (bmin.y - amin.y > amax.y - bmax.y) nmin.y = bmax.y; else nmax.y = bmin.y;
    } else {
        if (bmin.z - amin.z > amax.z - bmax.z) nmin.z = bmax.z; else nmax.z = bmin.z;
    }
    if (nmin.x > nmax.x || nmin.y > nmax.y || nmin.z > nmax.z) return false;
    out->type = OZ_BRUSH_BOX;
    out->as.box.center.x = (nmin.x + nmax.x) * 0.5f;
    out->as.box.center.y = (nmin.y + nmax.y) * 0.5f;
    out->as.box.center.z = (nmin.z + nmax.z) * 0.5f;
    out->as.box.half.x = (nmax.x - nmin.x) * 0.5f;
    out->as.box.half.y = (nmax.y - nmin.y) * 0.5f;
    out->as.box.half.z = (nmax.z - nmin.z) * 0.5f;
    out->as.box.rotation_z = 0.0f;
    return true;
}

// --- Very simple BSP: split along largest axis median (AABB of brushes) ---
static void bsp_tree_reset(OzBspTree* t) { if (!t) return; free(t->nodes); t->nodes = NULL; t->count = t->capacity = 0; t->root = -1; }
void oz_bsp_free(OzBspTree* t) { if (!t) return; if (t->nodes) {
        for (size_t i = 0; i < t->count; ++i) free(t->nodes[i].brush_indices);
        free(t->nodes);
    }
    t->nodes = NULL; t->count = t->capacity = 0; t->root = -1; }

static int bsp_alloc_node(OzBspTree* t) {
    if (t->count == t->capacity) {
        size_t nc = t->capacity ? t->capacity * 2 : 16;
        OzBspNode* mem = (OzBspNode*)realloc(t->nodes, nc * sizeof(OzBspNode));
        if (!mem) return -1;
        memset(mem + t->capacity, 0, (nc - t->capacity) * sizeof(OzBspNode));
        t->nodes = mem; t->capacity = nc;
    }
    int idx = (int)t->count++;
    OzBspNode* n = &t->nodes[idx];
    n->a = 1.0f; n->b = n->c = 0.0f; n->d = 0.0f;
    n->left = n->right = -1;
    n->brush_indices = NULL; n->brush_count = 0;
    return idx;
}

static void compute_brush_aabb_list(const OzMap* map, const int* indices, size_t count, OzVec3* out_min, OzVec3* out_max) {
    out_min->x = out_min->y = out_min->z = 1e9f;
    out_max->x = out_max->y = out_max->z = -1e9f;
    for (size_t i = 0; i < count; ++i) {
        OzVec3 mn, mx; brush_get_aabb(&map->brushes[indices[i]], &mn, &mx);
        if (mn.x < out_min->x) out_min->x = mn.x;
        if (mn.y < out_min->y) out_min->y = mn.y;
        if (mn.z < out_min->z) out_min->z = mn.z;
        if (mx.x > out_max->x) out_max->x = mx.x;
        if (mx.y > out_max->y) out_max->y = mx.y;
        if (mx.z > out_max->z) out_max->z = mx.z;
    }
}

static int bsp_build_recursive(const OzMap* map, OzBspTree* t, const int* indices, size_t count, int depth) {
    int node_idx = bsp_alloc_node(t);
    if (node_idx < 0) return -1;
    OzBspNode* node = &t->nodes[node_idx];
    if (count <= 2 || depth > 16) {
        node->brush_indices = (int*)malloc(sizeof(int) * count);
        if (node->brush_indices) { memcpy(node->brush_indices, indices, sizeof(int) * count); node->brush_count = count; }
        return node_idx;
    }
    OzVec3 mn, mx; compute_brush_aabb_list(map, indices, count, &mn, &mx);
    float sx = mx.x - mn.x, sy = mx.y - mn.y, sz = mx.z - mn.z;
    int axis = (sx >= sy && sx >= sz) ? 0 : (sy >= sz ? 1 : 2);
    float split = (axis == 0 ? (mn.x + mx.x) * 0.5f : (axis == 1 ? (mn.y + mx.y) * 0.5f : (mn.z + mx.z) * 0.5f));
    node->a = (axis == 0); node->b = (axis == 1); node->c = (axis == 2); node->d = -split;
    // Partition
    int* left = (int*)malloc(sizeof(int) * count);
    int* right = (int*)malloc(sizeof(int) * count);
    size_t lc = 0, rc = 0;
    for (size_t i = 0; i < count; ++i) {
        OzVec3 bmn, bmx; brush_get_aabb(&map->brushes[indices[i]], &bmn, &bmx);
        float center = (axis == 0 ? (bmn.x + bmx.x) * 0.5f : (axis == 1 ? (bmn.y + bmx.y) * 0.5f : (bmn.z + bmx.z) * 0.5f));
        if (center < split) left[lc++] = indices[i]; else right[rc++] = indices[i];
    }
    node->left = (lc ? bsp_build_recursive(map, t, left, lc, depth + 1) : -1);
    node->right = (rc ? bsp_build_recursive(map, t, right, rc, depth + 1) : -1);
    free(left); free(right);
    return node_idx;
}

bool oz_bsp_build_from_map(const OzMap* map, OzBspTree* out_tree) {
    if (!map || !out_tree || map->count == 0) return false;
    oz_bsp_free(out_tree);
    int* indices = (int*)malloc(sizeof(int) * map->count);
    if (!indices) return false;
    for (size_t i = 0; i < map->count; ++i) indices[i] = (int)i;
    out_tree->nodes = NULL; out_tree->count = out_tree->capacity = 0; out_tree->root = -1;
    int root = bsp_build_recursive(map, out_tree, indices, map->count, 0);
    free(indices);
    if (root < 0) { oz_bsp_free(out_tree); return false; }
    out_tree->root = root;
    return true;
}
