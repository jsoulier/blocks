#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stb_image.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "block.h"
#include "camera.h"
#include "database.h"
#include "noise.h"
#include "physics.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUCommandBuffer* commands;
static uint32_t width;
static uint32_t height;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUGraphicsPipeline* raycast_pipeline;
static SDL_GPUGraphicsPipeline* sky_pipeline;
static SDL_GPUGraphicsPipeline* ui_pipeline;
static SDL_GPUBuffer* quad_vbo;
static SDL_GPUBuffer* cube_vbo;
static SDL_GPUGraphicsPipeline* opaque_pipeline;
static SDL_GPUGraphicsPipeline* transparent_pipeline;
static SDL_Surface* atlas_surface;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUSampler* atlas_sampler;
static void* atlas_data;
static SDL_GPUGraphicsPipeline* shadow_pipeline;
static SDL_GPUTexture* shadow_texture;
static SDL_GPUSampler* shadow_sampler;
static camera_t player_camera;
static camera_t shadow_camera;
static uint64_t time1;
static uint64_t time2;
static block_t current_block = BLOCK_GRASS;

static SDL_GPUShader* load_shader(
    SDL_GPUDevice* device,
    const char* file,
    const int uniforms,
    const int samplers)
{
    assert(device);
    assert(file);
    SDL_GPUShaderCreateInfo info = {0};
    void* code = SDL_LoadFile(file, &info.code_size);
    if (!code)
    {
        SDL_Log("Failed to load %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    info.code = code;
    if (strstr(file, ".vert"))
    {
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else
    {
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.entrypoint = "main";
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader)
    {
        SDL_Log("Failed to create %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    return shader;
}

static void load_raycast_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load_shader(device, "raycast.vert", 2, 0),
        .fragment_shader = load_shader(device, "raycast.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state =
                {
                    .enable_blend = 1,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        raycast_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!raycast_pipeline)
    {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_sky_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load_shader(device, "sky.vert", 2, 0),
        .fragment_shader = load_shader(device, "sky.frag", 0, 0),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window)
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!sky_pipeline)
    {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_ui_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load_shader(device, "ui.vert", 0, 0),
        .fragment_shader = load_shader(device, "ui.frag", 2, 1),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state =
                {
                    .enable_blend = 1,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 8,
            }},
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        ui_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!ui_pipeline)
    {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_opaque_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load_shader(device, "world.vert", 4, 0),
        .fragment_shader = load_shader(device, "world.frag", 1, 2),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        opaque_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!opaque_pipeline)
    {
        SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_transparent_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load_shader(device, "world.vert", 4, 0),
        .fragment_shader = load_shader(device, "world.frag", 1, 2),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state =
                {
                    .enable_blend = 1,
                    .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                    .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                    .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                    .color_blend_op = SDL_GPU_BLENDOP_ADD,
                    .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                },
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 0,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        transparent_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!transparent_pipeline)
    {
        SDL_Log("Failed to create transparent pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_shadow_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = load_shader(device, "shadow.vert", 2, 0),
        .fragment_shader = load_shader(device, "shadow.frag", 0, 0),
        .target_info =
        {
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state =
        {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[])
            {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[])
            {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state =
        {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    if (!shadow_pipeline)
    {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_atlas()
{
    int w;
    int h;
    int channels;
    atlas_data = stbi_load("atlas.png", &w, &h, &channels, 4);
    if (!atlas_data || channels != 4)
    {
        SDL_Log("Failed to create atlas image: %s", stbi_failure_reason());
        return;
    }
    atlas_surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, atlas_data, w * 4);
    if (!atlas_surface)
    {
        SDL_Log("Failed to create atlas surface: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureCreateInfo tci = {0};
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = w;
    tci.height = h;
    atlas_texture = SDL_CreateGPUTexture(device, &tci);
    if (!atlas_texture)
    {
        SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.size = w * h * 4;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!buffer)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, atlas_surface->pixels, w * h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureTransferInfo tti = {0};
    tti.transfer_buffer = buffer;
    SDL_GPUTextureRegion region = {0};
    region.texture = atlas_texture;
    region.w = w;
    region.h = h;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &tti, &region, 0);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
}

static void create_samplers()
{
    SDL_GPUSamplerCreateInfo sci = {0};
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    atlas_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!atlas_sampler)
    {
        SDL_Log("Failed to create atlas sampler: %s", SDL_GetError());
        return;
    }
    shadow_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!shadow_sampler)
    {
        SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
        return;
    }
}

static void create_textures()
{
    SDL_GPUTextureCreateInfo tci = {0};
    tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    tci.width = SHADOW_SIZE;
    tci.height = SHADOW_SIZE;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    shadow_texture = SDL_CreateGPUTexture(device, &tci);
    if (!shadow_texture)
    {
        SDL_Log("Failed to create shadow texture: %s", SDL_GetError());
        return;
    }
}

SDL_Surface* create_icon(const block_t block)
{
    assert(block < BLOCK_COUNT);
    if (!atlas_surface)
    {
        return NULL;
    }
    SDL_Surface* icon = SDL_CreateSurface(ATLAS_FACE_WIDTH,
        ATLAS_FACE_HEIGHT, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
    {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return NULL;
    }
    SDL_Rect src;
    src.x = blocks[block][0][0] * ATLAS_FACE_WIDTH;
    src.y = blocks[block][0][1] * ATLAS_FACE_HEIGHT;
    src.w = ATLAS_FACE_WIDTH;
    src.h = ATLAS_FACE_HEIGHT;
    SDL_Rect dst;
    dst.x = 0;
    dst.y = 0;
    dst.w = ATLAS_FACE_WIDTH;
    dst.h = ATLAS_FACE_HEIGHT;
    if (!SDL_BlitSurface(atlas_surface, &src, icon, &dst))
    {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return NULL;
    }
    return icon;
}

static void create_vbos()
{
    const float quad[][2] =
    {
        {-1, -1 },
        { 1, -1 },
        {-1,  1 },
        { 1,  1 },
        { 1, -1 },
        {-1,  1 },
    };
    const float cube[][3] =
    {
        {-1, -1, -1 }, { 1, -1, -1 }, { 1,  1, -1 },
        {-1, -1, -1 }, { 1,  1, -1 }, {-1,  1, -1 },
        { 1, -1,  1 }, { 1,  1,  1 }, {-1,  1,  1 },
        { 1, -1,  1 }, {-1,  1,  1 }, {-1, -1,  1 },
        {-1, -1, -1 }, {-1,  1, -1 }, {-1,  1,  1 },
        {-1, -1, -1 }, {-1,  1,  1 }, {-1, -1,  1 },
        { 1, -1, -1 }, { 1, -1,  1 }, { 1,  1,  1 },
        { 1, -1, -1 }, { 1,  1,  1 }, { 1,  1, -1 },
        {-1,  1, -1 }, { 1,  1, -1 }, { 1,  1,  1 },
        {-1,  1, -1 }, { 1,  1,  1 }, {-1,  1,  1 },
        {-1, -1, -1 }, {-1, -1,  1 }, { 1, -1,  1 },
        {-1, -1, -1 }, { 1, -1,  1 }, { 1, -1, -1 },
    };
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = sizeof(quad);
    quad_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!quad_vbo)
    {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    bci.size = sizeof(cube);
    cube_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!cube_vbo)
    {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    void* data;
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(quad);
    SDL_GPUTransferBuffer* qtbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!qtbo)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    data = SDL_MapGPUTransferBuffer(device, qtbo, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, quad, sizeof(quad));
    tbci.size = sizeof(cube);
    SDL_GPUTransferBuffer* ctbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!ctbo)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    data = SDL_MapGPUTransferBuffer(device, ctbo, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, cube, sizeof(cube));
    SDL_UnmapGPUTransferBuffer(device, ctbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return;
    } 
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = qtbo;
    region.size = sizeof(quad);
    region.buffer = quad_vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    location.transfer_buffer = ctbo;
    region.size = sizeof(cube);
    region.buffer = cube_vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands); 
    SDL_ReleaseGPUTransferBuffer(device, qtbo);
    SDL_ReleaseGPUTransferBuffer(device, ctbo);
}

static void draw_raycast()
{
    float x;
    float y;
    float z;
    float a;
    float b;
    float c;
    camera_get_position(&player_camera, &x, &y, &z);
    camera_vector(&player_camera, &a, &b, &c);
    if (!physics_raycast(&x, &y, &z, a, b, c, RAYCAST_LENGTH, false))
    {
        return;
    }
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.load_op = SDL_GPU_LOADOP_LOAD;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t position[3] = { x, y, z };
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    SDL_BindGPUGraphicsPipeline(pass, raycast_pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 1, position, 12);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_sky()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_CLEAR;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    SDL_PushGPUVertexUniformData(commands, 0, player_camera.view, 64);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.proj, 64);
    SDL_BindGPUGraphicsPipeline(pass, sky_pipeline);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_ui()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t viewport[2] = { player_camera.width, player_camera.height };
    int32_t block[2] = { blocks[current_block][0][0], blocks[current_block][0][1] };
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = quad_vbo;
    SDL_BindGPUGraphicsPipeline(pass, ui_pipeline);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    SDL_PushGPUFragmentUniformData(commands, 0, &viewport, 8);
    SDL_PushGPUFragmentUniformData(commands, 1, &block, 8);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_opaque()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1.0f;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    float vector[3];
    camera_get_position(&player_camera, &position[0], &position[1], &position[2]);
    camera_vector(&shadow_camera, &vector[0], &vector[1], &vector[2]);
    SDL_BindGPUGraphicsPipeline(pass, opaque_pipeline);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 2, position, 12);
    SDL_PushGPUVertexUniformData(commands, 3, shadow_camera.matrix, 64);
    SDL_PushGPUFragmentUniformData(commands, 0, vector, 12);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    if (shadow_sampler && shadow_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = shadow_sampler;
        tsb.texture = shadow_texture;
        SDL_BindGPUFragmentSamplers(pass, 1, &tsb, 1);
    }
    world_render(&player_camera, commands, pass, true);
    SDL_EndGPURenderPass(pass);
}

static void draw_transparent()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.load_op = SDL_GPU_LOADOP_LOAD;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    float vector[3];
    camera_get_position(&player_camera, &position[0], &position[1], &position[2]);
    camera_vector(&shadow_camera, &vector[0], &vector[1], &vector[2]);
    SDL_BindGPUGraphicsPipeline(pass, transparent_pipeline);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 2, position, 12);
    SDL_PushGPUVertexUniformData(commands, 3, shadow_camera.matrix, 64);
    SDL_PushGPUFragmentUniformData(commands, 0, vector, 12);
    if (atlas_sampler && atlas_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = atlas_sampler;
        tsb.texture = atlas_texture;
        SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    }
    if (shadow_sampler && shadow_texture)
    {
        SDL_GPUTextureSamplerBinding tsb = {0};
        tsb.sampler = shadow_sampler;
        tsb.texture = shadow_texture;
        SDL_BindGPUFragmentSamplers(pass, 1, &tsb, 1);
    }
    world_render(&player_camera, commands, pass, false);
    SDL_EndGPURenderPass(pass);
}

static void draw_shadow()
{
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1.0f;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.texture = shadow_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, NULL, 0, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, shadow_pipeline);
    SDL_PushGPUVertexUniformData(commands, 1, shadow_camera.matrix, 64);
    world_render(NULL, commands, pass, true);
    SDL_EndGPURenderPass(pass);
}

static void draw()
{
    commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    uint32_t w;
    uint32_t h;
    if (!SDL_AcquireGPUSwapchainTexture(commands, window, &color_texture, &w, &h))
    {
        SDL_Log("Failed to aqcuire swapchain image: %s", SDL_GetError());
        return;
    }
    if (w == 0 || h == 0)
    {
        SDL_SubmitGPUCommandBuffer(commands);
        return;
    }
    if (width != w || height != h)
    {
        if (depth_texture)
        {
            SDL_ReleaseGPUTexture(device, depth_texture);
            depth_texture = NULL;
        }
        SDL_GPUTextureCreateInfo tci = {0};
        tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        tci.width = w;
        tci.height = h;
        tci.layer_count_or_depth = 1;
        tci.num_levels = 1;
        depth_texture = SDL_CreateGPUTexture(device, &tci);
        if (!depth_texture)
        {
            SDL_Log("Failed to create depth texture: %s", SDL_GetError());
            return;
        }
        width = w;
        height = h;
    }
    camera_update(&player_camera);
    if (shadow_pipeline)
    {
        draw_shadow();
    }
    if (sky_pipeline && cube_vbo)
    {
        draw_sky();
    }
    if (opaque_pipeline)
    {
        draw_opaque();
    }
    if (transparent_pipeline)
    {
        draw_transparent();
    }
    if (raycast_pipeline && cube_vbo)
    {
        draw_raycast();
    }
    if (ui_pipeline && quad_vbo)
    {
        draw_ui();
    }
    SDL_SubmitGPUCommandBuffer(commands);
}

static bool poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_RESIZED:
            camera_viewport(&player_camera, event.window.data1, event.window.data2);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (SDL_GetWindowRelativeMouseMode(window))
            {
                const float yaw = event.motion.xrel * PLAYER_SENSITIVITY;
                const float pitch = -event.motion.yrel * PLAYER_SENSITIVITY;
                camera_rotate(&player_camera, pitch, yaw);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!SDL_GetWindowRelativeMouseMode(window))
            {
                SDL_SetWindowRelativeMouseMode(window, true);
                break;
            }
            if (event.button.button == BUTTON_PLACE ||
                event.button.button == BUTTON_BREAK)
            {
                bool previous = true;
                block_t block = current_block;
                if (event.button.button == BUTTON_BREAK)
                {
                    previous = false;
                    block = BLOCK_EMPTY;
                }
                float x, y, z;
                float a, b, c;
                camera_get_position(&player_camera, &x, &y, &z);
                camera_vector(&player_camera, &a, &b, &c);
                if (physics_raycast(&x, &y, &z, a, b, c, RAYCAST_LENGTH,
                    previous) && y >= 1.0f)
                {
                    world_set_block(x, y, z, block);
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == BUTTON_PAUSE)
            {
                SDL_SetWindowRelativeMouseMode(window, false);
            }
            else if (event.key.scancode == BUTTON_BLOCK)
            {
                current_block = (current_block + 1) % BLOCK_COUNT;
                current_block = clamp(current_block, BLOCK_EMPTY + 1, BLOCK_COUNT - 1);
            }
            else if (event.key.scancode == BUTTON_FULLSCREEN)
            {
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
                {
                    SDL_SetWindowFullscreen(window, false);
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
                else
                {
                    SDL_SetWindowFullscreen(window, true);
                    SDL_SetWindowRelativeMouseMode(window, true);
                }
            }
            break;
        }
    }
    return true;
}

