// This file now only contains the editor UI implementation (activate impl) and
// related callbacks.
#include "oz/editor.h"
#include "oz/editor/editor_entrypoint.h"
#include "oz/editor/input.h"
#include "oz/editor/ui.h"
#include "oz/oz_assets.h"
#include "oz/render/oz_bsp.h"
#include "oz/oz_bundle.h"
#include "oz/render/oz_camera.h"
#include "oz/oz_log.h"
#include "oz/render/oz_render.h"
#include "oz/render/oz_gl_modern.h"
#include "oz/oz_tex.h"
#include <GL/gl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for actions implemented in src/editor/ui/ui.c
void action_open(GSimpleAction *action, GVariant *parameter,
                 gpointer user_data);
void action_save(GSimpleAction *action, GVariant *parameter,
                 gpointer user_data);
void action_quit(GSimpleAction *action, GVariant *parameter,
                 gpointer user_data);

GtkApplication *g_app_singleton = NULL;

// Forward declarations for cross-calls
struct EditorUi;
static void open_object_properties(struct EditorUi *ui, int index);
// Property editors callbacks
typedef struct PropCtx {
  struct EditorUi *ui;
  int index;
} PropCtx;
static void on_zone_radius_changed(GtkEditable *editable, gpointer user_data) {
  PropCtx *ctx = (PropCtx *)user_data;
  if (!ctx || !ctx->ui)
    return;
  const char *t = gtk_entry_get_text(GTK_ENTRY(editable));
  float v = (float)g_ascii_strtod(t ? t : "0", NULL);
  if (v < 0.01f)
    v = 0.01f;
  if ((size_t)ctx->index < ctx->ui->obj_count &&
      ctx->ui->objects[ctx->index].type == OBJ_ZONE) {
    ctx->ui->objects[ctx->index].as.zone.radius = v;
    if (ctx->ui->viewport)
      gtk_widget_queue_draw(ctx->ui->viewport);
  }
}
static void on_pstart_cam_changed(GtkComboBox *combo, gpointer user_data) {
  PropCtx *ctx = (PropCtx *)user_data;
  if (!ctx || !ctx->ui)
    return;
  int a = gtk_combo_box_get_active(combo);
  if (a < 0)
    a = 0;
  if (a > 2)
    a = 2;
  if ((size_t)ctx->index < ctx->ui->obj_count &&
      ctx->ui->objects[ctx->index].type == OBJ_PLAYER_START) {
    ctx->ui->objects[ctx->index].as.pstart.camera_mode = (OzCameraMode)a;
    if (ctx->ui->viewport)
      gtk_widget_queue_draw(ctx->ui->viewport);
  }
}
static void on_pstart_yaw_changed(GtkEditable *editable, gpointer user_data) {
  PropCtx *ctx = (PropCtx *)user_data;
  if (!ctx || !ctx->ui)
    return;
  const char *t = gtk_entry_get_text(GTK_ENTRY(editable));
  float deg = (float)g_ascii_strtod(t ? t : "0", NULL);
  float rad = deg * (float)G_PI / 180.0f;
  if ((size_t)ctx->index < ctx->ui->obj_count &&
      ctx->ui->objects[ctx->index].type == OBJ_PLAYER_START) {
    ctx->ui->objects[ctx->index].as.pstart.yaw = rad;
    if (ctx->ui->viewport)
      gtk_widget_queue_draw(ctx->ui->viewport);
  }
}

static gboolean env_flag_is_true(const char *name) {
  const char *v = g_getenv(name);
  if (!v)
    return FALSE;
  return g_ascii_strcasecmp(v, "1") == 0 ||
         g_ascii_strcasecmp(v, "true") == 0 ||
         g_ascii_strcasecmp(v, "yes") == 0;
}

static void action_build_common(const char *what) {
  OZ_INFO("Build: %s (stub)", what);
}

// Dialog helpers
static void dialog_set_entry(GtkDialog *dlg, const char *label,
                             GtkWidget **out_entry, const char *initial) {
  GtkWidget *content = gtk_dialog_get_content_area(dlg);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *lab = gtk_label_new(label);
  GtkWidget *ent = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(ent), initial);
  gtk_box_pack_start(GTK_BOX(box), lab, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), ent, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(content), box);
  if (out_entry)
    *out_entry = ent;
}

void action_bsp_box_dialog(GSimpleAction *a, GVariant *p, gpointer user_data) {
  (void)a;
  (void)p;
  EditorUi *ui = (EditorUi *)user_data;
  if (ui->last_box_w <= 0) {
    ui->last_box_w = 1.0f;
    ui->last_box_h = 1.0f;
    ui->last_box_d = 1.0f;
  }
  char wbuf[32], hbuf[32], dbuf[32];
  g_snprintf(wbuf, sizeof(wbuf), "%.2f", ui->last_box_w);
  g_snprintf(hbuf, sizeof(hbuf), "%.2f", ui->last_box_h);
  g_snprintf(dbuf, sizeof(dbuf), "%.2f", ui->last_box_d);
  GtkWidget *dlg = gtk_dialog_new_with_buttons(
      "Box Brush", GTK_WINDOW(ui->window), GTK_DIALOG_MODAL, "_Cancel",
      GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
  GtkWidget *ew = NULL, *eh = NULL, *ed = NULL;
  dialog_set_entry(GTK_DIALOG(dlg), "Width", &ew, wbuf);
  dialog_set_entry(GTK_DIALOG(dlg), "Height", &eh, hbuf);
  dialog_set_entry(GTK_DIALOG(dlg), "Depth", &ed, dbuf);
  gtk_widget_show_all(dlg);
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
    ui->last_box_w =
        (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(ew)), NULL);
    ui->last_box_h =
        (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(eh)), NULL);
    ui->last_box_d =
        (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(ed)), NULL);
    OZ_INFO("Box brush saved: %.2fx%.2fx%.2f", ui->last_box_w, ui->last_box_h,
            ui->last_box_d);
    // Add a new box brush at origin and redraw
    oz_map_add_box(&ui->map, (OzVec3){0, 0, 0},
                   (OzVec3){ui->last_box_w, ui->last_box_h, ui->last_box_d});
    if (ui->viewport && GTK_IS_WIDGET(ui->viewport))
      gtk_widget_queue_draw(ui->viewport);
  }
  gtk_widget_destroy(dlg);
}

void action_bsp_cyl_dialog(GSimpleAction *a, GVariant *p, gpointer user_data) {
  (void)a;
  (void)p;
  EditorUi *ui = (EditorUi *)user_data;
  if (ui->last_cyl_rx <= 0) {
    ui->last_cyl_rx = 0.5f;
    ui->last_cyl_ry = 0.5f;
    ui->last_cyl_h = 1.0f;
    ui->last_cyl_seg = 16;
  }
  char rxbuf[32], rybuf[32], hbuf[32], sbuf[32];
  g_snprintf(rxbuf, sizeof(rxbuf), "%.2f", ui->last_cyl_rx);
  g_snprintf(rybuf, sizeof(rybuf), "%.2f", ui->last_cyl_ry);
  g_snprintf(hbuf, sizeof(hbuf), "%.2f", ui->last_cyl_h);
  g_snprintf(sbuf, sizeof(sbuf), "%d", ui->last_cyl_seg);
  GtkWidget *dlg = gtk_dialog_new_with_buttons(
      "Cylinder Brush", GTK_WINDOW(ui->window), GTK_DIALOG_MODAL, "_Cancel",
      GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
  GtkWidget *erx = NULL, *ery = NULL, *eh = NULL, *es = NULL;
  dialog_set_entry(GTK_DIALOG(dlg), "Radius X", &erx, rxbuf);
  dialog_set_entry(GTK_DIALOG(dlg), "Radius Y", &ery, rybuf);
  dialog_set_entry(GTK_DIALOG(dlg), "Height", &eh, hbuf);
  dialog_set_entry(GTK_DIALOG(dlg), "Segments", &es, sbuf);
  gtk_widget_show_all(dlg);
  if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
    ui->last_cyl_rx =
        (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(erx)), NULL);
    ui->last_cyl_ry =
        (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(ery)), NULL);
    ui->last_cyl_h =
        (float)g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(eh)), NULL);
    ui->last_cyl_seg =
        (int)g_ascii_strtoll(gtk_entry_get_text(GTK_ENTRY(es)), NULL, 10);
    if (ui->last_cyl_seg < 3)
      ui->last_cyl_seg = 3;
    OZ_INFO("Cylinder brush saved: rx=%.2f ry=%.2f h=%.2f seg=%d",
            ui->last_cyl_rx, ui->last_cyl_ry, ui->last_cyl_h, ui->last_cyl_seg);
    oz_map_add_cylinder(&ui->map, (OzVec3){0, 0, 0}, ui->last_cyl_rx,
                        ui->last_cyl_ry, ui->last_cyl_h, ui->last_cyl_seg);
    if (ui->viewport && GTK_IS_WIDGET(ui->viewport))
      gtk_widget_queue_draw(ui->viewport);
  }
  gtk_widget_destroy(dlg);
}

// Stubs for CSG/tools
void action_csg_add(GSimpleAction *a, GVariant *p, gpointer user_data) {
  (void)a;
  (void)p;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui || ui->map.count < 2) {
    OZ_WARN("CSG Add requires at least 2 brushes");
    return;
  }
  size_t i0 = (size_t)(ui->selected_index >= 0 ? ui->selected_index : 0);
  size_t i1 = (i0 + 1) % ui->map.count;
  OzBrush out;
  if (oz_csg_union(&ui->map.brushes[i0], &ui->map.brushes[i1], &out)) {
    ui->csg_preview = out;
    ui->has_csg_preview = TRUE;
    OZ_INFO("CSG Union preview ready (not committed)");
    if (ui->viewport)
      gtk_widget_queue_draw(ui->viewport);
  } else {
    OZ_WARN("CSG Union produced no result");
  }
}

