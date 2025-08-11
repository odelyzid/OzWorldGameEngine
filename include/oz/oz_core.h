#ifndef OZ_CORE_H
#define OZ_CORE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OZ_VERSION "0.1.0"

const char* oz_core_version(void);

// Simple JIT interface (runtime compilation and symbol load)
// Returns true on success. On success, the shared object remains loaded until oz_jit_unload.
bool oz_jit_compile_and_load(const char* name, const char* c_source, const char* compile_args, void** out_handle);
void* oz_jit_get_symbol(void* handle, const char* symbol_name);
void  oz_jit_unload(void* handle);

#ifdef __cplusplus
}
#endif

#endif // OZ_CORE_H
