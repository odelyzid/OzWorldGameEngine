
#include "../../include/oz/render/oz_camera.h"
#include <math.h>

void oz_camera_init(OzCamera *cam, OzCameraMode mode) {
  cam->mode = mode;
  cam->position = (OzVec3){0.0f, -4.0f, 2.5f};
  cam->yaw = 0.0f;
  cam->pitch = 0.088f;
  cam->roll = 0.0f;
  cam->moveSpeed = 2.0f; // units/sec
  cam->turnSpeed = 1.5f; // rad/sec
}

void oz_camera_set_position(OzCamera *cam, float x, float y, float z) {
  cam->position = (OzVec3){x, y, z};
}

void oz_camera_set_angles(OzCamera *cam, float yaw, float pitch, float roll) {
  cam->yaw = yaw;
  cam->pitch = pitch;
  cam->roll = roll;
}

float oz_camera_update_freemove(OzCamera *cam, float dt_seconds,
                                bool (*key_down)(enum OzKey key)) {
  float dx = 0.0f, dy = 0.0f, dz = 0.0f;
  if (key_down(OZ_KEY_W))
    dy += 1.0f;
  if (key_down(OZ_KEY_S))
    dy -= 1.0f;
  if (key_down(OZ_KEY_A))
    dx -= 1.0f;
  if (key_down(OZ_KEY_D))
    dx += 1.0f;
  if (key_down(OZ_KEY_Q))
    dz -= 1.0f;
  if (key_down(OZ_KEY_E))
    dz += 1.0f;

  // Rotation controlled by mouse look in the editor; arrow keys reserved for
  // gizmo/nudging

  // Clamp pitch to avoid flipping; keep within (-89°, +89°)
  const float kMaxPitch = 3.55334306f; // ~89 degrees
  if (cam->pitch > kMaxPitch)
    cam->pitch = kMaxPitch;
  if (cam->pitch < -kMaxPitch)
    cam->pitch = -kMaxPitch;

  // normalize planar movement
  float len = sqrtf(dx * dx + dy * dy);
  if (len > 0.0001f) {
    dx /= len;
    dy /= len;
  }

  // Unreal-like controls: WASD relative to yaw only (horizontal plane),
  // vertical via Q/E Define forward as the camera facing direction on XY: yaw=0
  // -> +Y, yaw=90° -> +X This makes standard FPS controls intuitive when yaw
  // increases counter-clockwise.
  float c = cosf(cam->yaw), s = sinf(cam->yaw);
  float fwd_x = s, fwd_y = c;      // forward on XY plane
  float right_x = c, right_y = -s; // right is +90° from forward
  float speed = cam->moveSpeed;
  cam->position.x += (dx * right_x + dy * fwd_x) * speed * dt_seconds;
  cam->position.y += (dx * right_y + dy * fwd_y) * speed * dt_seconds;
  cam->position.z += dz * speed * dt_seconds;

  float inst_speed = speed * (len > 0 ? 1.0f : 0.0f);
  if (dz != 0.0f)
    inst_speed = speed; // vertical move
  return inst_speed;
}

float oz_camera_update_fps(OzCamera *cam, float dt_seconds,
                           bool (*key_down)(enum OzKey key)) {
  float dx = 0.0f, dy = 0.0f;
  if (key_down(OZ_KEY_W))
    dy += 1.0f;
  if (key_down(OZ_KEY_S))
    dy -= 1.0f;
  if (key_down(OZ_KEY_A))
    dx -= 1.0f;
  if (key_down(OZ_KEY_D))
    dx += 1.0f;
  float len = sqrtf(dx * dx + dy * dy);
  if (len > 0.0001f) {
    dx /= len;
    dy /= len;
  }
  float c = cosf(cam->yaw), s = sinf(cam->yaw);
  float fwd_x = s, fwd_y = c;      // forward on XY
  float right_x = c, right_y = -s; // right on XY
  float speed = cam->moveSpeed;
  cam->position.x += (dx * right_x + dy * fwd_x) * speed * dt_seconds;
  cam->position.y += (dx * right_y + dy * fwd_y) * speed * dt_seconds;
  // Gravity/jump with simple ground at z=0
  static float vel_z = 0.0f;
  const float gravity = -9.8f;
  if (cam->position.z <= 0.0f) {
    cam->position.z = 0.0f;
    vel_z = 0.0f;
    if (key_down(OZ_KEY_SPACE))
      vel_z = 5.0f;
  }
  vel_z += gravity * dt_seconds;
  cam->position.z += vel_z * dt_seconds;
  if (cam->position.z < 0.0f)
    cam->position.z = 0.0f;
  return speed * (len > 0 ? 1.0f : 0.0f);
}