void action_csg_sub(GSimpleAction *a, GVariant *p, gpointer user_data) {
  (void)a;
  (void)p;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui || ui->map.count < 2) {
    OZ_WARN("CSG Subtract requires at least 2 brushes");
    return;
  }
  size_t i0 = (size_t)(ui->selected_index >= 0 ? ui->selected_index : 0);
  size_t i1 = (i0 + 1) % ui->map.count;
  OzBrush out;
  if (oz_csg_subtract(&ui->map.brushes[i0], &ui->map.brushes[i1], &out)) {
    ui->csg_preview = out;
    ui->has_csg_preview = TRUE;
    OZ_INFO("CSG Subtract preview ready (not committed)");
    if (ui->viewport)
      gtk_widget_queue_draw(ui->viewport);
  } else {
    OZ_WARN("CSG Subtract produced no result");
  }
}

void action_csg_isect(GSimpleAction *a, GVariant *p, gpointer user_data) {
  (void)a;
  (void)p;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui || ui->map.count < 2) {
    OZ_WARN("CSG Intersect requires at least 2 brushes");
    return;
  }
  size_t i0 = (size_t)(ui->selected_index >= 0 ? ui->selected_index : 0);
  size_t i1 = (i0 + 1) % ui->map.count;
  OzBrush out;
  if (oz_csg_intersect(&ui->map.brushes[i0], &ui->map.brushes[i1], &out)) {
    ui->csg_preview = out;
    ui->has_csg_preview = TRUE;
    OZ_INFO("CSG Intersect preview ready (not committed)");
    if (ui->viewport)
      gtk_widget_queue_draw(ui->viewport);
  } else {
    OZ_WARN("CSG Intersect produced no result");
  }
}
void action_tool_carve(GSimpleAction *a, GVariant *p, gpointer u) {
  (void)a;
  (void)p;
  (void)u;
  OZ_INFO("Carve (stub)");
}
void action_tool_slope(GSimpleAction *a, GVariant *p, gpointer u) {
  (void)a;
  (void)p;
  (void)u;
  OZ_INFO("Slope (stub)");
}

// Toolbar button shims (match GTK signal signature)
static void toolbar_open_clicked(GtkToolButton *btn, gpointer user_data) {
  (void)btn;
  action_open(NULL, NULL, user_data);
}
static void toolbar_save_clicked(GtkToolButton *btn, gpointer user_data) {
  (void)btn;
  action_save(NULL, NULL, user_data);
}

// --- Camera + input state for editor viewport ---
typedef struct EditorInputState {
  gboolean keys[256];
  OzCamera cam;
  double last_tick;
  float last_inst_speed;
  float fps;
  gboolean mouse_look_active;
  double last_mouse_x;
  double last_mouse_y;
  float mouse_sensitivity; // radians per pixel
} EditorInputState;

static EditorInputState *g_editor_state = NULL;
// Helper: screen-space distance from point to segment within tolerance
static bool is_near_segment(float px, float py, float x0, float y0, float x1,
                            float y1, float tol_pix) {
  float vx = x1 - x0, vy = y1 - y0;
  float wx = px - x0, wy = py - y0;
  float vv = vx * vx + vy * vy;
  if (vv < 1e-5f)
    return false;
  float t = (vx * wx + vy * wy) / vv;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;
  float dxp = (x0 + t * vx) - px;
  float dyp = (y0 + t * vy) - py;
  return (dxp * dxp + dyp * dyp) <= tol_pix * tol_pix;
}
static bool editor_key_down(enum OzKey key) {
  if (!g_editor_state)
    return false;
  switch (key) {
  case OZ_KEY_W:
    return g_editor_state->keys['W'] || g_editor_state->keys['w'];
  case OZ_KEY_A:
    return g_editor_state->keys['A'] || g_editor_state->keys['a'];
  case OZ_KEY_S:
    return g_editor_state->keys['S'] || g_editor_state->keys['s'];
  case OZ_KEY_D:
    return g_editor_state->keys['D'] || g_editor_state->keys['d'];
  case OZ_KEY_Q:
    return g_editor_state->keys['Q'] || g_editor_state->keys['q'] ||
           g_editor_state->keys['C'] || g_editor_state->keys['c'];
  case OZ_KEY_E:
    return g_editor_state->keys['E'] || g_editor_state->keys['e'] ||
           g_editor_state->keys[' '];
  case OZ_KEY_LEFT:
    return g_editor_state->keys[GDK_KEY_Left & 0xFF];
  case OZ_KEY_RIGHT:
    return g_editor_state->keys[GDK_KEY_Right & 0xFF];
  case OZ_KEY_UP:
    return g_editor_state->keys[GDK_KEY_Up & 0xFF];
  case OZ_KEY_DOWN:
    return g_editor_state->keys[GDK_KEY_Down & 0xFF];
  default:
    return false;
  }
}
static void ensure_viewport_focus(EditorUi *ui) {
  if (ui && ui->viewport) {
    gtk_widget_set_can_focus(ui->viewport, TRUE);
    gtk_widget_grab_focus(ui->viewport);
  }
}

static void editor_project_point(EditorUi *ui, float x, float y, float z,
                                 float *out_x, float *out_y) {
  int w = gtk_widget_get_allocated_width(ui->viewport);
  int h = gtk_widget_get_allocated_height(ui->viewport);
  oz_render_soft_project_point(g_editor_state ? &g_editor_state->cam : NULL, x,
                               y, z, w, h, out_x, out_y);
}

static gboolean on_key_press(GtkWidget *w, GdkEventKey *e, gpointer user_data) {
  (void)w;
  EditorInputState *s = (EditorInputState *)user_data;
  guint k = e->keyval;
  editor_input_handle_key(k, true, e->state);
  if (env_flag_is_true("OZ_DEBUG_INPUT")) {
    OZ_INFO("key down: %u '%c' state=%u", k,
            (k >= 32 && k < 127) ? (int)k : '.', (unsigned)e->state);
  }
  // Lighting toggle with 'L'
  if (e->keyval == GDK_KEY_l || e->keyval == GDK_KEY_L) {
    EditorUi *ui = (EditorUi *)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
    if (ui) {
      ui->dbg_lighting = !ui->dbg_lighting;
      if (ui->viewport)
        gtk_widget_queue_draw(ui->viewport);
    }
  }
  return FALSE;
}

static gboolean on_key_release(GtkWidget *w, GdkEventKey *e,
                               gpointer user_data) {
  (void)w;
  (void)user_data;
  guint k = e->keyval;
  editor_input_handle_key(k, false, e->state);
  if (env_flag_is_true("OZ_DEBUG_INPUT")) {
    OZ_INFO("key up: %u '%c' state=%u", k, (k >= 32 && k < 127) ? (int)k : '.',
            (unsigned)e->state);
  }
  return FALSE;
}

// --- Mouse look handlers ---
static gboolean on_button_press(GtkWidget *w, GdkEventButton *e,
                                gpointer user_data) {
  EditorUi *ui = (EditorUi *)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
  EditorInputState *s = (EditorInputState *)user_data;
  ensure_viewport_focus(ui);
  if (e->button == 1) {
    // Object picking (single-click)
    if (ui && ui->obj_count) {
      float best_d2 = 1e9f;
      int best_idx = -1;
      for (size_t i = 0; i < ui->obj_count; ++i) {
        const EditorObject *o = &ui->objects[i];
        float ox, oy, oz;
        if (o->type == OBJ_ZONE) {
          ox = o->as.zone.center[0];
          oy = o->as.zone.center[1];
          oz = o->as.zone.center[2];
        } else if (o->type == OBJ_PICKUP) {
          ox = o->as.pickup.position[0];
          oy = o->as.pickup.position[1];
          oz = o->as.pickup.position[2];
        } else if (o->type == OBJ_PLAYER_START) {
          ox = o->as.pstart.position[0];
          oy = o->as.pstart.position[1];
          oz = o->as.pstart.position[2];
        } else
          continue;
        float sx, sy;
        editor_project_point(ui, ox, oy, oz, &sx, &sy);
        float dx = (float)e->x - sx;
        float dy = (float)e->y - sy;
        float d2 = dx * dx + dy * dy;
        if (d2 < best_d2) {
          best_d2 = d2;
          best_idx = (int)i;
        }
      }
      if (best_idx >= 0 && best_d2 < 14.0f * 14.0f) {
        ui->selected_object = best_idx;
        open_object_properties(ui, best_idx);
        if (GTK_IS_WIDGET(w))
          gtk_widget_queue_draw(w);
        return TRUE;
      }
    }
    // If Shift held, start drag-select/move; else mouse-look or gizmo drag
    gboolean shift = (e->state & GDK_SHIFT_MASK) != 0;
    s->last_mouse_x = e->x;
    s->last_mouse_y = e->y;
    if (shift && ui) {
      ui->dragging = TRUE;
      ui->drag_last_x = e->x;
      ui->drag_last_y = e->y;
      // If nothing selected yet, select nearest brush by simple screen-space
      // hit
      if (ui->selected_index < 0 && ui->map.count > 0) {
        ui->selected_index =
            0; // TODO: implement proper picking; select first for now
        OZ_INFO("Selected brush #%d", ui->selected_index);
      }
      // If an object is selected, start dragging it instead; cancel brush drag
      if (ui->selected_object >= 0 &&
          (size_t)ui->selected_object < ui->obj_count) {
        ui->selected_index = -1;
      }
    } else if (ui && (ui->selected_index >= 0 ||
                      (ui->selected_object >= 0 &&
                       (size_t)ui->selected_object < ui->obj_count))) {
      // Begin gizmo drag for brush or object; axis chosen by current setting or
      // dominant delta (set later)
      ui->gizmo_dragging = TRUE;
      ui->gizmo_start_x = e->x;
      ui->gizmo_start_y = e->y;
      if (ui->selected_index >= 0)
        ui->gizmo_start_brush = ui->map.brushes[ui->selected_index];
      if (ui->selected_object >= 0 &&
          (size_t)ui->selected_object < ui->obj_count) {
        const EditorObject *o = &ui->objects[ui->selected_object];
        if (o->type == OBJ_ZONE) {
          ui->gizmo_start_obj_pos[0] = o->as.zone.center[0];
          ui->gizmo_start_obj_pos[1] = o->as.zone.center[1];
          ui->gizmo_start_obj_pos[2] = o->as.zone.center[2];
        } else if (o->type == OBJ_PICKUP) {
          ui->gizmo_start_obj_pos[0] = o->as.pickup.position[0];
          ui->gizmo_start_obj_pos[1] = o->as.pickup.position[1];
          ui->gizmo_start_obj_pos[2] = o->as.pickup.position[2];
        } else {
          ui->gizmo_start_obj_pos[0] = o->as.pstart.position[0];
          ui->gizmo_start_obj_pos[1] = o->as.pstart.position[1];
          ui->gizmo_start_obj_pos[2] = o->as.pstart.position[2];
        }
      }
      s->mouse_look_active = FALSE;
      gdk_window_set_cursor(
          gtk_widget_get_window(w),
          gdk_cursor_new_from_name(gdk_display_get_default(), "crosshair"));
    } else {
      s->mouse_look_active = TRUE;
      gdk_window_set_cursor(
          gtk_widget_get_window(w),
          gdk_cursor_new_from_name(gdk_display_get_default(), "none"));
    }
  }
  return TRUE;
}

