#include "oz/editor/scene.h"
#include "oz/oz_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void scene_init_defaults(EditorScene* scene);
static void scene_reserve_objects(EditorScene* scene, size_t capacity);

EditorScene* editor_scene_create(void) {
    EditorScene* scene = calloc(1, sizeof(EditorScene));
    if (!scene) {
        OZ_ERROR("Failed to allocate EditorScene");
        return NULL;
    }
    
    // Initialize map
    oz_map_init(&scene->map);
    
    // Initialize object storage
    scene->objects = NULL;
    scene->obj_count = 0;
    scene->obj_capacity = 0;
    
    // Set defaults
    scene_init_defaults(scene);
    
    OZ_INFO("Editor scene created");
    return scene;
}

void editor_scene_destroy(EditorScene* scene) {
    if (!scene) return;
    
    // Cleanup map
    oz_map_free(&scene->map);
    
    // Cleanup objects
    for (size_t i = 0; i < scene->obj_count; i++) {
        EditorObject* obj = &scene->objects[i];
        if (obj->type == OBJ_ZONE && obj->as.zone.name) {
            free(obj->as.zone.name);
        } else if (obj->type == OBJ_PICKUP && obj->as.pickup.name) {
            free(obj->as.pickup.name);
        }
    }
    free(scene->objects);
    
    free(scene);
    OZ_INFO("Editor scene destroyed");
}

void editor_scene_clear(EditorScene* scene) {
    if (!scene) return;
    
    // Clear map
    oz_map_free(&scene->map);
    oz_map_init(&scene->map);
    
    // Clear objects
    for (size_t i = 0; i < scene->obj_count; i++) {
        EditorObject* obj = &scene->objects[i];
        if (obj->type == OBJ_ZONE && obj->as.zone.name) {
            free(obj->as.zone.name);
        } else if (obj->type == OBJ_PICKUP && obj->as.pickup.name) {
            free(obj->as.pickup.name);
        }
    }
    scene->obj_count = 0;
    
    // Reset state
    scene_init_defaults(scene);
    
    OZ_INFO("Scene cleared");
}

static void scene_init_defaults(EditorScene* scene) {
    // Selection state
    scene->selected_brush_index = -1;
    scene->selected_object_index = -1;
    scene->hover_object_index = -1;
    
    // Transform state
    scene->gizmo_translate = true;
    scene->gizmo_rotate = false;
    scene->gizmo_scale = false;
    scene->gizmo_axis = -1;
    scene->gizmo_dragging = false;
    scene->gizmo_start_x = scene->gizmo_start_y = 0.0;
    
    // CSG state
    scene->has_csg_preview = false;
    
    // Default brush settings
    scene->last_box_w = scene->last_box_h = scene->last_box_d = 1.0f;
    scene->last_cyl_rx = scene->last_cyl_ry = 0.5f;
    scene->last_cyl_h = 1.0f;
    scene->last_cyl_seg = 16;
    
    // Initialize camera
    oz_camera_init(&scene->camera, OZ_CAMERA_FREEMOVE);
    scene->last_tick = 0.0;
    scene->last_inst_speed = 0.0f;
    scene->fps = 60.0f;
}

bool editor_scene_load(EditorScene* scene, const char* filename) {
    if (!scene || !filename) return false;
    
    OZ_INFO("Loading scene from: %s", filename);
    
    // Clear current scene
    editor_scene_clear(scene);
    
    // Load map data
    if (!oz_map_load_text(filename, &scene->map)) {
        OZ_ERROR("Failed to load scene file: %s", filename);
        return false;
    }
    
    // TODO: Load object data and scene metadata
    
    OZ_INFO("Scene loaded successfully: %zu brushes", scene->map.count);
    return true;
}

bool editor_scene_save(EditorScene* scene, const char* filename) {
    if (!scene || !filename) return false;
    
    OZ_INFO("Saving scene to: %s", filename);
    
    // Save map data
    if (!oz_map_save_text(filename, &scene->map)) {
        OZ_ERROR("Failed to save scene file: %s", filename);
        return false;
    }
    
    // TODO: Save object data and scene metadata
    
    OZ_INFO("Scene saved successfully");
    return true;
}

bool editor_scene_export_bsp(EditorScene* scene, const char* filename) {
    if (!scene || !filename) return false;
    
    OZ_INFO("Exporting BSP to: %s", filename);
    
    // TODO: Implement BSP compilation and export
    OZ_WARN("BSP export not yet implemented");
    
    return true;
}

// Map operations
void editor_scene_add_box(EditorScene* scene, const OzVec3* center, const OzVec3* half_extents) {
    if (!scene || !center || !half_extents) return;
    
    oz_map_add_box(&scene->map, *center, *half_extents);
    scene->selected_brush_index = (int)(scene->map.count - 1);
    
    OZ_INFO("Added box at (%.2f, %.2f, %.2f)", center->x, center->y, center->z);
}

