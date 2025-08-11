#ifndef OZ_MATH_H
#define OZ_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OzVec3 {
    float x;
    float y;
    float z;
} OzVec3;

static inline OzVec3 oz_vec3(float x, float y, float z) {
    OzVec3 v; v.x = x; v.y = y; v.z = z; return v;
}

#ifdef __cplusplus
}
#endif

#endif // OZ_MATH_H
