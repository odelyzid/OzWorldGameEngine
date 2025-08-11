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

#define OZ_WATCH_VAR(name, var, type) \
    do { static OzWatchHandle _oz_wh = -1; if (_oz_wh < 0) _oz_wh = oz_debug_watch_register((name), (const void*)&(var), (type), 1); } while(0)

#ifdef __cplusplus
}
#endif

#endif // OZ_DEBUG_H
