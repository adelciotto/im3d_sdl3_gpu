#include "im3d_sdl3_gpu.h"
#include "imgui_font.h"
#include "util.h"

#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <HandmadeMath.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <new>

struct App_State {
  SDL_GPUDevice*       device;
  SDL_Window*          window;
  SDL_GPUTextureFormat swapchain_texture_format;
  SDL_GPUTexture*      render_target_color_texture;
  SDL_GPUTexture*      render_target_resolve_texture;
  float                content_scale;
  HMM_Vec2             window_size_pixels;
  bool                 window_minimized;
  uint64_t             count_per_second;
  uint64_t             last_counter;
  uint64_t             max_counter_delta;
  double               elapsed_time;
  bool                 fullscreen;
  bool                 key_state[SDL_SCANCODE_COUNT];
  bool                 last_key_state[SDL_SCANCODE_COUNT];
  int                  mouse_button_state;
  int                  last_mouse_button_state;
  HMM_Vec2             mouse_position;
  HMM_Vec2             last_mouse_position;
  float                mouse_scroll;

  ImFont* imgui_font;

  Camera camera;
};

static void update_demo(App_State* as, float dt);
static void draw_demo(App_State* as);
static bool is_key_down(App_State* as, SDL_Scancode scancode);
static bool is_key_pressed(App_State* as, SDL_Scancode scancode);
static bool is_key_released(App_State* as, SDL_Scancode scancode);
static bool is_mouse_button_down(App_State* as, int button);
static bool is_mouse_button_pressed(App_State* as, int button);
static bool is_mouse_button_released(App_State* as, int button);
static void on_window_size_changed(App_State* as, int w, int h);
static void on_display_content_scale_changed(App_State* as, float content_scale);

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

  int           window_width       = 800;
  int           window_height      = 600;
  float         content_scale      = 1.0f;
  SDL_DisplayID primary_display_id = SDL_GetPrimaryDisplay();
  if (primary_display_id != 0) {
    const SDL_DisplayMode* display_mode = SDL_GetDesktopDisplayMode(primary_display_id);
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
    Im3d_SDL3_GPU_Init_Info info = {};
    info.device                  = as->device;
    info.color_target_format     = as->swapchain_texture_format;
    info.msaa_samples            = SDL_GPU_SAMPLECOUNT_4;
    if (!im3d_sdl3_gpu_init(info)) { return SDL_APP_FAILURE; }
  }

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

    ImGui_ImplSDL3_InitForSDLGPU(as->window);

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = as->device;
    init_info.ColorTargetFormat          = as->swapchain_texture_format;
    ImGui_ImplSDLGPU3_Init(&init_info);
  }

  {
    int w, h;
    SDL_GetWindowSizeInPixels(as->window, &w, &h);
    on_window_size_changed(as, w, h);
  }

  as->count_per_second  = SDL_GetPerformanceFrequency();
  as->last_counter      = SDL_GetPerformanceCounter();
  as->max_counter_delta = as->count_per_second / 60 * 8;

  as->camera.position   = HMM_V3(0.0f, 2.0f, 3.0f);
  as->camera.target     = HMM_V3(0.0f, 0.0f, 0.0f);
  as->camera.up         = HMM_V3(0.0f, 1.0f, 0.0f);
  as->camera.projection = CAMERA_PROJECTION_PERSPECTIVE;
  as->camera.fov_deg    = 50.0f;
  as->camera.near_clip  = 0.05f;
  as->camera.far_clip   = 4000.0f;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto as = static_cast<App_State*>(appstate);

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    if (event->window.data1 != 0 && event->window.data2 != 0) {
      on_window_size_changed(as, event->window.data1, event->window.data2);
    }
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
  case SDL_EVENT_KEY_DOWN:
    as->key_state[event->key.scancode] = true;
    break;
  case SDL_EVENT_KEY_UP:
    as->key_state[event->key.scancode] = false;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    as->mouse_button_state |= SDL_BUTTON_MASK(event->button.button);
    break;
  case SDL_EVENT_MOUSE_BUTTON_UP:
    as->mouse_button_state &= ~SDL_BUTTON_MASK(event->button.button);
    break;
  case SDL_EVENT_MOUSE_MOTION:
    as->mouse_position = HMM_V2(event->motion.x, event->motion.y);
    break;
  case SDL_EVENT_MOUSE_WHEEL:
    as->mouse_scroll += event->wheel.y;
    break;
  default:
    break;
  }

  ImGui_ImplSDL3_ProcessEvent(event);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto as = static_cast<App_State*>(appstate);

  uint64_t counter       = SDL_GetPerformanceCounter();
  uint64_t counter_delta = counter - as->last_counter;
  as->last_counter       = counter;
  if (counter_delta > as->max_counter_delta) { counter_delta = as->count_per_second / 60; }

  float delta_time = static_cast<double>(counter_delta) / static_cast<double>(as->count_per_second);
  as->elapsed_time += delta_time;

  static constexpr double TIME_RESET_PERIOD = 3600.0;
  if (as->elapsed_time >= TIME_RESET_PERIOD) { as->elapsed_time = 0.0; }

  update_demo(as, delta_time);

  HMM_Mat4 view_matrix = camera_view_matrix(as->camera);
  HMM_Mat4 proj_matrix =
      camera_projection_matrix(as->camera, as->window_size_pixels.X / as->window_size_pixels.Y);
  Im3d_SDL3_GPU_Frame_Info frame_info = {};
  frame_info.delta_time               = delta_time;
  frame_info.viewport_size            = as->window_size_pixels;
  frame_info.view_position            = as->camera.position;
  frame_info.view_direction           = camera_forward(as->camera);
  frame_info.world_to_view_transform  = view_matrix;
  frame_info.view_to_clip_transform   = proj_matrix;
  frame_info.ortho                    = as->camera.projection == CAMERA_PROJECTION_ORTHOGRAPHIC;
  frame_info.fov_rad                  = HMM_AngleDeg(as->camera.fov_deg);
  im3d_sdl3_gpu_new_frame(frame_info);
  Im3d::NewFrame();

  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  draw_demo(as);

  ImGui::Render();

  Im3d::EndFrame();

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
      target_info.texture                = as->render_target_color_texture;
      target_info.resolve_texture        = as->render_target_resolve_texture;
      target_info.clear_color            = {0.308f, 0.306f, 0.3008f, 1.0001};
      target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
      target_info.store_op               = SDL_GPU_STOREOP_RESOLVE;
      SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);
      defer(SDL_EndGPURenderPass(render_pass));

      im3d_sdl3_gpu_render_draw_data(cmd_buf, render_pass);

      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd_buf, render_pass);
    }

    {
      SDL_GPUBlitInfo info     = {};
      info.source.texture      = as->render_target_resolve_texture;
      info.source.w            = static_cast<uint32_t>(as->window_size_pixels.Width);
      info.source.h            = static_cast<uint32_t>(as->window_size_pixels.Height);
      info.destination.texture = swapchain_texture;
      info.destination.w       = static_cast<uint32_t>(as->window_size_pixels.Width);
      info.destination.h       = static_cast<uint32_t>(as->window_size_pixels.Height);
      info.load_op             = SDL_GPU_LOADOP_DONT_CARE;
      info.filter              = SDL_GPU_FILTER_LINEAR;
      SDL_BlitGPUTexture(cmd_buf, &info);
    }
  }

  SDL_SubmitGPUCommandBuffer(cmd_buf);

  SDL_memcpy(as->last_key_state, as->key_state, sizeof(as->key_state));
  as->last_mouse_button_state = as->mouse_button_state;
  as->last_mouse_position     = as->mouse_position;
  as->mouse_scroll            = 0.0f;

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

