#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stb_image.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "block.h"
#include "camera.h"
#include "database.h"
#include "noise.h"
#include "physics.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUGraphicsPipeline* raycast_pipeline;
static SDL_GPUGraphicsPipeline* sky_pipeline;
static SDL_GPUGraphicsPipeline* ui_pipeline;
static SDL_GPUGraphicsPipeline* opaque_pipeline;
static SDL_GPUGraphicsPipeline* transp_pipeline;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUSampler* atlas_sampler;
static SDL_Surface* atlas_surface;
static void* atlas_data;
static SDL_GPUBuffer* quad_vbo;
static SDL_GPUBuffer* cube_vbo;
static uint32_t width;
static uint32_t height;
static camera_t camera;
static block_t hand = BLOCK_GRASS;

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
    if (!code) {
        SDL_Log("Failed to load %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    info.code = code;
    if (strstr(file, ".vert")) {
        info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    } else {
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.entrypoint = "main";
    info.num_uniform_buffers = uniforms;
    info.num_samplers = samplers;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    if (!shader) {
        SDL_Log("Failed to create %s shader: %s", file, SDL_GetError());
        return NULL;
    }
    return shader;
}

static void load_atlas()
{
    int w, h, channels;
    atlas_data = stbi_load("atlas.png", &w, &h, &channels, 4);
    if (!atlas_data || channels != 4) {
        SDL_Log("Failed to create atlas image: %s", stbi_failure_reason());
        return;
    }
    atlas_surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, atlas_data, w * 4);
    if (!atlas_surface) {
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
    if (!atlas_texture) {
        SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
        return;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.size = w * h * 4;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!buffer) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, atlas_surface->pixels, w * h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
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
    SDL_GPUSamplerCreateInfo sci = {0};
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    atlas_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!atlas_sampler) {
        SDL_Log("Failed to create atlas sampler: %s", SDL_GetError());
        return;
    }
}

SDL_Surface* get_atlas_icon(const block_t block)
{
    if (!atlas_surface) {
        return NULL;
    }
    const int w = 16;
    const int h = 16;
    SDL_Surface* icon = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!icon) {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return NULL;
    }
    SDL_Rect src;
    src.x = blocks[block][0][0] * w;
    src.y = blocks[block][0][1] * h;
    src.w = w;
    src.h = h;
    SDL_Rect dst;
    dst.x = 0;
    dst.y = 0;
    dst.w = w;
    dst.h = h;
    if (!SDL_BlitSurface(atlas_surface, &src, icon, &dst)) {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return NULL;
    }
    return icon;
}

