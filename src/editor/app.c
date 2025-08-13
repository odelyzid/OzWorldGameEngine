#include <gtk/gtk.h>
#include "oz/oz_core.h"
#include "oz/oz_log.h"
#include "oz/editor_app.h"

int main(int argc, char** argv) {
    OZ_INFO("OzWorld Editor starting (v%s)", oz_core_version());
    const char* disp = g_getenv("DISPLAY");
    const char* libgl = g_getenv("LIBGL_ALWAYS_INDIRECT");
    const char* gdkgl = g_getenv("GDK_GL");
    OZ_INFO("DISPLAY=%s", disp ? disp : "(null)");
    OZ_INFO("LIBGL_ALWAYS_INDIRECT=%s", libgl ? libgl : "(null)");
    OZ_INFO("GDK_GL=%s", gdkgl ? gdkgl : "(null)");

    GtkApplication* app = gtk_application_new("com.ozworld.editor", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
