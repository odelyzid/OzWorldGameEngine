#include "oz/oz_bundle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* dup_cstr_len(const char* s, size_t n){ char* r=(char*)malloc(n+1); if(!r) return NULL; memcpy(r,s,n); r[n]='\0'; return r; }

bool oz_bundle_load(const char* path, OzBundle* out) {
    if (!path || !out) return false;
    memset(out, 0, sizeof(*out));
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char header[32];
    if (!fgets(header, sizeof(header), f)) { fclose(f); return false; }
    if (strncmp(header, "OZBAG 1", 7) != 0) { fclose(f); return false; }
    size_t count = 0;
    if (fscanf(f, "count %zu\n", &count) != 1) { fclose(f); return false; }
    if (count == 0) { out->entries = NULL; out->count = 0; fclose(f); return true; }
    OzBundleEntry* entries = (OzBundleEntry*)calloc(count, sizeof(OzBundleEntry));
    if (!entries) { fclose(f); return false; }
    for (size_t i = 0; i < count; ++i) {
        size_t name_len = 0;
        if (fscanf(f, "name_len %zu\n", &name_len) != 1) { fclose(f); free(entries); return false; }
        if (name_len == 0 || name_len > 4096) { fclose(f); free(entries); return false; }
        char* name = (char*)malloc(name_len + 1);
        if (!name) { fclose(f); free(entries); return false; }
        size_t nr = fread(name, 1, name_len, f);
        if (nr != name_len) { free(name); fclose(f); free(entries); return false; }
        name[name_len] = '\0';
        int c = fgetc(f); if (c == '\r') c = fgetc(f); if (c != '\n') { free(name); fclose(f); free(entries); return false; }
        size_t size = 0;
        if (fscanf(f, "size %zu\n", &size) != 1) { free(name); fclose(f); free(entries); return false; }
        unsigned char* data = (unsigned char*)malloc(size);
        if (!data) { free(name); fclose(f); free(entries); return false; }
        size_t dr = fread(data, 1, size, f);
        if (dr != size) { free(name); free(data); fclose(f); free(entries); return false; }
        entries[i].name = name;
        entries[i].data = data;
        entries[i].size = size;
    }
    fclose(f);
    out->entries = entries; out->count = count;
    return true;
}

void oz_bundle_free(OzBundle* b) {
    if (!b) return;
    if (b->entries) {
        for (size_t i = 0; i < b->count; ++i) { free(b->entries[i].name); free(b->entries[i].data); }
        free(b->entries);
    }
    b->entries = NULL; b->count = 0;
}

const OzBundleEntry* oz_bundle_find(const OzBundle* b, const char* name) {
    if (!b || !name) return NULL;
    for (size_t i = 0; i < b->count; ++i) {
        if (strcmp(b->entries[i].name, name) == 0) return &b->entries[i];
    }
    return NULL;
}