static void load_raycast_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader(device, "raycast.vert", 2, 0),
        .fragment_shader = load_shader(device, "raycast.frag", 0, 0),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state = {
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
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
        .depth_stencil_state = {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
    };
    raycast_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!raycast_pipeline) {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_sky_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader(device, "sky.vert", 2, 0),
        .fragment_shader = load_shader(device, "sky.frag", 0, 0),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window)
            }},
        },
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 12,
            }},
        },
    };
    sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!sky_pipeline) {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_ui_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader(device, "ui.vert", 0, 0),
        .fragment_shader = load_shader(device, "ui.frag", 2, 1),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
            }},
        },
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 8,
            }},
        },
    };
    ui_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!ui_pipeline) {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_opaque_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader(device, "world.vert", 3, 0),
        .fragment_shader = load_shader(device, "world.frag", 0, 1),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
            }},
            .has_depth_stencil_target = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        },
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state = {
            .enable_depth_test = 1,
            .enable_depth_write = 1,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state = {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    opaque_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!opaque_pipeline) {
        SDL_Log("Failed to create world pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void load_transp_pipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = load_shader(device, "world.vert", 3, 0),
        .fragment_shader = load_shader(device, "world.frag", 0, 1),
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat(device, window),
                .blend_state = {
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
        .vertex_input_state = {
            .num_vertex_attributes = 1,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {{
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            }},
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {{
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .pitch = 4,
            }},
        },
        .depth_stencil_state = {
            .enable_depth_test = 1,
            .enable_depth_write = 0,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state = {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
            .fill_mode = SDL_GPU_FILLMODE_FILL,
        },
    };
    transp_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (!transp_pipeline) {
        SDL_Log("Failed to create world pipeline: %s", SDL_GetError());
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
}

static void draw_raycast(SDL_GPUCommandBuffer* commands)
{
    float x, y, z;
    float a, b, c;
    camera_get_position(&camera, &x, &y, &z);
    camera_vector(&camera, &a, &b, &c);
    if (!physics_raycast(&x, &y, &z, a, b, c, RAYCAST_LENGTH, true)) {
        return;
    }
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.texture = depth_texture;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t raycast[3] = { x, y, z };
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    SDL_BindGPUGraphicsPipeline(pass, raycast_pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 1, raycast, sizeof(raycast));
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_sky(SDL_GPUCommandBuffer* commands)
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_DONT_CARE;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    SDL_PushGPUVertexUniformData(commands, 0, camera.view, 64);
    SDL_PushGPUVertexUniformData(commands, 1, camera.proj, 64);
    SDL_BindGPUGraphicsPipeline(pass, sky_pipeline);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_ui(SDL_GPUCommandBuffer* commands)
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    int32_t viewport[2] = { camera.width, camera.height };
    int32_t block[2] = { blocks[hand][0][0], blocks[hand][0][1] };
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = quad_vbo;
    SDL_GPUTextureSamplerBinding tsb = {0};
    tsb.sampler = atlas_sampler;
    tsb.texture = atlas_texture;
    SDL_BindGPUGraphicsPipeline(pass, ui_pipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    SDL_PushGPUFragmentUniformData(commands, 0, &viewport, sizeof(viewport));
    SDL_PushGPUFragmentUniformData(commands, 1, &block, sizeof(block));
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_opaque(SDL_GPUCommandBuffer* commands)
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.texture = depth_texture;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    camera_get_position(&camera, &position[0], &position[1], &position[2]);
    SDL_GPUTextureSamplerBinding tsb = {0};
    tsb.sampler = atlas_sampler;
    tsb.texture = atlas_texture;
    SDL_BindGPUGraphicsPipeline(pass, opaque_pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 1, position, sizeof(position));
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    world_render_opaque(&camera, commands, pass);
    SDL_EndGPURenderPass(pass);
}

static void draw_transp(SDL_GPUCommandBuffer* commands)
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = color_texture;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.load_op = SDL_GPU_LOADOP_LOAD;
    dsti.texture = depth_texture;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, &dsti);
    if (!pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    camera_get_position(&camera, &position[0], &position[1], &position[2]);
    SDL_GPUTextureSamplerBinding tsb = {0};
    tsb.sampler = atlas_sampler;
    tsb.texture = atlas_texture;
    SDL_BindGPUGraphicsPipeline(pass, transp_pipeline);
    SDL_PushGPUVertexUniformData(commands, 0, camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 1, position, sizeof(position));
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    world_render_transp(&camera, commands, pass);
    SDL_EndGPURenderPass(pass);
}

static void draw()
{
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    uint32_t w;
    uint32_t h;
    if (!SDL_AcquireGPUSwapchainTexture(commands, window, &color_texture, &w, &h)) {
        SDL_Log("Failed to aqcuire swapchain image: %s", SDL_GetError());
        return;
    }
    if (width != w || height != h) {
        if (depth_texture) {
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
        if (!depth_texture) {
            SDL_Log("Failed to create depth texture: %s", SDL_GetError());
            return;
        }
        width = w;
        height = h;
    }
    camera_update(&camera);
    draw_sky(commands);
    draw_opaque(commands);
    draw_transp(commands);
    draw_raycast(commands);
    draw_ui(commands);
    SDL_SubmitGPUCommandBuffer(commands);
}

static void create_vbos()
{
    const float quad[][2] = {
        {-1,-1},
        { 1,-1},
        {-1, 1},
        { 1, 1},
        { 1,-1},
        {-1, 1},
    };
    const float cube[][3] = {
        {-1,-1,-1},{ 1,-1,-1},{ 1, 1,-1},
        {-1,-1,-1},{ 1, 1,-1},{-1, 1,-1},
        { 1,-1, 1},{ 1, 1, 1},{-1, 1, 1},
        { 1,-1, 1},{-1, 1, 1},{-1,-1, 1},
        {-1,-1,-1},{-1, 1,-1},{-1, 1, 1},
        {-1,-1,-1},{-1, 1, 1},{-1,-1, 1},
        { 1,-1,-1},{ 1,-1, 1},{ 1, 1, 1},
        { 1,-1,-1},{ 1, 1, 1},{ 1, 1,-1},
        {-1, 1,-1},{ 1, 1,-1},{ 1, 1, 1},
        {-1, 1,-1},{ 1, 1, 1},{-1, 1, 1},
        {-1,-1,-1},{-1,-1, 1},{ 1,-1, 1},
        {-1,-1,-1},{ 1,-1, 1},{ 1,-1,-1},
    };
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = sizeof(quad);
    quad_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!quad_vbo) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    bci.size = sizeof(cube);
    cube_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!cube_vbo) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return;
    }
    void* data;
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(quad);
    SDL_GPUTransferBuffer* qtbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!qtbo) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    data = SDL_MapGPUTransferBuffer(device, qtbo, 0);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, quad, sizeof(quad));
    tbci.size = sizeof(cube);
    SDL_GPUTransferBuffer* ctbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!ctbo) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return;
    }
    data = SDL_MapGPUTransferBuffer(device, ctbo, 0);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return;
    }
    memcpy(data, cube, sizeof(cube));
    SDL_UnmapGPUTransferBuffer(device, ctbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
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

static bool poll()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_RESIZED:
            camera_viewport(&camera, event.window.data1, event.window.data2);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (SDL_GetWindowRelativeMouseMode(window)) {
                float sensitivity = 0.1f;
                float yaw = event.motion.xrel * sensitivity;
                float pitch = -event.motion.yrel * sensitivity;
                camera_rotate(&camera, pitch, yaw);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!SDL_GetWindowRelativeMouseMode(window)) {
                SDL_SetWindowRelativeMouseMode(window, true);
            } else {
                if (event.button.button == SDL_BUTTON_LEFT ||
                    event.button.button == SDL_BUTTON_RIGHT) {
                    bool previous = true;
                    block_t block = hand;
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        previous = false;
                        block = BLOCK_EMPTY;
                    }
                    float x, y, z;
                    float a, b, c;
                    camera_get_position(&camera, &x, &y, &z);
                    camera_vector(&camera, &a, &b, &c);
                    if (physics_raycast(&x, &y, &z, a, b, c, RAYCAST_LENGTH, previous) && y >= 1.0f) {
                        world_set_block(x, y, z, block);
                    }
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                SDL_SetWindowRelativeMouseMode(window, false);
                SDL_SetWindowFullscreen(window, false);
            } else if (event.key.scancode == SDL_SCANCODE_B) {
                hand = (hand + 1) % BLOCK_COUNT;
                hand = clamp(hand, BLOCK_EMPTY + 1, BLOCK_COUNT - 1);
                SDL_Surface* icon = get_atlas_icon(hand);
                SDL_SetWindowIcon(window, icon);
                SDL_DestroySurface(icon);
            } else if (event.key.scancode == SDL_SCANCODE_F11) {
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
                    SDL_SetWindowFullscreen(window, false);
                } else {
                    SDL_SetWindowFullscreen(window, true);
                    SDL_SetWindowRelativeMouseMode(window, true);
                }
            }
            break;
        }
    }
    return true;
}

