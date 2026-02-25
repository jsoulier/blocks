#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "camera.h"
#include "check.h"
#include "player.h"
#include "save.h"
#include "shader.h"
#include "world.h"

static const char* SAVE_PATH = "blocks.sqlite3";
static const int PLAYER_ID = 0;
static const float ATLAS_WIDTH = 512.0f;
static const int ATLAS_MIP_LEVELS = 4;
static const float BLOCK_WIDTH = 16.0f;
static const float PLAYER_SENSITIVITY = 0.1f;
static const float PLAYER_REACH = 10.0f;
static const int SHADOW_RESOLUTION = 4096.0f;
static const float SHADOW_Y = 30.0f;
static const float SHADOW_ORTHO = 300.0f;
static const float SHADOW_FAR = 300.0f;
static const float SHADOW_PITCH = -45.0f;
static const float SHADOW_YAW = 45.0f;

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTextureFormat color_format;
static SDL_GPUTextureFormat depth_format;
static SDL_GPUGraphicsPipeline* opaque_pipeline;
static SDL_GPUGraphicsPipeline* transparent_pipeline;
static SDL_GPUGraphicsPipeline* depth_pipeline;
static SDL_GPUGraphicsPipeline* shadow_pipeline;
static SDL_GPUGraphicsPipeline* sky_pipeline;
static SDL_GPUGraphicsPipeline* raycast_pipeline;
static SDL_GPUComputePipeline* ui_pipeline;
static SDL_GPUComputePipeline* ssao_pipeline;
static SDL_GPUComputePipeline* composite_pipeline;
static SDL_GPUComputePipeline* blur_pipeline;
static SDL_Surface* atlas_surface;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* position_texture;
static SDL_GPUTexture* light_texture;
static SDL_GPUTexture* voxel_texture;
static SDL_GPUTexture* ssao_texture;
static SDL_GPUTexture* blur_texture;
static SDL_GPUTexture* composite_texture;
static SDL_GPUTexture* shadow_texture;
static SDL_GPUSampler* linear_sampler;
static SDL_GPUSampler* nearest_sampler;
static camera_t shadow_camera;
static player_t player;
static world_query_t player_query;
static Uint64 ticks1;
static Uint64 ticks2;

static void update_shadow_camera()
{
    camera_init(&shadow_camera, CAMERA_TYPE_ORTHO);
    shadow_camera.ortho = SHADOW_ORTHO;
    shadow_camera.far = SHADOW_FAR;
    shadow_camera.x = SDL_floor(player.camera.x / CHUNK_WIDTH) * CHUNK_WIDTH;
    shadow_camera.y = SHADOW_Y;
    shadow_camera.z = SDL_floor(player.camera.z / CHUNK_WIDTH) * CHUNK_WIDTH;
    shadow_camera.pitch = SHADOW_PITCH;
    shadow_camera.yaw = SHADOW_YAW;
    camera_update(&shadow_camera);
}

static void save_or_load_player(bool save)
{
    struct
    {
        float x;
        float y;
        float z;
        float pitch;
        float yaw;
        block_t block;
    }
    data;
    if (save)
    {
        data.x = player.camera.x;
        data.y = player.camera.y;
        data.z = player.camera.z;
        data.pitch = player.camera.pitch;
        data.yaw = player.camera.yaw;
        data.block = player.block;
        save_set_player(PLAYER_ID, &data, sizeof(data));
    }
    else
    {
        player_init(&player);
        if (save_get_player(PLAYER_ID, &data, sizeof(data)))
        {
            player.block = data.block;
            player.camera.x = data.x;
            player.camera.y = data.y;
            player.camera.z = data.z;
            player.camera.pitch = data.pitch;
            player.camera.yaw = data.yaw;
        }
        player_update_grounded(&player);
    }
}

