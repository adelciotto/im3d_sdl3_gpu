#pragma once

#include <SDL3/SDL.h>

struct Im3d_SDL3GPU_InitInfo {
  SDL_GPUDevice*       device              = nullptr;
  SDL_GPUTextureFormat color_target_format = SDL_GPU_TEXTUREFORMAT_INVALID;
  SDL_GPUSampleCount   msaa_samples        = SDL_GPU_SAMPLECOUNT_1;
};

bool im3d_sdl3_gpu_init(const Im3d_SDL3GPU_InitInfo& info);
void im3d_sdl3_gpu_shutdown();
void im3d_sdl3_gpu_new_frame();
void im3d_sdl3_gpu_prepare_draw_data(SDL_GPUCommandBuffer* command_buffer);
void im3d_sdl3_gpu_render_draw_data(SDL_GPUCommandBuffer* command_buffer);
