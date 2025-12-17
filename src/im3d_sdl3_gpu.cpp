// -- Local Header Includes ---------------------------------------------------
#include "im3d_sdl3_gpu.h"
#include "im3d_math.h"

// -- Local Source Includes ---------------------------------------------------
#include "im3d_sdl3_gpu_shaders.cpp"

struct Vertex_Uniforms {
  Im3d::Mat4 world_to_clip_transform;
  Im3d::Vec2 resolution;
  uint32_t   instance_offset;
};

static struct {
  Im3d_SDL3_GPU_Init_Info  init_info;
  SDL_GPUGraphicsPipeline* pipeline_points;
  SDL_GPUGraphicsPipeline* pipeline_lines;
  SDL_GPUGraphicsPipeline* pipeline_triangles;
  SDL_GPUBuffer*           vertex_buffer;
  SDL_GPUBuffer*           data_buffer;
  SDL_GPUTransferBuffer*   transfer_buffer;
  uint32_t                 data_buffer_size;
  uint32_t                 total_vertex_count;
  Im3d::Mat4               world_to_clip_transform = Im3d::Mat4(1.0f);
} g_data = {};

bool im3d_sdl3_gpu_init(const Im3d_SDL3_GPU_Init_Info& info) {
  SDL_assert(info.device != nullptr);

  g_data.init_info = info;

  {
    const uint8_t *shader_points_vert_data, *shader_points_frag_data;
    uint64_t       shader_points_vert_size, shader_points_frag_size;
    const uint8_t *shader_lines_vert_data, *shader_lines_frag_data;
    uint64_t       shader_lines_vert_size, shader_lines_frag_size;
    const uint8_t *shader_triangles_vert_data, *shader_triangles_frag_data;
    uint64_t       shader_triangles_vert_size, shader_triangles_frag_size;

    auto                driver = SDL_GetGPUDeviceDriver(g_data.init_info.device);
    SDL_GPUShaderFormat shader_format;
    if (SDL_strcmp(driver, "vulkan") == 0) {
      shader_format              = SDL_GPU_SHADERFORMAT_SPIRV;
      shader_points_vert_data    = im3d_points_vert_spv;
      shader_points_vert_size    = sizeof(im3d_points_vert_spv);
      shader_points_frag_data    = im3d_points_frag_spv;
      shader_points_frag_size    = sizeof(im3d_points_frag_spv);
      shader_lines_vert_data     = im3d_lines_vert_spv;
      shader_lines_vert_size     = sizeof(im3d_lines_vert_spv);
      shader_lines_frag_data     = im3d_lines_frag_spv;
      shader_lines_frag_size     = sizeof(im3d_lines_frag_spv);
      shader_triangles_vert_data = im3d_triangles_vert_spv;
      shader_triangles_vert_size = sizeof(im3d_triangles_vert_spv);
      shader_triangles_frag_data = im3d_triangles_frag_spv;
      shader_triangles_frag_size = sizeof(im3d_triangles_frag_spv);
    } else if (SDL_strcmp(driver, "direct3d12") == 0) {
      shader_format              = SDL_GPU_SHADERFORMAT_DXIL;
      shader_points_vert_data    = im3d_points_vert_dxil;
      shader_points_vert_size    = sizeof(im3d_points_vert_dxil);
      shader_points_frag_data    = im3d_points_frag_dxil;
      shader_points_frag_size    = sizeof(im3d_points_frag_dxil);
      shader_lines_vert_data     = im3d_lines_vert_dxil;
      shader_lines_vert_size     = sizeof(im3d_lines_vert_dxil);
      shader_lines_frag_data     = im3d_lines_frag_dxil;
      shader_lines_frag_size     = sizeof(im3d_lines_frag_dxil);
      shader_triangles_vert_data = im3d_triangles_vert_dxil;
      shader_triangles_vert_size = sizeof(im3d_triangles_vert_dxil);
      shader_triangles_frag_data = im3d_triangles_frag_dxil;
      shader_triangles_frag_size = sizeof(im3d_triangles_frag_dxil);
    } else if (SDL_strcmp(driver, "metal") == 0) {
      auto supported_formats = SDL_GetGPUShaderFormats(g_data.init_info.device);
      if (supported_formats & SDL_GPU_SHADERFORMAT_METALLIB) {
        shader_format = SDL_GPU_SHADERFORMAT_METALLIB;
        // TODO: Add metallib shaders.
        SDL_assert(false);
      } else if (supported_formats & SDL_GPU_SHADERFORMAT_MSL) {
        shader_format              = SDL_GPU_SHADERFORMAT_MSL;
        shader_points_vert_data    = im3d_points_vert_msl;
        shader_points_vert_size    = sizeof(im3d_points_vert_msl);
        shader_points_frag_data    = im3d_points_frag_msl;
        shader_points_frag_size    = sizeof(im3d_points_frag_msl);
        shader_lines_vert_data     = im3d_lines_vert_msl;
        shader_lines_vert_size     = sizeof(im3d_lines_vert_msl);
        shader_lines_frag_data     = im3d_lines_frag_msl;
        shader_lines_frag_size     = sizeof(im3d_lines_frag_msl);
        shader_triangles_vert_data = im3d_triangles_vert_msl;
        shader_triangles_vert_size = sizeof(im3d_triangles_vert_msl);
        shader_triangles_frag_data = im3d_triangles_frag_msl;
        shader_triangles_frag_size = sizeof(im3d_triangles_frag_msl);
      }
    } else {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Im3d SDL3 GPU backend does not support this GPU device driver");
      return false;
    }

    SDL_GPUColorTargetDescription color_target_desc     = {};
    color_target_desc.format                            = g_data.init_info.color_target_format;
    color_target_desc.blend_state.enable_blend          = true;
    color_target_desc.blend_state.color_blend_op        = SDL_GPU_BLENDOP_ADD;
    color_target_desc.blend_state.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;
    color_target_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    color_target_desc.blend_state.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B |
        SDL_GPU_COLORCOMPONENT_A;

    SDL_GPUVertexBufferDescription vertex_buffer_desc = {};
    vertex_buffer_desc.slot                           = 0;
    vertex_buffer_desc.pitch                          = sizeof(Im3d::Vec4);
    vertex_buffer_desc.input_rate                     = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertex_attribute = {};
    vertex_attribute.location               = 0;
    vertex_attribute.buffer_slot            = 0;
    vertex_attribute.format                 = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info             = {};
    pipeline_info.vertex_input_state.num_vertex_buffers         = 1;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;
    pipeline_info.vertex_input_state.num_vertex_attributes      = 1;
    pipeline_info.vertex_input_state.vertex_attributes          = &vertex_attribute;
    pipeline_info.target_info.num_color_targets                 = 1;
    pipeline_info.target_info.color_target_descriptions         = &color_target_desc;
    pipeline_info.primitive_type                 = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.multisample_state.sample_count = g_data.init_info.msaa_samples;

    // Points pipeline.
    {
      SDL_GPUShader* vertex_shader;
      {
        SDL_GPUShaderCreateInfo info = {};
        info.code                    = shader_points_vert_data;
        info.code_size               = shader_points_vert_size;
        info.entrypoint              = "main";
        info.format                  = shader_format;
        info.num_storage_buffers     = 1;
        info.num_uniform_buffers     = 1;
        info.stage                   = SDL_GPU_SHADERSTAGE_VERTEX;
        vertex_shader                = SDL_CreateGPUShader(g_data.init_info.device, &info);
        if (vertex_shader == nullptr) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to create points vertex shader: %s",
              SDL_GetError());
          return false;
        }
      }

      SDL_GPUShader* fragment_shader;
      {
        SDL_GPUShaderCreateInfo info = {};
        info.code                    = shader_points_frag_data;
        info.code_size               = shader_points_frag_size;
        info.entrypoint              = "main";
        info.format                  = shader_format;
        info.stage                   = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fragment_shader              = SDL_CreateGPUShader(g_data.init_info.device, &info);
        if (fragment_shader == nullptr) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to create points fragment shader: %s",
              SDL_GetError());
          return false;
        }
      }

      auto info              = pipeline_info;
      info.primitive_type    = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
      info.vertex_shader     = vertex_shader;
      info.fragment_shader   = fragment_shader;
      g_data.pipeline_points = SDL_CreateGPUGraphicsPipeline(g_data.init_info.device, &info);
      if (g_data.pipeline_points == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create points pipeline: %s",
            SDL_GetError());
        return false;
      }

      SDL_ReleaseGPUShader(g_data.init_info.device, vertex_shader);
      SDL_ReleaseGPUShader(g_data.init_info.device, fragment_shader);
    }

    // Lines pipeline.
    {
      SDL_GPUShader* vertex_shader;
      {
        SDL_GPUShaderCreateInfo info = {};
        info.code                    = shader_lines_vert_data;
        info.code_size               = shader_lines_vert_size;
        info.entrypoint              = "main";
        info.format                  = shader_format;
        info.num_storage_buffers     = 1;
        info.num_uniform_buffers     = 1;
        info.stage                   = SDL_GPU_SHADERSTAGE_VERTEX;
        vertex_shader                = SDL_CreateGPUShader(g_data.init_info.device, &info);
        if (vertex_shader == nullptr) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to create lines vertex shader: %s",
              SDL_GetError());
          return false;
        }
      }

      SDL_GPUShader* fragment_shader;
      {
        SDL_GPUShaderCreateInfo info = {};
        info.code                    = shader_lines_frag_data;
        info.code_size               = shader_lines_frag_size;
        info.entrypoint              = "main";
        info.format                  = shader_format;
        info.stage                   = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fragment_shader              = SDL_CreateGPUShader(g_data.init_info.device, &info);
        if (fragment_shader == nullptr) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to create lines fragment shader: %s",
              SDL_GetError());
          return false;
        }
      }

      auto info             = pipeline_info;
      info.primitive_type   = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
      info.vertex_shader    = vertex_shader;
      info.fragment_shader  = fragment_shader;
      g_data.pipeline_lines = SDL_CreateGPUGraphicsPipeline(g_data.init_info.device, &info);
      if (g_data.pipeline_lines == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create lines pipeline: %s",
            SDL_GetError());
        return false;
      }

      SDL_ReleaseGPUShader(g_data.init_info.device, vertex_shader);
      SDL_ReleaseGPUShader(g_data.init_info.device, fragment_shader);
    }

    // Triangles pipeline.
    {
      SDL_GPUShader* vertex_shader;
      {
        SDL_GPUShaderCreateInfo info = {};
        info.code                    = shader_triangles_vert_data;
        info.code_size               = shader_triangles_vert_size;
        info.entrypoint              = "main";
        info.format                  = shader_format;
        info.num_storage_buffers     = 1;
        info.num_uniform_buffers     = 1;
        info.stage                   = SDL_GPU_SHADERSTAGE_VERTEX;
        vertex_shader                = SDL_CreateGPUShader(g_data.init_info.device, &info);
        if (vertex_shader == nullptr) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to create triangles vertex shader: %s",
              SDL_GetError());
          return false;
        }
      }

      SDL_GPUShader* fragment_shader;
      {
        SDL_GPUShaderCreateInfo info = {};
        info.code                    = shader_triangles_frag_data;
        info.code_size               = shader_triangles_frag_size;
        info.entrypoint              = "main";
        info.format                  = shader_format;
        info.stage                   = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fragment_shader              = SDL_CreateGPUShader(g_data.init_info.device, &info);
        if (fragment_shader == nullptr) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to create triangles fragment shader: %s",
              SDL_GetError());
          return false;
        }
      }

      auto info                 = pipeline_info;
      info.vertex_shader        = vertex_shader;
      info.fragment_shader      = fragment_shader;
      g_data.pipeline_triangles = SDL_CreateGPUGraphicsPipeline(g_data.init_info.device, &info);
      if (g_data.pipeline_triangles == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create triangles pipeline: %s",
            SDL_GetError());
        return false;
      }

      SDL_ReleaseGPUShader(g_data.init_info.device, vertex_shader);
      SDL_ReleaseGPUShader(g_data.init_info.device, fragment_shader);
    }
  }

  // Upload vertex data to vertex_buffer.
  {
    Im3d::Vec4 vertex_data[] = {
        Im3d::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        Im3d::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
        Im3d::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
        Im3d::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
    };
    uint32_t vertex_data_size = sizeof(vertex_data);

    {
      SDL_GPUBufferCreateInfo info = {};
      info.size                    = vertex_data_size;
      info.usage                   = SDL_GPU_BUFFERUSAGE_VERTEX;
      g_data.vertex_buffer         = SDL_CreateGPUBuffer(g_data.init_info.device, &info);
      if (g_data.vertex_buffer == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create vertex buffer: %s",
            SDL_GetError());
        return false;
      }
    }

    SDL_GPUTransferBuffer* transfer_buffer;
    {
      SDL_GPUTransferBufferCreateInfo info = {};
      info.size                            = vertex_data_size;
      info.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      transfer_buffer = SDL_CreateGPUTransferBuffer(g_data.init_info.device, &info);
      if (transfer_buffer == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create vertex transfer buffer: %s",
            SDL_GetError());
        return false;
      }
    }

    {
      void* mapped_data = SDL_MapGPUTransferBuffer(g_data.init_info.device, transfer_buffer, false);
      if (mapped_data == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to maptransfer buffer: %s",
            SDL_GetError());
        return false;
      }
      SDL_memcpy(mapped_data, vertex_data, vertex_data_size);
      SDL_UnmapGPUTransferBuffer(g_data.init_info.device, transfer_buffer);
    }

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(g_data.init_info.device);
    {
      SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);

      SDL_GPUTransferBufferLocation location = {};
      location.transfer_buffer               = transfer_buffer;

      SDL_GPUBufferRegion buffer_region = {};
      buffer_region.buffer              = g_data.vertex_buffer;
      buffer_region.size                = vertex_data_size;

      SDL_UploadToGPUBuffer(copy_pass, &location, &buffer_region, false);
      SDL_EndGPUCopyPass(copy_pass);
    }
    SDL_SubmitGPUCommandBuffer(command_buffer);

    SDL_WaitForGPUIdle(g_data.init_info.device);
    SDL_ReleaseGPUTransferBuffer(g_data.init_info.device, transfer_buffer);
  }

  return true;
}

