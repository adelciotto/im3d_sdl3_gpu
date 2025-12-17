// -- Local Header Includes ---------------------------------------------------
#include "im3d_sdl3_gpu.h"

// -- External Header Includes ------------------------------------------------
#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <HandmadeMath.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

// -- Std Header Includes -----------------------------------------------------
#include <new>

// -- Local Source Includes ---------------------------------------------------
#include "common.cpp"
#include "imgui_font.cpp"

struct Camera {
  HMM_Vec3 position  = HMM_V3(0.0f, 0.5f, 3.0f);
  HMM_Vec3 direction = HMM_NormV3(HMM_V3(0.0f, -0.2f, -1.0f));
  float    fov_rad   = HMM_AngleDeg(50.0f);
  float    near_clip = 0.1f;
  float    far_clip  = 500.0f;
  bool     ortho     = false;
};

struct App_State {
  SDL_GPUDevice*       device;
  SDL_Window*          window;
  SDL_GPUTextureFormat swapchain_texture_format;
  float                content_scale;
  HMM_Vec2             window_size_pixels;
  bool                 window_minimized;
  uint64_t             count_per_second;
  uint64_t             last_counter;
  uint64_t             max_counter_delta;
  double               elapsed_time;
  bool                 fullscreen = false;

  Camera  camera;
  ImFont* imgui_font;
};

static void on_window_pixel_size_changed(App_State* as, int width, int height) {
  as->window_size_pixels = HMM_V2(static_cast<float>(width), static_cast<float>(height));
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

  SDL_GPUShaderFormat format_flags =
      SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV;
  const char* driver;
#ifdef SDL_PLATFORM_WINDOWS
  driver = "direct3d12";
#elif SDL_PLATFORM_LINUX
  driver = "vulkan";
#else
#error "Platform not supported"
#endif
  bool debug = false;
#ifdef BUILD_DEBUG
  debug = true;
#endif
  as->device = SDL_CreateGPUDevice(format_flags, debug, driver);
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
    ImGuiStyle& style   = ImGui::GetStyle();
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
    Im3d_SDL3_GPU_Init_Info info = {};
    info.device                  = as->device;
    info.color_target_format     = as->swapchain_texture_format;
    if (!im3d_sdl3_gpu_init(info)) { return SDL_APP_FAILURE; }
  }

  {
    ImGui_ImplSDL3_InitForSDLGPU(as->window);

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = as->device;
    init_info.ColorTargetFormat          = as->swapchain_texture_format;
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

  float    aspect_ratio = as->window_size_pixels.X / as->window_size_pixels.Y;
  HMM_Vec3 cam_target   = as->camera.position + as->camera.direction;
  HMM_Vec3 world_up     = HMM_V3(0.0f, 1.0f, 0.0f);
  HMM_Mat4 view_matrix  = HMM_LookAt_RH(as->camera.position, {}, world_up);
  HMM_Mat4 proj_matrix  = as->camera.ortho ? HMM_Orthographic_RH_ZO(
                                                -5.0f,
                                                5.0f,
                                                -5.0f,
                                                5.0f,
                                                as->camera.near_clip,
                                                as->camera.far_clip)
                                           : HMM_Perspective_RH_ZO(
                                                as->camera.fov_rad,
                                                aspect_ratio,
                                                as->camera.near_clip,
                                                as->camera.far_clip);

  Im3d_SDL3_GPU_Frame_Info frame_info = {};
  frame_info.delta_time               = static_cast<float>(delta_time);
  frame_info.viewport_size            = as->window_size_pixels;
  frame_info.view_position            = as->camera.position;
  frame_info.view_direction           = as->camera.direction;
  frame_info.world_to_view_transform  = view_matrix;
  frame_info.view_to_clip_transform   = proj_matrix;
  frame_info.ortho                    = as->camera.ortho;
  frame_info.fov_rad                  = as->camera.fov_rad;
  im3d_sdl3_gpu_new_frame(frame_info);
  Im3d::NewFrame();
  Im3d::PushDrawState();
  Im3d::SetSize(4.0f);
  Im3d::DrawAlignedBox(HMM_V3(-1.0f, 1.0f, -1.0f), HMM_V3(1.0f, -1.0f, 1.0f));
  Im3d::DrawXyzAxes();
  Im3d::PopDrawState();
  Im3d::EndFrame();

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
    im3d_sdl3_gpu_prepare_draw_data(cmd_buf);

    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buf);

    {
      SDL_GPUColorTargetInfo target_info = {};
      target_info.texture                = swapchain_texture;
      target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
      target_info.store_op               = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);
      defer(SDL_EndGPURenderPass(render_pass));

      im3d_sdl3_gpu_render_draw_data(cmd_buf, render_pass);

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

  im3d_sdl3_gpu_shutdown();

  SDL_ReleaseWindowFromGPUDevice(as->device, as->window);
  SDL_DestroyWindow(as->window);
  SDL_DestroyGPUDevice(as->device);

  delete as;

  SDL_Quit();
}
