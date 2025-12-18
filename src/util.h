#pragma once

#include <HandmadeMath.h>

// -- Defer -------------------------------------------------------------------

template<typename F> struct Priv_Defer {
  F f;
  Priv_Defer(F f) : f(f) {
  }
  ~Priv_Defer() {
    f();
  }
};

template<typename F> Priv_Defer<F> defer_func(F f) {
  return Priv_Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&]() { code; })

// -- Camera ------------------------------------------------------------------

enum Camera_Projection {
  CAMERA_PROJECTION_PERSPECTIVE,
  CAMERA_PROJECTION_ORTHOGRAPHIC,
};

enum Camera_Mode {
  CAMERA_MODE_CUSTOM,
  CAMERA_MODE_FREE,
  CAMERA_MODE_ORBITAL,
  CAMERA_MODE_FIRST_PERSON,
  CAMERA_MODE_THIRD_PERSON,
};

struct Camera {
  HMM_Vec3          position;
  HMM_Vec3          target;
  HMM_Vec3          up;
  Camera_Projection projection;
  float             fov_deg;
  float             near_clip;
  float             far_clip;
};

HMM_Vec3 camera_forward(const Camera& camera);
HMM_Vec3 camera_up(const Camera& camera);
HMM_Vec3 camera_right(const Camera& camera);
HMM_Mat4 camera_view_matrix(const Camera& camera);
HMM_Mat4 camera_projection_matrix(const Camera& camera, float aspect);
void     camera_move_forward(Camera* camera, float distance, bool move_in_world_plane);
void     camera_move_right(Camera* camera, float distance, bool move_in_world_plane);
void     camera_move_up(Camera* camera, float distance);
void     camera_move_to_target(Camera* camera, float delta);
void     camera_yaw(Camera* camera, float angle_rad, bool rotate_around_target);
void     camera_roll(Camera* camera, float angle_rad);
void     camera_pitch(
        Camera* camera,
        float   angle_rad,
        bool    lock_view,
        bool    rotate_around_target,
        bool    rotate_up);
