#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "block.h"
#include "camera.h"
#include "database.h"
#include "pipeline.h"
#include "raycast.h"
#include "world.h"

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUCommandBuffer* commands;
static uint32_t width;
static uint32_t height;
static uint32_t render_width;
static uint32_t render_height;
static SDL_GPUBuffer* cube_vbo;
static SDL_GPUTexture* color_texture;
static SDL_GPUTexture* depth_texture;
static SDL_GPUTexture* shadow_texture;
static SDL_GPUTexture* position_texture;
static SDL_GPUTexture* uv_texture;
static SDL_GPUTexture* voxel_texture;
static SDL_GPUTexture* ssao_texture;
static SDL_GPUTexture* random_texture;
static SDL_GPUTexture* atlas_texture;
static SDL_GPUTexture* composite_texture;
static SDL_GPUSampler* nearest_sampler;
static SDL_GPUSampler* linear_sampler;
static SDL_Surface* atlas_surface;
static camera_t player_camera;
static camera_t shadow_camera;
static uint64_t time1;
static uint64_t time2;
static float cooldown;
static block_t selected = BLOCK_GRASS;

static bool create_atlas()
{
    char path[512] = {0};
    snprintf(path, sizeof(path), "%satlas.png", SDL_GetBasePath());
    atlas_surface = SDL_LoadPNG(path);
    if (!atlas_surface)
    {
        SDL_Log("Failed to create atlas surface: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureCreateInfo tci = {0};
    tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tci.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    tci.layer_count_or_depth = ATLAS_WIDTH / BLOCK_WIDTH;
    tci.num_levels = ATLAS_LEVELS;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tci.width = BLOCK_WIDTH;
    tci.height = BLOCK_WIDTH;
    atlas_texture = SDL_CreateGPUTexture(device, &tci);
    if (!atlas_texture)
    {
        SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
        return false;
    }
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tci.width = atlas_surface->w;
    tci.height = atlas_surface->h;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &tci);
    if (!texture)
    {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.size = atlas_surface->w * atlas_surface->h * 4;
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!buffer)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return false;
    }
    void* data = SDL_MapGPUTransferBuffer(device, buffer, 0);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return false;
    }
    memcpy(data, atlas_surface->pixels, atlas_surface->w * atlas_surface->h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureTransferInfo tti = {0};
    SDL_GPUTextureRegion region = {0};
    tti.transfer_buffer = buffer;
    region.texture = texture;
    region.w = atlas_surface->w;
    region.h = atlas_surface->h;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &tti, &region, 0);
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
        SDL_BlitGPUTexture(commands, &info);
    }
    SDL_GenerateMipmapsForGPUTexture(commands, atlas_texture);
    SDL_SubmitGPUCommandBuffer(commands);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return true;
}

static bool create_samplers()
{
    SDL_GPUSamplerCreateInfo sci = {0};
    sci.min_filter = SDL_GPU_FILTER_LINEAR;
    sci.mag_filter = SDL_GPU_FILTER_LINEAR;
    sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linear_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!linear_sampler)
    {
        SDL_Log("Failed to create linear sampler: %s", SDL_GetError());
        return false;
    }
    sci.min_filter = SDL_GPU_FILTER_NEAREST;
    sci.mag_filter = SDL_GPU_FILTER_NEAREST;
    nearest_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!nearest_sampler)
    {
        SDL_Log("Failed to create nearest sampler: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool create_textures()
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
        return false;
    }
    return true;
}