static gboolean on_button_release(GtkWidget *w, GdkEventButton *e,
                                  gpointer user_data) {
  EditorUi *ui = (EditorUi *)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
  EditorInputState *s = (EditorInputState *)user_data;
  if (e->button == 1) {
    if (s->mouse_look_active) {
      s->mouse_look_active = FALSE;
      gdk_window_set_cursor(gtk_widget_get_window(w), NULL);
    }
    if (ui && ui->dragging)
      ui->dragging = FALSE;
    if (ui && ui->gizmo_dragging) {
      ui->gizmo_dragging = FALSE;
      gdk_window_set_cursor(gtk_widget_get_window(w), NULL);
    }
  }
  return TRUE;
}

static gboolean on_motion(GtkWidget *w, GdkEventMotion *e, gpointer user_data) {
  EditorUi *ui = (EditorUi *)g_object_get_data(G_OBJECT(w), "oz_editor_ui");
  EditorInputState *s = (EditorInputState *)user_data;
  double dx = e->x - s->last_mouse_x;
  double dy = e->y - s->last_mouse_y;
  s->last_mouse_x = e->x;
  s->last_mouse_y = e->y;
  if (s->mouse_look_active) {
    // Flip pitch direction and invert yaw X-axis mapping (Unreal-like)
    s->cam.yaw -= (float)(dx)*s->mouse_sensitivity;
    s->cam.pitch -= (float)(dy)*s->mouse_sensitivity;
    const float kMaxPitch = 1.55334306f;
    if (s->cam.pitch > kMaxPitch)
      s->cam.pitch = kMaxPitch;
    if (s->cam.pitch < -kMaxPitch)
      s->cam.pitch = -kMaxPitch;
  } else if (ui && ui->gizmo_dragging &&
             (ui->selected_index >= 0 ||
              (ui->selected_object >= 0 &&
               (size_t)ui->selected_object < ui->obj_count))) {
    // Gizmo drag: move along selected axis; if axis is -1, choose dominant
    // mouse delta X->X axis, Y->Y axis
    double ddx = e->x - ui->gizmo_start_x;
    double ddy = e->y - ui->gizmo_start_y;
    int axis = ui->gizmo_axis;
    if (axis < 0)
      axis = (fabs(ddx) > fabs(ddy)) ? 0 : 1;
    float scale = 0.01f;
    if (ui->selected_index >= 0 && ui->selected_index < (int)ui->map.count) {
      OzBrush *br = &ui->map.brushes[ui->selected_index];
      if (ui->gizmo_translate) {
        if (br->type == OZ_BRUSH_BOX) {
          *br = ui->gizmo_start_brush;
          if (axis == 0)
            br->as.box.center.x += (float)(ddx * scale);
          else if (axis == 1)
            br->as.box.center.y -= (float)(ddy * scale);
          else
            br->as.box.center.z += (float)((-ddy) * scale);
        }
      } else if (ui->gizmo_scale) {
        if (br->type == OZ_BRUSH_BOX) {
          *br = ui->gizmo_start_brush;
          float delta = (float)((fabs(ddx) > fabs(ddy) ? ddx : -ddy) * scale);
          if (axis == 0)
            br->as.box.half.x =
                fmaxf(0.05f, ui->gizmo_start_brush.as.box.half.x + delta);
          else if (axis == 1)
            br->as.box.half.y =
                fmaxf(0.05f, ui->gizmo_start_brush.as.box.half.y + delta);
          else
            br->as.box.half.z =
                fmaxf(0.05f, ui->gizmo_start_brush.as.box.half.z + delta);
        }
      } else if (ui->gizmo_rotate) {
        if (br->type == OZ_BRUSH_BOX) {
          *br = ui->gizmo_start_brush;
          float angle =
              (float)(ddx * 0.005f); // rotate around Z by horizontal drag
          br->as.box.rotation_z =
              ui->gizmo_start_brush.as.box.rotation_z + angle;
        }
      }
    } else if (ui->selected_object >= 0 &&
               (size_t)ui->selected_object < ui->obj_count) {
      EditorObject *o = &ui->objects[ui->selected_object];
      float dx = (float)(ddx * scale), dy = (float)(-ddy * scale);
      float nx = 0, ny = 0, nz = 0;
      if (axis == 0)
        nx = dx;
      else if (axis == 1)
        ny = dy;
      else
        nz = dy; // map vertical mouse movement to Z when axis=2
      if (o->type == OBJ_ZONE) {
        o->as.zone.center[0] = ui->gizmo_start_obj_pos[0] + nx;
        o->as.zone.center[1] = ui->gizmo_start_obj_pos[1] + ny;
        o->as.zone.center[2] = ui->gizmo_start_obj_pos[2] + nz;
      } else if (o->type == OBJ_PICKUP) {
        o->as.pickup.position[0] = ui->gizmo_start_obj_pos[0] + nx;
        o->as.pickup.position[1] = ui->gizmo_start_obj_pos[1] + ny;
        o->as.pickup.position[2] = ui->gizmo_start_obj_pos[2] + nz;
      } else {
        o->as.pstart.position[0] = ui->gizmo_start_obj_pos[0] + nx;
        o->as.pstart.position[1] = ui->gizmo_start_obj_pos[1] + ny;
        o->as.pstart.position[2] = ui->gizmo_start_obj_pos[2] + nz;
      }
    }
    if (GTK_IS_WIDGET(w))
      gtk_widget_queue_draw(w);
  } else if (ui && ui->dragging && ui->selected_index >= 0 &&
             ui->selected_index < (int)ui->map.count) {
    // Simple screen-space drag: move selected brush in X/Y plane according to
    // dx/dy, scaled by camera
    OzBrush *br = &ui->map.brushes[ui->selected_index];
    float pixelsPerUnit = 100.0f; // tweakable
    float mx = (float)(dx) / pixelsPerUnit;
    float my = (float)(-dy) / pixelsPerUnit;
    if (br->type == OZ_BRUSH_BOX) {
      br->as.box.center.x += mx;
      br->as.box.center.y += my;
    } else if (br->type == OZ_BRUSH_CYLINDER) {
      br->as.cyl.center.x += mx;
      br->as.cyl.center.y += my;
    }
    if (GTK_IS_WIDGET(w))
      gtk_widget_queue_draw(w);
  } else if (ui && ui->dragging && ui->selected_object >= 0 &&
             (size_t)ui->selected_object < ui->obj_count) {
    // Drag selected object in X/Y plane
    EditorObject *o = &ui->objects[ui->selected_object];
    float pixelsPerUnit = 100.0f;
    float mx = (float)(dx) / pixelsPerUnit;
    float my = (float)(-dy) / pixelsPerUnit;
    if (o->type == OBJ_ZONE) {
      o->as.zone.center[0] += mx;
      o->as.zone.center[1] += my;
    } else if (o->type == OBJ_PICKUP) {
      o->as.pickup.position[0] += mx;
      o->as.pickup.position[1] += my;
    } else if (o->type == OBJ_PLAYER_START) {
      o->as.pstart.position[0] += mx;
      o->as.pstart.position[1] += my;
    }
    if (GTK_IS_WIDGET(w))
      gtk_widget_queue_draw(w);
  } else if (ui) {
    // Hover feedback: axis or object under cursor
    // Check axis hover at origin gizmo in screen space
    float ax0, ay0, ax1, ay1, bx0, by0, bx1, by1, cx0, cy0, cx1, cy1;
    editor_project_point(ui, 0, 0, 0, &ax0, &ay0);
    editor_project_point(ui, 3, 0, 0, &ax1, &ay1);
    editor_project_point(ui, 0, 3, 0, &bx1, &by1);
    bx0 = ax0;
    by0 = ay0;
    editor_project_point(ui, 0, 0, 3, &cx1, &cy1);
    cx0 = ax0;
    cy0 = ay0;
    ui->gizmo_axis = -1;
    const float tol = 6.0f;
    if (is_near_segment((float)e->x, (float)e->y, ax0, ay0, ax1, ay1, tol))
      ui->gizmo_axis = 0;
    else if (is_near_segment((float)e->x, (float)e->y, bx0, by0, bx1, by1, tol))
      ui->gizmo_axis = 1;
    else if (is_near_segment((float)e->x, (float)e->y, cx0, cy0, cx1, cy1, tol))
      ui->gizmo_axis = 2;
    // Object hover (nearest projected center within radius)
    ui->hover_object = -1;
    float best_d2 = 1e9f;
    int best = -1;
    for (size_t i = 0; i < ui->obj_count; ++i) {
      float ox = 0, oy = 0, oz = 0;
      if (ui->objects[i].type == OBJ_ZONE) {
        ox = ui->objects[i].as.zone.center[0];
        oy = ui->objects[i].as.zone.center[1];
        oz = ui->objects[i].as.zone.center[2];
      } else if (ui->objects[i].type == OBJ_PICKUP) {
        ox = ui->objects[i].as.pickup.position[0];
        oy = ui->objects[i].as.pickup.position[1];
        oz = ui->objects[i].as.pickup.position[2];
      } else {
        ox = ui->objects[i].as.pstart.position[0];
        oy = ui->objects[i].as.pstart.position[1];
        oz = ui->objects[i].as.pstart.position[2];
      }
      float sx, sy;
      editor_project_point(ui, ox, oy, oz, &sx, &sy);
      float dxp = (float)e->x - sx, dyp = (float)e->y - sy;
      float d2 = dxp * dxp + dyp * dyp;
      if (d2 < best_d2) {
        best_d2 = d2;
        best = (int)i;
      }
    }
    if (best >= 0 && best_d2 < 18.0f * 18.0f)
      ui->hover_object = best;
    if (GTK_IS_WIDGET(w))
      gtk_widget_queue_draw(w);
  }
  return TRUE;
}

