// -- External Header Includes ------------------------------------------------
#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include "../im3d_sdl3_gpu.h"

// -- Std Header Includes -----------------------------------------------------
#include <array>
#include <string>
#include <vector>

// -- Local Source Includes ---------------------------------------------------
#include "common.cpp"
#include "imgui_font.cpp"

struct App_State {
  SDL_GPUDevice*       device;
  SDL_Window*          window;
  SDL_GPUTextureFormat swapchain_texture_format;
  float                content_scale;
  int                  window_width_pixels;
  int                  window_height_pixels;
  bool                 window_minimized;
  uint64_t             count_per_second;
  uint64_t             last_counter;
  uint64_t             max_counter_delta;
  double               elapsed_time;
  bool                 fullscreen = false;

  ImFont* imgui_font;
};

static void on_window_pixel_size_changed(App_State* as, int width, int height) {
  as->window_width_pixels  = width;
  as->window_height_pixels = height;
}

static void on_display_content_scale_changed(App_State* as, float content_scale) {
  as->content_scale = content_scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(as->content_scale);
  style.FontScaleDpi = as->content_scale;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  auto as = new (std::nothrow) App_State {};
  if (as == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate App_State");
    return SDL_APP_FAILURE;
  }
  *appstate = as;

  SDL_GPUShaderFormat format_flags = 0;
#ifdef SDL_PLATFORM_WINDOWS
  format_flags |= SDL_GPU_SHADERFORMAT_DXIL;
#elif SDL_PLATFORM_LINUX
  format_flags |= SDL_GPU_SHADERFORMAT_SPIRV;
#else
#error "Platform not supported"
#endif
  bool debug = false;
#ifdef BUILD_DEBUG
  debug = true;
#endif
  as->device = SDL_CreateGPUDevice(format_flags, debug, nullptr);
  if (as->device == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create gpu device: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  int   window_width       = 800;
  int   window_height      = 600;
  float content_scale      = 1.0f;
  auto  primary_display_id = SDL_GetPrimaryDisplay();
  if (primary_display_id != 0) {
    auto display_mode = SDL_GetDesktopDisplayMode(primary_display_id);
    if (display_mode != nullptr) {
      window_width  = display_mode->w / 2;
      window_height = display_mode->h / 2;
    } else {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to get primary display mode: %s",
          SDL_GetError());
    }
    content_scale = SDL_GetDisplayContentScale(primary_display_id);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get primary display: %s", SDL_GetError());
  }

  as->window = SDL_CreateWindow(
      "Im3d SDL3 GPU Example",
      window_width,
      window_height,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (as->window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(as->device, as->window)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to claim window for gpu device: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  as->swapchain_texture_format = SDL_GetGPUSwapchainTextureFormat(as->device, as->window);

  {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    auto& style         = ImGui::GetStyle();
    style.ItemSpacing.y = 8.0f;

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    as->imgui_font                = io.Fonts->AddFontFromMemoryCompressedTTF(
        const_cast<unsigned char*>(IMGUI_FONT_DATA),
        IMGUI_FONT_DATA_SIZE,
        18.0f,
        &font_cfg);

    on_display_content_scale_changed(as, content_scale);
  }

  {
    ImGui_ImplSDL3_InitForSDLGPU(as->window);

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = as->device;
    init_info.ColorTargetFormat          = SDL_GetGPUSwapchainTextureFormat(as->device, as->window);
    ImGui_ImplSDLGPU3_Init(&init_info);
  }

  {
    int w, h;
    SDL_GetWindowSizeInPixels(as->window, &w, &h);
    on_window_pixel_size_changed(as, w, h);
  }

  as->count_per_second  = SDL_GetPerformanceFrequency();
  as->last_counter      = SDL_GetPerformanceCounter();
  as->max_counter_delta = as->count_per_second / 60 * 8;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto as = static_cast<App_State*>(appstate);

  auto& io                  = ImGui::GetIO();
  bool  process_imgui_event = true;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    on_window_pixel_size_changed(as, event->window.data1, event->window.data2);
    break;
  case SDL_EVENT_WINDOW_MINIMIZED:
    as->window_minimized = true;
    break;
  case SDL_EVENT_WINDOW_RESTORED:
    as->window_minimized = false;
    break;
  case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    on_display_content_scale_changed(as, SDL_GetDisplayContentScale(event->display.displayID));
    break;
  default:
    break;
  }

  if (process_imgui_event) { ImGui_ImplSDL3_ProcessEvent(event); }

  return SDL_APP_CONTINUE;
}

static void draw_imgui(App_State* as) {
  if (ImGui::Begin("Im3d SDL3 GPU Example", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
    auto& io = ImGui::GetIO();
    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / io.Framerate,
        io.Framerate);

    if (ImGui::Button("Toggle Fullscreen")) {
      as->fullscreen = !as->fullscreen;
      SDL_SetWindowFullscreen(as->window, as->fullscreen);
    }

    ImGui::Separator();
  }
  ImGui::End();
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto as = static_cast<App_State*>(appstate);

  auto counter       = SDL_GetPerformanceCounter();
  auto counter_delta = counter - as->last_counter;
  as->last_counter   = counter;
  if (counter_delta > as->max_counter_delta) { counter_delta = as->count_per_second / 60; }

  auto delta_time = static_cast<double>(counter_delta) / static_cast<double>(as->count_per_second);
  as->elapsed_time += delta_time;

  static constexpr double TIME_RESET_PERIOD = 3600.0;
  if (as->elapsed_time >= TIME_RESET_PERIOD) { as->elapsed_time = 0.0; }

  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  draw_imgui(as);
  ImGui::Render();

  SDL_GPUCommandBuffer* cmd_buf = SDL_AcquireGPUCommandBuffer(as->device);
  if (cmd_buf == nullptr) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire command buffer: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture* swapchain_texture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
          cmd_buf,
          as->window,
          &swapchain_texture,
          nullptr,
          nullptr)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire swapchain texture: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (swapchain_texture != nullptr && !as->window_minimized) {
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buf);

    {
      SDL_GPUColorTargetInfo target_info = {};
      target_info.texture                = swapchain_texture;
      target_info.load_op                = SDL_GPU_LOADOP_DONT_CARE;
      target_info.store_op               = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);
      defer(SDL_EndGPURenderPass(render_pass));

      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd_buf, render_pass);
    }
  }

  SDL_SubmitGPUCommandBuffer(cmd_buf);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto as = static_cast<App_State*>(appstate);

  SDL_WaitForGPUIdle(as->device);

  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseWindowFromGPUDevice(as->device, as->window);
  SDL_DestroyWindow(as->window);
  SDL_DestroyGPUDevice(as->device);

  delete as;

  SDL_Quit();
}