static void update_camera(App_State* as, float dt, Camera_Mode mode) {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard || io.WantCaptureMouse) { return; }

  switch (mode) {
  case CAMERA_MODE_FREE: {
    constexpr float MOVE_SPEED        = 5.0f;
    constexpr float MOUSE_SENSITIVITY = 0.003f;

    bool ctrl_down = is_key_down(as, SDL_SCANCODE_LCTRL) || is_key_down(as, SDL_SCANCODE_RCTRL);
    if (!ctrl_down && is_key_down(as, SDL_SCANCODE_W))
      camera_move_forward(&as->camera, MOVE_SPEED * dt, true);
    if (!ctrl_down && is_key_down(as, SDL_SCANCODE_S))
      camera_move_forward(&as->camera, -MOVE_SPEED * dt, true);
    if (!ctrl_down && is_key_down(as, SDL_SCANCODE_D))
      camera_move_right(&as->camera, MOVE_SPEED * dt, true);
    if (!ctrl_down && is_key_down(as, SDL_SCANCODE_A))
      camera_move_right(&as->camera, -MOVE_SPEED * dt, true);
    if (!ctrl_down && is_key_down(as, SDL_SCANCODE_Q)) camera_move_up(&as->camera, MOVE_SPEED * dt);
    if (!ctrl_down && is_key_down(as, SDL_SCANCODE_E))
      camera_move_up(&as->camera, -MOVE_SPEED * dt);

    if (is_mouse_button_down(as, SDL_BUTTON_RIGHT)) {
      HMM_Vec2 mouse_delta = as->mouse_position - as->last_mouse_position;
      camera_yaw(&as->camera, -mouse_delta.X * MOUSE_SENSITIVITY, false);
      camera_pitch(&as->camera, -mouse_delta.Y * MOUSE_SENSITIVITY, true, false, false);
    }

    camera_move_to_target(&as->camera, -as->mouse_scroll);
  } break;
  default:
    break;
  }
}