static gboolean on_focus_out(GtkWidget *w, GdkEvent *e, gpointer user_data) {
  (void)w;
  (void)e;
  (void)user_data;
  editor_input_focus_clear_all();
  return FALSE;
}

static gboolean tick_update(gpointer user_data) {
  EditorUi *ui = (EditorUi *)user_data;
  if (ui && ui->shutting_down)
    return FALSE; // stop timer cleanly
  if (!g_editor_state)
    return TRUE;
  double now = g_get_monotonic_time() / 1000000.0;
  float dt = (float)(now - g_editor_state->last_tick);
  if (dt < 0.0001f)
    dt = 0.0001f;
  g_editor_state->last_tick = now;
  // Update using shared editor input adapter
  g_editor_state->last_inst_speed = oz_camera_update_freemove(
      &g_editor_state->cam, dt, editor_input_is_down_oz);
  // Here, just update fps and request redraw
  g_editor_state->fps = 1.0f / dt;
  if (env_flag_is_true("OZ_DEBUG_LOOP")) {
    OZ_INFO("tick dt=%.3f fps=%.0f pos=(%.2f,%.2f,%.2f) yaw=%.2f pitch=%.2f "
            "speed=%.2f",
            dt, g_editor_state->fps, g_editor_state->cam.position.x,
            g_editor_state->cam.position.y, g_editor_state->cam.position.z,
            g_editor_state->cam.yaw, g_editor_state->cam.pitch,
            g_editor_state->last_inst_speed);
  }
  // Update HUD label
  if (ui && ui->info_label && GTK_IS_LABEL(ui->info_label) &&
      ui->dbg_show_fps) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Pos: (%.2f, %.2f, %.2f)  Yaw: %.1f  Pitch: %.1f  FPS: %.0f",
             g_editor_state->cam.position.x, g_editor_state->cam.position.y,
             g_editor_state->cam.position.z,
             g_editor_state->cam.yaw * 57.29578f,
             g_editor_state->cam.pitch * 57.29578f, g_editor_state->fps);
    gtk_label_set_text(GTK_LABEL(ui->info_label), buf);
  }
  if (ui && ui->viewport && GTK_IS_WIDGET(ui->viewport))
    gtk_widget_queue_draw(ui->viewport);
  editor_input_tick(now);
  return TRUE; // keep timer
}

// Modern GL rendering using VBO/VAO + shaders
static gboolean gl_area_render_modern(EditorUi *ui, int w, int h) {
  if (!ui || !ui->modern_renderer) return FALSE;
  
  // Set viewport and camera
  oz_gl_modern_set_viewport(ui->modern_renderer, w, h);
  if (g_editor_state) {
    oz_gl_modern_apply_camera(ui->modern_renderer, &g_editor_state->cam);
  }
  
  // Set lighting
  if (ui->dbg_lighting) {
    oz_gl_modern_set_ambient_light(ui->modern_renderer, 0.15f, 0.15f, 0.18f);
  } else {
    oz_gl_modern_set_ambient_light(ui->modern_renderer, 0.7f, 0.7f, 0.75f);
  }
  
  // Clear and draw
  oz_gl_modern_clear(ui->modern_renderer, 0.12f, 0.12f, 0.14f, 1.0f);
  
  // Grid and axes
  if (ui->dbg_show_grid) {
    OzVec3 grid_color = {0.3f, 0.32f, 0.35f};
    oz_gl_modern_draw_grid(ui->modern_renderer, 20.0f, 1.0f, &grid_color);
    
    if (ui->dbg_show_axes) {
      oz_gl_modern_draw_axes(ui->modern_renderer, 20.0f);
    }
  }
  
  // Draw map
  oz_gl_modern_draw_map_filled(ui->modern_renderer, &ui->map);
  oz_gl_modern_draw_map_wireframe(ui->modern_renderer, &ui->map, ui->selected_index);
  
  // Draw objects
  if (ui->obj_count) {
    for (size_t i = 0; i < ui->obj_count; ++i) {
      const EditorObject *o = &ui->objects[i];
      if (o->type == OBJ_ZONE) {
        OzVec3 center = {o->as.zone.center[0], o->as.zone.center[1], o->as.zone.center[2]};
        OzVec3 color = {0.3f, 0.9f, 0.9f};
        oz_gl_modern_draw_zone_circle(ui->modern_renderer, &center, o->as.zone.radius, &color);
      } else if (o->type == OBJ_PICKUP) {
        OzVec3 center = {o->as.pickup.position[0], o->as.pickup.position[1], o->as.pickup.position[2]};
        OzVec3 color = {0.9f, 0.85f, 0.2f};
        oz_gl_modern_draw_pickup_cube(ui->modern_renderer, &center, 0.1f, &color);
      } else if (o->type == OBJ_PLAYER_START) {
        OzVec3 pos = {o->as.pstart.position[0], o->as.pstart.position[1], o->as.pstart.position[2]};
        OzVec3 color = {0.2f, 1.0f, 0.3f};
        oz_gl_modern_draw_player_start(ui->modern_renderer, &pos, o->as.pstart.yaw, &color);
      }
    }
  }
  
  // Draw transform gizmo for selected object
  if ((ui->selected_index >= 0 && ui->selected_index < (int)ui->map.count) ||
      (ui->selected_object >= 0 && (size_t)ui->selected_object < ui->obj_count)) {
    OzVec3 gizmo_center = {0, 0, 0};
    
    if (ui->selected_index >= 0) {
      const OzBrush *sel = &ui->map.brushes[ui->selected_index];
      OzVec3 p = (sel->type == OZ_BRUSH_BOX) ? sel->as.box.center : sel->as.cyl.center;
      gizmo_center = p;
    } else {
      const EditorObject *o = &ui->objects[ui->selected_object];
      if (o->type == OBJ_ZONE) {
        gizmo_center = (OzVec3){o->as.zone.center[0], o->as.zone.center[1], o->as.zone.center[2]};
      } else if (o->type == OBJ_PICKUP) {
        gizmo_center = (OzVec3){o->as.pickup.position[0], o->as.pickup.position[1], o->as.pickup.position[2]};
      } else {
        gizmo_center = (OzVec3){o->as.pstart.position[0], o->as.pstart.position[1], o->as.pstart.position[2]};
      }
    }
    
    oz_gl_modern_draw_gizmo(ui->modern_renderer, &gizmo_center, 3.0f, ui->gizmo_axis);
  }
  
  return TRUE;
}

