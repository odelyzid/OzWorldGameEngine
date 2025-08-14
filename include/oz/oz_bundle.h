#ifndef OZ_BUNDLE_H
#define OZ_BUNDLE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Oz bundle format (.ozbag) is a very simple archive:
// ASCII header: OZBAG 1\n
// count <N>\n
// For each entry i in 0..N-1:
//   name_len <L>\n
//   name bytes (L, not null-terminated)
//   size <S>\n
//   data bytes (S)
// The bundle keeps data in memory for now; for large bundles
// a streaming API could be added in the future.

typedef struct OzBundleEntry {
    char* name;        // null-terminated
    unsigned char* data;
    size_t size;
} OzBundleEntry;

typedef struct OzBundle {
    OzBundleEntry* entries;
    size_t count;
} OzBundle;

bool oz_bundle_load(const char* path, OzBundle* out);
void oz_bundle_free(OzBundle* b);
const OzBundleEntry* oz_bundle_find(const OzBundle* b, const char* name);

#ifdef __cplusplus
}
#endif

#endif // OZ_BUNDLE_H
