#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "block.h"
#include "camera.h"
#include "player.h"
#include "save.h"
#include "shader.h"
#include "sky.h"
#include "world.h"

static const char* SAVE_PATH = "blocks.sqlite3";
static const int ATLAS_WIDTH = 512;
static const int ATLAS_MIP_LEVELS = 4;
static const int BLOCK_WIDTH = 16;
static const int SHADOW_RESOLUTION = 2048;
static const SDL_GPUSampleCount SAMPLE_COUNT = SDL_GPU_SAMPLECOUNT_4;

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
static SDL_GPUGraphicsPipeline* ui_pipeline;
static SDL_Surface* atlas_surface;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUTexture* position_texture;
static SDL_GPUTexture* multisample_color_texture;
static SDL_GPUTexture* multisample_position_texture;
static SDL_GPUTexture* shadow_texture;
static SDL_GPUBuffer* block_buffer;
static SDL_GPUSampler* nearest_sampler;
static SDL_GPUSampler* shadow_sampler;
static Sky sky;
static Player player;
static Uint64 previous_ticks;

static bool CreateAtlas()
{
    char path[512] = {0};
    SDL_snprintf(path, sizeof(path), "%satlas.png", SDL_GetBasePath());
    atlas_surface = SDL_LoadPNG(path);
    if (!atlas_surface)
    {
        SDL_Log("Failed to create atlas surface: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTexture* source_texture = NULL;
    SDL_GPUTransferBuffer* transfer_buffer = NULL;
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
        source_texture = SDL_CreateGPUTexture(device, &info);
        if (!source_texture)
        {
            SDL_Log("Failed to create texture: %s", SDL_GetError());
            return false;
        }
    }
    {
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.size = atlas_surface->w * atlas_surface->h * 4;
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_buffer = SDL_CreateGPUTransferBuffer(device, &info);
        if (!transfer_buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return false;
        }
    }
    void* pixels = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (!pixels)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return false;
    }
    SDL_memcpy(pixels, atlas_surface->pixels, atlas_surface->w * atlas_surface->h * 4);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureTransferInfo transfer_info = {0};
    SDL_GPUTextureRegion region = {0};
    transfer_info.transfer_buffer = transfer_buffer;
    region.texture = source_texture;
    region.w = atlas_surface->w;
    region.h = atlas_surface->h;
    region.d = 1;
    SDL_UploadToGPUTexture(copy_pass, &transfer_info, &region, false);
    SDL_EndGPUCopyPass(copy_pass);
    for (int layer = 0; layer < ATLAS_WIDTH / BLOCK_WIDTH; layer++)
    {
        SDL_GPUBlitInfo info = {0};
        info.source.texture = source_texture;
        info.source.x = layer * BLOCK_WIDTH;
        info.source.y = 0;
        info.source.w = BLOCK_WIDTH;
        info.source.h = BLOCK_WIDTH;
        info.destination.texture = atlas_texture;
        info.destination.x = 0;
        info.destination.y = 0;
        info.destination.w = BLOCK_WIDTH;
        info.destination.h = BLOCK_WIDTH;
        info.destination.layer_or_depth_plane = layer;
        SDL_BlitGPUTexture(command_buffer, &info);
    }
    if (ATLAS_MIP_LEVELS > 1)
    {
        SDL_GenerateMipmapsForGPUTexture(command_buffer, atlas_texture);
    }
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTexture(device, source_texture);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
    return true;
}

static void SetWindowIcon()
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
    SDL_Rect source = {0};
    source.x = Block_GetIndex(BLOCK_GRASS, DIRECTION_NORTH) * BLOCK_WIDTH;
    source.w = BLOCK_WIDTH;
    source.h = BLOCK_WIDTH;
    SDL_Rect destination = {0};
    destination.w = BLOCK_WIDTH;
    destination.h = BLOCK_WIDTH;
    if (!SDL_BlitSurface(atlas_surface, &source, icon, &destination))
    {
        SDL_Log("Failed to blit icon surface: %s", SDL_GetError());
        SDL_DestroySurface(icon);
        return;
    }
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
}