static void move()
{
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float speed = 0.2f;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_D]) dx++;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]) dx--;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_E]) dy++;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_Q]) dy--;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_W]) dz++;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_S]) dz--;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL]) speed = 1.0f;
    if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_Z]) speed = 100.0f;
    dx *= speed;
    dy *= speed;
    dz *= speed;
    camera_move(&camera, dx, dy, dz);
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    SDL_SetAppMetadata("blocks", NULL, NULL);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    window = SDL_CreateWindow("blocks", 1024, 764, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, 1, NULL);
    if (!device) {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Failed to create swapchain: %s", SDL_GetError());
        return false;
    }
    load_atlas();
    load_raycast_pipeline();
    load_sky_pipeline();
    load_ui_pipeline();
    load_opaque_pipeline();
    load_transp_pipeline();
    create_vbos();
    database_init(DATABASE_PATH);

    {
        noise_type_t type;
        int seed;
        database_get_noise(&type, &seed);
        noise_init(type, seed);
        noise_data(&type, &seed);
        database_set_noise(type, seed);
    }

    if (!world_init(device)) {
        return EXIT_FAILURE;
    }
    SDL_SetWindowResizable(window, true);
    SDL_Surface* icon = get_atlas_icon(hand);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
    camera_init(&camera);
    camera_move(&camera, 0, 30, 0);
    camera_viewport(&camera, 1024, 764);

    float x, y, z;
    float pitch, yaw;

    float a, b, c, d, e;
    if (database_get_player(0, &x, &y, &z, &pitch, &yaw)) {
        camera_set_position(&camera, x, y, z);
        camera_set_rotation(&camera, pitch, yaw);
    }

    int cooldown = 0;
    int time = 100;

    int loop = 1;
    while (loop) {
        if (!poll()) {
            break;
        }
        move();
        camera_get_position(&camera, &x, &y, &z);
        world_update(x, y, z);
        draw();

        if (cooldown++ > time) {
            camera_get_position(&camera, &x, &y, &z);
            camera_get_rotation(&camera, &pitch, &yaw);
            database_set_player(0, x, y, z, pitch, yaw);
            database_commit();
            cooldown = 0;
        }
    }
    world_free();
    database_free();
    SDL_DestroySurface(atlas_surface);
    stbi_image_free(atlas_data);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_ReleaseGPUSampler(device, atlas_sampler);
    SDL_ReleaseGPUGraphicsPipeline(device, raycast_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, opaque_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, transp_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, ui_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, sky_pipeline);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUBuffer(device, cube_vbo);
    SDL_ReleaseGPUBuffer(device, quad_vbo);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}