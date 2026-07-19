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
static const int MIP_LEVELS = 4;
static const SDL_GPUSampleCount SAMPLE_COUNT = SDL_GPU_SAMPLECOUNT_4;

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTextureFormat color_format;
static SDL_GPUTextureFormat depth_format;
static SDL_GPUGraphicsPipeline* opaque_pipeline;
static SDL_GPUGraphicsPipeline* transparent_pipeline;
static SDL_GPUGraphicsPipeline* sky_pipeline;
static SDL_GPUGraphicsPipeline* raycast_pipeline;
static SDL_GPUGraphicsPipeline* hud_pipeline;
static SDL_Surface* atlas_surface;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUTexture* position_texture;
static SDL_GPUTexture* msaa_color_texture;
static SDL_GPUTexture* msaa_position_texture;
static SDL_GPUBuffer* block_buffer;
static SDL_GPUSampler* nearest_sampler;
static Sky sky;
static Player player;
static Uint64 ticks1;

static bool CreateAtlas()
{
    char path[1024] = {0};
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
        info.layer_count_or_depth = atlas_surface->w / BLOCK_WIDTH;
        info.num_levels = MIP_LEVELS;
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
    for (int layer = 0; layer < atlas_surface->w / BLOCK_WIDTH; layer++)
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
    if (MIP_LEVELS > 1)
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
    SDL_assert(atlas_surface);
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
    opaque_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return opaque_pipeline != NULL;
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
    transparent_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return transparent_pipeline != NULL;
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
    sky_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
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
    raycast_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return raycast_pipeline != NULL;
}

static bool CreateHudPipeline()
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
    info.vertex_shader = Shader_Load(device, "hud.vert");
    info.fragment_shader = Shader_Load(device, "hud.frag");
    info.target_info.num_color_targets = 1;
    info.target_info.color_target_descriptions = &color_target;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    hud_pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return hud_pipeline != NULL;
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
    window = SDL_CreateWindow("Blocks", 960, 720, SDL_WINDOW_RESIZABLE);
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
    SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
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
    if (!CreateHudPipeline())
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
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    SetWindowIcon();
    Save_Init(SAVE_PATH);
    Sky_Load(&sky);
    World_Init(device);
    Player_Load(&player);
    Sky_Update(&sky, 0.0f);
    World_Update(&player.camera);
    ticks1 = SDL_GetTicks();
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
    SDL_ReleaseGPUTexture(device, msaa_position_texture);
    SDL_ReleaseGPUTexture(device, msaa_color_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_ReleaseGPUBuffer(device, block_buffer);
    SDL_DestroySurface(atlas_surface);
    SDL_ReleaseGPUGraphicsPipeline(device, hud_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, raycast_pipeline);
    SDL_ReleaseGPUGraphicsPipeline(device, sky_pipeline);
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
    SDL_ReleaseGPUTexture(device, msaa_color_texture);
    SDL_ReleaseGPUTexture(device, msaa_position_texture);
    depth_texture = NULL;
    position_texture = NULL;
    msaa_color_texture = NULL;
    msaa_position_texture = NULL;
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
    msaa_color_texture = SDL_CreateGPUTexture(device, &info);
    if (!msaa_color_texture)
    {
        SDL_Log("Failed to create multisample color texture: %s", SDL_GetError());
        return false;
    }
    info.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    msaa_position_texture = SDL_CreateGPUTexture(device, &info);
    if (!msaa_position_texture)
    {
        SDL_Log("Failed to create multisample position texture: %s", SDL_GetError());
        return false;
    }
    Camera_Resize(&player.camera, width, height);
    return true;
}

static void RenderOpaquePass(SDL_GPUCommandBuffer* command_buffer)
{
    SDL_GPUColorTargetInfo color_info[2] = {0};
    color_info[0].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[0].texture = msaa_color_texture;
    color_info[0].cycle = true;
    color_info[0].store_op = SDL_GPU_STOREOP_STORE;
    color_info[1].load_op = SDL_GPU_LOADOP_CLEAR;
    color_info[1].texture = msaa_position_texture;
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
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, color_info, 2, &depth_info);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_PushGPUDebugGroup(command_buffer, "sky");
    SDL_BindGPUGraphicsPipeline(render_pass, sky_pipeline);
    SDL_PushGPUVertexUniformData(command_buffer, 0, player.camera.proj, sizeof(player.camera.proj));
    SDL_PushGPUVertexUniformData(command_buffer, 1, player.camera.view, sizeof(player.camera.view));
    SDL_PushGPUFragmentUniformData(command_buffer, 0, sky.sun, sizeof(float) * 16);
    SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
    SDL_PopGPUDebugGroup(command_buffer);
    SDL_GPUTextureSamplerBinding sampler_binding = {0};
    sampler_binding.texture = atlas_texture;
    sampler_binding.sampler = nearest_sampler;
    SDL_PushGPUDebugGroup(command_buffer, "opaque");
    SDL_BindGPUGraphicsPipeline(render_pass, opaque_pipeline);
    SDL_PushGPUFragmentUniformData(command_buffer, 1, player.camera.position, sizeof(player.camera.position));
    SDL_PushGPUFragmentUniformData(command_buffer, 2, sky.sun, sizeof(float) * 16);
    SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);
    SDL_BindGPUFragmentStorageBuffers(render_pass, 0, &block_buffer, 1);
    World_Render(&player.camera, WORLD_MESH_TYPE_OPAQUE, command_buffer, render_pass);
    SDL_PopGPUDebugGroup(command_buffer);
    SDL_EndGPURenderPass(render_pass);
}

