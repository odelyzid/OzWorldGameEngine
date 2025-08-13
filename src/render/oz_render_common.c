#include "oz/oz_render.h"

OzProjectionParams oz_render_default_projection(void) {
    OzProjectionParams p;
    p.verticalFovDegrees = 60.0f;
    p.nearPlane = 0.1f;
    p.farPlane = 100.0f;
    return p;
}
