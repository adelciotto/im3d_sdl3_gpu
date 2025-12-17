#pragma once

#include "im3d.h"
#include <SDL3/SDL.h>

struct Im3d_SDL3_GPU_Init_Info {
  SDL_GPUDevice*       device              = nullptr;
  SDL_GPUTextureFormat color_target_format = SDL_GPU_TEXTUREFORMAT_INVALID;
  SDL_GPUSampleCount   msaa_samples        = SDL_GPU_SAMPLECOUNT_1;
};

struct Im3d_SDL3_GPU_Frame_Info {
  float      delta_time;
  Im3d::Vec2 viewport_size;
  Im3d::Vec2 cursor_position;
  Im3d::Vec3 view_position;
  Im3d::Vec3 view_direction;
  Im3d::Mat4 world_to_view_transform = {1.0f};
  Im3d::Mat4 view_to_clip_transform  = {1.0f};
  bool       ortho;
  float      fov_rad;
};

bool im3d_sdl3_gpu_init(const Im3d_SDL3_GPU_Init_Info& info);
void im3d_sdl3_gpu_shutdown();
void im3d_sdl3_gpu_new_frame(const Im3d_SDL3_GPU_Frame_Info& info);
void im3d_sdl3_gpu_prepare_draw_data(SDL_GPUCommandBuffer* command_buffer);
void im3d_sdl3_gpu_render_draw_data(
    SDL_GPUCommandBuffer* command_buffer,
    SDL_GPURenderPass*    render_pass);