void im3d_sdl3_gpu_shutdown() {
  SDL_ReleaseGPUGraphicsPipeline(g_data.init_info.device, g_data.pipeline_points);
  SDL_ReleaseGPUGraphicsPipeline(g_data.init_info.device, g_data.pipeline_lines);
  SDL_ReleaseGPUGraphicsPipeline(g_data.init_info.device, g_data.pipeline_triangles);

  SDL_ReleaseGPUBuffer(g_data.init_info.device, g_data.vertex_buffer);
  SDL_ReleaseGPUBuffer(g_data.init_info.device, g_data.data_buffer);
  SDL_ReleaseGPUTransferBuffer(g_data.init_info.device, g_data.transfer_buffer);
}

void im3d_sdl3_gpu_new_frame(const Im3d_SDL3_GPU_Frame_Info& info) {
  SDL_assert(g_data.init_info.device != nullptr);

  Im3d::AppData& app_data = Im3d::GetAppData();

  app_data.m_deltaTime     = info.delta_time;
  app_data.m_viewportSize  = info.viewport_size;
  app_data.m_viewOrigin    = info.view_position;
  app_data.m_viewDirection = info.view_direction;
  app_data.m_worldUp       = Im3d::Vec3(0.0f, 1.0f, 0.0f);
  app_data.m_projOrtho     = info.ortho;

  Im3d::Mat4 view_to_world = Im3d::Inverse(info.world_to_view_transform);

  if (info.ortho) {
    app_data.m_projScaleY = 2.0f / info.view_to_clip_transform(1, 1);
  } else {
    app_data.m_projScaleY = SDL_tanf(info.fov_rad * 0.5f) * 2.0f;
  }

  Im3d::Vec2 cursor_position = info.cursor_position;
  cursor_position.x          = (cursor_position.x / info.viewport_size.x) * 2.0f - 1.0f;
  cursor_position.y          = (cursor_position.y / info.viewport_size.y) * 2.0f - 1.0f;
  cursor_position.y          = -cursor_position.y;
  if (info.ortho) {
    app_data.m_cursorRayOrigin.x = cursor_position.x / info.view_to_clip_transform(0, 0);
    app_data.m_cursorRayOrigin.y = cursor_position.y / info.view_to_clip_transform(1, 1);
    app_data.m_cursorRayOrigin.z = 0.0f;
    app_data.m_cursorRayOrigin =
        view_to_world * Im3d::Vec4(Im3d::Normalize(app_data.m_cursorRayOrigin), 1.0f);
    app_data.m_cursorRayDirection = view_to_world * Im3d::Vec4(0.0f, 0.0f, -1.0f, 0.0f);
  } else {
    app_data.m_cursorRayOrigin      = app_data.m_viewOrigin;
    app_data.m_cursorRayDirection.x = cursor_position.x / info.view_to_clip_transform(0, 0);
    app_data.m_cursorRayDirection.y = cursor_position.y / info.view_to_clip_transform(1, 1);
    app_data.m_cursorRayDirection.z = -1.0f;
    app_data.m_cursorRayDirection =
        view_to_world * Im3d::Vec4(Im3d::Normalize(app_data.m_cursorRayDirection), 1.0f);
  }

  g_data.world_to_clip_transform = info.view_to_clip_transform * info.world_to_view_transform;
  app_data.setCullFrustum(g_data.world_to_clip_transform, false);
}

