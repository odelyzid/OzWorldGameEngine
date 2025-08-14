#include "oz/oz_core.h"
#include "oz/oz_log.h"
#include "oz/oz_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <unistd.h>
#  include <sys/wait.h>
#endif

const char* oz_core_version(void) {
    return OZ_VERSION;
}

#ifndef _WIN32
static char* oz_write_temp_source(const char* name, const char* src, char* out_so_path, size_t so_path_cap, char* out_c_path, size_t c_path_cap) {
    const char* tmp = getenv("TMPDIR"); if (!tmp) tmp = "/tmp";
    snprintf(out_c_path, c_path_cap, "%s/%s.c", tmp, name);
    snprintf(out_so_path, so_path_cap, "%s/%s.so", tmp, name);
    FILE* f = fopen(out_c_path, "wb"); if (!f) return NULL;
    size_t n = fwrite(src, 1, strlen(src), f); fclose(f);
    return (n == strlen(src)) ? out_c_path : NULL;
}

bool oz_jit_compile_and_load(const char* name, const char* c_source, const char* compile_args, void** out_handle) {
    if (!name || !c_source || !out_handle) return false;
    char so_path[512], c_path[512];
    if (!oz_write_temp_source(name, c_source, so_path, sizeof(so_path), c_path, sizeof(c_path))) {
        OZ_ERROR("JIT: failed to write temp source");
        return false;
    }
    char cmd[1024];
    const char* args = (compile_args && *compile_args) ? compile_args : "-O2 -fPIC";
    snprintf(cmd, sizeof(cmd), "cc %s -shared -o %s %s", args, so_path, c_path);
    int rc = system(cmd);
    if (rc != 0) { OZ_ERROR("JIT: compiler returned %d", rc); return false; }
    void* h = dlopen(so_path, RTLD_NOW);
    if (!h) { OZ_ERROR("JIT: dlopen failed: %s", dlerror()); return false; }
    *out_handle = h;
    return true;
}

void* oz_jit_get_symbol(void* handle, const char* symbol_name) {
    if (!handle || !symbol_name) return NULL;
    return dlsym(handle, symbol_name);
}

void oz_jit_unload(void* handle) {
    if (handle) dlclose(handle);
}
#else
bool oz_jit_compile_and_load(const char* name, const char* c_source, const char* compile_args, void** out_handle) {
    (void)name; (void)c_source; (void)compile_args; (void)out_handle; return false;
}
void* oz_jit_get_symbol(void* handle, const char* symbol_name) { (void)handle; (void)symbol_name; return NULL; }
void oz_jit_unload(void* handle) { (void)handle; }
#endif
