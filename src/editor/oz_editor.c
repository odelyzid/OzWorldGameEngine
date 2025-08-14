#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"
#include "oz/oz_platform.h"
#include <string.h>

// Minimal in-process editor state and operations for the software demo.

typedef struct OzEditorState {
    OzMap* map;           // external map we operate on
    OzWorldProps world;   // ambient light, gravity
    int selected_index;   // selected brush index, -1 if none
    enum { TOOL_NONE, TOOL_ADD_BOX, TOOL_ADD_CYL, TOOL_CSG_ADD, TOOL_CSG_SUB, TOOL_CSG_ISECT } tool;
    // Brush creation parameters (UE1-like primitives)
    float box_size[3];
    float cyl_radius[2];
    float cyl_height;
    int cyl_segments;
} OzEditorState;

static OzEditorState g_editor = {0};

void oz_editor_init(OzMap* target_map) {
    memset(&g_editor, 0, sizeof(g_editor));
    g_editor.map = target_map;
    oz_world_props_default(&g_editor.world);
    g_editor.selected_index = -1;
    g_editor.box_size[0] = 1.0f; g_editor.box_size[1] = 1.0f; g_editor.box_size[2] = 1.0f;
    g_editor.cyl_radius[0] = 0.5f; g_editor.cyl_radius[1] = 0.5f; g_editor.cyl_height = 1.0f; g_editor.cyl_segments = 16;
}

OzWorldProps* oz_editor_world_props(void) { return &g_editor.world; }
int oz_editor_selected(void) { return g_editor.selected_index; }
void oz_editor_select(int index) { if (!g_editor.map) return; if (index >= 0 && (size_t)index < g_editor.map->count) g_editor.selected_index = index; else g_editor.selected_index = -1; }

void oz_editor_begin_add_box(void) { g_editor.tool = TOOL_ADD_BOX; }
void oz_editor_begin_add_cyl(void) { g_editor.tool = TOOL_ADD_CYL; }
void oz_editor_set_box_size(float sx, float sy, float sz) { g_editor.box_size[0]=sx; g_editor.box_size[1]=sy; g_editor.box_size[2]=sz; }
void oz_editor_set_cyl_params(float rx, float ry, float h, int seg) { g_editor.cyl_radius[0]=rx; g_editor.cyl_radius[1]=ry; g_editor.cyl_height=h; g_editor.cyl_segments = (seg<3?3:seg); }

static void add_box_at(const OzCamera* cam) {
    if (!g_editor.map) return; (void)cam;
    oz_map_add_box(g_editor.map, (OzVec3){0.0f,0.0f,0.0f}, (OzVec3){g_editor.box_size[0], g_editor.box_size[1], g_editor.box_size[2]});
}
static void add_cyl_at(const OzCamera* cam) {
    if (!g_editor.map) return; (void)cam;
    oz_map_add_cylinder(g_editor.map, (OzVec3){0.0f,0.0f,0.0f}, g_editor.cyl_radius[0], g_editor.cyl_radius[1], g_editor.cyl_height, g_editor.cyl_segments);
}

void oz_editor_apply_current_tool(const OzCamera* cam) {
    switch (g_editor.tool) {
        case TOOL_ADD_BOX: add_box_at(cam); break;
        case TOOL_ADD_CYL: add_cyl_at(cam); break;
        default: break;
    }
    g_editor.tool = TOOL_NONE;
}

// High-level CSG operations using coarse AABB-based helpers from oz_bsp.c
bool oz_editor_csg_add(int a, int b) {
    if (!g_editor.map) return false; if (a<0||b<0) return false; if ((size_t)a>=g_editor.map->count||(size_t)b>=g_editor.map->count) return false;
    OzBrush out; if (!oz_csg_union(&g_editor.map->brushes[a], &g_editor.map->brushes[b], &out)) return false; return oz_map_append_brush(g_editor.map, &out);
}
bool oz_editor_csg_sub(int a, int b) {
    if (!g_editor.map) return false; if (a<0||b<0) return false; if ((size_t)a>=g_editor.map->count||(size_t)b>=g_editor.map->count) return false;
    OzBrush out; if (!oz_csg_subtract(&g_editor.map->brushes[a], &g_editor.map->brushes[b], &out)) return false; return oz_map_append_brush(g_editor.map, &out);
}
bool oz_editor_csg_isect(int a, int b) {
    if (!g_editor.map) return false; if (a<0||b<0) return false; if ((size_t)a>=g_editor.map->count||(size_t)b>=g_editor.map->count) return false;
    OzBrush out; if (!oz_csg_intersect(&g_editor.map->brushes[a], &g_editor.map->brushes[b], &out)) return false; return oz_map_append_brush(g_editor.map, &out);
}