static bool resize_textures()
{
    if (depth_texture)
    {
        SDL_ReleaseGPUTexture(device, depth_texture);
        depth_texture = NULL;
    }
    if (position_texture)
    {
        SDL_ReleaseGPUTexture(device, position_texture);
        position_texture = NULL;
    }
    if (uv_texture)
    {
        SDL_ReleaseGPUTexture(device, uv_texture);
        uv_texture = NULL;
    }
    if (voxel_texture)
    {
        SDL_ReleaseGPUTexture(device, voxel_texture);
        voxel_texture = NULL;
    }
    if (ssao_texture)
    {
        SDL_ReleaseGPUTexture(device, ssao_texture);
        ssao_texture = NULL;
    }
    if (random_texture)
    {
        SDL_ReleaseGPUTexture(device, random_texture);
        random_texture = NULL;
    }
    if (composite_texture)
    {
        SDL_ReleaseGPUTexture(device, composite_texture);
        composite_texture = NULL;
    }
    SDL_GPUTextureCreateInfo tci = {0};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    tci.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    tci.width = render_width;
    tci.height = render_height;
    depth_texture = SDL_CreateGPUTexture(device, &tci);
    if (!depth_texture)
    {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    position_texture = SDL_CreateGPUTexture(device, &tci);
    if (!position_texture)
    {
        SDL_Log("Failed to create position texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    uv_texture = SDL_CreateGPUTexture(device, &tci);
    if (!uv_texture)
    {
        SDL_Log("Failed to create uv texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32_UINT;
    voxel_texture = SDL_CreateGPUTexture(device, &tci);
    if (!voxel_texture)
    {
        SDL_Log("Failed to create voxel texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    ssao_texture = SDL_CreateGPUTexture(device, &tci);
    if (!ssao_texture)
    {
        SDL_Log("Failed to create ssao texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
    random_texture = SDL_CreateGPUTexture(device, &tci);
    if (!random_texture)
    {
        SDL_Log("Failed to create random texture: %s", SDL_GetError());
        return false;
    }
    tci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
    composite_texture = SDL_CreateGPUTexture(device, &tci);
    if (!composite_texture)
    {
        SDL_Log("Failed to create composite texture: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_Surface* create_icon(
    const block_t block)
{
    assert(block < BLOCK_COUNT);
    if (!atlas_surface)
    {
        return NULL;
    }
    SDL_Surface* icon = SDL_CreateSurface(BLOCK_WIDTH, BLOCK_WIDTH, SDL_PIXELFORMAT_RGBA32);
    if (!icon)
    {
        SDL_Log("Failed to create icon surface: %s", SDL_GetError());
        return NULL;
    }
    SDL_Rect src;
    src.x = blocks[block][0] * BLOCK_WIDTH;
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
        return NULL;
    }
    return icon;
}

static bool create_vbos()
{
    const float cube[][3] =
    {
        {-1,-1,-1 }, { 1,-1,-1 }, { 1, 1,-1 },
        {-1,-1,-1 }, { 1, 1,-1 }, {-1, 1,-1 },
        { 1,-1, 1 }, { 1, 1, 1 }, {-1, 1, 1 },
        { 1,-1, 1 }, {-1, 1, 1 }, {-1,-1, 1 },
        {-1,-1,-1 }, {-1, 1,-1 }, {-1, 1, 1 },
        {-1,-1,-1 }, {-1, 1, 1 }, {-1,-1, 1 },
        { 1,-1,-1 }, { 1,-1, 1 }, { 1, 1, 1 },
        { 1,-1,-1 }, { 1, 1, 1 }, { 1, 1,-1 },
        {-1, 1,-1 }, { 1, 1,-1 }, { 1, 1, 1 },
        {-1, 1,-1 }, { 1, 1, 1 }, {-1, 1, 1 },
        {-1,-1,-1 }, {-1,-1, 1 }, { 1,-1, 1 },
        {-1,-1,-1 }, { 1,-1, 1 }, { 1,-1,-1 },
    };
    SDL_GPUBufferCreateInfo bci = {0};
    bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bci.size = sizeof(cube);
    cube_vbo = SDL_CreateGPUBuffer(device, &bci);
    if (!cube_vbo)
    {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTransferBufferCreateInfo tbci = {0};
    tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbci.size = sizeof(cube);
    SDL_GPUTransferBuffer* tbo = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!tbo)
    {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return false;
    }
    void* data = SDL_MapGPUTransferBuffer(device, tbo, false);
    if (!data)
    {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return false;
    }
    memcpy(data, cube, sizeof(cube));
    SDL_UnmapGPUTransferBuffer(device, tbo);
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    } 
    SDL_GPUTransferBufferLocation location = {0};
    SDL_GPUBufferRegion region = {0};
    location.transfer_buffer = tbo;
    region.size = sizeof(cube);
    region.buffer = cube_vbo;
    SDL_UploadToGPUBuffer(pass, &location, &region, 1);
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(commands); 
    SDL_ReleaseGPUTransferBuffer(device, tbo);
    return true;
}

static void draw_random()
{
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (!commands)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return;
    }
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_CLEAR;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = random_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands);
        return;
    }
    pipeline_bind(pass, PIPELINE_RANDOM);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(commands);
}

static void draw_sky()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_DONT_CARE;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = composite_texture;
    cti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUBufferBinding bb = {0};
    bb.buffer = cube_vbo;
    pipeline_bind(pass, PIPELINE_SKY);
    SDL_PushGPUVertexUniformData(commands, 0, player_camera.view, 64);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.proj, 64);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_shadow()
{
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1.0f;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dsti.texture = shadow_texture;
    dsti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, NULL, 0, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    pipeline_bind(pass, PIPELINE_SHADOW);
    SDL_PushGPUVertexUniformData(commands, 1, shadow_camera.matrix, 64);
    world_render(NULL, commands, pass, CHUNK_TYPE_OPAQUE);
    SDL_EndGPURenderPass(pass);
}

static void draw_opaque()
{
    SDL_GPUColorTargetInfo cti[3] = {0};
    cti[0].load_op = SDL_GPU_LOADOP_CLEAR;
    cti[0].store_op = SDL_GPU_STOREOP_STORE;
    cti[0].texture = position_texture;
    cti[0].cycle = true;
    cti[1].load_op = SDL_GPU_LOADOP_CLEAR;
    cti[1].store_op = SDL_GPU_STOREOP_STORE;
    cti[1].texture = uv_texture;
    cti[2].cycle = true;
    cti[2].load_op = SDL_GPU_LOADOP_DONT_CARE;
    cti[2].store_op = SDL_GPU_STOREOP_STORE;
    cti[2].texture = voxel_texture;
    cti[2].cycle = true;
    SDL_GPUDepthStencilTargetInfo dsti = {0};
    dsti.clear_depth = 1.0f;
    dsti.load_op = SDL_GPU_LOADOP_CLEAR;
    dsti.store_op = SDL_GPU_STOREOP_STORE;
    dsti.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dsti.texture = depth_texture;
    dsti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, cti, 3, &dsti);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureSamplerBinding tsb = {0};
    tsb.sampler = nearest_sampler;
    tsb.texture = atlas_texture;
    pipeline_bind(pass, PIPELINE_OPAQUE);
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.view, 64);
    SDL_PushGPUVertexUniformData(commands, 2, player_camera.proj, 64);
    world_render(&player_camera, commands, pass, CHUNK_TYPE_OPAQUE);
    SDL_EndGPURenderPass(pass);
}

static void draw_ssao()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_CLEAR;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = ssao_texture;
    cti.cycle = true;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureSamplerBinding tsb[4] = {0};
    tsb[0].sampler = nearest_sampler;
    tsb[0].texture = position_texture;
    tsb[1].sampler = nearest_sampler;
    tsb[1].texture = uv_texture;
    tsb[2].sampler = nearest_sampler;
    tsb[2].texture = voxel_texture;
    tsb[3].sampler = nearest_sampler;
    tsb[3].texture = random_texture;
    pipeline_bind(pass, PIPELINE_SSAO);
    SDL_BindGPUFragmentSamplers(pass, 0, tsb, 4);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void composite()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = composite_texture;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &cti, 1, NULL);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    float position[3];
    float vector[3];
    SDL_GPUTextureSamplerBinding tsb[6] = {0};
    tsb[0].sampler = nearest_sampler;
    tsb[0].texture = atlas_texture;
    tsb[1].sampler = nearest_sampler;
    tsb[1].texture = position_texture;
    tsb[2].sampler = nearest_sampler;
    tsb[2].texture = uv_texture;
    tsb[3].sampler = nearest_sampler;
    tsb[3].texture = voxel_texture;
    tsb[4].sampler = linear_sampler;
    tsb[4].texture = shadow_texture;
    tsb[5].sampler = nearest_sampler;
    tsb[5].texture = ssao_texture;
    camera_get_position(&player_camera, &position[0], &position[1], &position[2]);
    camera_get_vector(&shadow_camera, &vector[0], &vector[1], &vector[2]);
    pipeline_bind(pass, PIPELINE_COMPOSITE);
    SDL_BindGPUFragmentSamplers(pass, 0, tsb, 6);
    SDL_PushGPUFragmentUniformData(commands, 0, position, 12);
    SDL_PushGPUFragmentUniformData(commands, 1, vector, 12);
    SDL_PushGPUFragmentUniformData(commands, 2, shadow_camera.matrix, 64);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw_transparent()
{
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = composite_texture;
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
    SDL_GPUTextureSamplerBinding tsb[3] = {0};
    tsb[0].sampler = nearest_sampler;
    tsb[0].texture = atlas_texture;
    tsb[1].sampler = linear_sampler;
    tsb[1].texture = shadow_texture;
    tsb[2].sampler = nearest_sampler;
    tsb[2].texture = position_texture;
    camera_get_position(&player_camera, &position[0], &position[1], &position[2]);
    camera_get_vector(&shadow_camera, &vector[0], &vector[1], &vector[2]);
    pipeline_bind(pass, PIPELINE_TRANSPARENT);
    SDL_PushGPUVertexUniformData(commands, 1, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 2, position, 12);
    SDL_PushGPUVertexUniformData(commands, 3, shadow_camera.matrix, 64);
    SDL_PushGPUFragmentUniformData(commands, 0, vector, 12);
    SDL_PushGPUFragmentUniformData(commands, 1, position, 12);
    SDL_BindGPUFragmentSamplers(pass, 0, tsb, 3);
    world_render(&player_camera, commands, pass, CHUNK_TYPE_TRANSPARENT);
    SDL_EndGPURenderPass(pass);
}

static void draw_raycast()
{
    float x, y, z;
    float a, b, c;
    camera_get_position(&player_camera, &x, &y, &z);
    camera_get_vector(&player_camera, &a, &b, &c);
    if (!raycast(&x, &y, &z, a, b, c, false))
    {
        return;
    }
    SDL_GPUColorTargetInfo cti = {0};
    cti.load_op = SDL_GPU_LOADOP_LOAD;
    cti.store_op = SDL_GPU_STOREOP_STORE;
    cti.texture = composite_texture;
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
    pipeline_bind(pass, PIPELINE_RAYCAST);
    SDL_PushGPUVertexUniformData(commands, 0, player_camera.matrix, 64);
    SDL_PushGPUVertexUniformData(commands, 1, position, 12);
    SDL_BindGPUVertexBuffers(pass, 0, &bb, 1);
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void blit()
{
    SDL_GPUBlitInfo blit = {0};
    blit.source.x = 0;
    blit.source.y = 0;
    blit.source.w = render_width;
    blit.source.h = render_height;
    blit.source.texture = composite_texture;
    blit.destination.x = 0;
    blit.destination.y = 0;
    blit.destination.w = width;
    blit.destination.h = height;
    blit.destination.texture = color_texture;
    blit.load_op = SDL_GPU_LOADOP_CLEAR;
    blit.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(commands, &blit);
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
    int32_t viewport[2] = { width, height };
    SDL_GPUTextureSamplerBinding tsb = {0};
    tsb.sampler = nearest_sampler;
    tsb.texture = atlas_texture;
    pipeline_bind(pass, PIPELINE_UI);
    SDL_BindGPUFragmentSamplers(pass, 0, &tsb, 1);
    SDL_PushGPUFragmentUniformData(commands, 0, viewport, 8);
    SDL_PushGPUFragmentUniformData(commands, 1, blocks[selected], 4);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

static void draw()
{
    SDL_WaitForGPUSwapchain(device, window);
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
        SDL_CancelGPUCommandBuffer(commands);
        return;
    }
    if (!color_texture || !w || !h)
    {
        SDL_SubmitGPUCommandBuffer(commands);
        return;
    }
    if (width != w || height != h)
    {
        const float ratio = (float) w / (float) h;
        render_width = RENDERER_SIZE;
        render_height = (float) RENDERER_SIZE / ratio;
        if (!resize_textures(render_width, render_height))
        {
            SDL_Log("Failed to resize textures");
            SDL_SubmitGPUCommandBuffer(commands);
            return;
        }
        camera_set_viewport(&player_camera, render_width, render_height);
        width = w;
        height = h;
        draw_random();
    }
    camera_update(&player_camera);
    camera_update(&shadow_camera);
    {
        SDL_PushGPUDebugGroup(commands, "sky");
        draw_sky();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "shadow");
        draw_shadow();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "opaque");
        draw_opaque();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "ssao");
        draw_ssao();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "composite");
        composite();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "transparent");
        draw_transparent();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "raycast");
        draw_raycast();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "blit");
        blit();
        SDL_PopGPUDebugGroup(commands);
    }
    {
        SDL_PushGPUDebugGroup(commands, "ui");
        draw_ui();
        SDL_PopGPUDebugGroup(commands);
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
            if (event.button.button & (INPUT_PLACE | INPUT_BREAK))
            {
                bool previous = true;
                block_t block = selected;
                if (event.button.button == INPUT_BREAK)
                {
                    previous = false;
                    block = BLOCK_EMPTY;
                }
                float x, y, z;
                float a, b, c;
                camera_get_position(&player_camera, &x, &y, &z);
                camera_get_vector(&player_camera, &a, &b, &c);
                if (raycast(&x, &y, &z, a, b, c, previous) && y >= 1.0f)
                {
                    world_set_block(x, y, z, block);
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == INPUT_PAUSE)
            {
                SDL_SetWindowRelativeMouseMode(window, false);
                SDL_SetWindowFullscreen(window, false);
            }
            else if (event.key.scancode == INPUT_BLOCK)
            {
                selected = (selected + 1) % BLOCK_COUNT;
                selected = clamp(selected, BLOCK_EMPTY + 1, BLOCK_COUNT - 1);
            }
            else if (event.key.scancode == INPUT_FULLSCREEN)
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
        case SDL_EVENT_QUIT:
            return false;
        }
    }
    return true;
}

