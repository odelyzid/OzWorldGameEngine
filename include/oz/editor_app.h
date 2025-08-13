#ifndef OZ_EDITOR_APP_H
#define OZ_EDITOR_APP_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

// Activation callback implemented in the editor UI module
void on_activate(GtkApplication* app, gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_APP_H
