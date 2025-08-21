#ifndef OZ_CAMERA_H
#define OZ_CAMERA_H

#include "../oz_math.h"
#include "../oz_platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OzCameraMode {
  OZ_CAMERA_FREEMOVE = 0,
  OZ_CAMERA_FPS = 1,
  OZ_CAMERA_CINEMATIC = 2,
} OzCameraMode;

typedef struct OzCamera {
  OzCameraMode mode;
  OzVec3 position;
  float yaw;       // radians around Z
  float pitch;     // radians around X
  float roll;      // radians around Y (unused)
  float moveSpeed; // units per second
  float turnSpeed; // radians per second
} OzCamera;

void oz_camera_init(OzCamera *cam, OzCameraMode mode);
void oz_camera_set_position(OzCamera *cam, float x, float y, float z);
void oz_camera_set_angles(OzCamera *cam, float yaw, float pitch, float roll);

// Updates a FreeMove camera using a key query callback (keys from OzKey)
// Returns instantaneous speed magnitude (units/sec) for debugging
float oz_camera_update_freemove(OzCamera *cam, float dt_seconds,
                                bool (*key_down)(enum OzKey key));
// FPS mode: gravity and jump (space), no vertical Q/E movement
float oz_camera_update_fps(OzCamera *cam, float dt_seconds,
                           bool (*key_down)(enum OzKey key));

#ifdef __cplusplus
}
#endif

#endif // OZ_CAMERA_H