// GL rendering helpers for the viewport
static gboolean gl_area_render(GtkGLArea *area, GdkGLContext *context,
                               gpointer user_data) {
  (void)area;
  (void)context;
  EditorUi *ui = (EditorUi *)user_data;
  gtk_gl_area_make_current(area);
  if (gtk_gl_area_get_error(area))
    return FALSE;
  int w = gtk_widget_get_allocated_width(GTK_WIDGET(area));
  int h = gtk_widget_get_allocated_height(GTK_WIDGET(area));
  if (env_flag_is_true("OZ_DEBUG_RENDER")) {
    OZ_INFO("gl_area_render w=%d h=%d brushes=%zu", w, h,
            ui ? ui->map.count : 0);
  }
  
  // Use modern renderer if available
  if (ui && ui->modern_renderer) {
    return gl_area_render_modern(ui, w, h);
  }
  
  // Fall back to legacy immediate-mode rendering
  glViewport(0, 0, w, h);
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  float aspect = (float)(h ? w / (float)h : 1.0f);
  float f = 60.0f, n = 0.1f, fa = 100.0f;
  float top = n * tanf(f * 3.14159f / 360.0f);
  float right = top * aspect;
  glFrustum(-right, right, -top, top, n, fa);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  if (g_editor_state) {
    // View matrix: inverse of camera transform -> rotate by -pitch, -yaw, then
    // translate by -position
    glRotatef(-g_editor_state->cam.pitch * 57.29578f, 1, 0, 0);
    glRotatef(-g_editor_state->cam.yaw * 57.29578f, 0, 0, 1);
    glTranslatef(-g_editor_state->cam.position.x,
                 -g_editor_state->cam.position.y,
                 -g_editor_state->cam.position.z);
  }

  // Grid + axes (toggle)
  if (ui->dbg_show_grid) {
    glDisable(GL_CULL_FACE);
    glColor3f(0.3f, 0.32f, 0.35f);
    const float gridExtent = 20.0f;
    const float gridStep = 1.0f;
    glBegin(GL_LINES);
    for (float v = -gridExtent; v <= gridExtent + 0.001f; v += gridStep) {
      glVertex3f(-gridExtent, v, 0.0f);
      glVertex3f(gridExtent, v, 0.0f);
      glVertex3f(v, -gridExtent, 0.0f);
      glVertex3f(v, gridExtent, 0.0f);
    }
    glEnd();
    if (ui->dbg_show_axes) {
      glColor3f(0.6f, 0.6f, 0.7f);
      glBegin(GL_LINES);
      glVertex3f(-gridExtent, 0.0f, 0.0f);
      glVertex3f(gridExtent, 0.0f, 0.0f); // X axis
      glVertex3f(0.0f, -gridExtent, 0.0f);
      glVertex3f(0.0f, gridExtent, 0.0f); // Y axis
      glEnd();
    }
  }

  // Unified render: compute time for animated lights and draw via shared GL
  // path
  double now = g_get_monotonic_time() / 1000000.0;
  oz_lights_set_time_seconds((float)now);
  if (ui && ui->dbg_lighting) {
    // Default low ambient when lighting is enabled
    oz_lights_set_ambient(0.15f, 0.15f, 0.18f);
  } else {
    // If lighting disabled, crank ambient to near-white
    oz_lights_set_ambient(0.7f, 0.7f, 0.75f);
  }
  oz_render_gl_draw_map_filled(&ui->map);
  // Draw objects (Zones, Pickups, PlayerStart)
  if (ui && ui->obj_count) {
    glDisable(GL_CULL_FACE);
    for (size_t i = 0; i < ui->obj_count; ++i) {
      const EditorObject *o = &ui->objects[i];
      if (o->type == OBJ_ZONE) {
        // Draw a wire circle at zone center with radius
        glColor3f(0.3f, 0.9f, 0.9f);
        glBegin(GL_LINE_LOOP);
        int seg = 48;
        float r = o->as.zone.radius;
        float cx = o->as.zone.center[0], cy = o->as.zone.center[1],
              cz = o->as.zone.center[2];
        for (int k = 0; k < seg; ++k) {
          float a = (float)k / (float)seg * 6.2831853f;
          glVertex3f(cx + cosf(a) * r, cy + sinf(a) * r, cz);
        }
        glEnd();
      } else if (o->type == OBJ_PICKUP) {
        // Small solid cube
        glColor3f(0.9f, 0.85f, 0.2f);
        float cx = o->as.pickup.position[0], cy = o->as.pickup.position[1],
              cz = o->as.pickup.position[2];
        float s = 0.1f;
        float v[8][3] = {{cx - s, cy - s, cz - s}, {cx + s, cy - s, cz - s},
                         {cx + s, cy + s, cz - s}, {cx - s, cy + s, cz - s},
                         {cx - s, cy - s, cz + s}, {cx + s, cy - s, cz + s},
                         {cx + s, cy + s, cz + s}, {cx - s, cy + s, cz + s}};
        int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                           {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; ++f) {
          for (int vi = 0; vi < 4; ++vi) {
            float *a = v[faces[f][vi]];
            glVertex3f(a[0], a[1], a[2]);
          }
        }
        glEnd();
      } else if (o->type == OBJ_PLAYER_START) {
        // Arrow glyph: base at position, pointing by yaw
        glColor3f(0.2f, 1.0f, 0.3f);
        float px = o->as.pstart.position[0], py = o->as.pstart.position[1],
              pz = o->as.pstart.position[2];
        float yaw = o->as.pstart.yaw;
        float fx = cosf(yaw), fy = sinf(yaw);
        float len = 0.6f;
        float hx = -fy * 0.2f, hy = fx * 0.2f;
        glBegin(GL_LINES);
        glVertex3f(px, py, pz);
        glVertex3f(px + fx * len, py + fy * len, pz);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex3f(px + fx * len, py + fy * len, pz);
        glVertex3f(px + fx * (len - 0.2f) + hx, py + fy * (len - 0.2f) + hy,
                   pz);
        glVertex3f(px + fx * (len - 0.2f) - hx, py + fy * (len - 0.2f) - hy,
                   pz);
        glEnd();
        // Small visibility cube icon at the base
        glColor3f(1.0f, 0.3f, 0.6f);
        float s = 0.12f;
        float v[8][3] = {{px - s, py - s, pz - s}, {px + s, py - s, pz - s},
                         {px + s, py + s, pz - s}, {px - s, py + s, pz - s},
                         {px - s, py - s, pz + s}, {px + s, py - s, pz + s},
                         {px + s, py + s, pz + s}, {px - s, py + s, pz + s}};
        int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                           {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
        glBegin(GL_QUADS);
        for (int f = 0; f < 6; ++f) {
          for (int vi = 0; vi < 4; ++vi) {
            float *a = v[faces[f][vi]];
            glVertex3f(a[0], a[1], a[2]);
          }
        }
        glEnd();
      }
    }
  }
  // Wireframe overlay (and selection highlight)
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  for (size_t i = 0; i < ui->map.count; ++i) {
    const OzBrush *br = &ui->map.brushes[i];
    if (br->type == OZ_BRUSH_BOX) {
      const OzBrushBox *b = &br->as.box;
      float cx = b->center.x, cy = b->center.y, cz = b->center.z;
      float hx = b->half.x, hy = b->half.y, hz = b->half.z;
      float v[8][3] = {
          {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
          {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
          {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
          {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
      };
      int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                         {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
      if ((int)i == ui->selected_index)
        glColor3f(1.0f, 0.2f, 0.2f);
      else
        glColor3f(0.05f, 0.05f, 0.05f);
      glBegin(GL_QUADS);
      for (int fidx = 0; fidx < 6; ++fidx) {
        for (int vi = 0; vi < 4; ++vi) {
          float *a = v[faces[fidx][vi]];
          glVertex3f(a[0], a[1], a[2]);
        }
      }
      glEnd();
    }
  }
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  // Draw CSG preview (filled, translucent)
  if (ui->has_csg_preview) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.2f, 0.8f, 0.4f, 0.35f);
    if (ui->csg_preview.type == OZ_BRUSH_BOX) {
      const OzBrushBox *b = &ui->csg_preview.as.box;
      float cx = b->center.x, cy = b->center.y, cz = b->center.z;
      float hx = b->half.x, hy = b->half.y, hz = b->half.z;
      float v[8][3] = {
          {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
          {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
          {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
          {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
      };
      int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                         {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
      glBegin(GL_QUADS);
      for (int fidx = 0; fidx < 6; ++fidx) {
        for (int vi = 0; vi < 4; ++vi) {
          float *a = v[faces[fidx][vi]];
          glVertex3f(a[0], a[1], a[2]);
        }
      }
      glEnd();
    }
    glDisable(GL_BLEND);
  }
  // Draw transform gizmo at selected brush or object center (3 axes, prominent)
  if ((ui->selected_index >= 0 && ui->selected_index < (int)ui->map.count) ||
      (ui->selected_object >= 0 &&
       (size_t)ui->selected_object < ui->obj_count)) {
    float gx, gy, gz;
    if (ui->selected_index >= 0) {
      const OzBrush *sel = &ui->map.brushes[ui->selected_index];
      OzVec3 p =
          (sel->type == OZ_BRUSH_BOX) ? sel->as.box.center : sel->as.cyl.center;
      gx = p.x;
      gy = p.y;
      gz = p.z;
    } else {
      const EditorObject *o = &ui->objects[ui->selected_object];
      if (o->type == OBJ_ZONE) {
        gx = o->as.zone.center[0];
        gy = o->as.zone.center[1];
        gz = o->as.zone.center[2];
      } else if (o->type == OBJ_PICKUP) {
        gx = o->as.pickup.position[0];
        gy = o->as.pickup.position[1];
        gz = o->as.pickup.position[2];
      } else {
        gx = o->as.pstart.position[0];
        gy = o->as.pstart.position[1];
        gz = o->as.pstart.position[2];
      }
    }
    float axisLen = 3.0f;
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glColor3f(1.0f, 0.2f, 0.2f);
    glVertex3f(gx, gy, gz);
    glVertex3f(gx + axisLen, gy, gz);
    glColor3f(0.2f, 1.0f, 0.2f);
    glVertex3f(gx, gy, gz);
    glVertex3f(gx, gy + axisLen, gz);
    glColor3f(0.3f, 0.6f, 1.0f);
    glVertex3f(gx, gy, gz);
    glVertex3f(gx, gy, gz + axisLen);
    glEnd();
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
  }
  GLenum glerr = glGetError();
  if (glerr != GL_NO_ERROR) {
    OZ_WARN("OpenGL error: 0x%X", (unsigned)glerr);
  }
  return TRUE;
}

static void project_point(float x, float y, float z, int w, int h, float *out_x,
                          float *out_y) {
  // Use live editor camera to match GL viewport movement
  float yaw = g_editor_state ? g_editor_state->cam.yaw : 0.0f;
  float pitch = g_editor_state ? g_editor_state->cam.pitch
                               : (-15.0f * (float)G_PI / 180.0f);
  float px = g_editor_state ? g_editor_state->cam.position.x : 0.0f;
  float py = g_editor_state ? g_editor_state->cam.position.y : -5.0f;
  float pz = g_editor_state ? g_editor_state->cam.position.z : -2.5f;
  // Translate by camera position (inverse)
  x -= px;
  y -= py;
  z -= pz;
  // Rotate by inverse yaw (around Z)
  float cy = cosf(-yaw), sy = sinf(-yaw);
  float x1 = cy * x + sy * y;
  float y1 = -sy * x + cy * y;
  float z1 = z;
  // Rotate by inverse pitch (around X)
  float cp = cosf(-pitch), sp = sinf(-pitch);
  float x2 = x1;
  float y2 = cp * y1 - sp * z1;
  float z2 = sp * y1 + cp * z1;
  // Perspective projection (roughly match GL path)
  float f = 1.0f / tanf(60.0f * (float)G_PI / 360.0f);
  float denom = (z2 + 5.0f);
  if (denom < 0.05f)
    denom = 0.05f; // avoid extreme blow-up behind camera
  float ndc_x = (x2 * f) / denom;
  float ndc_y = (y2 * f) / denom;
  *out_x = (float)w * 0.5f + ndc_x * (float)w * 0.5f;
  *out_y = (float)h * 0.5f - ndc_y * (float)h * 0.5f;
}

static gboolean fallback_draw(GtkWidget *widget, cairo_t *cr,
                              gpointer user_data) {
  EditorUi *ui = (EditorUi *)user_data;
  static gboolean logged_once = FALSE;
  if (!logged_once) {
    OZ_INFO("fallback_draw called");
    logged_once = TRUE;
  }
  int w = gtk_widget_get_allocated_width(widget);
  int h = gtk_widget_get_allocated_height(widget);
  // Background similar to SW demo for visual consistency
  cairo_set_source_rgb(cr, 0.1, 0.105, 0.12);
  cairo_paint(cr);
  oz_render_soft_draw_grid_axes(cr, w, h,
                                g_editor_state ? &g_editor_state->cam : NULL,
                                TRUE, ui->dbg_show_axes);
  // Render shared gizmos (tripod)
  oz_render_soft_draw_gizmos(cr, w, h,
                             g_editor_state ? &g_editor_state->cam : NULL);
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 12);
  cairo_move_to(cr, 8, 18);
  cairo_show_text(cr, "Fallback viewport (software)");

  // Draw CSG preview (semi-transparent fill)
  if (ui->has_csg_preview && ui->csg_preview.type == OZ_BRUSH_BOX) {
    const OzBrushBox *b = &ui->csg_preview.as.box;
    float cx = b->center.x, cy = b->center.y, cz = b->center.z;
    float hx = b->half.x, hy = b->half.y, hz = b->half.z;
    float v[8][3] = {
        {cx - hx, cy - hy, cz - hz}, {cx + hx, cy - hy, cz - hz},
        {cx + hx, cy + hy, cz - hz}, {cx - hx, cy + hy, cz - hz},
        {cx - hx, cy - hy, cz + hz}, {cx + hx, cy - hy, cz + hz},
        {cx + hx, cy + hy, cz + hz}, {cx - hx, cy + hy, cz + hz},
    };
    int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                       {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
    cairo_set_source_rgba(cr, 0.2, 0.8, 0.4, 0.3);
    for (int f = 0; f < 6; ++f) {
      float x0, y0;
      project_point(v[faces[f][0]][0], v[faces[f][0]][1], v[faces[f][0]][2], w,
                    h, &x0, &y0);
      cairo_move_to(cr, x0, y0);
      for (int vi = 1; vi < 4; ++vi) {
        float x, y;
        project_point(v[faces[f][vi]][0], v[faces[f][vi]][1],
                      v[faces[f][vi]][2], w, h, &x, &y);
        cairo_line_to(cr, x, y);
      }
      cairo_close_path(cr);
      cairo_fill(cr);
    }
  }

  // Draw objects with hover feedback
  if (ui->obj_count) {
    for (size_t i = 0; i < ui->obj_count; ++i) {
      const EditorObject *o = &ui->objects[i];
      float sx, sy;
      float ox = 0, oy = 0, oz = 0;
      if (o->type == OBJ_ZONE) {
        ox = o->as.zone.center[0];
        oy = o->as.zone.center[1];
        oz = o->as.zone.center[2];
      } else if (o->type == OBJ_PICKUP) {
        ox = o->as.pickup.position[0];
        oy = o->as.pickup.position[1];
        oz = o->as.pickup.position[2];
      } else {
        ox = o->as.pstart.position[0];
        oy = o->as.pstart.position[1];
        oz = o->as.pstart.position[2];
      }
      oz_render_soft_project_point(g_editor_state ? &g_editor_state->cam : NULL,
                                   ox, oy, oz, w, h, &sx, &sy);
      if ((int)i == ui->hover_object)
        cairo_set_source_rgb(cr, 1.0, 0.9, 0.3);
      else
        cairo_set_source_rgb(cr, 0.9, 0.85, 0.2);
      cairo_arc(cr, sx, sy, 6.0, 0, 6.28318);
      cairo_fill(cr);
    }
  }
  // Draw map (selection highlight handled inside)
  oz_render_soft_draw_map(cr, w, h, &ui->map,
                          g_editor_state ? &g_editor_state->cam : NULL,
                          ui->selected_index);
  // Draw transform gizmo hover axis highlight (screen-space lines)
  if (ui->gizmo_axis >= 0) {
    float ax0, ay0, ax1, ay1;
    oz_render_soft_project_point(g_editor_state ? &g_editor_state->cam : NULL,
                                 0, 0, 0, w, h, &ax0, &ay0);
    if (ui->gizmo_axis == 0) {
      oz_render_soft_project_point(g_editor_state ? &g_editor_state->cam : NULL,
                                   3, 0, 0, w, h, &ax1, &ay1);
      cairo_set_source_rgb(cr, 1, 0.3, 0.3);
    } else if (ui->gizmo_axis == 1) {
      oz_render_soft_project_point(g_editor_state ? &g_editor_state->cam : NULL,
                                   0, 3, 0, w, h, &ax1, &ay1);
      cairo_set_source_rgb(cr, 0.3, 1, 0.3);
    } else {
      oz_render_soft_project_point(g_editor_state ? &g_editor_state->cam : NULL,
                                   0, 0, 3, w, h, &ax1, &ay1);
      cairo_set_source_rgb(cr, 0.3, 0.6, 1);
    }
    cairo_set_line_width(cr, 4.0);
    cairo_move_to(cr, ax0, ay0);
    cairo_line_to(cr, ax1, ay1);
    cairo_stroke(cr);
  }
  // We handled drawing fully; stop further processing to avoid overdraw
  return TRUE;
}

// Periodic redraw helper for software DrawingArea
static gboolean queue_draw_cb(gpointer data) {
  EditorUi *ui = (EditorUi *)data;
  if (ui && ui->shutting_down)
    return FALSE;
  if (ui && ui->viewport && GTK_IS_WIDGET(ui->viewport)) {
    gtk_widget_queue_draw(ui->viewport);
  }
  return TRUE;
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event,
                                 gpointer user_data) {
  (void)widget;
  (void)event;
  action_quit(NULL, NULL, user_data);
  return TRUE; // we destroy explicitly in action_quit
}

static void switch_to_software_viewport(GtkWidget *area_widget, EditorUi *ui) {
  // Replace GLArea with software GtkDrawingArea and wire up redraws
  GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(area_widget));
  GtkWidget *da = gtk_drawing_area_new();
  gtk_widget_set_hexpand(da, TRUE);
  gtk_widget_set_vexpand(da, TRUE);
  gtk_widget_set_app_paintable(da, TRUE);
  gtk_widget_set_size_request(da, 320, 200);
  g_signal_connect(da, "draw", G_CALLBACK(fallback_draw), ui);
  if (parent)
    gtk_container_remove(GTK_CONTAINER(parent), GTK_WIDGET(area_widget));
  ui->gl_area = NULL;
  ui->viewport = da;
  // Rewire input on the new viewport
  gtk_widget_add_events(ui->viewport,
                        GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                            GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                            GDK_POINTER_MOTION_MASK);
  gtk_widget_set_can_focus(ui->viewport, TRUE);
  gtk_widget_grab_focus(ui->viewport);
  g_object_set_data(G_OBJECT(ui->viewport), "oz_editor_ui", ui);
  if (g_editor_state) {
    g_signal_connect(ui->viewport, "key-press-event", G_CALLBACK(on_key_press),
                     g_editor_state);
    g_signal_connect(ui->viewport, "key-release-event",
                     G_CALLBACK(on_key_release), g_editor_state);
    g_signal_connect(ui->viewport, "button-press-event",
                     G_CALLBACK(on_button_press), g_editor_state);
    g_signal_connect(ui->viewport, "button-release-event",
                     G_CALLBACK(on_button_release), g_editor_state);
    g_signal_connect(ui->viewport, "motion-notify-event", G_CALLBACK(on_motion),
                     g_editor_state);
  }
  if (!ui->redraw_id)
    ui->redraw_id = g_timeout_add(16, queue_draw_cb, ui);
  if (parent)
    gtk_box_pack_start(GTK_BOX(parent), da, TRUE, TRUE, 0);
  if (ui->window)
    gtk_widget_show_all(ui->window);
}

typedef struct SwapCtx {
  GtkWidget *area;
  EditorUi *ui;
} SwapCtx;
static gboolean do_swap_to_software(gpointer data) {
  SwapCtx *ctx = (SwapCtx *)data;
  if (ctx && ctx->area && ctx->ui) {
    switch_to_software_viewport(ctx->area, ctx->ui);
  }
  g_free(ctx);
  return FALSE; // one-shot
}

static void gl_area_realize(GtkGLArea *area, gpointer user_data) {
  EditorUi *ui = (EditorUi *)user_data;
  // Trap potential X/GLX errors and fallback gracefully
  gdk_error_trap_push();
  gtk_gl_area_make_current(area);
  int xerr = gdk_error_trap_pop();
  if (xerr != 0 || gtk_gl_area_get_error(area)) {
    OZ_WARN("GLX/GDK error during GtkGLArea realize (code=%d). Falling back to "
            "software viewport.",
            xerr);
    // Fallback: replace GLArea within its current parent container
    switch_to_software_viewport(GTK_WIDGET(area), ui);
    return;
  }
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  const GLubyte *vendor = glGetString(GL_VENDOR);
  const GLubyte *renderer = glGetString(GL_RENDERER);
  const GLubyte *version = glGetString(GL_VERSION);
  OZ_INFO("GL realized: vendor=%s renderer=%s version=%s",
          vendor ? (const char *)vendor : "?",
          renderer ? (const char *)renderer : "?",
          version ? (const char *)version : "?");

  // If this is an OpenGL ES context, our immediate-mode path will not work.
  // Fallback to the software viewport to avoid a blank/grey GLArea.
  if (version && (g_str_has_prefix((const char *)version, "OpenGL ES") ||
                  g_strrstr((const char *)version, "GLES") != NULL)) {
    OZ_WARN("OpenGL ES context detected (%s). Falling back to software viewport.",
            (const char *)version);
    SwapCtx *ctx = g_new0(SwapCtx, 1);
    ctx->area = GTK_WIDGET(area);
    ctx->ui = ui;
    g_idle_add(do_swap_to_software, ctx);
    return;
  }

  // Try to initialize modern GL renderer (VBO/VAO + shaders).
  // If it succeeds, we can handle both core and compatibility profiles.
  // If it fails, fall back to immediate mode (compatibility only) or software.
  ui->modern_renderer = oz_gl_modern_init();
  if (ui->modern_renderer) {
    OZ_INFO("Using modern OpenGL renderer (VBO/VAO + shaders)");
    return;
  }
  
  // Modern renderer failed, check if we can use immediate mode
#ifdef GL_CONTEXT_PROFILE_MASK
  GLint profileMask = 0;
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profileMask);
#ifndef GL_CONTEXT_CORE_PROFILE_BIT
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif
  if (profileMask & GL_CONTEXT_CORE_PROFILE_BIT) {
    OZ_WARN("Core profile context detected, modern renderer failed. Falling back to software viewport.");
    SwapCtx *ctx = g_new0(SwapCtx, 1);
    ctx->area = GTK_WIDGET(area);
    ctx->ui = ui;
    g_idle_add(do_swap_to_software, ctx);
    return;
  }
#endif
  
  // Compatibility profile - use legacy immediate mode renderer
  OZ_INFO("Using legacy immediate-mode OpenGL renderer");

  // Prefer a compatibility profile that supports fixed-function/immediate mode.
  // GtkGLArea tends to create a Core profile when requesting >= 3.2, which
  // removes glMatrixMode/glBegin and friends used by the editor. Accept 2.1+
  // here to keep a compatibility context and avoid falling back to software.
  int vmaj = 0, vmin = 0;
  if (version) {
    const char *vs = (const char *)version;
    // Skip to first digit
    while (*vs && !g_ascii_isdigit((gchar)*vs))
      ++vs;
    if (sscanf(vs, "%d.%d", &vmaj, &vmin) != 2) {
      vmaj = 0;
      vmin = 0;
    }
  }
  if (vmaj < 2 || (vmaj == 2 && vmin < 1)) {
    OZ_WARN("OpenGL version %d.%d detected (< 2.1). Falling back to software "
            "viewport.",
            vmaj, vmin);
    // Defer widget replacement to idle to avoid re-entrancy during realize
    SwapCtx *ctx = g_new0(SwapCtx, 1);
    ctx->area = GTK_WIDGET(area);
    ctx->ui = ui;
    g_idle_add(do_swap_to_software, ctx);
    return;
  }

  // Create a simple checker texture for brush visibility
  if (ui) {
    ui->gl_tex_w = 64;
    ui->gl_tex_h = 64;
    ui->gl_tex_brush = 0;
    unsigned char *pixels = (unsigned char *)malloc((size_t)ui->gl_tex_w *
                                                    (size_t)ui->gl_tex_h * 3);
    if (pixels) {
      for (int y = 0; y < ui->gl_tex_h; ++y) {
        for (int x = 0; x < ui->gl_tex_w; ++x) {
          int idx = (y * ui->gl_tex_w + x) * 3;
          int c = (((x >> 3) ^ (y >> 3)) & 1) ? 200 : 120;
          pixels[idx + 0] = (unsigned char)c;
          pixels[idx + 1] = (unsigned char)c;
          pixels[idx + 2] = (unsigned char)c;
        }
      }
      GLuint tex = 0;
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ui->gl_tex_w, ui->gl_tex_h, 0,
                   GL_RGB, GL_UNSIGNED_BYTE, pixels);
      ui->gl_tex_brush = tex;
      free(pixels);
    }
  }
}

// --- Menu wiring (split by domain) ---

// removed: moved to src/editor/ui/ui.c

// --- Object system helpers ---
static void objects_reserve(EditorUi *ui, size_t cap) {
  if (!ui)
    return;
  if (cap <= ui->obj_capacity)
    return;
  size_t nc = ui->obj_capacity ? ui->obj_capacity : 8;
  while (nc < cap)
    nc *= 2;
  EditorObject *mem =
      (EditorObject *)realloc(ui->objects, nc * sizeof(EditorObject));
  if (!mem)
    return;
  ui->objects = mem;
  ui->obj_capacity = nc;
}

static int objects_add(EditorUi *ui, const EditorObject *obj) {
  if (!ui || !obj)
    return -1;
  objects_reserve(ui, ui->obj_count + 1);
  ui->objects[ui->obj_count] = *obj;
  return (int)ui->obj_count++;
}

static void open_object_properties(EditorUi *ui, int index) {
  if (!ui || index < 0 || (size_t)index >= ui->obj_count)
    return;
  EditorObject *o = &ui->objects[index];
  GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(win), "Object Properties");
  gtk_window_set_default_size(GTK_WINDOW(win), 360, 260);
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add(GTK_CONTAINER(win), vbox);
  char title[128];
  switch (o->type) {
  case OBJ_ZONE:
    snprintf(title, sizeof(title), "Zone: %s",
             o->as.zone.name ? o->as.zone.name : "(unnamed)");
    gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(title), FALSE, FALSE, 0);
    // Radius editor (persist to object)
    {
      GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hb), gtk_label_new("Radius"), FALSE, FALSE, 0);
      GtkWidget *entry = gtk_entry_new();
      char buf[32];
      g_snprintf(buf, sizeof(buf), "%.2f", o->as.zone.radius);
      gtk_entry_set_text(GTK_ENTRY(entry), buf);
      PropCtx *pc = g_new0(PropCtx, 1);
      pc->ui = ui;
      pc->index = index;
      g_signal_connect(entry, "activate", G_CALLBACK(on_zone_radius_changed),
                       pc);
      g_signal_connect(entry, "changed", G_CALLBACK(on_zone_radius_changed),
                       pc);
      gtk_box_pack_start(GTK_BOX(hb), entry, TRUE, TRUE, 0);
    }
    break;
  case OBJ_PICKUP:
    snprintf(title, sizeof(title), "Pickup: %s",
             o->as.pickup.name ? o->as.pickup.name : "(item)");
    gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(title), FALSE, FALSE, 0);
    break;
  case OBJ_PLAYER_START:
    gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("PlayerStart"), FALSE,
                       FALSE, 0);
    // Camera mode chooser
    gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Camera Mode"), FALSE,
                       FALSE, 0);
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FreeMove");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "FPS");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Cinematic");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo),
                             (int)o->as.pstart.camera_mode);
    PropCtx *pc_cam = g_new0(PropCtx, 1);
    pc_cam->ui = ui;
    pc_cam->index = index;
    g_signal_connect(combo, "changed", G_CALLBACK(on_pstart_cam_changed),
                     pc_cam);
    gtk_box_pack_start(GTK_BOX(vbox), combo, FALSE, FALSE, 0);
    // Yaw editor (persist to object)
    {
      GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_box_pack_start(GTK_BOX(vbox), hb, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hb), gtk_label_new("Yaw (deg)"), FALSE, FALSE,
                         0);
      GtkWidget *entry = gtk_entry_new();
      char buf[32];
      g_snprintf(buf, sizeof(buf), "%.1f", o->as.pstart.yaw * 57.29578f);
      gtk_entry_set_text(GTK_ENTRY(entry), buf);
      PropCtx *pc_yaw = g_new0(PropCtx, 1);
      pc_yaw->ui = ui;
      pc_yaw->index = index;
      g_signal_connect(entry, "activate", G_CALLBACK(on_pstart_yaw_changed),
                       pc_yaw);
      g_signal_connect(entry, "changed", G_CALLBACK(on_pstart_yaw_changed),
                       pc_yaw);
      gtk_box_pack_start(GTK_BOX(hb), entry, TRUE, TRUE, 0);
    }
    break;
  default:
    break;
  }
  gtk_widget_show_all(win);
}

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// Placement helpers: drop near camera
// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// Shim to open object browser from a GtkButton clicked
// moved to src/editor/ui/ui.c