static void move(
    const float dt)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (SDL_GetWindowRelativeMouseMode(window))
    {
        float speed = PLAYER_SPEED;
        const bool* state = SDL_GetKeyboardState(NULL);
        x += state[INPUT_RIGHT];
        x -= state[INPUT_LEFT];
        y += state[INPUT_UP];
        y -= state[INPUT_DOWN];
        z += state[INPUT_FORWARD];
        z -= state[INPUT_BACKWARD];
        if (state[INPUT_FAST])
        {
            speed *= 5.0f;
        }
        else if (state[INPUT_SLOW])
        {
            speed /= 5.0f;
        }
        x *= speed * dt;
        y *= speed * dt;
        z *= speed * dt;
        camera_move(&player_camera, x, y, z);
    }
    camera_get_position(&player_camera, &x, &y, &z);
    int a = x;
    int c = z;
    a /= CHUNK_X;
    c /= CHUNK_Z;
    a *= CHUNK_X;
    c *= CHUNK_Z;
    camera_set_position(&shadow_camera, a, SHADOW_Y, c);
}

static void commit(
    const float dt)
{
    cooldown += dt;
    if (cooldown < DATABASE_COOLDOWN)
    {
        return;
    }
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    camera_get_position(&player_camera, &x, &y, &z);
    camera_get_rotation(&player_camera, &pitch, &yaw);
    database_set_player(DATABASE_PLAYER, x, y, z, pitch, yaw);
    database_commit();
    cooldown = 0.0f;
}