static void update_demo(App_State* as, float dt) {
  update_camera(as, dt, CAMERA_MODE_FREE);
}

static void draw_demo(App_State* as) {
  ImGui::Begin("Im3d SDL3 GPU Example", nullptr, ImGuiWindowFlags_HorizontalScrollbar);

  ImGuiIO& io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
  ImGui::Spacing();

  if (ImGui::TreeNodeEx("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::LabelText("WASD", "Move Forward/Left/Backward/Right");
    ImGui::LabelText("QE", "Move Down/Up");
    ImGui::LabelText("RMouse", "Look Around");
    ImGui::LabelText("CTRL-T", "Gizmo Translation");
    ImGui::LabelText("CTRL-R", "Gizmo Rotation");
    ImGui::LabelText("CTRL-S", "Gizmo Scale");
    ImGui::TreePop();
  }
  ImGui::Spacing();

  if (ImGui::Button("Toggle Fullscreen")) {
    as->fullscreen = !as->fullscreen;
    SDL_SetWindowFullscreen(as->window, as->fullscreen);
  }

  ImGui::Separator();

  if (ImGui::TreeNodeEx("High Order Shapes", ImGuiTreeNodeFlags_DefaultOpen)) {
    static Im3d::Mat4 transform(1.0f);
    Im3d::Gizmo("ShapeGizmo", transform);

    if (ImGui::Button("Reset Transform")) { transform = {1.0f}; }

    enum Shape {
      SHAPE_QUAD,
      SHAPE_QUAD_FILLED,
      SHAPE_CIRCLE,
      SHAPE_CIRCLE_FILLED,
      SHAPE_SPHERE,
      SHAPE_SPHERE_FILLED,
      SHAPE_ALIGNED_BOX,
      SHAPE_ALIGNED_BOX_FILLED,
      SHAPE_CYLINDER,
      SHAPE_CYLINDER_FILLED,
      SHAPE_CAPSULE,
      SHAPE_PRISM,
      SHAPE_ARROW,
      SHAPE_CONE,
      SHAPE_CONE_FILLED
    };
    static const char* shape_list    = "Quad\0"
                                       "Quad Filled\0"
                                       "Circle\0"
                                       "Circle Filled\0"
                                       "Sphere\0"
                                       "Sphere Filled\0"
                                       "AlignedBox\0"
                                       "AlignedBoxFilled\0"
                                       "Cylinder\0"
                                       "Cylinder Filled\0"
                                       "Capsule\0"
                                       "Prism\0"
                                       "Arrow\0"
                                       "Cone\0"
                                       "ConeFilled\0";
    static int         current_shape = SHAPE_CAPSULE;
    ImGui::Combo("Shape", &current_shape, shape_list);
    static Im3d::Vec4 color = Im3d::Vec4(1.0f, 0.0f, 0.6f, 1.0f);
    ImGui::ColorEdit4("Color", color);
    static float thickness = 4.0f;
    ImGui::SliderFloat("Thickness", &thickness, 0.0f, 16.0f);
    static int detail = -1;

    Im3d::PushMatrix(transform);
    Im3d::PushDrawState();
    Im3d::EnableSorting(true);
    Im3d::SetSize(thickness);
    Im3d::SetColor(Im3d::Color(color.x, color.y, color.z, color.w));

    switch ((Shape)current_shape) {
    case SHAPE_QUAD:
    case SHAPE_QUAD_FILLED: {
      static Im3d::Vec2 quad_size(1.0f);
      ImGui::SliderFloat2("Size", quad_size, 0.0f, 10.0f);
      if (current_shape == SHAPE_QUAD) {
        Im3d::DrawQuad(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), quad_size);
      } else {
        Im3d::DrawQuadFilled(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), quad_size);
      }
      break;
    }
    case SHAPE_CIRCLE:
    case SHAPE_CIRCLE_FILLED: {
      static float circle_radius = 1.0f;
      ImGui::SliderFloat("Radius", &circle_radius, 0.0f, 10.0f);
      ImGui::SliderInt("Detail", &detail, -1, 128);
      if (current_shape == SHAPE_CIRCLE) {
        Im3d::DrawCircle(Im3d::Vec3(0.0f), Im3d::Vec3(0.0f, 0.0f, 1.0f), circle_radius, detail);
      } else {
        Im3d::DrawCircleFilled(
            Im3d::Vec3(0.0f),
            Im3d::Vec3(0.0f, 0.0f, 1.0f),
            circle_radius,
            detail);
      }
      break;
    }
    case SHAPE_SPHERE:
    case SHAPE_SPHERE_FILLED: {
      static float sphere_radius = 1.0f;
      ImGui::SliderFloat("Radius", &sphere_radius, 0.0f, 10.0f);
      ImGui::SliderInt("Detail", &detail, -1, 128);
      if (current_shape == SHAPE_SPHERE) {
        Im3d::DrawSphere(Im3d::Vec3(0.0f), sphere_radius, detail);
      } else {
        Im3d::DrawSphereFilled(Im3d::Vec3(0.0f), sphere_radius, detail);
      }
      break;
    }
    case SHAPE_ALIGNED_BOX:
    case SHAPE_ALIGNED_BOX_FILLED: {
      static Im3d::Vec3 box_size(1.0f);
      ImGui::SliderFloat3("Size", box_size, 0.0f, 10.0f);
      if (current_shape == SHAPE_ALIGNED_BOX) {
        Im3d::DrawAlignedBox(-box_size, box_size);
      } else {
        Im3d::DrawAlignedBoxFilled(-box_size, box_size);
      }
      break;
    }
    case SHAPE_CYLINDER:
    case SHAPE_CYLINDER_FILLED: {
      static float cylinder_radius = 1.0f;
      static float cylinder_length = 1.0f;
      static bool  draw_cap_start  = true;
      static bool  draw_cap_end    = true;
      ImGui::SliderFloat("Radius", &cylinder_radius, 0.0f, 10.0f);
      ImGui::SliderFloat("Length", &cylinder_length, 0.0f, 10.0f);
      ImGui::SliderInt("Detail", &detail, -1, 128);

      if (current_shape == SHAPE_CYLINDER) {
        Im3d::DrawCylinder(
            Im3d::Vec3(0.0f, -cylinder_length, 0.0f),
            Im3d::Vec3(0.0f, cylinder_length, 0.0f),
            cylinder_radius,
            detail);
      } else {
        ImGui::Checkbox("Draw Cap Start", &draw_cap_start);
        ImGui::SameLine();
        ImGui::Checkbox("Draw Cap End", &draw_cap_end);
        Im3d::DrawCylinderFilled(
            Im3d::Vec3(0.0f, -cylinder_length, 0.0f),
            Im3d::Vec3(0.0f, cylinder_length, 0.0f),
            cylinder_radius,
            draw_cap_start,
            draw_cap_end,
            detail);
      }
      break;
    }
    case SHAPE_CAPSULE: {
      static float capsule_radius = 0.5f;
      static float capsule_length = 1.0f;
      ImGui::SliderFloat("Radius", &capsule_radius, 0.0f, 10.0f);
      ImGui::SliderFloat("Length", &capsule_length, 0.0f, 10.0f);
      ImGui::SliderInt("Detail", &detail, -1, 128);
      Im3d::DrawCapsule(
          Im3d::Vec3(0.0f, -capsule_length, 0.0f),
          Im3d::Vec3(0.0f, capsule_length, 0.0f),
          capsule_radius,
          detail);
      break;
    }
    case SHAPE_PRISM: {
      static float prism_radius = 1.0f;
      static float prism_length = 1.0f;
      static int   prism_sides  = 3;
      ImGui::SliderFloat("Radius", &prism_radius, 0.0f, 10.0f);
      ImGui::SliderFloat("Length", &prism_length, 0.0f, 10.0f);
      ImGui::SliderInt("Sides", &prism_sides, 3, 16);
      Im3d::DrawPrism(
          Im3d::Vec3(0.0f, -prism_length, 0.0f),
          Im3d::Vec3(0.0f, prism_length, 0.0f),
          prism_radius,
          prism_sides);
      break;
    }
    case SHAPE_ARROW: {
      static float arrow_length   = 1.0f;
      static float head_length    = -1.0f;
      static float head_thickness = -1.0f;
      ImGui::SliderFloat("Length", &arrow_length, 0.0f, 10.0f);
      ImGui::SliderFloat("Head Length", &head_length, 0.0f, 1.0f);
      ImGui::SliderFloat("Head Thickness", &head_thickness, 0.0f, 1.0f);
      Im3d::DrawArrow(
          Im3d::Vec3(0.0f),
          Im3d::Vec3(0.0f, arrow_length, 0.0f),
          head_length,
          head_thickness);
      break;
    }
    case SHAPE_CONE:
    case SHAPE_CONE_FILLED: {
      static float cone_height        = 1.0f;
      static float cone_radius_top    = 0.0f;
      static float cone_radius_bottom = 0.5f;
      static bool  draw_cap_top       = true;
      static bool  draw_cap_bottom    = true;
      ImGui::SliderFloat("Height", &cone_height, 0.0f, 10.0f);
      ImGui::SliderFloat("Radius Top", &cone_radius_top, 0.0f, 10.0f);
      ImGui::SliderFloat("Radius Bottom", &cone_radius_bottom, 0.0f, 10.0f);
      ImGui::SliderInt("Detail", &detail, -1, 128);

      if (current_shape == SHAPE_CONE) {
        Im3d::DrawCone2(
            Im3d::Vec3(0.0f, cone_height, 0.0f),
            Im3d::Vec3(0.0f, 0.0f, 0.0f),
            cone_radius_top,
            cone_radius_bottom,
            detail);
      } else {
        ImGui::Checkbox("Draw Cap Top", &draw_cap_top);
        ImGui::SameLine();
        ImGui::Checkbox("Draw Cap Bottom", &draw_cap_bottom);
        Im3d::DrawConeFilled2(
            Im3d::Vec3(0.0f, cone_height, 0.0f),
            Im3d::Vec3(0.0f, 0.0f, 0.0f),
            cone_radius_top,
            cone_radius_bottom,
            draw_cap_top,
            draw_cap_bottom,
            detail);
      }

      break;
    }
    default:
      break;
    };

    Im3d::PopDrawState();
    Im3d::PopMatrix();

    ImGui::TreePop();
  }

  if (ImGui::TreeNodeEx("Layers")) {
    Im3d::PushLayerId("DrawFirst");
    Im3d::PopLayerId();
    Im3d::PushLayerId("DrawSecond");
    Im3d::PopLayerId();

    Im3d::PushLayerId("DrawSecond");
    Im3d::BeginTriangles();
    Im3d::Vertex(-0.4f, 0.0f, 0.0f, 16.0f, Im3d::Color_Red);
    Im3d::Vertex(0.1f, 1.0f, 0.0f, 16.0f, Im3d::Color_Red);
    Im3d::Vertex(0.6f, 0.0f, 0.0f, 16.0f, Im3d::Color_Red);
    Im3d::End();
    Im3d::PopLayerId();
    Im3d::PushLayerId("DrawFirst");
    Im3d::BeginTriangles();
    Im3d::Vertex(-0.6f, 0.0f, 0.0f, 16.0f, Im3d::Color_Magenta);
    Im3d::Vertex(-0.1f, 1.0f, 0.0f, 16.0f, Im3d::Color_Magenta);
    Im3d::Vertex(0.4f, 0.0f, 0.0f, 16.0f, Im3d::Color_Magenta);
    Im3d::End();
    Im3d::PopLayerId();

    ImGui::TreePop();
  }

  if (ImGui::TreeNodeEx("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
    static int grid_size = 20;
    ImGui::SliderInt("Grid Size", &grid_size, 1, 50);
    float grid_half_size = (float)grid_size * 0.5f;
    Im3d::SetAlpha(1.0f);
    Im3d::SetSize(2.0f);
    Im3d::BeginLines();
    for (int x = 0; x <= grid_size; ++x) {
      Im3d::Vertex(-grid_half_size, 0.0f, (float)x - grid_half_size, Im3d::Color_Black);
      Im3d::Vertex(grid_half_size, 0.0f, (float)x - grid_half_size, Im3d::Color_Red);
    }
    for (int z = 0; z <= grid_size; ++z) {
      Im3d::Vertex((float)z - grid_half_size, 0.0f, -grid_half_size, Im3d::Color_Black);
      Im3d::Vertex((float)z - grid_half_size, 0.0f, grid_half_size, Im3d::Color_Blue);
    }
    Im3d::End();

    ImGui::TreePop();
  }

  ImGui::End();
}