void editor_scene_add_cylinder(EditorScene* scene, const OzVec3* center, float radius_x, float radius_y, float height, int segments) {
    if (!scene || !center) return;
    
    oz_map_add_cylinder(&scene->map, *center, radius_x, radius_y, height, segments);
    scene->selected_brush_index = (int)(scene->map.count - 1);
    
    OZ_INFO("Added cylinder at (%.2f, %.2f, %.2f)", center->x, center->y, center->z);
}

void editor_scene_add_sphere(EditorScene* scene, const OzVec3* center, float radius, int segments) {
    if (!scene || !center) return;
    
    oz_map_add_sphere(&scene->map, *center, radius, segments);
    scene->selected_brush_index = (int)(scene->map.count - 1);
    
    OZ_INFO("Added sphere at (%.2f, %.2f, %.2f)", center->x, center->y, center->z);
}

void editor_scene_add_pyramid(EditorScene* scene, const OzVec3* center, float width, float height, float depth) {
    if (!scene || !center) return;
    
    oz_map_add_pyramid(&scene->map, *center, width, height, depth);
    scene->selected_brush_index = (int)(scene->map.count - 1);
    
    OZ_INFO("Added pyramid at (%.2f, %.2f, %.2f)", center->x, center->y, center->z);
}

void editor_scene_add_plane(EditorScene* scene, const OzVec3* center, const OzVec3* normal, float size) {
    if (!scene || !center || !normal) return;
    
    oz_map_add_plane(&scene->map, *center, *normal, size);
    scene->selected_brush_index = (int)(scene->map.count - 1);
    
    OZ_INFO("Added plane at (%.2f, %.2f, %.2f)", center->x, center->y, center->z);
}

void editor_scene_delete_selected_brush(EditorScene* scene) {
    if (!scene || scene->selected_brush_index < 0 || 
        (size_t)scene->selected_brush_index >= scene->map.count) return;
    
    // TODO: Implement brush deletion
    OZ_INFO("Deleting brush %d (TODO: implement)", scene->selected_brush_index);
    scene->selected_brush_index = -1;
}

void editor_scene_duplicate_selected_brush(EditorScene* scene) {
    if (!scene || scene->selected_brush_index < 0 || 
        (size_t)scene->selected_brush_index >= scene->map.count) return;
    
    // TODO: Implement brush duplication
    OZ_INFO("Duplicating brush %d (TODO: implement)", scene->selected_brush_index);
}

// Object operations
static void scene_reserve_objects(EditorScene* scene, size_t capacity) {
    if (scene->obj_capacity >= capacity) return;
    
    size_t new_capacity = scene->obj_capacity == 0 ? 8 : scene->obj_capacity;
    while (new_capacity < capacity) {
        new_capacity *= 2;
    }
    
    EditorObject* new_objects = realloc(scene->objects, new_capacity * sizeof(EditorObject));
    if (!new_objects) {
        OZ_ERROR("Failed to reallocate object storage");
        return;
    }
    
    scene->objects = new_objects;
    scene->obj_capacity = new_capacity;
}

int editor_scene_add_object(EditorScene* scene, const EditorObject* object) {
    if (!scene || !object) return -1;
    
    scene_reserve_objects(scene, scene->obj_count + 1);
    
    int index = (int)scene->obj_count;
    scene->objects[index] = *object;
    
    // Deep copy strings
    if (object->type == OBJ_ZONE && object->as.zone.name) {
        scene->objects[index].as.zone.name = g_strdup(object->as.zone.name);
    } else if (object->type == OBJ_PICKUP && object->as.pickup.name) {
        scene->objects[index].as.pickup.name = g_strdup(object->as.pickup.name);
    }
    
    scene->obj_count++;
    scene->selected_object_index = index;
    
    OZ_INFO("Added object type %d at index %d", object->type, index);
    return index;
}

void editor_scene_delete_selected_object(EditorScene* scene) {
    if (!scene || scene->selected_object_index < 0 || 
        (size_t)scene->selected_object_index >= scene->obj_count) return;
    
    int index = scene->selected_object_index;
    EditorObject* obj = &scene->objects[index];
    
    // Free strings
    if (obj->type == OBJ_ZONE && obj->as.zone.name) {
        free(obj->as.zone.name);
    } else if (obj->type == OBJ_PICKUP && obj->as.pickup.name) {
        free(obj->as.pickup.name);
    }
    
    // Move remaining objects down
    if ((size_t)index < scene->obj_count - 1) {
        memmove(&scene->objects[index], &scene->objects[index + 1], 
                (scene->obj_count - index - 1) * sizeof(EditorObject));
    }
    
    scene->obj_count--;
    scene->selected_object_index = -1;
    
    OZ_INFO("Deleted object at index %d", index);
}