int main(
    int argc,
    char** argv)
{
    SDL_SetAppMetadata(WINDOW_NAME, NULL, NULL);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    window = SDL_CreateWindow(WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window)
    {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL, DEVICE_DEBUG, NULL);
    if (!device)
    {
        SDL_Log("Failed to create device: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(device, window))
    {
        SDL_Log("Failed to create swapchain: %s", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (!pipeline_init(device, SDL_GetGPUSwapchainTextureFormat(device, window)))
    {
        SDL_Log("Failed to create pipelines");
        return EXIT_FAILURE;
    }
    if (!create_atlas())
    {
        SDL_Log("Failed to create atlas");
        return EXIT_FAILURE;
    }
    if (!create_samplers())
    {
        SDL_Log("Failed to create samplers");
        return EXIT_FAILURE;
    }
    if (!create_textures())
    {
        SDL_Log("Failed to create textures");
        return EXIT_FAILURE;
    }
    if (!create_vbos())
    {
        SDL_Log("Failed to create vbos");
        return EXIT_FAILURE;
    }
    if (!database_init(DATABASE_PATH))
    {
        SDL_Log("Failed to create database");
        return EXIT_FAILURE;
    }
    if (!world_init(device))
    {
        SDL_Log("Failed to create world");
        return EXIT_FAILURE;
    }
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    SDL_Surface* icon = create_icon(WINDOW_ICON);
    SDL_SetWindowIcon(window, icon);
    SDL_DestroySurface(icon);
    SDL_GPUSwapchainComposition composition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    SDL_SetGPUSwapchainParameters(device, window, composition, mode);
    float x;
    float y;
    float z;
    float pitch;
    float yaw;
    camera_init(&player_camera, CAMERA_TYPE_PERSPECTIVE);
    if (database_get_player(DATABASE_PLAYER, &x, &y, &z, &pitch, &yaw))
    {
        camera_set_position(&player_camera, x, y, z);
        camera_set_rotation(&player_camera, pitch, yaw);
    }
    else
    {
        srand(time(NULL));
        x = rand() % INT16_MAX;
        z = rand() % INT16_MAX;
        camera_set_position(&player_camera, x, PLAYER_Y, z);
    }
    camera_init(&shadow_camera, CAMERA_TYPE_ORTHO);
    camera_set_rotation(&shadow_camera, SHADOW_PITCH, SHADOW_YAW);
    move(0.0f);
    time1 = SDL_GetPerformanceCounter();
    time2 = 0;
    while (true)
    {
        time2 = time1;
        time1 = SDL_GetPerformanceCounter();
        const float frequency = SDL_GetPerformanceFrequency();
        const float dt = (time1 - time2) * 1000.0f / frequency;
        if (!poll())
        {
            break;
        }
        move(dt);
        camera_get_position(&player_camera, &x, &y, &z);
        world_update(x, y, z);
        draw();
        commit(dt);
    }
    world_free();
    commit(DATABASE_COOLDOWN);
    database_free();
    pipeline_free();
    SDL_ReleaseGPUBuffer(device, cube_vbo);
    SDL_ReleaseGPUTexture(device, depth_texture);
    SDL_ReleaseGPUTexture(device, position_texture);
    SDL_ReleaseGPUTexture(device, uv_texture);
    SDL_ReleaseGPUTexture(device, voxel_texture);
    SDL_ReleaseGPUTexture(device, ssao_texture);
    SDL_ReleaseGPUTexture(device, random_texture);
    SDL_ReleaseGPUTexture(device, composite_texture);
    SDL_ReleaseGPUTexture(device, shadow_texture);
    SDL_ReleaseGPUTexture(device, atlas_texture);
    SDL_ReleaseGPUSampler(device, nearest_sampler);
    SDL_ReleaseGPUSampler(device, linear_sampler);
    SDL_DestroySurface(atlas_surface);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}