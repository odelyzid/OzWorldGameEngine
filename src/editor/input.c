#include "oz/editor_input.h"
#include <string.h>

#define MAX_KEYS 1024
static gboolean s_keys[MAX_KEYS];
static double s_last_change[MAX_KEYS];

void editor_input_init(void) {
    memset(s_keys, 0, sizeof(s_keys));
    memset(s_last_change, 0, sizeof(s_last_change));
}

static inline guint clamp_key(guint k){ return (k < MAX_KEYS) ? k : (MAX_KEYS-1); }

void editor_input_handle_key(guint keyval, bool pressed, guint state) {
    (void)state;
    guint k = clamp_key(keyval & 0x3FFu);
    s_keys[k] = pressed ? TRUE : FALSE;
    s_last_change[k] = g_get_monotonic_time() / 1000000.0;
}

void editor_input_focus_clear_all(void) {
    for (guint i=0;i<MAX_KEYS;++i){ s_keys[i] = FALSE; }
}

void editor_input_tick(double now_seconds) {
    // Sanity: if no change for a long time on modifiers, clear them
    (void)now_seconds;
    const guint mods[] = { GDK_KEY_Shift_L, GDK_KEY_Shift_R, GDK_KEY_Control_L, GDK_KEY_Control_R, GDK_KEY_Alt_L, GDK_KEY_Alt_R };
    for (guint i=0;i<sizeof(mods)/sizeof(mods[0]);++i){ guint k = clamp_key(mods[i] & 0x3FFu); s_keys[k] = FALSE; }
}

bool editor_input_is_down_gdk(guint keyval) {
    guint k = clamp_key(keyval & 0x3FFu);
    return s_keys[k] ? true : false;
}

bool editor_input_is_down_oz(enum OzKey key) {
    switch (key) {
        case OZ_KEY_W: return editor_input_is_down_gdk('W') || editor_input_is_down_gdk('w');
        case OZ_KEY_A: return editor_input_is_down_gdk('A') || editor_input_is_down_gdk('a');
        case OZ_KEY_S: return editor_input_is_down_gdk('S') || editor_input_is_down_gdk('s');
        case OZ_KEY_D: return editor_input_is_down_gdk('D') || editor_input_is_down_gdk('d');
        case OZ_KEY_Q: return editor_input_is_down_gdk('Q') || editor_input_is_down_gdk('q') || editor_input_is_down_gdk('C') || editor_input_is_down_gdk('c');
        case OZ_KEY_E: return editor_input_is_down_gdk('E') || editor_input_is_down_gdk('e') || editor_input_is_down_gdk(' ');
        case OZ_KEY_LEFT: return editor_input_is_down_gdk(GDK_KEY_Left & 0xFF);
        case OZ_KEY_RIGHT: return editor_input_is_down_gdk(GDK_KEY_Right & 0xFF);
        case OZ_KEY_UP: return editor_input_is_down_gdk(GDK_KEY_Up & 0xFF);
        case OZ_KEY_DOWN: return editor_input_is_down_gdk(GDK_KEY_Down & 0xFF);
        default: return false;
    }
}
