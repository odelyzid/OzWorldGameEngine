#ifndef OZ_EDITOR_H
#define OZ_EDITOR_H

#include <gtk/gtk.h>
#include "oz/oz_bsp.h"
#include "oz/oz_camera.h"

typedef struct EditorUi {
    GtkWidget* window;
    GtkWidget* menubar;
    GtkWidget* toolbar;
    GtkWidget* vbox;
    GtkWidget* gl_area;
    GtkWidget* viewport; // GLArea or DrawingArea
    GtkWidget* overlay;  // overlay container for HUD
    GtkWidget* info_label; // top-right debug info
    // GL resources (editor-local)
    unsigned int gl_tex_brush; // 0 if not created
    int gl_tex_w;
    int gl_tex_h;
    gboolean shutting_down;
    unsigned int tick_id;
    unsigned int redraw_id; // for software viewport redraw timer

    // Last used brush settings
    float last_box_w, last_box_h, last_box_d;
    float last_cyl_rx, last_cyl_ry, last_cyl_h;
    int last_cyl_seg;

    // Current map
    OzMap map;
    char* current_path;

    // Editing state
    int selected_index;      // -1 if none
    gboolean dragging;       // in-flight drag of selected brush
    double drag_last_x;      // last mouse x for drag
    double drag_last_y;      // last mouse y for drag

    // Gizmo state
    gboolean gizmo_translate;
    gboolean gizmo_rotate;
    gboolean gizmo_scale;
    int gizmo_axis; // 0=X,1=Y,2=Z, -1=all
    gboolean gizmo_dragging;
    double gizmo_start_x;
    double gizmo_start_y;
    OzBrush gizmo_start_brush;

    // Debug toggles
    gboolean dbg_show_grid;
    gboolean dbg_show_axes;
    gboolean dbg_show_fps;

    // CSG preview (not yet committed)
    gboolean has_csg_preview;
    OzBrush csg_preview;

    // Debug window/JIT
    GtkWidget* debug_window;
    void* jit_handle;

    // Object system (Zones, Pickups, PlayerStart)
    struct EditorObject* objects;
    size_t obj_count;
    size_t obj_capacity;
    int selected_object; // -1 none
    GtkWidget* obj_browser_win;
    GtkWidget* obj_scene_list;
} EditorUi;

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

#endif // OZ_EDITOR_H
