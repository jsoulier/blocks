#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "camera.h"
#include "noise.h"
#include "save.h"
#include "shader.h"
#include "world.h"

static const float kAtlasWidth = 512.0f;
static const int kAtlasMipLevels = 4;
static const float kBlockWidth = 16.0f;

static const float kSpeed = 0.015f;
static const float kSensitivity = 0.1f;

static SDL_Window* window;
static SDL_GPUDevice* device;
static SDL_GPUTextureFormat colorTextureFormat;
static SDL_GPUTextureFormat depthTextureFormat;
static SDL_GPUTexture* depthTexture;
static SDL_GPUGraphicsPipeline* chunkPipeline;
static SDL_Surface* atlasSurface;
static SDL_GPUTexture* atlasTexture;
static SDL_GPUSampler* linearSampler;
static SDL_GPUSampler* nearestSampler;
static World world;
static Save save;
static Camera camera;
static Noise noise;
static Uint64 ticks;

static bool CreateAtlas()
{
    char path[512] = {0};
    SDL_snprintf(path, sizeof(path), "%satlas.png", SDL_GetBasePath());
    atlasSurface = SDL_LoadPNG(path);
    if (!atlasSurface)
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
        info.layer_count_or_depth = kAtlasWidth / kBlockWidth;
        info.num_levels = kAtlasMipLevels;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        info.width = kBlockWidth;
        info.height = kBlockWidth;
        atlasTexture = SDL_CreateGPUTexture(device, &info);
        if (!atlasTexture)
        {
            SDL_Log("Failed to create atlas texture: %s", SDL_GetError());
            return false;
        }
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = atlasSurface->w;
        info.height = atlasSurface->h;
        texture = SDL_CreateGPUTexture(device, &info);
        if (!texture)
        {
            SDL_Log("Failed to create texture: %s", SDL_GetError());
            return false;
        }
    }
    {
        SDL_GPUTransferBufferCreateInfo info = {0};
        info.size = atlasSurface->w * atlasSurface->h * 4;
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
    SDL_memcpy(data, atlasSurface->pixels, atlasSurface->w * atlasSurface->h * 4);
    SDL_UnmapGPUTransferBuffer(device, buffer);
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commandBuffer);
    if (!pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureTransferInfo info = {0};
    SDL_GPUTextureRegion region = {0};
    info.transfer_buffer = buffer;
    region.texture = texture;
    region.w = atlasSurface->w;
    region.h = atlasSurface->h;
    region.d = 1;
    SDL_UploadToGPUTexture(pass, &info, &region, 0);
    SDL_EndGPUCopyPass(pass);
    for (int i = 0; i < kAtlasWidth / kBlockWidth; i++)
    {
        SDL_GPUBlitInfo info = {0};
        info.source.texture = texture;
        info.source.x = i * kBlockWidth;
        info.source.y = 0;
        info.source.w = kBlockWidth;
        info.source.h = kBlockWidth;
        info.destination.texture = atlasTexture;
        info.destination.x = 0;
        info.destination.y = 0;
        info.destination.w = kBlockWidth;
        info.destination.h = kBlockWidth;
        info.destination.layer_or_depth_plane = i;
        SDL_BlitGPUTexture(commandBuffer, &info);
    }
    SDL_GenerateMipmapsForGPUTexture(commandBuffer, atlasTexture);
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_ReleaseGPUTransferBuffer(device, buffer);
    return true;
}

