#include "oz/oz_debug.h"
#include <string.h>
#include <stdlib.h>

#ifndef OZ_MAX_WATCHES
#define OZ_MAX_WATCHES 256
#endif

typedef struct WatchEntry {
    const char* name;
    const void* address;
    OzDebugType type;
    size_t count;
} WatchEntry;

static WatchEntry g_watches[OZ_MAX_WATCHES];
static size_t g_watch_count = 0;

OzWatchHandle oz_debug_watch_register(const char* name, const void* address, OzDebugType type, size_t count) {
    if (!name || !address) return -1;
    if (g_watch_count >= OZ_MAX_WATCHES) return -1;
    g_watches[g_watch_count].name = strdup(name);
    g_watches[g_watch_count].address = address;
    g_watches[g_watch_count].type = type;
    g_watches[g_watch_count].count = count;
    return (OzWatchHandle)g_watch_count++;
}

void oz_debug_watch_unregister(OzWatchHandle handle) {
    if (handle < 0 || (size_t)handle >= g_watch_count) return;
    free((void*)g_watches[handle].name);
    // compact remove
    for (size_t i = (size_t)handle + 1; i < g_watch_count; ++i) g_watches[i-1] = g_watches[i];
    --g_watch_count;
}

size_t oz_debug_watch_count(void) { return g_watch_count; }
const char* oz_debug_watch_name(size_t index) { return index < g_watch_count ? g_watches[index].name : NULL; }
OzDebugType oz_debug_watch_type(size_t index) { return index < g_watch_count ? g_watches[index].type : OZ_DEBUG_T_RAW; }
const void* oz_debug_watch_address(size_t index) { return index < g_watch_count ? g_watches[index].address : NULL; }
size_t oz_debug_watch_count_elems(size_t index) { return index < g_watch_count ? g_watches[index].count : 0; }