static void RenderTransparentPass(SDL_GPUCommandBuffer* command_buffer, SDL_GPUTexture* swapchain_texture)
{
    SDL_GPUColorTargetInfo color_info = {0};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.texture = msaa_color_texture;
    color_info.store_op = SDL_GPU_STOREOP_RESOLVE;
    color_info.resolve_texture = swapchain_texture;
    SDL_GPUDepthStencilTargetInfo depth_info = {0};
    depth_info.load_op = SDL_GPU_LOADOP_LOAD;
    depth_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_info.texture = depth_texture;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_info, 1, &depth_info);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureSamplerBinding sampler_bindings[2] = {0};
    sampler_bindings[0].texture = atlas_texture;
    sampler_bindings[0].sampler = nearest_sampler;
    sampler_bindings[1].texture = position_texture;
    sampler_bindings[1].sampler = nearest_sampler;
    SDL_PushGPUDebugGroup(command_buffer, "transparent");
    SDL_BindGPUGraphicsPipeline(render_pass, transparent_pipeline);
    SDL_PushGPUFragmentUniformData(command_buffer, 1, player.camera.position, sizeof(player.camera.position));
    SDL_PushGPUFragmentUniformData(command_buffer, 2, sky.sun, sizeof(float) * 16);
    SDL_BindGPUFragmentSamplers(render_pass, 0, sampler_bindings, 2);
    SDL_BindGPUFragmentStorageBuffers(render_pass, 0, &block_buffer, 1);
    World_Render(&player.camera, WORLD_MESH_TYPE_TRANSPARENT, command_buffer, render_pass);
    SDL_PopGPUDebugGroup(command_buffer);
    if (player.query.block != BLOCK_EMPTY)
    {
        SDL_PushGPUDebugGroup(command_buffer, "raycast");
        SDL_BindGPUGraphicsPipeline(render_pass, raycast_pipeline);
        SDL_PushGPUVertexUniformData(command_buffer, 0, player.camera.matrix, sizeof(player.camera.matrix));
        SDL_PushGPUVertexUniformData(command_buffer, 1, player.query.current, sizeof(player.query.current));
        SDL_DrawGPUPrimitives(render_pass, 36, 1, 0, 0);
        SDL_PopGPUDebugGroup(command_buffer);
    }
    SDL_EndGPURenderPass(render_pass);
}

static void RenderHudPass(SDL_GPUCommandBuffer* command_buffer, SDL_GPUTexture* swapchain_texture)
{
    SDL_GPUColorTargetInfo color_info = {0};
    color_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_info.store_op = SDL_GPU_STOREOP_STORE;
    color_info.texture = swapchain_texture;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_info, 1, NULL);
    if (!render_pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    Uint32 block = Block_GetIndex(player.block, DIRECTION_NORTH);
    SDL_GPUTextureSamplerBinding sampler_binding = {0};
    sampler_binding.texture = atlas_texture;
    sampler_binding.sampler = nearest_sampler;
    SDL_PushGPUDebugGroup(command_buffer, "ui");
    SDL_BindGPUGraphicsPipeline(render_pass, hud_pipeline);
    SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);
    SDL_PushGPUVertexUniformData(command_buffer, 0, &player.camera.size, sizeof(player.camera.size));
    SDL_PushGPUFragmentUniformData(command_buffer, 0, &block, sizeof(block));
    SDL_DrawGPUPrimitives(render_pass, 6, 3, 0, 0);
    SDL_PopGPUDebugGroup(command_buffer);
    SDL_EndGPURenderPass(render_pass);
}

static void Render()
{
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUTexture* swapchain_texture;
    Uint32 width;
    Uint32 height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, &width, &height))
    {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        return;
    }
    if (!swapchain_texture || !width || !height)
    {
        SDL_SubmitGPUCommandBuffer(command_buffer);
        return;
    }
    if ((width != player.camera.width || height != player.camera.height) && !Resize(width, height))
    {
        SDL_SubmitGPUCommandBuffer(command_buffer);
        return;
    }
    Camera_Update(&player.camera);
    RenderOpaquePass(command_buffer);
    RenderTransparentPass(command_buffer, swapchain_texture);
    RenderHudPass(command_buffer, swapchain_texture);
    SDL_SubmitGPUCommandBuffer(command_buffer);
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    Uint64 ticks2 = SDL_GetTicks();
    float dt = ticks2 - ticks1;
    ticks1 = ticks2;
    if (SDL_GetWindowRelativeMouseMode(window))
    {
        Player_Move(&player, dt);
        Sky_Update(&sky, dt / 1000.0f);
    }
    World_Update(&player.camera);
    Render();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event)
{
    switch (event->type)
    {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
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
            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
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
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            Player_ChangeBlock(&player, event->wheel.y);
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (SDL_GetWindowRelativeMouseMode(window))
        {
            Player_Rotate(&player, event->motion.yrel, event->motion.xrel);
        }
        break;
    }
    return SDL_APP_CONTINUE;
}