static bool CreateOpaquePipeline()
{
    SDL_GPUColorTargetDescription color_targets[2] = {0};
    color_targets[0].format = color_format;
    color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = Shader_Load(device, "opaque.vert");
    info.fragment_shader = Shader_Load(device, "opaque.frag");
    info.target_info.num_color_targets = 2;
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
    info.multisample_state.sample_count = SAMPLE_COUNT;
    if (info.vertex_shader && info.fragment_shader)
    {
        opaque_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return opaque_pipeline != NULL;
}

static bool CreateDepthPipeline()
{
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = Shader_Load(device, "depth.vert");
    info.fragment_shader = Shader_Load(device, "depth.frag");
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
    info.multisample_state.sample_count = SAMPLE_COUNT;
    if (info.vertex_shader && info.fragment_shader)
    {
        depth_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return depth_pipeline != NULL;
}

static bool CreateTransparentPipeline()
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
    info.vertex_shader = Shader_Load(device, "transparent.vert");
    info.fragment_shader = Shader_Load(device, "transparent.frag");
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
    info.multisample_state.sample_count = SAMPLE_COUNT;
    if (info.vertex_shader && info.fragment_shader)
    {
        transparent_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return transparent_pipeline != NULL;
}

static bool CreateShadowPipeline()
{
    SDL_GPUVertexAttribute vertex_attributes[1] = {0};
    SDL_GPUVertexBufferDescription vertex_buffers[1] = {0};
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    vertex_buffers[0].pitch = 4;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = Shader_Load(device, "shadow.vert");
    info.fragment_shader = Shader_Load(device, "depth.frag");
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
    info.rasterizer_state.depth_bias_constant_factor = 1.0f;
    info.rasterizer_state.depth_bias_slope_factor = 2.0f;
    info.rasterizer_state.enable_depth_bias = true;
    if (info.vertex_shader && info.fragment_shader)
    {
        shadow_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return shadow_pipeline != NULL;
}

static bool CreateSkyPipeline()
{
    SDL_GPUColorTargetDescription color_targets[2] = {0};
    color_targets[0].format = color_format;
    color_targets[1].format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = Shader_Load(device, "sky.vert");
    info.fragment_shader = Shader_Load(device, "sky.frag");
    info.target_info.num_color_targets = 2;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.multisample_state.sample_count = SAMPLE_COUNT;
    if (info.vertex_shader && info.fragment_shader)
    {
        sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return sky_pipeline != NULL;
}

static bool CreateRaycastPipeline()
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
    info.vertex_shader = Shader_Load(device, "raycast.vert");
    info.fragment_shader = Shader_Load(device, "raycast.frag");
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = color_targets;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depth_format;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.rasterizer_state.enable_depth_bias = true;
    info.rasterizer_state.depth_bias_constant_factor = -1.0f;
    info.rasterizer_state.depth_bias_slope_factor = -1.0f;
    info.multisample_state.sample_count = SAMPLE_COUNT;
    if (info.vertex_shader && info.fragment_shader)
    {
        raycast_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return raycast_pipeline != NULL;
}

static bool CreateUIPipeline()
{
    SDL_GPUColorTargetDescription color_target = {0};
    color_target.format = color_format;
    color_target.blend_state.enable_blend = true;
    color_target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = Shader_Load(device, "ui.vert");
    info.fragment_shader = Shader_Load(device, "ui.frag");
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color_target;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    if (info.vertex_shader && info.fragment_shader)
    {
        ui_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return ui_pipeline != NULL;
}

static bool CreateSamplers()
{
    SDL_GPUSamplerCreateInfo info = {0};
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    nearest_sampler = SDL_CreateGPUSampler(device, &info);
    if (!nearest_sampler)
    {
        SDL_Log("Failed to create nearest sampler: %s", SDL_GetError());
        return false;
    }
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.enable_compare = true;
    shadow_sampler = SDL_CreateGPUSampler(device, &info);
    if (!shadow_sampler)
    {
        SDL_Log("Failed to create shadow sampler: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool CreateTextures()
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
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
    color_format = SDL_GetGPUSwapchainTextureFormat(device, window);
    depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    if (!CreateAtlas())
    {
        SDL_Log("Failed to create atlas: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateSamplers())
    {
        SDL_Log("Failed to create samplers: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateTextures())
    {
        SDL_Log("Failed to create textures: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateOpaquePipeline())
    {
        SDL_Log("Failed to create opaque pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateTransparentPipeline())
    {
        SDL_Log("Failed to create transparent pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateDepthPipeline())
    {
        SDL_Log("Failed to create predepth pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateShadowPipeline())
    {
        SDL_Log("Failed to create shadow pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateSkyPipeline())
    {
        SDL_Log("Failed to create sky pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateRaycastPipeline())
    {
        SDL_Log("Failed to create raycast pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!CreateUIPipeline())
    {
        SDL_Log("Failed to create ui pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    block_buffer = Block_GetBuffer(device);
    if (!block_buffer)
    {
        SDL_Log("Failed to create block buffer");
        return SDL_APP_FAILURE;
    }
    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    SetWindowIcon();
    Save_Init(SAVE_PATH);
    Sky_Load(&sky);
    World_Init(device);
    Player_Load(&player);
    World_Update(&player.camera);
    previous_ticks = SDL_GetTicks();
    return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    SDL_HideWindow(window);
    World_Free();
    Player_Save(&player);
    Sky_Save(&sky);
    Save_Free();
    SDL_ReleaseGPUSampler(device, nearest_sampler);
    SDL_ReleaseGPUSampler(device, shadow_sampler);
    SDL_ReleaseGPUTexture(device, shadow_texture);
    SDL_ReleaseGPUTexture(device, multisample_position_texture);
    SDL_ReleaseGPUTexture(device, multisample_color_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_ReleaseGPUBuffer(device, block_buffer);
    SDL_DestroySurface(atlas_surface);
    SDL_ReleaseGPUGraphicsPipeline(device, ui_pipeline);
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

static bool Resize(int width, int height)
{
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, multisample_color_texture);
    SDL_ReleaseGPUTexture(device, multisample_position_texture);
    depth_texture = NULL;
    position_texture = NULL;
    multisample_color_texture = NULL;
    multisample_position_texture = NULL;
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SAMPLE_COUNT;
    depth_texture = SDL_CreateGPUTexture(device, &info);
    if (!depth_texture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    position_texture = SDL_CreateGPUTexture(device, &info);
    if (!position_texture)
    {
        SDL_Log("Failed to create position texture: %s", SDL_GetError());
        return false;
    }
    info.format = color_format;
    info.sample_count = SAMPLE_COUNT;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    multisample_color_texture = SDL_CreateGPUTexture(device, &info);
    if (!multisample_color_texture)
    {
        SDL_Log("Failed to create multisample color texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    multisample_position_texture = SDL_CreateGPUTexture(device, &info);
    if (!multisample_position_texture)
    {
        SDL_Log("Failed to create multisample position texture: %s", SDL_GetError());
        return false;
    }
    Camera_Resize(&player.camera, width, height);
    return true;
}

static void RenderShadowPass(SDL_GPUCommandBuffer* cbuf)
{
    if (!sky.is_shadow_on)
    {
        return;
    }
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
    SDL_BindGPUVertexStorageBuffers(pass, 0, &block_buffer, 1);
    SDL_PushGPUDebugGroup(cbuf, "shadow");
    World_Render(&sky.camera, cbuf, pass, WORLD_FLAGS_OPAQUE);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}

static void DrawSky(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    SDL_PushGPUDebugGroup(cbuf, "sky");
    SDL_BindGPUGraphicsPipeline(pass, sky_pipeline);
    SDL_PushGPUVertexUniformData(cbuf, 0, player.camera.proj, sizeof(player.camera.proj));
    SDL_PushGPUVertexUniformData(cbuf, 1, player.camera.view, sizeof(player.camera.view));
    SDL_PushGPUFragmentUniformData(cbuf, 0, sky.sun, sizeof(float) * 16);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
}

static void DrawOpaque(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    SDL_GPUTextureSamplerBinding sampler_bindings[2] = {0};
    sampler_bindings[0].texture = atlas_texture;
    sampler_bindings[0].sampler = nearest_sampler;
    sampler_bindings[1].texture = shadow_texture;
    sampler_bindings[1].sampler = shadow_sampler;
    SDL_PushGPUDebugGroup(cbuf, "opaque");
    SDL_BindGPUGraphicsPipeline(pass, opaque_pipeline);
    SDL_PushGPUFragmentUniformData(cbuf, 1, &sky.camera.matrix, sizeof(sky.camera.matrix));
    SDL_PushGPUFragmentUniformData(cbuf, 2, player.camera.position, sizeof(player.camera.position));
    SDL_PushGPUFragmentUniformData(cbuf, 3, sky.sun, sizeof(float) * 16);
    SDL_BindGPUFragmentSamplers(pass, 0, sampler_bindings, 2);
    SDL_BindGPUFragmentStorageBuffers(pass, 0, &block_buffer, 1);
    World_Render(&player.camera, cbuf, pass, WORLD_FLAGS_OPAQUE | WORLD_FLAGS_LIGHT);
    SDL_PopGPUDebugGroup(cbuf);
}

static void RenderOpaquePass(SDL_GPUCommandBuffer* cbuf)
{
    SDL_GPUColorTargetInfo color_info[2] = {0};
    color_info[0].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[0].texture = multisample_color_texture;
    color_info[0].cycle = true;
    color_info[0].store_op = SDL_GPU_STOREOP_STORE;
    color_info[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[1].texture = multisample_position_texture;
    color_info[1].cycle = true;
    color_info[1].store_op = SDL_GPU_STOREOP_RESOLVE;
    color_info[1].resolve_texture = position_texture;
    color_info[1].cycle_resolve_texture = true;
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    depth_info.clear_depth = 1.0f;
    depth_info.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, color_info, 2, &depth_info);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    DrawSky(cbuf, pass);
    DrawOpaque(cbuf, pass);
    SDL_EndGPURenderPass(pass);
}

static void RenderTransparentDepthPass(SDL_GPUCommandBuffer* cbuf)
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
    World_Render(&player.camera, cbuf, pass, WORLD_FLAGS_TRANSPARENT);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}

static void DrawTransparent(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    SDL_GPUTextureSamplerBinding sampler_bindings[3] = {0};
    sampler_bindings[0].texture = atlas_texture;
    sampler_bindings[0].sampler = nearest_sampler;
    sampler_bindings[1].texture = shadow_texture;
    sampler_bindings[1].sampler = shadow_sampler;
    sampler_bindings[2].texture = position_texture;
    sampler_bindings[2].sampler = nearest_sampler;
    SDL_PushGPUDebugGroup(cbuf, "transparent");
    SDL_BindGPUGraphicsPipeline(pass, transparent_pipeline);
    SDL_PushGPUFragmentUniformData(cbuf, 1, &sky.camera.matrix, sizeof(sky.camera.matrix));
    SDL_PushGPUFragmentUniformData(cbuf, 2, player.camera.position, sizeof(player.camera.position));
    SDL_PushGPUFragmentUniformData(cbuf, 3, sky.sun, sizeof(float) * 16);
    SDL_BindGPUFragmentSamplers(pass, 0, sampler_bindings, 3);
    SDL_BindGPUFragmentStorageBuffers(pass, 0, &block_buffer, 1);
    World_Render(&player.camera, cbuf, pass, WORLD_FLAGS_TRANSPARENT | WORLD_FLAGS_LIGHT);
    SDL_PopGPUDebugGroup(cbuf);
}

static void DrawRaycast(SDL_GPUCommandBuffer* cbuf, SDL_GPURenderPass* pass)
{
    if (player.query.block == BLOCK_EMPTY)
    {
        return;
    }
    SDL_PushGPUDebugGroup(cbuf, "raycast");
    SDL_BindGPUGraphicsPipeline(pass, raycast_pipeline);
    SDL_PushGPUVertexUniformData(cbuf, 0, player.camera.matrix, sizeof(player.camera.matrix));
    SDL_PushGPUVertexUniformData(cbuf, 1, player.query.current, sizeof(player.query.current));
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
}

static void RenderTransparentPass(SDL_GPUCommandBuffer* cbuf, SDL_GPUTexture* swapchain_texture)
{
    SDL_GPUColorTargetInfo color_info = {0};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.texture = multisample_color_texture;
    color_info.store_op = SDL_GPU_STOREOP_RESOLVE;
    color_info.resolve_texture = swapchain_texture;
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
    DrawTransparent(cbuf, pass);
    DrawRaycast(cbuf, pass);
    SDL_EndGPURenderPass(pass);
}

static void RenderUIPass(SDL_GPUCommandBuffer* cbuf, SDL_GPUTexture* swapchain_texture)
{
    SDL_GPUColorTargetInfo color_info = {0};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    color_info.texture = swapchain_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cbuf, &color_info, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureSamplerBinding sampler_binding = {0};
    sampler_binding.texture = atlas_texture;
    sampler_binding.sampler = nearest_sampler;
    struct
    {
        int viewport[2];
        Uint32 index;
    } uniform;
    uniform.viewport[0] = player.camera.width;
    uniform.viewport[1] = player.camera.height;
    uniform.index = Block_GetIndex(player.block, DIRECTION_NORTH);
    SDL_PushGPUDebugGroup(cbuf, "ui");
    SDL_BindGPUGraphicsPipeline(pass, ui_pipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, &sampler_binding, 1);
    SDL_PushGPUVertexUniformData(cbuf, 0, &uniform, sizeof(uniform));
    SDL_DrawGPUPrimitives(pass, 6, 3, 0, 0);
    SDL_PopGPUDebugGroup(cbuf);
    SDL_EndGPURenderPass(pass);
}

static void RenderFrame()
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
    if ((width != player.camera.width || height != player.camera.height) && !Resize(width, height))
    {
        SDL_SubmitGPUCommandBuffer(cbuf);
        return;
    }
    Camera_Update(&player.camera);
    RenderShadowPass(cbuf);
    RenderOpaquePass(cbuf);
    RenderTransparentDepthPass(cbuf);
    RenderTransparentPass(cbuf, swapchain_texture);
    RenderUIPass(cbuf, swapchain_texture);
    SDL_SubmitGPUCommandBuffer(cbuf);
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    Uint64 ticks = SDL_GetTicks();
    float dt = ticks - previous_ticks;
    previous_ticks = ticks;
    if (SDL_GetWindowRelativeMouseMode(window))
    {
        Player_Move(&player, dt);
    }
    Sky_Update(&sky, &player.camera, SHADOW_RESOLUTION, dt / 1000.0f);
    World_Update(&player.camera);
    RenderFrame();
    return SDL_APP_CONTINUE;
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
            Player_Rotate(&player, event->motion.yrel, event->motion.xrel);
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
            Player_ToggleController(&player);
        }
        else if (event->key.scancode == SDL_SCANCODE_T)
        {
            Sky_Reset(&sky);
        }
        else if (event->key.scancode == SDL_SCANCODE_F11)
        {
            bool fullscreen = !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN);
            SDL_SetWindowFullscreen(window, fullscreen);
            SDL_SetWindowRelativeMouseMode(window, fullscreen);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!SDL_GetWindowRelativeMouseMode(window))
        {
            SDL_SetWindowRelativeMouseMode(window, true);
        }
        else if (event->button.button == SDL_BUTTON_LEFT)
        {
            Player_BreakBlock(&player);
        }
        else if (event->button.button == SDL_BUTTON_MIDDLE)
        {
            Player_SelectBlock(&player);
        }
        else if (event->button.button == SDL_BUTTON_RIGHT)
        {
            Player_PlaceBlock(&player);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        Player_ChangeBlock(&player, event->wheel.y);
        break;
    }
    return SDL_APP_CONTINUE;
}
