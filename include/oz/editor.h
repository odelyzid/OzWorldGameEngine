#ifndef OZ_EDITOR_H
#define OZ_EDITOR_H

#include "oz/render/oz_bsp.h"
#include "oz/render/oz_camera.h"

void oz_editor_init(OzMap* target_map);
OzWorldProps* oz_editor_world_props(void);
int oz_editor_selected(void);
void oz_editor_select(int index);

void oz_editor_begin_add_box(void);
void oz_editor_begin_add_cyl(void);
void oz_editor_set_box_size(float sx, float sy, float sz);
void oz_editor_set_cyl_params(float rx, float ry, float h, int seg);
void oz_editor_apply_current_tool(const OzCamera* cam);

bool oz_editor_csg_add(int a, int b);
bool oz_editor_csg_sub(int a, int b);
bool oz_editor_csg_isect(int a, int b);

#endif

