#ifndef OZ_DEBUG_H
#define OZ_DEBUG_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OzDebugType {
    OZ_DEBUG_T_U8,
    OZ_DEBUG_T_I32,
    OZ_DEBUG_T_U32,
    OZ_DEBUG_T_I64,
    OZ_DEBUG_T_U64,
    OZ_DEBUG_T_F32,
    OZ_DEBUG_T_F64,
    OZ_DEBUG_T_VEC3_F32,
    OZ_DEBUG_T_RAW
} OzDebugType;

typedef int OzWatchHandle;

OzWatchHandle oz_debug_watch_register(const char* name, const void* address, OzDebugType type, size_t count);
void oz_debug_watch_unregister(OzWatchHandle handle);

size_t oz_debug_watch_count(void);
const char* oz_debug_watch_name(size_t index);
OzDebugType oz_debug_watch_type(size_t index);
const void* oz_debug_watch_address(size_t index);
size_t oz_debug_watch_count_elems(size_t index);
#include "oz/oz_log.h"
static inline void oz_debug_hexdump(const char* label, const void* data, size_t len) {
    if (!data || len == 0) { OZ_DEBUG("%s: (null)", label ? label : "hexdump"); return; }
    const unsigned char* p = (const unsigned char*)data;
    OZ_DEBUG("%s: %zu bytes", label ? label : "hexdump", len);
    char line[128];
    for (size_t i = 0; i < len; i += 16) {
        size_t n = (len - i) < 16 ? (len - i) : 16;
        int o = snprintf(line, sizeof(line), "%08zx  ", i);
        for (size_t j = 0; j < 16; ++j) {
            if (j < n) o += snprintf(line+o, sizeof(line)-o, "%02x ", p[i+j]);
            else o += snprintf(line+o, sizeof(line)-o, "   ");
            if (j == 7) o += snprintf(line+o, sizeof(line)-o, " ");
        }
        o += snprintf(line+o, sizeof(line)-o, " |");
        for (size_t j = 0; j < n; ++j) { unsigned char c = p[i+j]; line[o++] = (c >= 32 && c < 127) ? (char)c : '.'; }
        line[o++] = '|'; line[o] = '\0';
        OZ_DEBUG("%s", line);
    }
}

#define OZ_WATCH_VAR(name, var, type) \
    do { static OzWatchHandle _oz_wh = -1; if (_oz_wh < 0) _oz_wh = oz_debug_watch_register((name), (const void*)&(var), (type), 1); } while(0)

#ifdef __cplusplus
}
#endif

#endif // OZ_DEBUG_H