static bool CreateSamplers()
{
    SDL_GPUSamplerCreateInfo info = {0};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linearSampler = SDL_CreateGPUSampler(device, &info);
    if (!linearSampler)
    {
        SDL_Log("Failed to create linear sampler: %s", SDL_GetError());
        return false;
    }
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    nearestSampler = SDL_CreateGPUSampler(device, &info);
    if (!nearestSampler)
    {
        SDL_Log("Failed to create nearest sampler: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool CreateChunkPipeline()
{
    SDL_GPUGraphicsPipelineCreateInfo info =
    {
        .vertex_shader = LoadShader(device, "chunk.vert"),
        .fragment_shader = LoadShader(device, "chunk.frag"),
        .target_info =
        {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[])
            {{
                .format = colorTextureFormat,
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = depthTextureFormat,
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
            .enable_depth_test = true,
            .enable_depth_write = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
        },
        .rasterizer_state =
        {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
        },
    };
    if (info.vertex_shader && info.fragment_shader)
    {
        chunkPipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    }
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    return chunkPipeline != NULL;
}

SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char** argv)
{
#ifndef NDEBUG
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#endif
    SDL_SetAppMetadata("Voxel Raytracer", NULL, NULL);
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
    if (SDL_WindowSupportsGPUPresentMode(device, window, SDL_GPU_PRESENTMODE_MAILBOX))
    {
        SDL_SetGPUSwapchainParameters(device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_MAILBOX);
    }
    colorTextureFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    if (SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
    {
        depthTextureFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    else
    {
        depthTextureFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    }
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
    if (!CreateChunkPipeline())
    {
        SDL_Log("Failed to create chunk pipeline: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_ShowWindow(window);
    SDL_SetWindowResizable(window, true);
    SDL_FlashWindow(window, SDL_FLASH_BRIEFLY);
    CreateNoise(&noise, NoiseType1x1x1, 1337);
    CreateWorld(&world, device);
    CreateCamera(&camera, CameraTypePerspective);
    ticks = SDL_GetTicks();
    return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    DestroyWorld(&world);
    SDL_ReleaseGPUSampler(device, linearSampler);
    SDL_ReleaseGPUSampler(device, nearestSampler);
    SDL_ReleaseGPUTexture(device, atlasTexture);
    SDL_DestroySurface(atlasSurface);
    SDL_ReleaseGPUGraphicsPipeline(device, chunkPipeline);
    SDL_ReleaseGPUTexture(device, depthTexture);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

static void RenderWorldPass(SDL_GPUCommandBuffer* commandBuffer, SDL_GPUTexture* swapchainTexture)
{
    SDL_GPUColorTargetInfo colorInfo = {0};
    colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorInfo.store_op = SDL_GPU_STOREOP_STORE;
    colorInfo.texture = swapchainTexture;
    SDL_GPUDepthStencilTargetInfo depthInfo = {0};
    depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    depthInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depthInfo.store_op = SDL_GPU_STOREOP_STORE;
    depthInfo.texture = depthTexture;
    depthInfo.clear_depth = 1.0f;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorInfo, 1, &depthInfo);
    if (!pass)
    {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        return;
    }
    SDL_GPUTextureSamplerBinding texture = {0};
    texture.sampler = nearestSampler;
    texture.texture = atlasTexture;
    SDL_BindGPUGraphicsPipeline(pass, chunkPipeline);
    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
    SDL_PushGPUVertexUniformData(commandBuffer, 0, camera.Matrix, 64);
    RenderWorld(&world, &camera, commandBuffer, pass, ChunkMeshTypeDefault);
    SDL_EndGPURenderPass(pass);
}

void Move(float deltaTime)
{
    if (!SDL_GetWindowRelativeMouseMode(window))
    {
        return;
    }
    float speed = kSpeed;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    const bool* state = SDL_GetKeyboardState(NULL);
    dx += state[SDL_SCANCODE_D];
    dx -= state[SDL_SCANCODE_A];
    dy += state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE];
    dy -= state[SDL_SCANCODE_Q] || state[SDL_SCANCODE_LSHIFT];
    dz += state[SDL_SCANCODE_W];
    dz -= state[SDL_SCANCODE_S];
    if (state[SDL_SCANCODE_LCTRL])
    {
        speed *= 5.0f;
    }
    dx *= speed * deltaTime;
    dy *= speed * deltaTime;
    dz *= speed * deltaTime;
    MoveCamera(&camera, dx, dy, dz);
}

SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)
{
    Uint64 newTicks = SDL_GetTicks();
    float deltaTime = newTicks - ticks;
    ticks = newTicks;
    Move(deltaTime);
    UpdateWorld(&world, &camera, &save, &noise);
    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!commandBuffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_CONTINUE;
    }
    SDL_GPUTexture* swapchainTexture;
    Uint32 width;
    Uint32 height;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, &width, &height))
    {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commandBuffer);
        return SDL_APP_CONTINUE;
    }
    if (!swapchainTexture || !width || !height)
    {
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return SDL_APP_CONTINUE;
    }
    if (width != camera.Width || height != camera.Height)
    {
        SDL_ReleaseGPUTexture(device, depthTexture);
        SDL_GPUTextureCreateInfo info = {0};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = depthTextureFormat;
        info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        depthTexture = SDL_CreateGPUTexture(device, &info);
        if (!depthTexture)
        {
            SDL_Log("Failed to create depth texture: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(commandBuffer);
            return SDL_APP_CONTINUE;
        }
        SetCameraViewport(&camera, width, height);
    }
    UpdateCamera(&camera);
    {
        SDL_PushGPUDebugGroup(commandBuffer, "RenderWorld");
        RenderWorldPass(commandBuffer, swapchainTexture);
        SDL_PopGPUDebugGroup(commandBuffer);
    }
    SDL_SubmitGPUCommandBuffer(commandBuffer);
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
            float yaw = event->motion.xrel * kSensitivity;
            float pitch = -event->motion.yrel * kSensitivity;
            RotateCamera(&camera, pitch, yaw);
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode == SDL_SCANCODE_ESCAPE)
        {
            SDL_SetWindowRelativeMouseMode(window, false);
            SDL_SetWindowFullscreen(window, false);
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
            break;
        }
        break;
    }
    return SDL_APP_CONTINUE;
}