static void build_menu_bsp(GtkApplication *app, EditorUi *ui, GMenu *menubar) {
  (void)app;
  (void)ui;
  GMenu *bsp_menu = g_menu_new();
  GMenu *bsp_section_brushes = g_menu_new();
  g_menu_append(bsp_section_brushes, "Box...", "app.bsp_box_dialog");
  g_menu_append(bsp_section_brushes, "Cylinder...", "app.bsp_cyl_dialog");
  g_menu_append_section(bsp_menu, "Brushes", G_MENU_MODEL(bsp_section_brushes));

  GMenu *bsp_section_csg = g_menu_new();
  g_menu_append(bsp_section_csg, "CSG Add", "app.csg_add");
  g_menu_append(bsp_section_csg, "CSG Subtract", "app.csg_sub");
  g_menu_append(bsp_section_csg, "CSG Intersect", "app.csg_isect");
  g_menu_append_section(bsp_menu, "CSG", G_MENU_MODEL(bsp_section_csg));

  GMenu *bsp_section_tools = g_menu_new();
  g_menu_append(bsp_section_tools, "Carve", "app.tool_carve");
  g_menu_append(bsp_section_tools, "Slope", "app.tool_slope");
  g_menu_append_section(bsp_menu, "Tools", G_MENU_MODEL(bsp_section_tools));

  g_menu_append_submenu(menubar, "BSP", G_MENU_MODEL(bsp_menu));
  g_object_unref(bsp_menu);
  g_object_unref(bsp_section_brushes);
  g_object_unref(bsp_section_csg);
  g_object_unref(bsp_section_tools);
}

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c