static bool create_atlas()
{
    char path[512] = {0};
    SDL_snprintf(path, sizeof(path), "%satlas.png", SDL_GetBasePath());
    atlas_surface = SDL_LoadPNG(path);
    if (!atlas_surface)
    {
        SDL_Log("Failed to create atlas surface: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTexture* texture = NULL;
    SDL_GPUTransferBuffer* buffer = NULL;
    {
        SDL_GPUTextureCreateInfo info = {0};
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
        info.layer_count_or_depth = ATLAS_WIDTH / BLOCK_WIDTH;
        info.num_levels = ATLAS_MIP_LEVELS;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width = BLOCK_WIDTH;
        info.height = BLOCK_WIDTH;
        atlas_texture = SDL_CreateGPUTexture(device, &info);
        if (!atlas_texture)
        {
            SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
            return false;
        }
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = atlas_surface->w;
        info.height = atlas_surface->h;
        texture = SDL_CreateGPUTexture(device, &info);
        if (!texture)
        {
            SDL_Log("Failed to create texture: %s", SDL_GetError());
            return false;
        }
    }
    {
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.size = atlas_surface->w * atlas_surface->h * 4;
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        buffer = SDL_CreateGPUTransferBuffer(device, &info);
        if (!buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return false;
        }
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return false;
    }
    SDL_memcpy(data, atlas_surface->pixels, atlas_surface->w * atlas_surface->h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cbuf)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cbuf);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureTransferInfo info = {0};
    SDL_GPUTextureRegion region = {0};
    info.transfer_buffer = buffer;
    region.texture = texture;
    region.w = atlas_surface->w;
    region.h = atlas_surface->h;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &info, &region, 0);
    SDL_EndGPUCopyPass(pass);
    for (int i = 0; i < ATLAS_WIDTH / BLOCK_WIDTH; i++)
    {
        SDL_GPUBlitInfo info = {0};
        info.source.texture = texture;
        info.source.x = i * BLOCK_WIDTH;
        info.source.y = 0;
        info.source.w = BLOCK_WIDTH;
        info.source.h = BLOCK_WIDTH;
        info.destination.texture = atlas_texture;
        info.destination.x = 0;
        info.destination.y = 0;
        info.destination.w = BLOCK_WIDTH;
        info.destination.h = BLOCK_WIDTH;
        info.destination.layer_or_depth_plane = i;
        SDL_BlitGPUTexture(cbuf, &info);
    }
    if (ATLAS_MIP_LEVELS > 1)
    {
        SDL_GenerateMipmapsForGPUTexture(cbuf, atlas_texture);
    }
    SDL_SubmitGPUCommandBuffer(cbuf);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return true;
}

