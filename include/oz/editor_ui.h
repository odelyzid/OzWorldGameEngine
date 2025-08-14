#ifndef OZ_EDITOR_UI_H
#define OZ_EDITOR_UI_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

struct EditorUi; // fwd

// Build top-level menus and wire actions into the application
void editor_ui_populate_menus(GtkApplication* app, struct EditorUi* ui);

// Toolbar helpers (optional)
GtkWidget* editor_ui_build_toolbar(struct EditorUi* ui);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_UI_H
