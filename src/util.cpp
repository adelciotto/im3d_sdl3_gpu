#include "util.h"

// -- Camera ------------------------------------------------------------------

static HMM_Vec3 vec3_rotate_by_axis_angle(HMM_Vec3 v, HMM_Vec3 axis, float angle_rad) {
  float cos_a = cosf(angle_rad);
  float sin_a = sinf(angle_rad);

  HMM_Vec3 cross = HMM_Cross(axis, v);
  float    dot   = HMM_DotV3(axis, v);

  HMM_Vec3 result;
  result.X = v.X * cos_a + cross.X * sin_a + axis.X * dot * (1.0f - cos_a);
  result.Y = v.Y * cos_a + cross.Y * sin_a + axis.Y * dot * (1.0f - cos_a);
  result.Z = v.Z * cos_a + cross.Z * sin_a + axis.Z * dot * (1.0f - cos_a);
  return result;
}

static float vec3_angle(HMM_Vec3 a, HMM_Vec3 b) {
  float dot       = HMM_DotV3(a, b);
  float len_prod  = HMM_LenV3(a) * HMM_LenV3(b);
  float cos_angle = dot / len_prod;
  cos_angle       = HMM_Clamp(-1.0f, cos_angle, 1.0f);
  return acosf(cos_angle);
}

HMM_Vec3 camera_forward(const Camera& camera) {
  return HMM_NormV3(camera.target - camera.position);
}

HMM_Vec3 camera_up(const Camera& camera) {
  return HMM_NormV3(camera.up);
}

HMM_Vec3 camera_right(const Camera& camera) {
  HMM_Vec3 forward = camera_forward(camera);
  HMM_Vec3 up      = camera_up(camera);
  return HMM_NormV3(HMM_Cross(forward, up));
}

HMM_Mat4 camera_view_matrix(const Camera& camera) {
  return HMM_LookAt_RH(camera.position, camera.target, camera.up);
}

HMM_Mat4 camera_projection_matrix(const Camera& camera, float aspect) {
  if (camera.projection == CAMERA_PROJECTION_PERSPECTIVE) {
    return HMM_Perspective_RH_ZO(
        HMM_AngleDeg(camera.fov_deg),
        aspect,
        camera.near_clip,
        camera.far_clip);
  } else {
    float top   = camera.fov_deg / 2.0f;
    float right = top * aspect;
    return HMM_Orthographic_RH_ZO(-right, right, -top, top, camera.near_clip, camera.far_clip);
  }
}

void camera_move_forward(Camera* camera, float distance, bool move_in_world_plane) {
  HMM_Vec3 forward = camera_forward(*camera);

  if (move_in_world_plane) {
    forward.Y = 0.0f;
    forward   = HMM_NormV3(forward);
  }

  forward          = forward * distance;
  camera->position = camera->position + forward;
  camera->target   = camera->target + forward;
}

void camera_move_right(Camera* camera, float distance, bool move_in_world_plane) {
  HMM_Vec3 right = camera_right(*camera);

  if (move_in_world_plane) {
    right.Y = 0.0f;
    right   = HMM_NormV3(right);
  }

  right            = right * distance;
  camera->position = camera->position + right;
  camera->target   = camera->target + right;
}

void camera_move_up(Camera* camera, float distance) {
  HMM_Vec3 up      = camera_up(*camera);
  up               = up * distance;
  camera->position = camera->position + up;
  camera->target   = camera->target + up;
}

void camera_move_to_target(Camera* camera, float delta) {
  float distance = HMM_LenV3(camera->target - camera->position);
  distance += delta;

  if (distance <= 0.0f) { distance = 0.001f; }

  HMM_Vec3 forward = camera_forward(*camera);
  camera->position = camera->target - forward * distance;
}

void camera_yaw(Camera* camera, float angle_rad, bool rotate_around_target) {
  HMM_Vec3 up              = camera_up(*camera);
  HMM_Vec3 target_position = camera->target - camera->position;

  target_position = vec3_rotate_by_axis_angle(target_position, up, angle_rad);

  if (rotate_around_target) {
    camera->position = camera->target - target_position;
  } else {
    camera->target = camera->position + target_position;
  }
}

void camera_pitch(
    Camera* camera,
    float   angle_rad,
    bool    lock_view,
    bool    rotate_around_target,
    bool    rotate_up) {
  HMM_Vec3 up              = camera_up(*camera);
  HMM_Vec3 target_position = camera->target - camera->position;

  if (lock_view) {
    float max_angle_up = vec3_angle(up, target_position);
    max_angle_up -= 0.001f;
    if (angle_rad > max_angle_up) { angle_rad = max_angle_up; }

    float max_angle_down = vec3_angle(-up, target_position);
    max_angle_down *= -1.0f;
    max_angle_down += 0.001f;
    if (angle_rad < max_angle_down) { angle_rad = max_angle_down; }
  }

  HMM_Vec3 right  = camera_right(*camera);
  target_position = vec3_rotate_by_axis_angle(target_position, right, angle_rad);

  if (rotate_around_target) {
    camera->position = camera->target - target_position;
  } else {
    camera->target = camera->position + target_position;
  }

  if (rotate_up) { camera->up = vec3_rotate_by_axis_angle(camera->up, right, angle_rad); }
}

void camera_roll(Camera* camera, float angle_rad) {
  HMM_Vec3 forward = camera_forward(*camera);
  camera->up       = vec3_rotate_by_axis_angle(camera->up, forward, angle_rad);
}