static void move(const float dt)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float speed = PLAYER_SPEED;
    const bool* state = SDL_GetKeyboardState(NULL);
    if (state[BUTTON_RIGHT])
    {
        x++;
    }
    if (state[BUTTON_LEFT])
    {
        x--;
    }
    if (state[BUTTON_UP])
    {
        y++;
    }
    if (state[BUTTON_DOWN])
    {
        y--;
    }
    if (state[BUTTON_FORWARD])
    {
        z++;
    }
    if (state[BUTTON_BACKWARD])
    {
        z--;
    }
    if (state[BUTTON_SPRINT])
    {
        speed = PLAYER_SUPER_SPEED;
    }
    x *= speed * dt;
    y *= speed * dt;
    z *= speed * dt;
    camera_move(&player_camera, x, y, z);
    camera_get_position(&player_camera, &x, &y, &z);
    int a = x;
    int c = z;
    a /= CHUNK_X;
    c /= CHUNK_Z;
    a *= CHUNK_X;
    c *= CHUNK_Z;
    camera_set_position(&shadow_camera, a, SHADOW_Y, c);
    camera_update(&shadow_camera);
}

static void commit()
{
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    camera_get_position(&player_camera, &x, &y, &z);
    camera_get_rotation(&player_camera, &pitch, &yaw);
    database_set_player(DATABASE_PLAYER, x, y, z, pitch, yaw);
    database_commit();
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    SDL_SetAppMetadata(APP_NAME, APP_VERSION, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, APP_VALIDATION, NULL);
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to create swapchain: %s", SDL_GetError());
        return false;
    }
    load_atlas();
    load_shadow_pipeline();
    load_raycast_pipeline();
    load_sky_pipeline();
    load_ui_pipeline();
    load_opaque_pipeline();
    load_transparent_pipeline();
    create_samplers();
    create_textures();
    create_vbos();
    SDL_SetWindowResizable(window, true);
    SDL_Surface* icon = create_icon(APP_ICON);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
    if (!database_init(DATABASE_PATH))
    {
        SDL_Log("Failed to create database");
        return false;
    }
    if (!world_init(device))
    {
        SDL_Log("Failed to create world");
        return EXIT_FAILURE;
    }
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    camera_init(&player_camera, false);
    camera_viewport(&player_camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (database_get_player(DATABASE_PLAYER, &x, &y, &z, &pitch, &yaw))
    {
        camera_set_position(&player_camera, x, y, z);
        camera_set_rotation(&player_camera, pitch, yaw);
    }
    else
    {
        srand(time(NULL));
        camera_set_position(&player_camera, rand(), PLAYER_Y, rand());
    }
    camera_init(&shadow_camera, true);
    camera_set_rotation(&shadow_camera, SHADOW_PITCH, SHADOW_YAW);
    move(0.0f);
    int cooldown = 0;
    time1 = SDL_GetPerformanceCounter();
    time2 = 0;
    while (1)
    {
        time2 = time1;
        time1 = SDL_GetPerformanceCounter();
        const float frequency = SDL_GetPerformanceFrequency();
        const float dt = (time1 - time2) * 1000 / frequency;
        if (!poll())
        {
            break;
        }
        move(dt);
        camera_get_position(&player_camera, &x, &y, &z);
        world_update(x, y, z);
        draw();
        if (cooldown++ > DATABASE_TIME)
        {
            commit();
            cooldown = 0;
        }
    }
    world_free();
    commit();
    database_free();
    if (cube_vbo)
    {
        SDL_ReleaseGPUBuffer(device, cube_vbo);
    }
    if (quad_vbo)
    {
        SDL_ReleaseGPUBuffer(device, quad_vbo);
    }
    if (depth_texture)
    {
        SDL_ReleaseGPUTexture(device, depth_texture);
    }
    if (raycast_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, raycast_pipeline);
    }
    if (sky_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, sky_pipeline);
    }
    if (ui_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, ui_pipeline);
    }
    if (opaque_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, opaque_pipeline);
    }
    if (transparent_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, transparent_pipeline);
    }
    if (atlas_surface)
    {
        SDL_DestroySurface(atlas_surface);
    }
    if (atlas_texture)
    {
        SDL_ReleaseGPUTexture(device, atlas_texture);
    }
    if (atlas_sampler)
    {
        SDL_ReleaseGPUSampler(device, atlas_sampler);
    }
    if (shadow_pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(device, shadow_pipeline);
    }
    if (shadow_texture)
    {
        SDL_ReleaseGPUTexture(device, shadow_texture);
    }
    if (shadow_sampler)
    {
        SDL_ReleaseGPUSampler(device, shadow_sampler);
    }
    if (device && window)
    {
        SDL_ReleaseWindowFromGPUDevice(device, window);
    }
    if (device)
    {
        SDL_DestroyGPUDevice(device);
    }
    if (window)
    {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    stbi_image_free(atlas_data);
    return EXIT_SUCCESS;
}