void im3d_sdl3_gpu_prepare_draw_data(SDL_GPUCommandBuffer* command_buffer) {
  SDL_assert(g_data.init_info.device != nullptr);
  SDL_assert(command_buffer != nullptr);

  g_data.total_vertex_count = 0;
  for (uint32_t i = 0; i < Im3d::GetDrawListCount(); i++) {
    g_data.total_vertex_count += Im3d::GetDrawLists()[i].m_vertexCount;
  }

  const Im3d::AppData& app_data = Im3d::GetAppData();
  if (app_data.m_viewportSize.x <= 0.0f || app_data.m_viewportSize.y <= 0.0f ||
      g_data.total_vertex_count == 0) {
    return;
  }

  uint32_t new_data_buffer_size = g_data.total_vertex_count * sizeof(Im3d::VertexData);
  if (g_data.data_buffer == nullptr || g_data.data_buffer_size < new_data_buffer_size) {
    SDL_WaitForGPUIdle(g_data.init_info.device);
    SDL_ReleaseGPUBuffer(g_data.init_info.device, g_data.data_buffer);
    SDL_ReleaseGPUTransferBuffer(g_data.init_info.device, g_data.transfer_buffer);
    {
      SDL_GPUBufferCreateInfo info = {};
      info.size                    = new_data_buffer_size;
      info.usage                   = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
      g_data.data_buffer           = SDL_CreateGPUBuffer(g_data.init_info.device, &info);
      if (g_data.data_buffer == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create data buffer: %s",
            SDL_GetError());
        SDL_assert(false);
        return;
      }
    }
    {
      SDL_GPUTransferBufferCreateInfo info = {};
      info.size                            = new_data_buffer_size;
      info.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      g_data.transfer_buffer = SDL_CreateGPUTransferBuffer(g_data.init_info.device, &info);
      if (g_data.transfer_buffer == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create transfer buffer: %s",
            SDL_GetError());
        SDL_assert(false);
        return;
      }
    }
    g_data.data_buffer_size = new_data_buffer_size;
  }

  {
    auto vertex_data_dst = static_cast<Im3d::VertexData*>(
        SDL_MapGPUTransferBuffer(g_data.init_info.device, g_data.transfer_buffer, true));
    if (vertex_data_dst == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to map transfer buffer: %s",
          SDL_GetError());
      SDL_assert(false);
      return;
    }
    for (uint32_t i = 0; i < Im3d::GetDrawListCount(); i++) {
      const Im3d::DrawList& draw_list = Im3d::GetDrawLists()[i];
      SDL_memcpy(
          vertex_data_dst,
          draw_list.m_vertexData,
          draw_list.m_vertexCount * sizeof(Im3d::VertexData));
      vertex_data_dst += draw_list.m_vertexCount;
    }
    SDL_UnmapGPUTransferBuffer(g_data.init_info.device, g_data.transfer_buffer);
  }

  {
    SDL_GPUTransferBufferLocation location = {};
    location.transfer_buffer               = g_data.transfer_buffer;

    SDL_GPUBufferRegion buffer_region = {};
    buffer_region.buffer              = g_data.data_buffer;
    buffer_region.size                = new_data_buffer_size;

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_UploadToGPUBuffer(copy_pass, &location, &buffer_region, true);
    SDL_EndGPUCopyPass(copy_pass);
  }
}