static bool is_key_down(App_State* as, SDL_Scancode scancode) {
  return as->key_state[scancode];
}

static bool is_key_pressed(App_State* as, SDL_Scancode scancode) {
  return !as->last_key_state[scancode] && as->key_state[scancode];
}

static bool is_key_released(App_State* as, SDL_Scancode scancode) {
  return as->last_key_state[scancode] && !as->key_state[scancode];
}

static bool is_mouse_button_down(App_State* as, int button) {
  return (as->mouse_button_state & SDL_BUTTON_MASK(button)) != 0;
}

static bool is_mouse_button_pressed(App_State* as, int button) {
  return (as->last_mouse_button_state & SDL_BUTTON_MASK(button)) == 0 &&
         (as->mouse_button_state & SDL_BUTTON_MASK(button)) != 0;
}

static bool is_mouse_button_released(App_State* as, int button) {
  return (as->last_mouse_button_state & SDL_BUTTON_MASK(button)) != 0 &&
         (as->mouse_button_state & SDL_BUTTON_MASK(button)) == 0;
}

static void on_window_size_changed(App_State* as, int w, int h) {
  if (as->window_size_pixels.Width == w && as->window_size_pixels.Height == h) { return; }
  as->window_size_pixels = HMM_V2(w, h);

  SDL_GPUTextureCreateInfo texture_create_info = {};
  texture_create_info.type                     = SDL_GPU_TEXTURETYPE_2D;
  texture_create_info.width                = static_cast<uint32_t>(as->window_size_pixels.Width);
  texture_create_info.height               = static_cast<uint32_t>(as->window_size_pixels.Height);
  texture_create_info.layer_count_or_depth = 1;
  texture_create_info.num_levels           = 1;
  texture_create_info.format               = as->swapchain_texture_format;

  SDL_GPUTexture* color_texture;
  {
    SDL_GPUTextureCreateInfo info = texture_create_info;
    info.usage                    = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.sample_count             = SDL_GPU_SAMPLECOUNT_4;
    color_texture                 = SDL_CreateGPUTexture(as->device, &info);
    if (color_texture == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create render target color texture: %s",
          SDL_GetError());
      return;
    }
  }
  SDL_GPUTexture* resolve_texture;
  {
    SDL_GPUTextureCreateInfo info = texture_create_info;
    info.usage      = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    resolve_texture = SDL_CreateGPUTexture(as->device, &info);
    if (resolve_texture == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create render target resolve_texture: %s",
          SDL_GetError());
      return;
    }
  }

  SDL_WaitForGPUIdle(as->device);
  SDL_ReleaseGPUTexture(as->device, as->render_target_color_texture);
  SDL_ReleaseGPUTexture(as->device, as->render_target_resolve_texture);

  as->render_target_color_texture   = color_texture;
  as->render_target_resolve_texture = resolve_texture;
}

static void on_display_content_scale_changed(App_State* as, float content_scale) {
  as->content_scale = content_scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(as->content_scale);
  style.FontScaleDpi = as->content_scale;
}