void editor_scene_move_object(EditorScene* scene, int object_index, const OzVec3* new_position) {
    if (!scene || !new_position || object_index < 0 || 
        (size_t)object_index >= scene->obj_count) return;
    
    EditorObject* obj = &scene->objects[object_index];
    
    switch (obj->type) {
        case OBJ_ZONE:
            obj->as.zone.center[0] = new_position->x;
            obj->as.zone.center[1] = new_position->y;
            obj->as.zone.center[2] = new_position->z;
            break;
        case OBJ_PICKUP:
            obj->as.pickup.position[0] = new_position->x;
            obj->as.pickup.position[1] = new_position->y;
            obj->as.pickup.position[2] = new_position->z;
            break;
        case OBJ_PLAYER_START:
            obj->as.pstart.position[0] = new_position->x;
            obj->as.pstart.position[1] = new_position->y;
            obj->as.pstart.position[2] = new_position->z;
            break;
    }
}

// Selection
void editor_scene_select_brush(EditorScene* scene, int index) {
    if (!scene) return;
    
    scene->selected_brush_index = index;
    scene->selected_object_index = -1; // Clear object selection
    
    if (index >= 0) {
        OZ_INFO("Selected brush %d", index);
    }
}

void editor_scene_select_object(EditorScene* scene, int index) {
    if (!scene) return;
    
    scene->selected_object_index = index;
    scene->selected_brush_index = -1; // Clear brush selection
    
    if (index >= 0) {
        OZ_INFO("Selected object %d", index);
    }
}

void editor_scene_clear_selection(EditorScene* scene) {
    if (!scene) return;
    
    scene->selected_brush_index = -1;
    scene->selected_object_index = -1;
    scene->hover_object_index = -1;
}

// Camera and view
void editor_scene_update_camera(EditorScene* scene, float dt) {
    if (!scene) return;
    
    scene->last_inst_speed = oz_camera_update_freemove(&scene->camera, dt, NULL);
    scene->fps = dt > 0.0001f ? 1.0f / dt : 60.0f;
}

void editor_scene_set_camera_mode(EditorScene* scene, OzCameraMode mode) {
    if (!scene) return;
    
    scene->camera.mode = mode;
    OZ_INFO("Camera mode changed to %d", mode);
}

void editor_scene_focus_on_selection(EditorScene* scene) {
    if (!scene) return;
    
    OzVec3 target = {0, 0, 0};
    bool has_target = false;
    
    // Focus on selected brush
    if (scene->selected_brush_index >= 0 && 
        (size_t)scene->selected_brush_index < scene->map.count) {
        const OzBrush* brush = &scene->map.brushes[scene->selected_brush_index];
        if (brush->type == OZ_BRUSH_BOX) {
            target = brush->as.box.center;
            has_target = true;
        } else if (brush->type == OZ_BRUSH_CYLINDER) {
            target = brush->as.cyl.center;
            has_target = true;
        }
    }
    // Focus on selected object
    else if (scene->selected_object_index >= 0 && 
             (size_t)scene->selected_object_index < scene->obj_count) {
        const EditorObject* obj = &scene->objects[scene->selected_object_index];
        if (obj->type == OBJ_ZONE) {
            target.x = obj->as.zone.center[0];
            target.y = obj->as.zone.center[1];
            target.z = obj->as.zone.center[2];
            has_target = true;
        } else if (obj->type == OBJ_PICKUP) {
            target.x = obj->as.pickup.position[0];
            target.y = obj->as.pickup.position[1];
            target.z = obj->as.pickup.position[2];
            has_target = true;
        } else if (obj->type == OBJ_PLAYER_START) {
            target.x = obj->as.pstart.position[0];
            target.y = obj->as.pstart.position[1];
            target.z = obj->as.pstart.position[2];
            has_target = true;
        }
    }
    
    if (has_target) {
        // Position camera to look at target from a good distance
        OzVec3 offset = {-5.0f, -5.0f, 3.0f};
        scene->camera.position.x = target.x + offset.x;
        scene->camera.position.y = target.y + offset.y;
        scene->camera.position.z = target.z + offset.z;
        
        // Look towards target
        float dx = target.x - scene->camera.position.x;
        float dy = target.y - scene->camera.position.y;
        scene->camera.yaw = atan2f(dy, dx);
        scene->camera.pitch = -0.2f; // Slight downward angle
        
        OZ_INFO("Camera focused on selection at (%.2f, %.2f, %.2f)", 
                target.x, target.y, target.z);
    }
}