void im3d_sdl3_gpu_render_draw_data(
    SDL_GPUCommandBuffer* command_buffer,
    SDL_GPURenderPass*    render_pass) {
  SDL_assert(g_data.init_info.device != nullptr);
  SDL_assert(command_buffer != nullptr);
  SDL_assert(render_pass != nullptr);

  const Im3d::AppData& app_data = Im3d::GetAppData();
  if (app_data.m_viewportSize.x <= 0.0f || app_data.m_viewportSize.y <= 0.0f) { return; }

  {
    SDL_GPUBufferBinding binding = {};
    binding.buffer               = g_data.vertex_buffer;
    SDL_BindGPUVertexBuffers(render_pass, 0, &binding, 1);
  }

  if (g_data.total_vertex_count > 0) {
    SDL_BindGPUVertexStorageBuffers(render_pass, 0, &g_data.data_buffer, 1);
  }

  Vertex_Uniforms uniforms         = {};
  uniforms.world_to_clip_transform = g_data.world_to_clip_transform;
  uniforms.resolution              = app_data.m_viewportSize;

  {
    SDL_GPUViewport viewport = {};
    viewport.w               = app_data.m_viewportSize.x;
    viewport.h               = app_data.m_viewportSize.y;
    viewport.min_depth       = 0.0f;
    viewport.max_depth       = 1.0f;
    SDL_SetGPUViewport(render_pass, &viewport);
  }

  uint32_t instance_offset = 0;
  for (uint32_t i = 0; i < Im3d::GetDrawListCount(); i++) {
    const Im3d::DrawList& draw_list = Im3d::GetDrawLists()[i];

    SDL_GPUGraphicsPipeline* prim_pipeline;
    uint32_t                 num_vertices;
    uint32_t                 num_instances;
    switch (draw_list.m_primType) {
    case Im3d::DrawPrimitive_Points:
      prim_pipeline = g_data.pipeline_points;
      num_vertices  = 4;
      num_instances = draw_list.m_vertexCount;
      break;
    case Im3d::DrawPrimitive_Lines:
      prim_pipeline = g_data.pipeline_lines;
      num_vertices  = 4;
      num_instances = draw_list.m_vertexCount / 2;
      break;
    case Im3d::DrawPrimitive_Triangles:
      prim_pipeline = g_data.pipeline_triangles;
      num_vertices  = 3;
      num_instances = draw_list.m_vertexCount / 3;
      break;
    default:
      SDL_assert(false);
      return;
    }

    uniforms.instance_offset = instance_offset;
    SDL_PushGPUVertexUniformData(command_buffer, 0, &uniforms, sizeof(uniforms));

    SDL_BindGPUGraphicsPipeline(render_pass, prim_pipeline);
    SDL_DrawGPUPrimitives(render_pass, num_vertices, num_instances, 0, 0);

    instance_offset += draw_list.m_vertexCount;
  }
}
