#ifndef OZ_EDITOR_INPUT_H
#define OZ_EDITOR_INPUT_H

#include "../oz_platform.h"
#include <gtk-3.0/gtk/gtk.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct EditorUI EditorUI;
typedef struct EditorScene EditorScene;

// Input State Management
typedef struct EditorInput {
  // Key states
  bool keys[256];

  // Mouse state
  bool mouse_look_active;
  double last_mouse_x;
  double last_mouse_y;
  float mouse_sensitivity; // radians per pixel

  // Dragging state
  bool dragging;
  double drag_last_x;
  double drag_last_y;

  // Transform gizmo state
  bool gizmo_dragging;
  double gizmo_start_x;
  double gizmo_start_y;

  // UI references
  EditorUI *ui;
  EditorScene *scene;
} EditorInput;

// Input System Creation and Management
EditorInput *editor_input_create(EditorUI *ui, EditorScene *scene);
void editor_input_destroy(EditorInput *input);
void editor_input_reset(EditorInput *input);

// Key State Query (compatible with OzKey enum)
bool editor_input_is_key_down(EditorInput *input, enum OzKey key);
bool editor_input_is_key_pressed(EditorInput *input,
                                 enum OzKey key); // Single frame press
bool editor_input_is_key_released(EditorInput *input,
                                  enum OzKey key); // Single frame release

// Mouse State
bool editor_input_is_mouse_look_active(EditorInput *input);
void editor_input_get_mouse_delta(EditorInput *input, float *dx, float *dy);
void editor_input_get_mouse_position(EditorInput *input, double *x, double *y);

// GTK Event Handlers
gboolean editor_input_on_key_press(GtkWidget *widget, GdkEventKey *event,
                                   EditorInput *input);
gboolean editor_input_on_key_release(GtkWidget *widget, GdkEventKey *event,
                                     EditorInput *input);
gboolean editor_input_on_button_press(GtkWidget *widget, GdkEventButton *event,
                                      EditorInput *input);
gboolean editor_input_on_button_release(GtkWidget *widget,
                                        GdkEventButton *event,
                                        EditorInput *input);
gboolean editor_input_on_motion_notify(GtkWidget *widget, GdkEventMotion *event,
                                       EditorInput *input);
gboolean editor_input_on_scroll(GtkWidget *widget, GdkEventScroll *event,
                                EditorInput *input);
gboolean editor_input_on_focus_out(GtkWidget *widget, GdkEvent *event,
                                   EditorInput *input);

// Input Processing
void editor_input_update(EditorInput *input, float dt);
void editor_input_process_camera_movement(EditorInput *input, float dt);
void editor_input_process_selection(EditorInput *input, double x, double y);
void editor_input_process_transform(EditorInput *input, double x, double y);

// Viewport Focus Management
void editor_input_ensure_viewport_focus(EditorInput *input);
void editor_input_set_cursor(EditorInput *input, const char *cursor_name);
void editor_input_reset_cursor(EditorInput *input);

// Utility Functions
void editor_input_clear_all_keys(EditorInput *input);
void editor_input_handle_shortcut(EditorInput *input, guint keyval,
                                  guint state);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_INPUT_H