// moved to src/editor/ui/ui.c: populate menus and actions

// --- Left-side BSP creation buttons ---
static void select_last_and_invalidate(EditorUi *ui) {
  if (!ui)
    return;
  ui->selected_index = (int)(ui->map.count ? ui->map.count - 1 : -1);
  ui->has_csg_preview = FALSE; // any edit cancels preview
  if (ui->viewport)
    gtk_widget_queue_draw(ui->viewport);
}
static void btn_add_cube(GtkButton *b, gpointer user_data) {
  (void)b;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui)
    return;
  oz_map_add_box(&ui->map, (OzVec3){0, 0, 0}, (OzVec3){1, 1, 1});
  select_last_and_invalidate(ui);
}
static void btn_add_cylinder(GtkButton *b, gpointer user_data) {
  (void)b;
  action_bsp_cyl_dialog(NULL, NULL, user_data);
}
static void btn_add_sphere(GtkButton *b, gpointer user_data) {
  (void)b;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui)
    return;
  oz_map_add_sphere(&ui->map, (OzVec3){0, 0, 0}, 0.5f, 16);
  select_last_and_invalidate(ui);
}
static void btn_add_pyramid(GtkButton *b, gpointer user_data) {
  (void)b;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui)
    return;
  oz_map_add_pyramid(&ui->map, (OzVec3){0, 0, 0}, 0.5f, 0.5f, 1.0f);
  select_last_and_invalidate(ui);
}
static void btn_add_plane(GtkButton *b, gpointer user_data) {
  (void)b;
  EditorUi *ui = (EditorUi *)user_data;
  if (!ui)
    return;
  oz_map_add_plane(&ui->map, (OzVec3){0, 0, 0}, (OzVec3){0, 0, 1}, 1.0f);
  select_last_and_invalidate(ui);
}