static void set_window_icon(block_t block)
{
    if (!atlas_surface)
    {
        return;
    }
    SDL_Surface* icon = SDL_CreateSurface(BLOCK_WIDTH, BLOCK_WIDTH, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
    {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return;
    }
    SDL_Rect src;
    src.x = block_get_index(block, DIRECTION_NORTH) * BLOCK_WIDTH;
    src.y = 0;
    src.w = BLOCK_WIDTH;
    src.h = BLOCK_WIDTH;
    SDL_Rect dst;
    dst.x = 0;
    dst.y = 0;
    dst.w = BLOCK_WIDTH;
    dst.h = BLOCK_WIDTH;
    if (!SDL_BlitSurface(atlas_surface, &src, icon, &dst))
    {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return;
    }
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

static bool create_opaque_pipeline()
{
    SDL_GPUColorTargetDescription color_targets[4] = {0};
    color_targets[0].format = color_format;
    color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    color_targets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color_targets[3].format = SDL_GPU_TEXTUREFORMAT_R8_UINT;
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = shader_load(device, "opaque.vert");
    info.fragment_shader = shader_load(device, "opaque.frag");
    info.target_info.num_color_targets = 4;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.vertex_input_state.num_vertex_attributes = 1;
    info.vertex_input_state.vertex_attributes = vertex_attributes;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_buffer_descriptions = vertex_buffers;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    if (info.vertex_shader && info.fragment_shader)
    {
        opaque_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return opaque_pipeline != NULL;
}

static bool create_depth_pipeline()
{
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = shader_load(device, "depth.vert");
    info.fragment_shader = shader_load(device, "depth.frag");
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.vertex_input_state.num_vertex_attributes = 1;
    info.vertex_input_state.vertex_attributes = vertex_attributes;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_buffer_descriptions = vertex_buffers;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    if (info.vertex_shader && info.fragment_shader)
    {
        depth_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return depth_pipeline != NULL;
}

static bool create_transparent_pipeline()
{
    SDL_GPUColorTargetDescription color_targets[1] = {0};
    color_targets[0].format = color_format;
    color_targets[0].blend_state.enable_blend = true;
    color_targets[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_targets[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_targets[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_targets[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_targets[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_targets[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = shader_load(device, "transparent.vert");
    info.fragment_shader = shader_load(device, "transparent.frag");
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.vertex_input_state.num_vertex_attributes = 1;
    info.vertex_input_state.vertex_attributes = vertex_attributes;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_buffer_descriptions = vertex_buffers;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    if (info.vertex_shader && info.fragment_shader)
    {
        transparent_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return transparent_pipeline != NULL;
}

static bool create_shadow_pipeline()
{
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = shader_load(device, "shadow.vert");
    info.fragment_shader = shader_load(device, "shadow.frag");
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.vertex_input_state.num_vertex_attributes = 1;
    info.vertex_input_state.vertex_attributes = vertex_attributes;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_buffer_descriptions = vertex_buffers;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    if (info.vertex_shader && info.fragment_shader)
    {
        shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return shadow_pipeline != NULL;
}

static bool create_sky_pipeline()
{
    SDL_GPUColorTargetDescription color_targets[4] = {0};
    color_targets[0].format = color_format;
    color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    color_targets[2].format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    color_targets[3].format = SDL_GPU_TEXTUREFORMAT_R8_UINT;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = shader_load(device, "sky.vert");
    info.fragment_shader = shader_load(device, "sky.frag");
    info.target_info.num_color_targets = 4;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    if (info.vertex_shader && info.fragment_shader)
    {
        sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return sky_pipeline != NULL;
}

static bool create_raycast_pipeline()
{
    SDL_GPUColorTargetDescription color_targets[1] = {0};
    color_targets[0].format = color_format;
    color_targets[0].blend_state.enable_blend = true;
    color_targets[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_targets[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_targets[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_targets[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_targets[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_targets[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = shader_load(device, "raycast.vert");
    info.fragment_shader = shader_load(device, "raycast.frag");
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    if (info.vertex_shader && info.fragment_shader)
    {
        raycast_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return raycast_pipeline != NULL;
}

static bool create_samplers()
{
    SDL_GPUSamplerCreateInfo info = {0};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linear_sampler = SDL_CreateGPUSampler(device, &info);
    if (!linear_sampler)
    {
        SDL_Log("Failed to create linear sampler: %s", SDL_GetError());
        return false;
    }
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    nearest_sampler = SDL_CreateGPUSampler(device, &info);
    if (!nearest_sampler)
    {
        SDL_Log("Failed to create nearest sampler: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool create_textures()
{
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = SHADOW_RESOLUTION;
    info.height = SHADOW_RESOLUTION;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    shadow_texture = SDL_CreateGPUTexture(device, &info);
    if (!shadow_texture)
    {
        SDL_Log("Failed to create shadow texture: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char** argv)
{
#ifndef NDEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#endif
    SDL_SetAppMetadata("Blocks", NULL, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    window = SDL_CreateWindow("Blocks", 960, 720, SDL_WINDOW_HIDDEN);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
#ifndef NDEBUG
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, true, NULL);
#else
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, false, NULL);
#endif
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        return false;
    }
    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
    color_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    if (!create_atlas())
    {
        SDL_Log("Failed to create atlas: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_samplers())
    {
        SDL_Log("Failed to create samplers: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_textures())
    {
        SDL_Log("Failed to create textures: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_opaque_pipeline())
    {
        SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_transparent_pipeline())
    {
        SDL_Log("Failed to create transparent pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_depth_pipeline())
    {
        SDL_Log("Failed to create predepth pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_shadow_pipeline())
    {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_sky_pipeline())
    {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!create_raycast_pipeline())
    {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    ui_pipeline = shader_load(device, "ui.comp");
    if (!ui_pipeline)
    {
        SDL_Log("Failed to load ui pipeline");
        return SDL_APP_FAILURE;
    }
    ssao_pipeline = shader_load(device, "ssao.comp");
    if (!ssao_pipeline)
    {
        SDL_Log("Failed to load ssao pipeline");
        return SDL_APP_FAILURE;
    }
    composite_pipeline = shader_load(device, "composite.comp");
    if (!composite_pipeline)
    {
        SDL_Log("Failed to load composite pipeline");
        return SDL_APP_FAILURE;
    }
    blur_pipeline = shader_load(device, "blur.comp");
    if (!blur_pipeline)
    {
        SDL_Log("Failed to load blur pipeline");
        return SDL_APP_FAILURE;
    }
    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    set_window_icon(BLOCK_GRASS);
    save_init(SAVE_PATH);
    world_init(device);
    save_or_load_player(false);
    world_update(&player.camera);
    player_query = world_raycast(&player.camera, PLAYER_REACH);
    update_shadow_camera();
    ticks2 = SDL_GetTicks();
    ticks1 = 0;
    return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    SDL_HideWindow(window);
    world_free();
    save_or_load_player(true);
    save_free();
    SDL_ReleaseGPUSampler(device, linear_sampler);
    SDL_ReleaseGPUSampler(device, nearest_sampler);
    SDL_ReleaseGPUTexture(device, shadow_texture);
    SDL_ReleaseGPUTexture(device, composite_texture);
    SDL_ReleaseGPUTexture(device, blur_texture);
    SDL_ReleaseGPUTexture(device, ssao_texture);
    SDL_ReleaseGPUTexture(device, color_texture);
    SDL_ReleaseGPUTexture(device, light_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, voxel_texture);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_DestroySurface(atlas_surface);
    SDL_ReleaseGPUComputePipeline(device, blur_pipeline);
    SDL_ReleaseGPUComputePipeline(device, composite_pipeline);
    SDL_ReleaseGPUComputePipeline(device, ssao_pipeline);
    SDL_ReleaseGPUComputePipeline(device, ui_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, raycast_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, sky_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, shadow_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, depth_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, transparent_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, opaque_pipeline);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static bool resize(int width, int height)
{
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, color_texture);
    SDL_ReleaseGPUTexture(device, voxel_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, ssao_texture);
    SDL_ReleaseGPUTexture(device, blur_texture);
    SDL_ReleaseGPUTexture(device, composite_texture);
    SDL_ReleaseGPUTexture(device, light_texture);
    depth_texture = NULL;
    color_texture = NULL;
    voxel_texture = NULL;
    position_texture = NULL;
    ssao_texture = NULL;
    blur_texture = NULL;
    composite_texture = NULL;
    light_texture = NULL;
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    depth_texture = SDL_CreateGPUTexture(device, &info);
    if (!depth_texture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    info.format = color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    color_texture = SDL_CreateGPUTexture(device, &info);
    if (!color_texture)
    {
        SDL_Log("Failed to create color texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UINT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    voxel_texture = SDL_CreateGPUTexture(device, &info);
    if (!voxel_texture)
    {
        SDL_Log("Failed to create voxel texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    position_texture = SDL_CreateGPUTexture(device, &info);
    if (!position_texture)
    {
        SDL_Log("Failed to create position texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    light_texture = SDL_CreateGPUTexture(device, &info);
    if (!light_texture)
    {
        SDL_Log("Failed to create light texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    ssao_texture = SDL_CreateGPUTexture(device, &info);
    if (!ssao_texture)
    {
        SDL_Log("Failed to create ssao texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
    blur_texture = SDL_CreateGPUTexture(device, &info);
    if (!blur_texture)
    {
        SDL_Log("Failed to create ssao blur texture: %s", SDL_GetError());
        return false;
    }
    info.format = color_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    composite_texture = SDL_CreateGPUTexture(device, &info);
    if (!composite_texture)
    {
        SDL_Log("Failed to create composite texture: %s", SDL_GetError());
        return false;
    }
    camera_resize(&player.camera, width, height);
    return true;
}

static void render_shadow(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = shadow_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, NULL, 0, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, shadow_pipeline);
    SDL_PushGPUDebugGroup(cbuf, "shadow");
    world_render(&shadow_camera, cbuf, pass, WORLD_FLAGS_OPAQUE);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}

static void render_sky(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    SDL_PushGPUDebugGroup(cbuf, "sky");
    SDL_BindGPUGraphicsPipeline(pass, sky_pipeline);
    SDL_PushGPUVertexUniformData(cbuf, 0, player.camera.proj, 64);
    SDL_PushGPUVertexUniformData(cbuf, 1, player.camera.view, 64);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_opaque(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    SDL_GPUTextureSamplerBinding atlas_binding = {0};
    atlas_binding.texture = atlas_texture;
    atlas_binding.sampler = nearest_sampler;
    SDL_PushGPUDebugGroup(cbuf, "opaque");
    SDL_BindGPUGraphicsPipeline(pass, opaque_pipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, &atlas_binding, 1);
    world_render(&player.camera, cbuf, pass, WORLD_FLAGS_OPAQUE | WORLD_FLAGS_LIGHT);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_gbuffer(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUColorTargetInfo color_info[4] = {0};
    color_info[0].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[0].texture = color_texture;
    color_info[0].cycle = true;
    color_info[0].store_op = SDL_GPU_STOREOP_STORE;
    color_info[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[1].texture = position_texture;
    color_info[1].cycle = true;
    color_info[1].store_op = SDL_GPU_STOREOP_STORE;
    color_info[2].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[2].texture = light_texture;
    color_info[2].cycle = true;
    color_info[2].store_op = SDL_GPU_STOREOP_STORE;
    color_info[3].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[3].texture = voxel_texture;
    color_info[3].cycle = true;
    color_info[3].store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, color_info, 4, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    render_sky(cbuf, pass);
    render_opaque(cbuf, pass);
    SDL_EndGPURenderPass(pass);
}

static void render_ssao(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUStorageTextureReadWriteBinding write_textures = {0};
    write_textures.texture = ssao_texture;
    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(cbuf, &write_textures, 1, NULL, 0);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* read_textures[2] = {0};
    read_textures[0] = voxel_texture;
    read_textures[1] = position_texture;
    int groups_x = (player.camera.width + 8 - 1) / 8;
    int groups_y = (player.camera.height + 8 - 1) / 8;
    SDL_PushGPUDebugGroup(cbuf, "ssao");
    SDL_BindGPUComputePipeline(compute_pass, ssao_pipeline);
    SDL_BindGPUComputeStorageTextures(compute_pass, 0, read_textures, 2);
    SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
    SDL_EndGPUComputePass(compute_pass);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_blur(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUStorageTextureReadWriteBinding write_textures = {0};
    write_textures.texture = blur_texture;
    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(cbuf, &write_textures, 1, NULL, 0);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* read_textures[1];
    read_textures[0] = ssao_texture;
    int groups_x = (player.camera.width + 8 - 1) / 8;
    int groups_y = (player.camera.height + 8 - 1) / 8;
    SDL_PushGPUDebugGroup(cbuf, "blur");
    SDL_BindGPUComputePipeline(compute_pass, blur_pipeline);
    SDL_BindGPUComputeStorageTextures(compute_pass, 0, read_textures, 1);
    SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
    SDL_EndGPUComputePass(compute_pass);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_composite(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUStorageTextureReadWriteBinding write_textures = {0};
    write_textures.texture = composite_texture;
    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(cbuf, &write_textures, 1, NULL, 0);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* read_textures[5] = {0};
    SDL_GPUTextureSamplerBinding read_samplers = {0};
    read_textures[0] = color_texture;
    read_textures[1] = light_texture;
    read_textures[2] = blur_texture;
    read_textures[3] = voxel_texture;
    read_textures[4] = position_texture;
    read_samplers.texture = shadow_texture;
    read_samplers.sampler = linear_sampler;
    int groups_x = (player.camera.width + 8 - 1) / 8;
    int groups_y = (player.camera.height + 8 - 1) / 8;
    SDL_PushGPUDebugGroup(cbuf, "composite");
    SDL_BindGPUComputePipeline(compute_pass, composite_pipeline);
    SDL_BindGPUComputeStorageTextures(compute_pass, 0, read_textures, 5);
    SDL_BindGPUComputeSamplers(compute_pass, 0, &read_samplers, 1);
    SDL_PushGPUComputeUniformData(cbuf, 0, &shadow_camera.matrix, 64);
    SDL_PushGPUComputeUniformData(cbuf, 1, player.camera.position, 12);
    SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
    SDL_EndGPUComputePass(compute_pass);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_depth(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_LOAD;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, NULL, 0, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_PushGPUDebugGroup(cbuf, "depth");
    SDL_BindGPUGraphicsPipeline(pass, depth_pipeline);
    world_render(&player.camera, cbuf, pass, WORLD_FLAGS_TRANSPARENT);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}

static void render_transparent(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    SDL_GPUTextureSamplerBinding sampler_bindings[3] = {0};
    sampler_bindings[0].texture = atlas_texture;
    sampler_bindings[0].sampler = nearest_sampler;
    sampler_bindings[1].texture = shadow_texture;
    sampler_bindings[1].sampler = linear_sampler;
    sampler_bindings[2].texture = position_texture;
    sampler_bindings[2].sampler = nearest_sampler;
    SDL_PushGPUDebugGroup(cbuf, "transparent");
    SDL_BindGPUGraphicsPipeline(pass, transparent_pipeline);
    SDL_PushGPUFragmentUniformData(cbuf, 1, &shadow_camera.matrix, 64);
    SDL_PushGPUFragmentUniformData(cbuf, 2, player.camera.position, 12);
    SDL_BindGPUFragmentSamplers(pass, 0, sampler_bindings, 3);
    world_render(&player.camera, cbuf, pass, WORLD_FLAGS_TRANSPARENT | WORLD_FLAGS_LIGHT);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_raycast(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    if (player_query.block == BLOCK_EMPTY)
    {
        return;
    }
    SDL_PushGPUDebugGroup(cbuf, "raycast");
    SDL_BindGPUGraphicsPipeline(pass, raycast_pipeline);
    SDL_PushGPUVertexUniformData(cbuf, 0, player.camera.matrix, 64);
    SDL_PushGPUVertexUniformData(cbuf, 1, player_query.current, 12);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_forward(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUColorTargetInfo color_info = {0};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.texture = composite_texture;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_LOAD;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, &color_info, 1, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    render_transparent(cbuf, pass);
    render_raycast(cbuf, pass);
    SDL_EndGPURenderPass(pass);
}

static void render_ui(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUStorageTextureReadWriteBinding write_textures[1] = {0};
    write_textures[0].texture = composite_texture;
    SDL_GPUComputePass* compute_pass = SDL_BeginGPUComputePass(cbuf, write_textures, 1, NULL, 0);
    if (!compute_pass)
    {
        SDL_Log("Failed to begin compute pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureSamplerBinding read_textures[1] = {0};
    read_textures[0].texture = atlas_texture;
    read_textures[0].sampler = nearest_sampler;
    Sint32 index = block_get_index(player.block, DIRECTION_NORTH);
    int groups_x = (player.camera.width + 8 - 1) / 8;
    int groups_y = (player.camera.height + 8 - 1) / 8;
    SDL_PushGPUDebugGroup(cbuf, "ui");
    SDL_BindGPUComputePipeline(compute_pass, ui_pipeline);
    SDL_BindGPUComputeSamplers(compute_pass, 0, read_textures, 1);
    SDL_PushGPUComputeUniformData(cbuf, 0, player.camera.size, 8);
    SDL_PushGPUComputeUniformData(cbuf, 1, &index, 4);
    SDL_DispatchGPUCompute(compute_pass, groups_x, groups_y, 1);
    SDL_EndGPUComputePass(compute_pass);
    SDL_PopGPUDebugGroup(cbuf);
}

static void render_swapchain(SDL_GPUCommandBuffer* cbuf, SDL_GPUTexture* swapchain_texture)
{
    SDL_GPUBlitInfo info = {0};
    info.source.texture = composite_texture;
    info.source.w = player.camera.width;
    info.source.h = player.camera.height;
    info.destination.texture = swapchain_texture;
    info.destination.w = player.camera.width;
    info.destination.h = player.camera.height;
    info.load_op = SDL_GPU_LOADOP_DONT_CARE;
    info.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(cbuf, &info);
}

static void render()
{
    SDL_GPUCommandBuffer* cbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cbuf)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* swapchain_texture;
    Uint32 width;
    Uint32 height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cbuf, window, &swapchain_texture, &width, &height))
    {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(cbuf);
        return;
    }
    if (!swapchain_texture || !width || !height)
    {
        SDL_SubmitGPUCommandBuffer(cbuf);
        return;
    }
    if ((width != player.camera.width || height != player.camera.height) && !resize(width, height))
    {
        SDL_SubmitGPUCommandBuffer(cbuf);
        return;
    }
    camera_update(&player.camera);
    render_shadow(cbuf);
    render_gbuffer(cbuf);
    render_ssao(cbuf);
    render_blur(cbuf);
    render_composite(cbuf);
    render_depth(cbuf);
    render_forward(cbuf);
    render_ui(cbuf);
    render_swapchain(cbuf, swapchain_texture);
    SDL_SubmitGPUCommandBuffer(cbuf);
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    ticks2 = SDL_GetTicks();
    float dt = ticks2 - ticks1;
    ticks1 = ticks2;
    if (SDL_GetWindowRelativeMouseMode(window))
    {
        player_move(&player, dt, SDL_GetKeyboardState(NULL));
        player_query = world_raycast(&player.camera, PLAYER_REACH);
        save_or_load_player(true);
    }
    update_shadow_camera();
    world_update(&player.camera);
    render();
    return SDL_APP_CONTINUE;
}

static void rotate_player(float pitch, float yaw)
{
    player_rotate(&player, pitch, yaw, PLAYER_SENSITIVITY);
    player_query = world_raycast(&player.camera, PLAYER_REACH);
}

static void break_block()
{
    if (player_query.block != BLOCK_EMPTY)
    {
        world_set_block(player_query.current, BLOCK_EMPTY);
    }
}

static void select_block()
{
    if (player_query.block != BLOCK_EMPTY)
    {
        player.block = player_query.block;
    }
}

static void place_block()
{
    if (player_query.block != BLOCK_EMPTY && !player_overlaps_block(&player, player_query.previous))
    {
        world_set_block(player_query.previous, player.block);
    }
}

static void change_block(int dy)
{
    static const int COUNT = BLOCK_COUNT - BLOCK_EMPTY - 1;
    int block = player.block - (BLOCK_EMPTY + 1) + dy;
    block = (block + COUNT) % COUNT;
    player.block = block + BLOCK_EMPTY + 1;
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_MOUSE_MOTION:
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            rotate_player(event->motion.yrel, event->motion.xrel);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode == SDL_SCANCODE_ESCAPE)
        {
            SDL_SetWindowRelativeMouseMode(window, false);
            SDL_SetWindowFullscreen(window, false);
        }
        else if (event->key.scancode == SDL_SCANCODE_F5)
        {
            player_toggle_controller(&player);
            SDL_Log("Controller mode: %s", player_controller_name(player.controller));
        }
        else if (event->key.scancode == SDL_SCANCODE_F11)
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
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!SDL_GetWindowRelativeMouseMode(window))
        {
            SDL_SetWindowRelativeMouseMode(window, true);
        }
        else
        {
            if (event->button.button == SDL_BUTTON_LEFT)
            {
                break_block();
            }
            else if (event->button.button == SDL_BUTTON_MIDDLE)
            {
                select_block();
            }
            else if (event->button.button == SDL_BUTTON_RIGHT)
            {
                place_block();
            }
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        change_block(event->wheel.y);
        break;
    }
    return SDL_APP_CONTINUE;
}
