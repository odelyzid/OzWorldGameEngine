#ifndef OZ_EDITOR_INPUT_H
#define OZ_EDITOR_INPUT_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include "oz/oz_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

void editor_input_init(void);
void editor_input_handle_key(guint keyval, bool pressed, guint state);
void editor_input_focus_clear_all(void);
void editor_input_tick(double now_seconds);
bool editor_input_is_down_gdk(guint keyval);
bool editor_input_is_down_oz(enum OzKey key);

#ifdef __cplusplus
}
#endif

#endif // OZ_EDITOR_INPUT_H
