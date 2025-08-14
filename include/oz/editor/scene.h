#ifndef OZ_EDITOR_SCENE_H
#define OZ_EDITOR_SCENE_H

#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct EditorUI EditorUI;

// Scene Object Types
typedef enum EditorObjectType {
    OBJ_ZONE = 1,
    OBJ_PICKUP = 2,
    OBJ_PLAYER_START = 3,
} EditorObjectType;

typedef struct ObjZoneProps {
    char* name;
    float center[3];
    float radius;
} ObjZoneProps;

typedef struct ObjPickupProps {
    char* name; // type of pickup
    float position[3];
    float respawn_seconds;
} ObjPickupProps;

typedef struct ObjPlayerStartProps {
    float position[3];
    float yaw; // radians
    OzCameraMode camera_mode;
} ObjPlayerStartProps;

typedef struct EditorObject {
    EditorObjectType type;
    union {
        ObjZoneProps zone;
        ObjPickupProps pickup;
        ObjPlayerStartProps pstart;
    } as;
} EditorObject;

// Scene Management
typedef struct EditorScene {
    // Map data
    OzMap map;
    
    // Objects
    EditorObject* objects;
    size_t obj_count;
    size_t obj_capacity;
    
    // Selection state
    int selected_brush_index;      // -1 if none
    int selected_object_index;     // -1 if none
    int hover_object_index;        // -1 if none
    
    // Transform state
    bool gizmo_translate;
    bool gizmo_rotate;
    bool gizmo_scale;
    int gizmo_axis;               // 0=X,1=Y,2=Z, -1=all
    bool gizmo_dragging;
    double gizmo_start_x, gizmo_start_y;
    OzBrush gizmo_start_brush;
    float gizmo_start_obj_pos[3];
    
    // CSG preview (not yet committed)
    bool has_csg_preview;
    OzBrush csg_preview;
    
    // Last used brush settings
    float last_box_w, last_box_h, last_box_d;
    float last_cyl_rx, last_cyl_ry, last_cyl_h;
    int last_cyl_seg;
    
    // Camera
    OzCamera camera;
    double last_tick;
    float last_inst_speed;
    float fps;
} EditorScene;

// Scene Creation and Management
EditorScene* editor_scene_create(void);
void editor_scene_destroy(EditorScene* scene);
void editor_scene_clear(EditorScene* scene);

// File Operations
bool editor_scene_load(EditorScene* scene, const char* filename);
bool editor_scene_save(EditorScene* scene, const char* filename);
bool editor_scene_export_bsp(EditorScene* scene, const char* filename);

// Map Operations
void editor_scene_add_box(EditorScene* scene, const OzVec3* center, const OzVec3* half_extents);
void editor_scene_add_cylinder(EditorScene* scene, const OzVec3* center, float radius_x, float radius_y, float height, int segments);
void editor_scene_add_sphere(EditorScene* scene, const OzVec3* center, float radius, int segments);
void editor_scene_add_pyramid(EditorScene* scene, const OzVec3* center, float width, float height, float depth);
void editor_scene_add_plane(EditorScene* scene, const OzVec3* center, const OzVec3* normal, float size);

void editor_scene_delete_selected_brush(EditorScene* scene);
void editor_scene_duplicate_selected_brush(EditorScene* scene);

// Object Operations
int editor_scene_add_object(EditorScene* scene, const EditorObject* object);
void editor_scene_delete_selected_object(EditorScene* scene);
void editor_scene_move_object(EditorScene* scene, int object_index, const OzVec3* new_position);

// Selection
void editor_scene_select_brush(EditorScene* scene, int index);
void editor_scene_select_object(EditorScene* scene, int index);
void editor_scene_clear_selection(EditorScene* scene);

// CSG Operations
bool editor_scene_csg_union(EditorScene* scene, int brush1, int brush2);
bool editor_scene_csg_subtract(EditorScene* scene, int brush1, int brush2);
bool editor_scene_csg_intersect(EditorScene* scene, int brush1, int brush2);
void editor_scene_commit_csg_preview(EditorScene* scene);
void editor_scene_cancel_csg_preview(EditorScene* scene);

// Transform Operations
void editor_scene_start_transform(EditorScene* scene, double screen_x, double screen_y);
void editor_scene_update_transform(EditorScene* scene, double screen_x, double screen_y, double start_x, double start_y);
void editor_scene_end_transform(EditorScene* scene);

// Camera and View
void editor_scene_update_camera(EditorScene* scene, float dt);
void editor_scene_set_camera_mode(EditorScene* scene, OzCameraMode mode);
void editor_scene_focus_on_selection(EditorScene* scene);

// Picking and Intersection
int editor_scene_pick_brush(EditorScene* scene, float screen_x, float screen_y, int viewport_w, int viewport_h);
int editor_scene_pick_object(EditorScene* scene, float screen_x, float screen_y, int viewport_w, int viewport_h);
bool editor_scene_ray_intersect_brush(EditorScene* scene, int brush_index, const OzVec3* ray_origin, const OzVec3* ray_direction, float* out_distance);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_SCENE_H
