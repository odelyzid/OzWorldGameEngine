#include "oz/editor_app.h"
#include "oz/oz_core.h"
#include "oz/oz_log.h"
#include "oz/oz_render.h"
#include <gtk/gtk.h>

int main(int argc, char **argv) {
  OZ_INFO("OzWorld Editor starting (v%s)", oz_core_version());
  // Prefer desktop OpenGL for GtkGLArea unless the user overrides it
  if (!g_getenv("GDK_GL")) g_setenv("GDK_GL", "gl", FALSE);
  const char *disp = g_getenv("DISPLAY");
  const char *libgl = g_getenv("LIBGL_ALWAYS_INDIRECT");
  const char *gdkgl = g_getenv("GDK_GL");
  OZ_INFO("DISPLAY=%s", disp ? disp : "(null)");
  OZ_INFO("LIBGL_ALWAYS_INDIRECT=%s", libgl ? libgl : "(null)");
  OZ_INFO("GDK_GL=%s", gdkgl ? gdkgl : "(null)");

  GtkApplication *app =
      gtk_application_new("com.ozworld.editor", G_APPLICATION_FLAGS_NONE);
  // Default lighting for the editor preview scene
  oz_lights_clear();
  oz_lights_set_ambient(0.15f, 0.15f, 0.18f);
  (void)oz_lights_add_point(2.0f, 2.0f, 3.0f, 1.0f, 0.95f, 0.9f, 2.0f, 8.0f);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