// Simple splash window with animated GIF icon
static gboolean destroy_widget_cb(gpointer w) {
  if (w && GTK_IS_WIDGET(w))
    gtk_widget_destroy(GTK_WIDGET(w));
  return FALSE;
}

static void maybe_show_splash(void) {
  const char *candidates[] = {"gfx/ozworld_icon.gif", "../gfx/ozworld_icon.gif",
                              NULL};
  const char *path = NULL;
  for (int i = 0; candidates[i]; ++i) {
    if (g_file_test(candidates[i], G_FILE_TEST_IS_REGULAR)) {
      path = candidates[i];
      break;
    }
  }
  if (!path)
    return;
  GError *err = NULL;
  GdkPixbufAnimation *anim = gdk_pixbuf_animation_new_from_file(path, &err);
  if (!anim) {
    if (err) {
      g_error_free(err);
    }
    return;
  }
  GtkWidget *splash = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated(GTK_WINDOW(splash), FALSE);
  gtk_window_set_keep_above(GTK_WINDOW(splash), TRUE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(splash), TRUE);
  gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER);
  GtkWidget *img = gtk_image_new_from_animation(anim);
  gtk_container_add(GTK_CONTAINER(splash), img);
  gtk_widget_show_all(splash);
  // Auto-destroy after 1.2s
  g_timeout_add(1200, destroy_widget_cb, splash);
}

void on_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;
  EditorUi *ui = g_new0(EditorUi, 1);

  editor_ui_populate_menus(app, ui);

  // Show splash while we build the main window
  maybe_show_splash();

  ui->window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(ui->window), "OzWorld Editor");
  gtk_window_set_default_size(GTK_WINDOW(ui->window), 640, 480);

  ui->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(ui->window), ui->vbox);

  // Add toolbar from UI module
  ui->toolbar = editor_ui_build_toolbar(ui);
  gtk_box_pack_start(GTK_BOX(ui->vbox), ui->toolbar, FALSE, FALSE, 0);

  // Initialize input state before wiring events and rendering
  static EditorInputState state;
  memset(&state, 0, sizeof(state));
  oz_camera_init(&state.cam, OZ_CAMERA_FREEMOVE);
  state.last_tick = g_get_monotonic_time() / 1000000.0;
  g_editor_state = &state;
  state.mouse_look_active = FALSE;
  // Editing state defaults
  ui->selected_index = -1;
  ui->dragging = FALSE;
  ui->drag_last_x = ui->drag_last_y = 0.0;
  state.last_mouse_x = 0.0;
  state.last_mouse_y = 0.0;
  state.mouse_sensitivity = 0.0035f;

  // Prepare left-side BSP buttons + viewport content area early
  GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_size_request(left, 96, -1);
  GtkWidget *btn_cube = gtk_button_new_with_label("Cube");
  GtkWidget *btn_cyl = gtk_button_new_with_label("Cyl");
  GtkWidget *btn_sph = gtk_button_new_with_label("Sphere");
  GtkWidget *btn_pyr = gtk_button_new_with_label("Pyramid");
  GtkWidget *btn_pln = gtk_button_new_with_label("Plane");
  GtkWidget *btn_obj = gtk_button_new_with_label("Objects");
  g_signal_connect(btn_cube, "clicked", G_CALLBACK(btn_add_cube), ui);
  g_signal_connect(btn_cyl, "clicked", G_CALLBACK(btn_add_cylinder), ui);
  g_signal_connect(btn_sph, "clicked", G_CALLBACK(btn_add_sphere), ui);
  g_signal_connect(btn_pyr, "clicked", G_CALLBACK(btn_add_pyramid), ui);
  g_signal_connect(btn_pln, "clicked", G_CALLBACK(btn_add_plane), ui);
  extern void action_open_object_browser(GSimpleAction *, GVariant *, gpointer);
  g_signal_connect(btn_obj, "clicked", G_CALLBACK(action_open_object_browser),
                   ui);
  gtk_box_pack_start(GTK_BOX(left), btn_cube, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(left), btn_cyl, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(left), btn_sph, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(left), btn_pyr, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(left), btn_pln, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(left), btn_obj, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), left, FALSE, FALSE, 0);

  // Viewport: prefer GL when available and enabled; otherwise software
  // DrawingArea
  const char *gdkgl_env = g_getenv("GDK_GL");
  const gboolean gl_disabled =
      (gdkgl_env && g_strcmp0(gdkgl_env, "disable") == 0);
  const char *disp_env = g_getenv("DISPLAY");
  const gboolean is_remote = (disp_env && disp_env[0] != ':');
  const gboolean allow_gl_remote = env_flag_is_true("OZ_ALLOW_GL_REMOTE");
  const gboolean libgl_indirect = (g_getenv("LIBGL_ALWAYS_INDIRECT") != NULL);
  const gboolean force_software =
      gl_disabled || env_flag_is_true("OZ_FORCE_SOFTWARE") || libgl_indirect ||
      (is_remote && !allow_gl_remote);

  if (!force_software) {
    GtkWidget *gl = gtk_gl_area_new();
    // Request an OpenGL 2.1 context so GTK provides a compatibility profile
    // (immediate-mode APIs used by the editor).
    gtk_gl_area_set_required_version(GTK_GL_AREA(gl), 2, 1);
    gtk_gl_area_set_use_es(GTK_GL_AREA(gl), FALSE);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(gl), TRUE);
    gtk_gl_area_set_has_alpha(GTK_GL_AREA(gl), FALSE);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(gl), FALSE);
    g_signal_connect(gl, "render", G_CALLBACK(gl_area_render), ui);
    g_signal_connect(gl, "realize", G_CALLBACK(gl_area_realize), ui);
    gtk_widget_set_hexpand(gl, TRUE);
    gtk_widget_set_vexpand(gl, TRUE);
    // Attach GLArea to layout; realization will happen on show and may fallback
    // if needed
    ui->gl_area = gl;
    ui->viewport = gl;
    gtk_box_pack_start(GTK_BOX(content), gl, TRUE, TRUE, 0);
    OZ_INFO("Using GtkGLArea viewport");
  }
  if (!ui->viewport) {
    GtkWidget *da = gtk_drawing_area_new();
    if (libgl_indirect || (is_remote && !allow_gl_remote)) {
      OZ_INFO(
          "Using software viewport (GtkDrawingArea) due to indirect/remote GL");
    } else if (gl_disabled || env_flag_is_true("OZ_FORCE_SOFTWARE")) {
      OZ_INFO("Using software viewport (GtkDrawingArea) due to config");
    } else {
      OZ_INFO("Using software viewport (GtkDrawingArea)");
    }
    gtk_widget_set_hexpand(da, TRUE);
    gtk_widget_set_vexpand(da, TRUE);
    gtk_widget_set_app_paintable(da, TRUE);
    gtk_widget_set_size_request(da, 320, 200);
    g_signal_connect(da, "draw", G_CALLBACK(fallback_draw), ui);
    if (!ui->redraw_id)
      ui->redraw_id = g_timeout_add(16, queue_draw_cb, ui);
    ui->gl_area = NULL;
    ui->viewport = da;
    gtk_box_pack_start(GTK_BOX(content), da, TRUE, TRUE, 0);
  }

  oz_map_init(&ui->map);
  // Defaults for gizmos and debug
  ui->gizmo_translate = TRUE;
  ui->gizmo_rotate = FALSE;
  ui->gizmo_scale = FALSE;
  ui->gizmo_axis = -1;
  ui->dbg_show_grid = TRUE;
  ui->dbg_show_axes = TRUE;
  ui->dbg_show_fps = TRUE;
  // Add content to window layout
  gtk_box_pack_start(GTK_BOX(ui->vbox), content, TRUE, TRUE, 0);

  // Simple HUD label below content
  ui->overlay = NULL;
  ui->info_label = gtk_label_new("");
  gtk_widget_set_halign(ui->info_label, GTK_ALIGN_END);
  gtk_widget_set_valign(ui->info_label, GTK_ALIGN_START);
  gtk_box_pack_start(GTK_BOX(ui->vbox), ui->info_label, FALSE, FALSE, 0);
  // Input + timer
  gtk_widget_add_events(ui->window, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                                        GDK_FOCUS_CHANGE_MASK);
  gtk_widget_add_events(ui->viewport,
                        GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |
                            GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                            GDK_POINTER_MOTION_MASK | GDK_FOCUS_CHANGE_MASK);
  // Route key/mouse to viewport; also connect at window level as a fallback
  GtkWidget *key_target = ui->viewport ? ui->viewport : ui->window;
  gtk_widget_set_can_focus(key_target, TRUE);
  gtk_widget_grab_focus(key_target);
  // Allow handlers to fetch EditorUi via widget data
  g_object_set_data(G_OBJECT(key_target), "oz_editor_ui", ui);
  g_signal_connect(key_target, "key-press-event", G_CALLBACK(on_key_press),
                   &state);
  g_signal_connect(key_target, "key-release-event", G_CALLBACK(on_key_release),
                   &state);
  g_signal_connect(key_target, "button-press-event",
                   G_CALLBACK(on_button_press), &state);
  g_signal_connect(key_target, "button-release-event",
                   G_CALLBACK(on_button_release), &state);
  g_signal_connect(key_target, "motion-notify-event", G_CALLBACK(on_motion),
                   &state);
  g_signal_connect(key_target, "focus-out-event", G_CALLBACK(on_focus_out),
                   &state);
  // Redundant connections on window to ensure focus loss doesn't break movement
  g_object_set_data(G_OBJECT(ui->window), "oz_editor_ui", ui);
  g_signal_connect(ui->window, "key-press-event", G_CALLBACK(on_key_press),
                   &state);
  g_signal_connect(ui->window, "key-release-event", G_CALLBACK(on_key_release),
                   &state);
  g_signal_connect(ui->window, "focus-out-event", G_CALLBACK(on_focus_out),
                   &state);
  ui->tick_id = g_timeout_add(16, tick_update, ui);
  g_signal_connect(ui->window, "delete-event", G_CALLBACK(on_window_delete),
                   ui);
  gtk_widget_show_all(ui->window);
}

// main is now in src/editor/app.c
